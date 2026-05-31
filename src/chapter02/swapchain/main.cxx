#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <print>
#include <span>
#include <utility>
#include <vector>

#include <vk_mem_alloc.h>

#include "volk.h"
#include "GLFW/glfw3.h"

#include "vulkan/format.hxx"
#include "vulkan/assert.hxx"

import vkgc.bootstrap;
import vkgc.window;
import vkgc.vulkan_instance;
import vkgc.vulkan_surface;
import vkgc.vulkan_device;
import vkgc.vulkan_handle;
import vkgc.vulkan_object_registry;
import vkgc.vulkan_resource_helpers;
import vkgc.vulkan_presenter;
import vkgc.vulkan_frame_ring;

namespace vkgc
{
    std::uint32_t constexpr kFramesInFlight{2}; // belongs to renderer settings
}

[[nodiscard]]
static std::vector<vkgc::vk_image_view_handle> create_swapchain_image_views(
    vkgc::vulkan_object_registry& vk_object_registry,
    std::span<VkImage const> const images,
    VkFormat const format)
{
    std::vector<vkgc::vk_image_view_handle> image_view_handles;
    image_view_handles.reserve(images.size());

    for (std::uint32_t i{0}; auto image_handle : images)
    {
        VkImageViewCreateInfo const create_info{
            .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .image{image_handle},
            .viewType{VK_IMAGE_VIEW_TYPE_2D},
            .format{format},
            .components{
                .r{VK_COMPONENT_SWIZZLE_IDENTITY},
                .g{VK_COMPONENT_SWIZZLE_IDENTITY},
                .b{VK_COMPONENT_SWIZZLE_IDENTITY},
                .a{VK_COMPONENT_SWIZZLE_IDENTITY}
            },
            .subresourceRange{
                .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                .baseMipLevel{0},
                .levelCount{1},
                .baseArrayLayer{0},
                .layerCount{1}
            }
        };

        auto const debug_name = std::format("swapchain image [#{}]", i++);
        if (auto const view = vk_object_registry.create_image_view(create_info, debug_name.c_str()); view.is_valid())
        {
            image_view_handles.push_back(view);
            continue;
        }

        for (auto const image_view_handle : image_view_handles)
        {
            vk_object_registry.destroy_immediate(image_view_handle);
        }

        return {};
    }

    return image_view_handles;
}

[[nodiscard]]
static bool create_depth_attachment(
    vkgc::vulkan_object_registry& vk_object_registry,
    VkFormat const image_format,
    VkExtent3D const image_extent,
    vkgc::vk_image_handle& image,
    vkgc::vk_allocation_handle& memory,
    vkgc::vk_image_view_handle& image_view)
{
    VkImageCreateInfo const image_create_info{
        .sType{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .imageType{VK_IMAGE_TYPE_2D},
        .format{image_format},
        .extent{image_extent},
        .mipLevels{1},
        .arrayLayers{1},
        .samples{VK_SAMPLE_COUNT_1_BIT},
        .tiling{VK_IMAGE_TILING_OPTIMAL},
        .usage{VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
        .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
        .queueFamilyIndexCount{0},
        .pQueueFamilyIndices{nullptr},
        .initialLayout{VK_IMAGE_LAYOUT_UNDEFINED}
    };

    image = vk_object_registry.create_image(image_create_info, "depth attachment image");
    if (!image.is_valid())
    {
        return false;
    }

    VmaAllocationCreateInfo constexpr allocation_create_info{
        .flags{VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT},
        .usage{VMA_MEMORY_USAGE_UNKNOWN},
        .requiredFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
        .preferredFlags{0},
        .memoryTypeBits{0},
        .pool{VK_NULL_HANDLE},
        .pUserData{nullptr},
        .priority{0}
    };

    memory = vk_object_registry.allocate_image_memory(image, allocation_create_info);
    if (!memory.is_valid())
    {
        vk_object_registry.destroy_immediate(image);
        return false;
    }

    if (!bind_image_memory(vk_object_registry, image, memory))
    {
        vk_object_registry.destroy_immediate(image);
        vk_object_registry.destroy_immediate(memory);
        return false;
    }

    VkImageViewCreateInfo const depth_view_create_info{
        .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .image{vk_object_registry.resolve_handle(image)},
        .viewType{VK_IMAGE_VIEW_TYPE_2D},
        .format{image_format},
        .components{
            .r{VK_COMPONENT_SWIZZLE_IDENTITY},
            .g{VK_COMPONENT_SWIZZLE_IDENTITY},
            .b{VK_COMPONENT_SWIZZLE_IDENTITY},
            .a{VK_COMPONENT_SWIZZLE_IDENTITY}
        },
        .subresourceRange{
            .aspectMask{VK_IMAGE_ASPECT_DEPTH_BIT},
            .baseMipLevel{0},
            .levelCount{1},
            .baseArrayLayer{0},
            .layerCount{1}
        }
    };

    image_view = vk_object_registry.create_image_view(depth_view_create_info, "depth attachment image view");
    if (!image_view.is_valid())
    {
        vk_object_registry.destroy_immediate(image_view);
        vk_object_registry.destroy_immediate(image);
        vk_object_registry.destroy_immediate(memory);
        return false;
    }

    return true;
}

static bool run_app(std::uint32_t width, std::uint32_t height)
{
    vkgc::vulkan_instance vulkan_instance{{ .enable_validation{true} }};
    VKGC_VERIFY(vulkan_instance.is_valid());

    vkgc::window window{"Swapchain example", width, height};
    VKGC_VERIFY(window.is_valid());

    vkgc::vulkan_surface const window_surface = vulkan_instance.create_window_surface(window);
    VKGC_VERIFY(window_surface.is_valid());

    vkgc::vulkan_device vulkan_device = vulkan_instance.create_device({
        .surface{window_surface.handle()},
        .extensions{}
    });
    VKGC_VERIFY(vulkan_device.is_valid());

    vkgc::vulkan_object_registry vk_object_registry{vulkan_device};

    vkgc::swapchain_params swapchain_params{
        .preferred_surface_formats{ // display settings
            {.format{VK_FORMAT_B8G8R8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
            {.format{VK_FORMAT_R8G8B8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}
        },
        .preferred_present_modes{ // display settings
#if (VKGC_DEBUG_VULKAN == 0)
            VK_PRESENT_MODE_MAILBOX_KHR,
#endif
            VK_PRESENT_MODE_FIFO_RELAXED_KHR,
            VK_PRESENT_MODE_IMMEDIATE_KHR
        },
        .framebuffer_extent{.width{width}, .height{height}}, // display settings
        .min_image_count{2}, // renderer settings
        .image_usage_flags{ // renderer settings
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        }
    };

    vkgc::vulkan_presenter presenter{
        vulkan_device,
        vk_object_registry,
        window_surface.handle(),
        vkgc::kFramesInFlight,
        swapchain_params
    };
    VKGC_VERIFY(presenter.is_valid());

    auto connect_on_resize = window.connect_on_resize(
        [&width, &height, &presenter](std::uint32_t const new_width, std::uint32_t const new_height)
        {
            width = new_width;
            height = new_height;
            presenter.request_rebuild();
        });

    VkExtent2D const swapchain_image_extent = presenter.extent();

    auto swapchain_image_view_handles = create_swapchain_image_views(
        vk_object_registry,
        presenter.images(),
        presenter.surface_format().format);
    if (swapchain_image_view_handles.empty())
    {
        std::println(stderr, "[Vulkan] : Error : failed to crate swapchain image views");
        return false;
    }

    vkgc::vulkan_frame_ring frame_ring{vk_object_registry, vkgc::kFramesInFlight};

    auto const main_queue_command_pool = vk_object_registry.create_command_pool({
        .sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
        .pNext{nullptr},
        .flags{VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT},
        .queueFamilyIndex{vulkan_device.main_queue_family_index()}
    });
    if (!main_queue_command_pool.is_valid())
    {
        return false;
    }

    auto const command_buffers = vk_object_registry.allocate_command_buffers(
        main_queue_command_pool,
        vkgc::kFramesInFlight,
        true);
    if (command_buffers.empty())
    {
        return false;
    }

    VkFormat depth_attachment_format{VK_FORMAT_UNDEFINED};
    for (auto preferred_depth_format : {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT})
    {
        VkFormatProperties2 fmt_properties{
            .sType{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2},
            .pNext{nullptr},
            .formatProperties{}
        };

        vkGetPhysicalDeviceFormatProperties2(
            vulkan_device.physical_device(),
            preferred_depth_format,
            &fmt_properties);

        if ((fmt_properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        {
            depth_attachment_format = preferred_depth_format;
            break;
        }
    }

    if (!VKGC_ENSUREF(depth_attachment_format != VK_FORMAT_UNDEFINED, "failed to find supported depth format"))
    {
        return false;
    }

    vkgc::vk_image_handle depth_attachment_image;
    vkgc::vk_allocation_handle depth_attachment_image_memory;
    vkgc::vk_image_view_handle depth_attachment_image_view;

    if (!create_depth_attachment(
        vk_object_registry,
        depth_attachment_format,
        {.width{swapchain_image_extent.width}, .height{swapchain_image_extent.height}, .depth{1}},
        depth_attachment_image,
        depth_attachment_image_memory,
        depth_attachment_image_view))
    {
        std::println(stderr, "[Vulkan] : Error : failed to create depth attachment");
        return false;
    }

    auto rebuild_swapchain_resources = [&]() -> bool
    {
        if (width == 0 || height == 0)
        {
            return true;
        }

        VKGC_VERIFY_VKSUCCESS(vkDeviceWaitIdle(vulkan_device.handle()));

        vk_object_registry.destroy_immediate(depth_attachment_image_view);
        vk_object_registry.destroy_immediate(depth_attachment_image);
        vk_object_registry.destroy_immediate(depth_attachment_image_memory);

        for (auto const image_view_handle : swapchain_image_view_handles)
        {
            vk_object_registry.destroy_immediate(image_view_handle);
        }
        swapchain_image_view_handles.clear();

        swapchain_params.framebuffer_extent = {.width{width}, .height{height}};
        presenter.recreate_swapchain(window_surface.handle(), swapchain_params);
        VKGC_VERIFYF(presenter.is_valid(), "unexpected error occurred while recreating the swapchain");

        swapchain_image_view_handles = create_swapchain_image_views(
            vk_object_registry,
            presenter.images(),
            presenter.surface_format().format);
        if (swapchain_image_view_handles.empty())
        {
            std::println(stderr, "[Vulkan] : Error : failed to crate swapchain image views");
            return false;
        }

        if (!create_depth_attachment(
            vk_object_registry,
            depth_attachment_format,
            {.width{swapchain_image_extent.width}, .height{swapchain_image_extent.height}, .depth{1}},
            depth_attachment_image,
            depth_attachment_image_memory,
            depth_attachment_image_view))
        {
            std::println(stderr, "[Vulkan] : Error : failed to create depth attachment");
            return false;
        }

        return true;
    };

    std::uint32_t frame_index{vkgc::kFramesInFlight};

    vkgc::update_loop([&]
    {
        VKGC_CHECKF(
            frame_index >= vkgc::kFramesInFlight,
            "frame index has to be greater or equal to frames-in-flight number");

        if (window.should_close())
        {
            return false;
        }

        if (width == 0 || height == 0)
        {
            glfwWaitEvents();
            return true;
        }

        if (presenter.consume_rebuild_request())
        {
            if (!rebuild_swapchain_resources())
            {
                return false;
            }

            return true;
        }

        std::uint32_t const frame_slot_index = frame_ring.begin_frame(frame_index);
        if (!VKGC_ENSUREF(
            frame_slot_index != std::numeric_limits<std::uint32_t>::max(),
            "failed to begin [#{}] frame slot",
            frame_slot_index))
        {
            return false;
        }

        auto const frame_slot_semaphore = vk_object_registry.resolve_handle(frame_ring.frame_slot_semaphore());
        if (!VKGC_ENSURE_VKHANDLE(frame_slot_semaphore))
        {
            return false;
        }

        auto const acquired = presenter.acquire_image(frame_slot_index);
        if (!acquired.has_value())
        {
            VKGC_VERIFYF(
                acquired.error() != vkgc::present_status::out_of_date,
                "vulkan surface no longer compatible with the swapchain, unexpected on this stage of the frame");

            VKGC_VERIFYF(
                acquired.error() != vkgc::present_status::error,
                "unexpected error occurred while acquiring the swapchain image");
        }

        auto [swapchain_image_acquired_semaphore, present_wait_semaphore, swapchain_image_index] = acquired.value();

        auto const swapchain_image_acquired_semaphore_raw = vk_object_registry.resolve_handle(
            swapchain_image_acquired_semaphore);
        if (!VKGC_ENSURE_VKHANDLE(swapchain_image_acquired_semaphore_raw))
        {
            return false;
        }

        auto const present_wait_semaphore_raw = vk_object_registry.resolve_handle(present_wait_semaphore);
        if (!VKGC_ENSURE_VKHANDLE(present_wait_semaphore_raw))
        {
            return false;
        }

        auto const command_buffer = vk_object_registry.resolve_handle(command_buffers[frame_slot_index]);
        if (!VKGC_ENSURE_VKHANDLE(command_buffer))
        {
            return false;
        }

        if (!VKGC_ENSUREF_VKSUCCESS(
            vkResetCommandBuffer(command_buffer, 0),
            "failed to reset [#{}] frame render command buffer", frame_slot_index))
        {
            return false;
        }

        {
            VkCommandBufferBeginInfo constexpr begin_info{
                .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO},
                .pNext{nullptr},
                .flags{VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT},
                .pInheritanceInfo{nullptr}
            };

            if (!VKGC_ENSUREF_VKSUCCESS(
                vkBeginCommandBuffer(command_buffer, &begin_info),
                "failed to begin [#{}] frame render command buffer record", frame_slot_index))
            {
                return false;
            }
        }

        auto const depth_attachment_image_handle = vk_object_registry.resolve_handle(depth_attachment_image);
        if (!VKGC_ENSURE_VKHANDLE(depth_attachment_image_handle))
        {
            return false;
        }

        {
            std::array images_transition_barriers{
                VkImageMemoryBarrier2{
                    .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2},
                    .pNext{nullptr},
                    .srcStageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                    .srcAccessMask{0},
                    .dstStageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                    .dstAccessMask{VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
                    .oldLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                    .newLayout{VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                    .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .image{presenter.images()[swapchain_image_index]},
                    .subresourceRange{
                        .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                        .baseMipLevel{0},
                        .levelCount{1},
                        .baseArrayLayer{0},
                        .layerCount{1}
                    }
                },
                VkImageMemoryBarrier2{
                    .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2},
                    .pNext{nullptr},
                    .srcStageMask{VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT},
                    .srcAccessMask{VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
                    .dstStageMask{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT},
                    .dstAccessMask{VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
                    .oldLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                    .newLayout{VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                    .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .image{depth_attachment_image_handle},
                    .subresourceRange{
                        .aspectMask{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT},
                        .baseMipLevel{0},
                        .levelCount{1},
                        .baseArrayLayer{0},
                        .layerCount{1}
                    }
                }
            };

            VkDependencyInfo const dependency_info{
                .sType{VK_STRUCTURE_TYPE_DEPENDENCY_INFO},
                .pNext{nullptr},
                .dependencyFlags{},
                .memoryBarrierCount{0},
                .pMemoryBarriers{nullptr},
                .bufferMemoryBarrierCount{0},
                .pBufferMemoryBarriers{nullptr},
                .imageMemoryBarrierCount{static_cast<std::uint32_t>(images_transition_barriers.size())},
                .pImageMemoryBarriers{images_transition_barriers.data()},
            };

            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }

        // Update shader data
        // Record command buffer

        {
            // Swapchain image transition for following presentation
            VkImageMemoryBarrier2 const to_present{
                .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2},
                .pNext{nullptr},
                .srcStageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                .srcAccessMask{VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
                .dstStageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                .dstAccessMask{0},
                .oldLayout{VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                .newLayout{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
                .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .image{presenter.images()[swapchain_image_index]},
                .subresourceRange{
                    .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                    .baseMipLevel{0},
                    .levelCount{1},
                    .baseArrayLayer{0},
                    .layerCount{1}
                }
            };

            VkDependencyInfo const dependency_info{
                .sType{VK_STRUCTURE_TYPE_DEPENDENCY_INFO},
                .pNext{nullptr},
                .dependencyFlags{},
                .memoryBarrierCount{0},
                .pMemoryBarriers{nullptr},
                .bufferMemoryBarrierCount{0},
                .pBufferMemoryBarriers{nullptr},
                .imageMemoryBarrierCount{1},
                .pImageMemoryBarriers{&to_present},
            };

            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }

        if (!VKGC_ENSUREF_VKSUCCESS(
            vkEndCommandBuffer(command_buffer),
            "failed to end [#{}] frame render command buffer record", frame_slot_index))
        {
            return false;
        }

        {
            std::array const wait_semaphores_submit_info{
                VkSemaphoreSubmitInfo{
                    .sType{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},
                    .pNext{nullptr},
                    .semaphore{swapchain_image_acquired_semaphore_raw},
                    .value{0},
                    .stageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                    .deviceIndex{0}
                }
            };

            VkCommandBufferSubmitInfo const command_buffer_info{
                .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO},
                .pNext{nullptr},
                .commandBuffer{command_buffer},
                .deviceMask{0}
            };

            std::array const signal_semaphores_info{
                VkSemaphoreSubmitInfo{
                    .sType{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},
                    .pNext{nullptr},
                    .semaphore{frame_slot_semaphore},
                    .value{frame_index},
                    .stageMask{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT},
                    .deviceIndex{0}
                },
                VkSemaphoreSubmitInfo{
                    .sType{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},
                    .pNext{nullptr},
                    .semaphore{present_wait_semaphore_raw},
                    .value{0},
                    .stageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                    .deviceIndex{0}
                }
            };

            VkSubmitInfo2 const submit_info{
                .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO_2},
                .pNext{nullptr},
                .flags{0},
                .waitSemaphoreInfoCount{static_cast<std::uint32_t>(wait_semaphores_submit_info.size())},
                .pWaitSemaphoreInfos{wait_semaphores_submit_info.data()},
                .commandBufferInfoCount{1},
                .pCommandBufferInfos{&command_buffer_info},
                .signalSemaphoreInfoCount{static_cast<std::uint32_t>(signal_semaphores_info.size())},
                .pSignalSemaphoreInfos{signal_semaphores_info.data()}
            };

            VKGC_VERIFYF_VKSUCCESS(
                vkQueueSubmit2(vulkan_device.main_queue(), 1, &submit_info, VK_NULL_HANDLE),
                "failed to submit [#{}] frame render command buffer", frame_slot_index);
        }

        frame_ring.end_frame();

        ++frame_index;

        switch (presenter.request_presentation(vulkan_device.main_queue()))
        {
        case vkgc::present_status::ok:
            break;

        case vkgc::present_status::suboptimal:
            [[fallthrough]];
        case vkgc::present_status::out_of_date:
            if (!rebuild_swapchain_resources())
            {
                return false;
            }
            break;

        case vkgc::present_status::error:
            VKGC_VERIFYF(false, "unexpected error occurred while presenting to the swapchain");
            break;
        }

        glfwPollEvents();
        return true;
    });

    return true;
}

int main()
{
    vkgc::bootstrap_app();

    auto const [width, height] = std::pair<std::uint32_t, std::uint32_t>{1280, 800};

    if (!run_app(width, height))
    {
        return -1;
    }

    vkgc::terminate_app();
}
