module;

#include <algorithm>
#include <cstdint>
#include <optional>
#include <print>
#include <span>
#include <utility>
#include <vector>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vulkan/assert.hxx"

module vkgc.vulkan_presenter;

import vkgc.vulkan_device;

namespace
{
    struct presentation_capabilities
    {
        VkSurfaceCapabilitiesKHR surface_capabilities{};
        std::vector<VkSurfaceFormatKHR> supported_formats{};
        std::vector<VkPresentModeKHR> supported_modes{};
    };

    struct swapchain_create_info
    {
        VkExtent2D extent{};
        VkSurfaceFormatKHR surface_format{};
        VkPresentModeKHR present_mode{VK_PRESENT_MODE_FIFO_KHR};
        VkImageUsageFlags image_usage_flags{};
        std::uint32_t min_image_count{0};
        VkSurfaceTransformFlagBitsKHR pre_transform{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR};
    };

    std::optional<presentation_capabilities> query_presentation_capabilities(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface)
    {
        VkSurfaceCapabilitiesKHR surface_capabilities;
        if (!VKGC_ENSUREF_VKSUCCESS(
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                physical_device,
                surface,
                &surface_capabilities),
            "failed to retrieve device surface capabilities"))
        {
            return {};
        }

        std::vector<VkSurfaceFormatKHR> supported_formats;
        if (std::uint32_t surface_format_count{0}; !VKGC_ENSUREF_VKSUCCESS(
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                physical_device,
                surface,
                &surface_format_count,
                nullptr),
            "failed to retrieve device surface formats count"))
        {
            return {};
        }
        else
        {
            supported_formats.resize(surface_format_count);

            VKGC_ENSUREF_VKSUCCESS(
                vkGetPhysicalDeviceSurfaceFormatsKHR(
                    physical_device,
                    surface,
                    &surface_format_count,
                    supported_formats.data()),
                "failed to retrieve device surface formats");
        }

        std::vector<VkPresentModeKHR> supported_modes;
        if (std::uint32_t present_mode_count{0}; !VKGC_ENSUREF_VKSUCCESS(
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr),
            "failed to retrieve device surface presentation modes count"))
        {
            return {};
        }
        else
        {
            supported_modes.resize(present_mode_count);

            VKGC_ENSUREF_VKSUCCESS(
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physical_device,
                    surface,
                    &present_mode_count,
                    supported_modes.data()),
                "failed to retrieve device surface presentation modes");
        }

        presentation_capabilities presentation_capabilities{
            surface_capabilities,
            std::move(supported_formats),
            std::move(supported_modes)
        };
        return std::optional{std::move(presentation_capabilities)};
    }

    std::optional<VkSurfaceFormatKHR> get_first_supported_surface_format(
        std::span<VkSurfaceFormatKHR const> const supported_formats,
        std::span<VkSurfaceFormatKHR const> const preferred_formats)
    {
        if (supported_formats.size() == 1 && supported_formats[0].format == VK_FORMAT_UNDEFINED)
        {
            return VkSurfaceFormatKHR{
                .format{VK_FORMAT_B8G8R8A8_UNORM},
                .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
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

        if (first_it != supported_formats.end())
        {
            return *first_it;
        }

        return {};
    }

    std::optional<swapchain_create_info> build_swapchain_create_info(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface,
        vkgc::swapchain_params const& swapchain_params)
    {
        swapchain_create_info swapchain_info{};

        auto const present_capabilities = query_presentation_capabilities(physical_device, surface);
        if (!VKGC_ENSURE(present_capabilities.has_value()))
        {
            return {};
        }

        auto const& [capabilities, supported_formats, supported_modes] = present_capabilities.value();

        auto const surface_format_opt = get_first_supported_surface_format(
            supported_formats,
            swapchain_params.preferred_surface_formats);
        if (!VKGC_ENSURE(surface_format_opt.has_value()))
        {
            return {};
        }

        swapchain_info.surface_format = surface_format_opt.value();

        if (auto const it = std::ranges::find_first_of(swapchain_params.preferred_present_modes, supported_modes);
            it != swapchain_params.preferred_present_modes.end())
        {
            swapchain_info.present_mode = *it;
        }

        swapchain_info.min_image_count = std::max(swapchain_params.min_image_count, capabilities.minImageCount + 1);
        if (capabilities.maxImageCount > 0)
        {
            swapchain_info.min_image_count = std::min(swapchain_info.min_image_count, capabilities.maxImageCount);
        }

        swapchain_info.min_image_count = std::max(swapchain_info.min_image_count, capabilities.minImageCount);

        if (capabilities.currentExtent.width == 0xFFFFFFFF)
        {
            auto const [min_width, min_height] = capabilities.minImageExtent;
            auto const [max_width, max_height] = capabilities.maxImageExtent;

            swapchain_info.extent = {
                .width{std::clamp(swapchain_params.framebuffer_extent.width, min_width, max_width)},
                .height{std::clamp(swapchain_params.framebuffer_extent.height, min_height, max_height)}
            };
        }
        else
        {
            swapchain_info.extent = capabilities.currentExtent;
        }

        swapchain_info.image_usage_flags = swapchain_params.image_usage_flags;
        if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
        {
            VkFormatProperties2 fmt_properties{
                .sType{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2},
                .pNext{nullptr},
                .formatProperties{}
            };

            vkGetPhysicalDeviceFormatProperties2(
                physical_device,
                swapchain_info.surface_format.format,
                &fmt_properties);

            if ((fmt_properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0)
            {
                swapchain_info.image_usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
            }
        }

        swapchain_info.pre_transform = capabilities.currentTransform;

        return std::optional{swapchain_info};
    }
}

namespace vkgc
{
    void vulkan_presenter::create_swapchain(
        VkSurfaceKHR surface,
        VkSwapchainKHR old_handle,
        swapchain_params const& swapchain_params)
    {
        auto const info = build_swapchain_create_info(device_.physical_device(), surface, swapchain_params);
        if (!VKGC_ENSURE(info.has_value()))
        {
            return;
        }

        VkSwapchainCreateInfoKHR const create_info{
            .sType{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR},
            .pNext{nullptr},
            .flags{0},
            .surface{surface},
            .minImageCount{info->min_image_count},
            .imageFormat{info->surface_format.format},
            .imageColorSpace{info->surface_format.colorSpace},
            .imageExtent{info->extent},
            .imageArrayLayers{1},
            .imageUsage{info->image_usage_flags},
            .imageSharingMode{VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount{0},
            .pQueueFamilyIndices{nullptr},
            .preTransform{info->pre_transform},
            .compositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR},
            .presentMode{info->present_mode},
            .clipped{VK_TRUE},
            .oldSwapchain{old_handle}
        };

        VkSwapchainKHR new_handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateSwapchainKHR(device_.handle(), &create_info, nullptr, &new_handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create swapchain ({})", result);
            return;
        }

        swapchain_handle_ = new_handle;
        surface_format_ = info->surface_format;
        surface_extent_ = info->extent;
    }
}
