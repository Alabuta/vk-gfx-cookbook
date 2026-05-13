#include <utility>
#include <tuple>
#include <array>
#include <span>
#include <vector>
#include <print>
#include <optional>

// #include <vulkan/vulkan_core.h>

#include <algorithm>

#include "volk.h"

#include "vulkan_format.hxx"

import cookbook.bootstrap;
import cookbook.window;
import cookbook.vulkan_instance;
import cookbook.vulkan_surface;
import cookbook.vulkan_device;

namespace cookbook
{
    struct presentation_capabilities final
    {
        VkSurfaceCapabilitiesKHR surface_capabilities{};
        std::vector<VkSurfaceFormatKHR> supported_formats;
        std::vector<VkPresentModeKHR> supported_modes;
    };
}

std::optional<cookbook::presentation_capabilities> query_presentation_capabilities(
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
    if (auto result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
        result != VK_SUCCESS)
    {
        std::println(stderr, "[Vulkan] : Fatal : failed to retrieve device surface presentation modes count ({})", result);
        return {};
    }

    std::vector<VkPresentModeKHR> supported_modes(present_mode_count);
    if (auto result = vkGetPhysicalDeviceSurfacePresentModesKHR(
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

    cookbook::presentation_capabilities presentation_capabilities{
        surface_capabilities,
        std::move(supported_formats),
        std::move(supported_modes)
    };
    return std::optional{std::move(presentation_capabilities)};
}

std::optional<VkSurfaceFormatKHR> get_first_supported_surface_format(
    std::span<VkSurfaceFormatKHR> const requested_formats,
    std::span<VkSurfaceFormatKHR> const supported_formats)
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

    for (auto const requested_format : requested_formats)
    {
        auto const supported = std::ranges::any_of(supported_formats, [requested_format](auto&& supported_format)
        {
            return requested_format.format == supported_format.format &&
                requested_format.colorSpace == supported_format.colorSpace;
        });

        if (supported)
        {
            return requested_format;
        }
    }

    return {};
}

std::vector<VkImage> get_swapchain_image_handles(VkDevice vulkan_device, VkSwapchainKHR swapchain)
{
    std::uint32_t image_count = 0;
    if (auto result = vkGetSwapchainImagesKHR(vulkan_device, swapchain, &image_count, nullptr);
        result != VK_SUCCESS)
    {
        std::println(stderr, "[Vulkan] : Fatal : failed to retrieve swap chain images count ({})", result);
        return {};
    }

    std::vector<VkImage> image_handles(image_count);
    if (auto result = vkGetSwapchainImagesKHR(vulkan_device, swapchain, &image_count, image_handles.data());
        result != VK_SUCCESS)
    {
        std::println(stderr, "[Vulkan] : Fatal : failed to retrieve swap chain images ({})", result);
        return {};
    }

    return image_handles;
}

bool run_app()
{
    cookbook::vulkan_instance vulkan_instance{{
        .enable_validation{true}
    }};
    if (!vulkan_instance)
    {
        return false;
    }

    const auto [width, height] = std::pair<uint32_t, uint32_t>{1280, 800};

    cookbook::window const window{"Swapchain example", width, height};
    if (!window)
    {
        return false;
    }

    cookbook::vulkan_surface const surface = vulkan_instance.create_surface(window);
    if (!surface)
    {
        return false;
    }

    cookbook::vulkan_device const vulkan_device = vulkan_instance.create_device({
        .surface{surface.handle()},
        .extensions{}
    });
    if (!vulkan_device)
    {
        return false;
    }

    cookbook::presentation_capabilities swapchain_details;
    if (auto query_result = query_presentation_capabilities(vulkan_device.physical_device(), surface.handle());
        !query_result)
    {
        return false;
    }
    else
    {
        swapchain_details = std::move(query_result.value());
    }

    std::array preferred_surface_formats{
        VkSurfaceFormatKHR{.format{VK_FORMAT_B8G8R8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
        VkSurfaceFormatKHR{.format{VK_FORMAT_R8G8B8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}
    };

    VkSurfaceFormatKHR swapchain_surface_format;
    if (auto query_result = get_first_supported_surface_format(
            preferred_surface_formats,
            swapchain_details.supported_formats);
        !query_result)
    {
        return false;
    }
    else
    {
        swapchain_surface_format = query_result.value();
    }

    std::array preferred_present_modes{
#ifndef VULKAN_DEBUG
        VK_PRESENT_MODE_MAILBOX_KHR,
#endif
        VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR
    };

    VkPresentModeKHR present_mode;
    {
        auto it = std::ranges::find_first_of(preferred_present_modes, swapchain_details.supported_modes);
        // VK_PRESENT_MODE_FIFO_KHR is the only value of presentMode that is required to be supported.
        present_mode = it != std::cend(preferred_present_modes) ? *it : VK_PRESENT_MODE_FIFO_KHR;
    }

    auto min_image_count = swapchain_details.surface_capabilities.minImageCount + 1;
    if (swapchain_details.surface_capabilities.maxImageCount > 0)
    {
        min_image_count = std::min(min_image_count, swapchain_details.surface_capabilities.maxImageCount);
    }

    VkExtent2D image_extent;
    {
        if (const auto& surface_capabilities = swapchain_details.surface_capabilities;
            surface_capabilities.currentExtent.width == 0xFFFFFFFF)
        {
            auto const [min_width, min_height] = surface_capabilities.minImageExtent;
            auto const [max_width, max_height] = surface_capabilities.maxImageExtent;

            image_extent = {
                .width{std::clamp(width, min_width, max_width)},
                .height{std::clamp(height, min_height, max_height)}
            };
        }
        else
        {
            image_extent = surface_capabilities.currentExtent;
        }
    }

    VkSwapchainCreateInfoKHR swapchain_create_info{
        .sType{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR},
        .pNext{nullptr},
        .flags{0},
        .surface{surface.handle()},
        .minImageCount{min_image_count},
        .imageFormat{swapchain_surface_format.format},
        .imageColorSpace{swapchain_surface_format.colorSpace},
        .imageExtent{image_extent},
        .imageArrayLayers{1},
        .imageUsage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
        .imageSharingMode{VK_SHARING_MODE_EXCLUSIVE},
        .queueFamilyIndexCount{0},
        .pQueueFamilyIndices{nullptr},
        .preTransform{swapchain_details.surface_capabilities.currentTransform},
        .compositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR},
        .presentMode{present_mode},
        .clipped{VK_FALSE},
        .oldSwapchain{nullptr}
    };

    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    if (auto const result = vkCreateSwapchainKHR(vulkan_device.handle(), &swapchain_create_info, nullptr, &swapchain);
        result != VK_SUCCESS)
    {
        return false;
    }

    auto const swapchain_image_handles = get_swapchain_image_handles(vulkan_device.handle(), swapchain);
    if (swapchain_image_handles.empty())
    {
        if (swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(vulkan_device.handle(), swapchain, nullptr);
        }

        return false;
    }

    while (!window.should_close())
    {
        cookbook::tick_app(nullptr);
    }

    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(vulkan_device.handle(), swapchain, nullptr);
    }

    return true;
}

int main()
{
    if (!cookbook::bootstrap_app())
    {
        return -1;
    }

    if (!run_app())
    {
        return -1;
    }

    cookbook::terminate_app();
}
