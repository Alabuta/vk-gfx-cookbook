module;

#include <algorithm>
#include <cstdint>
#include <expected>
#include <limits>
#include <print>
#include <span>
#include <vector>

#include <volk.h>
#include "vulkan/format.hxx"

module vkgc.vulkan_presenter;

import vkgc.vulkan_device;
import vkgc.vulkan_handle;
import vkgc.vulkan_object_registry;

#include "vulkan/assert.hxx"

namespace vkgc
{
    vulkan_presenter::vulkan_presenter(
        vulkan_device const& device,
        vulkan_object_registry& object_registry,
        vulkan_swapchain_info const& info,
        std::uint32_t const frames_in_flight)
        : device_{device},
          object_registry_{object_registry}
    {
        create_swapchain(info, VK_NULL_HANDLE);
        if (!VKGC_ENSURE_VKHANDLE(swapchain_handle_))
        {
            return;
        }

        query_swapchain_images();
        if (!VKGC_ENSURE(!images_.empty()))
        {
            vkDestroySwapchainKHR(device_.handle(), swapchain_handle_, nullptr);
            swapchain_handle_ = VK_NULL_HANDLE;
            return;
        }

        image_format_ = info.surface_format.format;
        extent_ = info.extent;

        if (!VKGC_ENSURE(create_image_acquired_semaphores(frames_in_flight)))
        {
            destroy_image_acquired_semaphores();

            vkDestroySwapchainKHR(device_.handle(), swapchain_handle_, nullptr);
            swapchain_handle_ = VK_NULL_HANDLE;

            images_.clear();
        }
    }

    vulkan_presenter::~vulkan_presenter()
    {
        VkDevice device_handle = device_.handle();
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
        return std::span{images_.data(), images_.size()};
    }

    std::uint32_t vulkan_presenter::image_count() const noexcept
    {
        return static_cast<std::uint32_t>(images_.size());
    }

    VkFormat vulkan_presenter::image_format() const noexcept
    {
        return image_format_;
    }

    VkExtent2D vulkan_presenter::extent() const noexcept
    {
        return extent_;
    }

    std::expected<swapchain_image_acquire_result, present_status> vulkan_presenter::acquire_image(std::uint32_t const frame_index)
    {
        VkSemaphore image_acquired = object_registry_.resolve_handle(image_acquired_semaphores_[frame_index]);

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

    void vulkan_presenter::recreate(vulkan_swapchain_info const& info)
    {
        VkDevice device_handle = device_.handle();
        if (!VKGC_ENSURE_VKHANDLE(device_handle))
        {
            return;
        }

        VKGC_CHECK_VKSUCCESS(vkDeviceWaitIdle(device_handle));

        VkSwapchainKHR previous{swapchain_handle_};
        swapchain_handle_ = VK_NULL_HANDLE;
        images_.clear();

        create_swapchain(info, previous);

        if (previous != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device_handle, previous, nullptr);
        }

        if (swapchain_handle_ == VK_NULL_HANDLE)
        {
            return;
        }

        query_swapchain_images();
        image_format_ = info.surface_format.format;
        extent_ = info.extent;
    }

    void vulkan_presenter::create_swapchain(vulkan_swapchain_info const& info, VkSwapchainKHR old_handle)
    {
        swapchain_handle_ = VK_NULL_HANDLE;

        VkSwapchainCreateInfoKHR const create_info{
            .sType{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR},
            .pNext{nullptr},
            .flags{0},
            .surface{info.surface},
            .minImageCount{info.min_image_count},
            .imageFormat{info.surface_format.format},
            .imageColorSpace{info.surface_format.colorSpace},
            .imageExtent{info.extent},
            .imageArrayLayers{1},
            .imageUsage{info.image_usage},
            .imageSharingMode{VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount{0},
            .pQueueFamilyIndices{nullptr},
            .preTransform{info.pre_transform},
            .compositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR},
            .presentMode{info.present_mode},
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
    }

    void vulkan_presenter::query_swapchain_images()
    {
        VkDevice device_handle = device_.handle();

        std::uint32_t count{0};
        if (auto const result = vkGetSwapchainImagesKHR(device_handle, swapchain_handle_, &count, nullptr);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to query swapchain image count ({})", result);
            return;
        }

        images_.resize(count);

        if (auto const result = vkGetSwapchainImagesKHR(device_handle, swapchain_handle_, &count, images_.data());
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to query swapchain images ({})", result);
            images_.clear();
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
