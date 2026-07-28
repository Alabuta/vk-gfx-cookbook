#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <print>
#include <span>
#include <utility>
#include <vector>

#include <volk.h>
#include <vk_mem_alloc.h>

#include "GLFW/glfw3.h"

#include "vulkan/format.hxx"
#include "vulkan/assert.hxx"

import vkgc.bootstrap;
import vkgc.window;
import vkgc.vulkan_instance;
import vkgc.vulkan_context;
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

static bool run_app(std::uint32_t width, std::uint32_t height)
{
    vkgc::vulkan_context vk_context{{.enable_validation{true}}, {}};
    VKGC_VERIFY(vk_context.is_valid());

    vkgc::window window{"Swapchain example", width, height};
    VKGC_VERIFY(window.is_valid());
    VKGC_VERIFY(window.create_surface(vk_context.instance().handle()));

    vkgc::vulkan_object_registry vk_object_registry{vk_context.device()};

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
        vk_context.device(),
        vk_object_registry,
        window.surface(),
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

    auto swapchain_image_view_handles = vkgc::create_swapchain_image_views(
        vk_object_registry,
        presenter.images(),
        presenter.surface_format().format);
    if (swapchain_image_view_handles.empty())
    {
        std::println(stderr, "[Vulkan] : Error : failed to create swapchain image views");
        return false;
    }

    vkgc::vulkan_frame_ring frame_ring{vk_object_registry, vkgc::kFramesInFlight};

    auto const main_queue_command_pool = vk_object_registry.create_command_pool({
        .sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
        .pNext{nullptr},
        .flags{VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT},
        .queueFamilyIndex{vk_context.device().main_queue_family_index()}
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
            vk_context.device().physical_device(),
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

    auto depth_attachment_image = vkgc::create_depth_attachment(
        vk_object_registry,
        depth_attachment_format,
        {.width{presenter.surface_extent().width}, .height{presenter.surface_extent().height}, .depth{1}});
    if (!depth_attachment_image.is_valid())
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

        VKGC_VERIFY_VKSUCCESS(vkDeviceWaitIdle(vk_context.device().handle()));

        // Destroying the image takes its default view with it.
        vk_object_registry.destroy_immediate(depth_attachment_image);

        for (auto const image_view_handle : swapchain_image_view_handles)
        {
            vk_object_registry.destroy_immediate(image_view_handle);
        }
        swapchain_image_view_handles.clear();

        swapchain_params.framebuffer_extent = {.width{width}, .height{height}};
        presenter.recreate_swapchain(window.surface(), swapchain_params);
        VKGC_VERIFYF(presenter.is_valid(), "unexpected error occurred while recreating the swapchain");

        swapchain_image_view_handles = vkgc::create_swapchain_image_views(
            vk_object_registry,
            presenter.images(),
            presenter.surface_format().format);
        if (swapchain_image_view_handles.empty())
        {
            std::println(stderr, "[Vulkan] : Error : failed to create swapchain image views");
            return false;
        }

        depth_attachment_image = vkgc::create_depth_attachment(
            vk_object_registry,
            depth_attachment_format,
            {.width{presenter.surface_extent().width}, .height{presenter.surface_extent().height}, .depth{1}});
        if (!depth_attachment_image.is_valid())
        {
            std::println(stderr, "[Vulkan] : Error : failed to create depth attachment");
            return false;
        }

        return true;
    };

    std::uint32_t frame_index{vkgc::kFramesInFlight}; // renderer state

    vkgc::update_loop([&]
    {
        // Begin render loop

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

        [[maybe_unused]] auto [
            swapchain_image_acquired_semaphore,
            present_wait_semaphore,
            swapchain_image,
            swapchain_image_index] = acquired.value();

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
                    .image{swapchain_image},
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
                .image{swapchain_image},
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

            std::array const command_buffer_submit_info{
                VkCommandBufferSubmitInfo{
                   .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO},
                   .pNext{nullptr},
                   .commandBuffer{command_buffer},
                   .deviceMask{0}
               }
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
                .commandBufferInfoCount{static_cast<std::uint32_t>(command_buffer_submit_info.size())},
                .pCommandBufferInfos{command_buffer_submit_info.data()},
                .signalSemaphoreInfoCount{static_cast<std::uint32_t>(signal_semaphores_info.size())},
                .pSignalSemaphoreInfos{signal_semaphores_info.data()}
            };

            VKGC_VERIFYF_VKSUCCESS(
                vkQueueSubmit2(vk_context.device().main_queue(), 1, &submit_info, VK_NULL_HANDLE),
                "failed to submit [#{}] frame render command buffer", frame_slot_index);
        }

        frame_ring.end_frame();

        ++frame_index;

        // End render loop

        switch (presenter.request_presentation(vk_context.device().main_queue()))
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
