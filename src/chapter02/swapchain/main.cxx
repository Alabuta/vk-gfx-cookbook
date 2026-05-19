#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <print>
#include <ranges>
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
    std::uint32_t constexpr max_frames_in_flight{2};

    struct presentation_capabilities final
    {
        VkSurfaceCapabilitiesKHR surface_capabilities{};
        std::vector<VkSurfaceFormatKHR> supported_formats;
        std::vector<VkPresentModeKHR> supported_modes;
    };
}

std::optional<vkgc::presentation_capabilities> query_presentation_capabilities(
    VkPhysicalDevice device,
    VkSurfaceKHR surface)
{
    VkSurfaceCapabilitiesKHR surface_capabilities;
    if (auto const result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &surface_capabilities);
        result != VK_SUCCESS)
    {
        std::println(stderr, "[Vulkan] : Fatal : failed to retrieve device surface capabilities ({})", result);
        return {};
    }

    std::uint32_t surface_format_count = 0;
    if (auto const result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_format_count, nullptr);
        result != VK_SUCCESS)
    {
        std::println(stderr, "[Vulkan] : Fatal : failed to retrieve device surface formats count ({})", result);
        return {};
    }

    std::vector<VkSurfaceFormatKHR> supported_formats(surface_format_count);
    if (auto const result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            device,
            surface,
            &surface_format_count,
            supported_formats.data());
        result != VK_SUCCESS)
    {
        std::println(stderr, "[Vulkan] : Fatal : failed to retrieve device surface formats ({})", result);
        return {};
    }

    if (supported_formats.empty())
    {
        return {};
    }

    std::uint32_t present_mode_count = 0;
    if (auto const result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
        result != VK_SUCCESS)
    {
        std::println(stderr, "[Vulkan] : Fatal : failed to retrieve device surface presentation modes count ({})",
                     result);
        return {};
    }

    std::vector<VkPresentModeKHR> supported_modes(present_mode_count);
    if (auto const result = vkGetPhysicalDeviceSurfacePresentModesKHR(
            device,
            surface,
            &present_mode_count,
            supported_modes.data());
        result != VK_SUCCESS)
    {
        std::println(stderr, "[Vulkan] : Fatal : failed to retrieve device surface presentation modes ({})", result);
        return {};
    }

    if (supported_modes.empty())
    {
        return {};
    }

    vkgc::presentation_capabilities presentation_capabilities{
        surface_capabilities,
        std::move(supported_formats),
        std::move(supported_modes)
    };
    return std::optional{std::move(presentation_capabilities)};
}

std::optional<VkSurfaceFormatKHR> get_first_supported_surface_format(
    std::span<const VkSurfaceFormatKHR> const supported_formats,
    std::span<const VkSurfaceFormatKHR> const preferred_formats)
{
    if (supported_formats.size() == 1 && supported_formats[0].format == VK_FORMAT_UNDEFINED)
    {
        return {
            VkSurfaceFormatKHR{
                .format{VK_FORMAT_B8G8R8A8_UNORM},
                .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
            }
        };
    }

    // The first found supported surface format is the best
    auto const first_it = std::ranges::find_first_of(
        supported_formats,
        preferred_formats,
        [](VkSurfaceFormatKHR const lhs, VkSurfaceFormatKHR const rhs)
        {
            return lhs.format == rhs.format && lhs.colorSpace == rhs.colorSpace;
        });

    if (first_it != std::end(supported_formats))
    {
        return *first_it;
    }

    return {};
}

[[nodiscard]]
std::vector<vkgc::vk_image_view_handle> create_swapchain_image_views(
    vkgc::vulkan_object_registry& vk_object_registry,
    vkgc::vulkan_presenter const& presenter)
{
    std::vector<vkgc::vk_image_view_handle> image_view_handles;
    image_view_handles.reserve(presenter.image_count());

    for (VkImage image_handle : presenter.images())
    {
        VkImageViewCreateInfo const create_info{
            .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .image{image_handle},
            .viewType{VK_IMAGE_VIEW_TYPE_2D},
            .format{presenter.image_format()},
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

        if (auto const view = vk_object_registry.create_image_view(create_info); view.is_valid())
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

bool run_app()
{
    vkgc::vulkan_instance vulkan_instance{
        {
            .enable_validation{true}
        }
    };
    VKGC_VERIFY(vulkan_instance);

    auto const [width, height] = std::pair<std::uint32_t, std::uint32_t>{1280, 800};

    vkgc::window const window{"Swapchain example", width, height};
    VKGC_VERIFY(window);

    vkgc::vulkan_surface const window_surface = vulkan_instance.create_window_surface(window);
    VKGC_VERIFY(window_surface);

    vkgc::vulkan_device vulkan_device = vulkan_instance.create_device({
        .surface{window_surface.handle()},
        .extensions{}
    });
    VKGC_VERIFY(vulkan_device);

    vkgc::vulkan_object_registry vk_object_registry{vulkan_device};

    vkgc::presentation_capabilities presentation_capabilities;
    if (auto query_result = query_presentation_capabilities(vulkan_device.physical_device(), window_surface.handle());
        !query_result)
    {
        return false;
    }
    else
    {
        presentation_capabilities = std::move(query_result.value());
    }

    VkSurfaceFormatKHR swapchain_surface_format;
    {
        std::array preferred_surface_formats{
            VkSurfaceFormatKHR{.format{VK_FORMAT_B8G8R8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
            VkSurfaceFormatKHR{.format{VK_FORMAT_R8G8B8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}
        };

        if (auto query_result = get_first_supported_surface_format(
               presentation_capabilities.supported_formats,
               preferred_surface_formats);
           !query_result)
        {
            return false;
        }
        else
        {
            swapchain_surface_format = query_result.value();
        }
    }

    std::array preferred_present_modes{
#if (VKGC_DEBUG_VULKAN == 0)
        VK_PRESENT_MODE_MAILBOX_KHR,
#endif
        VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR
    };

    VkPresentModeKHR present_mode;
    {
        auto it = std::ranges::find_first_of(preferred_present_modes, presentation_capabilities.supported_modes);
        // VK_PRESENT_MODE_FIFO_KHR is the only value of presentMode that is required to be supported by driver.
        present_mode = it != std::cend(preferred_present_modes) ? *it : VK_PRESENT_MODE_FIFO_KHR;
    }

    auto min_image_count = presentation_capabilities.surface_capabilities.minImageCount + 1;
    if (presentation_capabilities.surface_capabilities.maxImageCount > 0)
    {
        min_image_count = std::min(min_image_count, presentation_capabilities.surface_capabilities.maxImageCount);
    }

    VkExtent2D swapchain_image_extent;
    {
        if (auto const& surface_capabilities = presentation_capabilities.surface_capabilities;
            surface_capabilities.currentExtent.width == 0xFFFFFFFF)
        {
            auto const [min_width, min_height] = surface_capabilities.minImageExtent;
            auto const [max_width, max_height] = surface_capabilities.maxImageExtent;

            swapchain_image_extent = {
                .width{std::clamp(width, min_width, max_width)},
                .height{std::clamp(height, min_height, max_height)}
            };
        }
        else
        {
            swapchain_image_extent = surface_capabilities.currentExtent;
        }
    }

    VkImageUsageFlags swapchain_image_usage_flags{
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    };

    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_device.physical_device(), window_surface.handle(), &capabilities);

        if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
        {
            VkFormatProperties2 fmt_properties{
                .sType{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2},
                .pNext{nullptr},
                .formatProperties{}
            };

            vkGetPhysicalDeviceFormatProperties2(
                vulkan_device.physical_device(),
                swapchain_surface_format.format,
                &fmt_properties);

            if ((fmt_properties.formatProperties.optimalTilingFeatures & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
            {
                swapchain_image_usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
            }
        }
    }

    vkgc::vulkan_swapchain_info const vulkan_swapchain_info{
        .surface{window_surface.handle()},
        .extent{swapchain_image_extent},
        .surface_format{swapchain_surface_format},
        .present_mode{present_mode},
        .image_usage{swapchain_image_usage_flags},
        .min_image_count{min_image_count},
        .pre_transform{presentation_capabilities.surface_capabilities.currentTransform}
    };

    vkgc::vulkan_presenter presenter{
        vulkan_device,
        vk_object_registry,
        vulkan_swapchain_info,
        vkgc::max_frames_in_flight
    };
    VKGC_VERIFY(presenter.is_valid());

    auto swapchain_image_view_handles = create_swapchain_image_views(vk_object_registry, presenter);
    if (swapchain_image_view_handles.empty())
    {
        std::println(stderr, "[Vulkan] : Error : failed to crate swapchain image views");
        return false;
    }

    vkgc::vulkan_frame_ring frame_ring{vk_object_registry, vkgc::max_frames_in_flight};

    // vkQueueSubmit set them signaled when submitted commands execution is done and then vkQueuePresentKHR becomes allowed
    std::vector<vkgc::vk_semaphore_handle> execution_complete_semaphores;
    {
        execution_complete_semaphores.reserve(presenter.image_count());

        VkSemaphoreCreateInfo constexpr semaphore_create_info{
            .sType{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0}
        };

        for (std::uint32_t i = 0; i < presenter.image_count(); ++i)
        {
            auto const semaphore = vk_object_registry.create_semaphore(semaphore_create_info);
            if (!semaphore.is_valid())
            {
                return false;
            }

            execution_complete_semaphores.push_back(semaphore);
        }
    }

    auto const command_pool = vk_object_registry.command_pool_create({
        .sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
        .pNext{nullptr},
        .flags{VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT},
        .queueFamilyIndex{vulkan_device.main_queue_family_index()}
    });
    if (!command_pool.is_valid())
    {
        return false;
    }

    auto const command_buffers = vk_object_registry.allocate_command_buffers(
        command_pool,
        vkgc::max_frames_in_flight,
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

    {
        VkImageCreateInfo const depth_image_create_info{
            .sType{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .imageType{VK_IMAGE_TYPE_2D},
            .format{depth_attachment_format},
            .extent{.width{swapchain_image_extent.width}, .height{swapchain_image_extent.height}, .depth{1}},
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

        VmaAllocationCreateInfo constexpr depth_allocation_create_info{
            .flags{VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT},
            .usage{VMA_MEMORY_USAGE_UNKNOWN},
            .requiredFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            .preferredFlags{0},
            .memoryTypeBits{0},
            .pool{VK_NULL_HANDLE},
            .pUserData{nullptr},
            .priority{0}
        };

        if (!create_memory_bound_image(
            vk_object_registry,
            depth_image_create_info,
            depth_allocation_create_info,
            depth_attachment_image,
            depth_attachment_image_memory))
        {
            return false;
        }
    }

    vkgc::vk_image_view_handle depth_image_view;
    {
        VkImageViewCreateInfo const depth_view_create_info{
            .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .image{vk_object_registry.resolve_handle(depth_attachment_image)},
            .viewType{VK_IMAGE_VIEW_TYPE_2D},
            .format{depth_attachment_format},
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

        depth_image_view = vk_object_registry.create_image_view(depth_view_create_info);
        if (!depth_image_view.is_valid())
        {
            return false;
        }
    }

    vkgc::run_app([&]
    {
        if (window.should_close())
        {
            return false;
        }

        std::uint32_t const frame_index = frame_ring.begin_frame();
        if (frame_index == std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        auto const acquired = presenter.acquire_image(frame_index);
        if (!acquired.has_value())
        {
            if (!VKGC_ENSUREF(
                acquired.error() != vkgc::present_status::out_of_date,
                "vulkan surface no longer compatible with the swapchain - recreate swapchain"))
            {
                return false;
            }

            if (!VKGC_ENSUREF(
                acquired.error() != vkgc::present_status::error,
                "unexpected error occurred while acquiring the swapchain"))
            {
                return false;
            }
        }

        auto const [image_acquired_semaphore, swapchain_image_index] = acquired.value();

        auto const command_buffer = vk_object_registry.resolve_handle(command_buffers[frame_index]);
        if (command_buffer == VK_NULL_HANDLE)
        {
            return false;
        }

        if (!VKGC_ENSUREF_VKSUCCESS(
            vkResetCommandBuffer(command_buffer, 0),
            "failed to reset #{} frame command buffer", frame_index))
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

            if (auto const result = vkBeginCommandBuffer(command_buffer, &begin_info); result != VK_SUCCESS)
            {
                std::println(stderr, "[Vulkan] : Error : failed to record command buffer");
                return false;
            }
        }

        auto const depth_attachment_image_handle = vk_object_registry.resolve_handle(depth_attachment_image);
        if (depth_attachment_image_handle == VK_NULL_HANDLE)
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
                .imageMemoryBarrierCount{images_transition_barriers.size()},
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

        if (auto const result = vkEndCommandBuffer(command_buffer); result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to end command buffer");
            return false;
        }

        VkFence current_frame_fence = vk_object_registry.resolve_handle(frame_ring.current_frame_fence());
        if (current_frame_fence == VK_NULL_HANDLE)
        {
            return false;
        }

        VkSemaphore execution_complete_semaphore = vk_object_registry.resolve_handle(
            execution_complete_semaphores[swapchain_image_index]);
        if (execution_complete_semaphore == VK_NULL_HANDLE)
        {
            return false;
        }

        {
            std::array constexpr wait_stages{
                VkPipelineStageFlags{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT}
            };

            VkSubmitInfo const submit_info{
                .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO},
                .pNext{nullptr},
                .waitSemaphoreCount{1},
                .pWaitSemaphores{&image_acquired_semaphore},
                .pWaitDstStageMask{wait_stages.data()},
                .commandBufferCount{1},
                .pCommandBuffers{&command_buffer},
                .signalSemaphoreCount{1},
                .pSignalSemaphores{&execution_complete_semaphore}
            };

            VKGC_VERIFYF_VKSUCCESS(
                vkQueueSubmit(vulkan_device.main_queue(), 1, &submit_info, current_frame_fence),
                "failed to submit command buffer for #{} frame-in-flight", frame_index);
        }

        frame_ring.end_frame();

        switch (presenter.present(vulkan_device.main_queue(), swapchain_image_index, execution_complete_semaphore))
        {
        case vkgc::present_status::ok:
            break;

        case vkgc::present_status::suboptimal:
        case vkgc::present_status::out_of_date:
            VKGC_ENSUREF(false, "unsupported swapchain recreation request");
            // recreate swapchain
            return false;

        case vkgc::present_status::error:
            return false;
        }

        // Poll events

        return true;
    });

    // All teardown is automatic, in reverse declaration order:
    //   ~frame_ring     - waits all slot fences, drains pending, destroys slot fences
    //   ~presenter      - vkDeviceWaitIdle, destroys image_acquired semaphores, vkDestroySwapchainKHR
    //   ~resources      - vkDeviceWaitIdle, walks pools, destroys remaining live entries
    //   ~vulkan_device  - vkDeviceWaitIdle (defensive), destroys device + VMA allocator

    return true;
}

int main()
{
    vkgc::bootstrap_app();

    if (!run_app())
    {
        return -1;
    }

    vkgc::terminate_app();
}
