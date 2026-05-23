module;

#include <algorithm>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vulkan/assert.hxx"

module vkgc.vulkan_presenter;

import vkgc.vulkan_device;
import vkgc.vulkan_handle;
import vkgc.vulkan_object_registry;


namespace
{
    struct presentation_capabilities
    {
        VkSurfaceCapabilitiesKHR surface_capabilities{};
        std::vector<VkSurfaceFormatKHR> supported_formats{};
        std::vector<VkPresentModeKHR> supported_modes{};
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

    std::optional<vkgc::swapchain_create_info> build_swapchain_create_info(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface,
        vkgc::swapchain_params const& swapchain_params)
    {
        vkgc::swapchain_create_info swapchain_info{};

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

        return std::optional{std::move(swapchain_info)};
    }
}

namespace vkgc
{
    vulkan_presenter::vulkan_presenter(
        vulkan_device const& device,
        vulkan_object_registry& object_registry,
        VkSurfaceKHR surface,
        std::uint32_t frames_in_flight,
        swapchain_params const& swapchain_params)
        : device_{device},
          object_registry_{object_registry}
    {
        if (!VKGC_ENSURE(device.is_valid()) || !VKGC_ENSURE_VKHANDLE(surface))
        {
            return;
        }

        create_swapchain(surface, VK_NULL_HANDLE, swapchain_params);
        if (!VKGC_ENSURE_VKHANDLE(swapchain_handle_))
        {
            return;
        }

        query_swapchain_images();
        if (!VKGC_ENSURE(!swapchain_images_.empty()))
        {
            vkDestroySwapchainKHR(device_.handle(), swapchain_handle_, nullptr);
            swapchain_handle_ = VK_NULL_HANDLE;
            return;
        }

        if (!VKGC_ENSURE(create_image_acquired_semaphores(frames_in_flight)))
        {
            destroy_image_acquired_semaphores();

            vkDestroySwapchainKHR(device_.handle(), swapchain_handle_, nullptr);
            swapchain_handle_ = VK_NULL_HANDLE;

            swapchain_images_.clear();
        }
    }

    vulkan_presenter::~vulkan_presenter()
    {
        auto const device_handle = device_.handle();
        if (!VKGC_ENSURE_VKHANDLE(device_handle))
        {
            return;
        }

        VKGC_CHECK_VKSUCCESS(vkDeviceWaitIdle(device_handle));

        destroy_image_acquired_semaphores();

        if (swapchain_handle_ != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device_handle, swapchain_handle_, nullptr);
            swapchain_handle_ = VK_NULL_HANDLE;
        }
    }

    bool vulkan_presenter::is_valid() const noexcept
    {
        return swapchain_handle_ != VK_NULL_HANDLE;
    }

    VkSwapchainKHR vulkan_presenter::swapchain_handle() const noexcept
    {
        return swapchain_handle_;
    }

    std::span<VkImage const> vulkan_presenter::images() const noexcept
    {
        return std::span{swapchain_images_.data(), swapchain_images_.size()};
    }

    std::uint32_t vulkan_presenter::image_count() const noexcept
    {
        return static_cast<std::uint32_t>(swapchain_images_.size());
    }

    VkSurfaceFormatKHR vulkan_presenter::surface_format() const noexcept
    {
        return surface_format_;
    }

    VkExtent2D vulkan_presenter::extent() const noexcept
    {
        return surface_extent_;
    }

    std::expected<swapchain_image_acquire_result, present_status> vulkan_presenter::acquire_image(
        std::uint32_t const frame_index)
    {
        if (needs_rebuild_)
        {
            needs_rebuild_ = false;
            return std::unexpected{present_status::out_of_date};
        }

        auto image_acquired = object_registry_.resolve_handle(image_acquired_semaphores_[frame_index]);

        std::uint32_t image_index{0};
        VkResult const result = vkAcquireNextImageKHR(
            device_.handle(),
            swapchain_handle_,
            std::numeric_limits<std::uint64_t>::max(),
            image_acquired,
            VK_NULL_HANDLE,
            &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            return std::unexpected{present_status::out_of_date};
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            std::println(
                stderr,
                "[Vulkan] : Error : vulkan_presenter::acquire_image failed ({})",
                result);
            return std::unexpected{present_status::error};
        }

        return swapchain_image_acquire_result{
            .image_acquired{image_acquired},
            .image_index{image_index}
        };
    }

    present_status vulkan_presenter::present(VkQueue queue, std::uint32_t const image_index, VkSemaphore wait_semaphore)
    {
        VkPresentInfoKHR const info{
            .sType{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR},
            .pNext{nullptr},
            .waitSemaphoreCount{1},
            .pWaitSemaphores{&wait_semaphore},
            .swapchainCount{1},
            .pSwapchains{&swapchain_handle_},
            .pImageIndices{&image_index},
            .pResults{nullptr}
        };

        switch (auto const result = vkQueuePresentKHR(queue, &info); result)
        {
        case VK_SUCCESS:
            return present_status::ok;

        case VK_SUBOPTIMAL_KHR:
            return present_status::suboptimal;

        case VK_ERROR_OUT_OF_DATE_KHR:
            return present_status::out_of_date;

        default:
            return present_status::error;
        }
    }

    void vulkan_presenter::recreate_swapchain(
        VkSurfaceKHR surface,
        swapchain_params const& swapchain_params)
    {
        VkSwapchainKHR previous{swapchain_handle_};
        swapchain_handle_ = VK_NULL_HANDLE;

        if (!VKGC_ENSURE(device_.is_valid()) || !VKGC_ENSURE_VKHANDLE(surface))
        {
            return;
        }

        auto const device_handle = device_.handle();

        VKGC_CHECK_VKSUCCESS(vkDeviceWaitIdle(device_handle));

        swapchain_images_.clear();

        create_swapchain(surface, previous, swapchain_params);

        if (previous != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device_handle, previous, nullptr);
        }

        if (!VKGC_ENSURE_VKHANDLE(swapchain_handle_))
        {
            return;
        }

        query_swapchain_images();
        if (!VKGC_ENSURE(!swapchain_images_.empty()))
        {
            vkDestroySwapchainKHR(device_.handle(), swapchain_handle_, nullptr);
            swapchain_handle_ = VK_NULL_HANDLE;
        }
    }

    void vulkan_presenter::request_rebuild() noexcept
    {
        needs_rebuild_ = true;
    }

    bool vulkan_presenter::consume_rebuild_request() noexcept
    {
        bool const was_requested = needs_rebuild_;
        needs_rebuild_ = false;
        return was_requested;
    }

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

    void vulkan_presenter::query_swapchain_images()
    {
        auto const device_handle = device_.handle();

        std::uint32_t count{0};
        if (auto const result = vkGetSwapchainImagesKHR(device_handle, swapchain_handle_, &count, nullptr);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to query swapchain image count ({})", result);
            return;
        }

        swapchain_images_.resize(count);

        if (auto const result = vkGetSwapchainImagesKHR(
                device_handle,
                swapchain_handle_,
                &count,
                swapchain_images_.data());
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to query swapchain images ({})", result);
            swapchain_images_.clear();
        }
    }

    bool vulkan_presenter::create_image_acquired_semaphores(std::uint32_t const count)
    {
        VkSemaphoreCreateInfo constexpr semaphore_create_info{
            .sType{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0}
        };

        image_acquired_semaphores_.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i)
        {
            auto const semaphore = object_registry_.create_semaphore(semaphore_create_info);
            if (!semaphore.is_valid())
            {
                return false;
            }

            image_acquired_semaphores_.push_back(semaphore);
        }

        return true;
    }

    void vulkan_presenter::destroy_image_acquired_semaphores()
    {
        for (auto const handle : image_acquired_semaphores_)
        {
            object_registry_.destroy_immediate(handle);
        }

        image_acquired_semaphores_.clear();
    }
}
