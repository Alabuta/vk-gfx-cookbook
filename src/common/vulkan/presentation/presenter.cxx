module;

#include <cstdint>
#include <expected>
#include <limits>
#include <print>
#include <span>
#include <vector>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vulkan/assert.hxx"

module vkgc.vulkan_presenter;

import vkgc.vulkan_device;
import vkgc.vulkan_handle;
import vkgc.vulkan_object_registry;
import vkgc.scope_guard;

namespace vkgc
{
    vulkan_presenter::vulkan_presenter(
        vulkan_device const& device,
        vulkan_object_registry& object_registry,
        VkSurfaceKHR surface,
        std::uint32_t const frames_in_flight,
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

        scope_guard cleanup{[&]
        {
            destroy_semaphores();

            vkDestroySwapchainKHR(device_.handle(), swapchain_handle_, nullptr);
            swapchain_handle_ = VK_NULL_HANDLE;

            swapchain_images_.clear();
        }};

        query_swapchain_images();
        if (!VKGC_ENSURE(!swapchain_images_.empty()))
        {
            return;
        }

        auto const semaphores_created = create_semaphores(
            frames_in_flight,
            static_cast<std::uint32_t>(swapchain_images_.size()));

        if (!VKGC_ENSURE(semaphores_created))
        {
            return;
        }

        is_valid_ = true;
        cleanup.dismiss();
    }

    vulkan_presenter::~vulkan_presenter()
    {
        auto const device_handle = device_.handle();
        if (!VKGC_ENSURE_VKHANDLE(device_handle))
        {
            return;
        }

        VKGC_VERIFY_VKSUCCESS(vkDeviceWaitIdle(device_handle));

        destroy_semaphores();

        if (swapchain_handle_ != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device_handle, swapchain_handle_, nullptr);
            swapchain_handle_ = VK_NULL_HANDLE;
        }
    }

    bool vulkan_presenter::is_valid() const noexcept
    {
        return is_valid_;
    }

    VkSwapchainKHR vulkan_presenter::swapchain_handle() const noexcept
    {
        return swapchain_handle_;
    }

    std::span<VkImage const> vulkan_presenter::images() const noexcept
    {
        return std::span{swapchain_images_};
    }

    std::uint32_t vulkan_presenter::image_count() const noexcept
    {
        return static_cast<std::uint32_t>(swapchain_images_.size());
    }

    VkSurfaceFormatKHR vulkan_presenter::surface_format() const noexcept
    {
        return surface_format_;
    }

    VkExtent2D vulkan_presenter::surface_extent() const noexcept
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

        VKGC_VERIFY(frame_index < image_acquired_semaphores_.size());
        auto image_acquired = image_acquired_semaphores_[frame_index];
        auto image_acquired_raw = object_registry_.resolve_handle(image_acquired);
        if (!VKGC_ENSURE_VKHANDLE(image_acquired_raw))
        {
            return std::unexpected{present_status::error};
        }

        auto const result = vkAcquireNextImageKHR(
            device_.handle(),
            swapchain_handle_,
            std::numeric_limits<std::uint64_t>::max(),
            image_acquired_raw,
            VK_NULL_HANDLE,
            &acquired_image_index_);

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

        VKGC_VERIFY(acquired_image_index_ < present_semaphores_.size());
        auto present_semaphore = present_semaphores_[acquired_image_index_];
        if (!VKGC_ENSURE(present_semaphore.is_valid()))
        {
            return std::unexpected{present_status::error};
        }

        VKGC_VERIFY(acquired_image_index_ < swapchain_images_.size());

        return swapchain_image_acquire_result{
            .image_acquired{image_acquired},
            .present_wait{present_semaphore},
            .swapchain_image{swapchain_images_[acquired_image_index_]},
            .image_index{acquired_image_index_}
        };
    }

    present_status vulkan_presenter::request_presentation(VkQueue queue)
    {
        VKGC_VERIFY(acquired_image_index_ < present_semaphores_.size());
        auto wait_semaphore = object_registry_.resolve_handle(present_semaphores_[acquired_image_index_]);
        if (!VKGC_ENSURE_VKHANDLE(wait_semaphore))
        {
            return present_status::error;
        }

        VkPresentInfoKHR const info{
            .sType{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR},
            .pNext{nullptr},
            .waitSemaphoreCount{1},
            .pWaitSemaphores{&wait_semaphore},
            .swapchainCount{1},
            .pSwapchains{&swapchain_handle_},
            .pImageIndices{&acquired_image_index_},
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

        VKGC_VERIFY_VKSUCCESS(vkDeviceWaitIdle(device_.handle()));

        swapchain_images_.clear();

        create_swapchain(surface, previous, swapchain_params);

        if (previous != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device_.handle(), previous, nullptr);
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

    void vulkan_presenter::query_swapchain_images()
    {
        if (!VKGC_ENSURE(device_.is_valid()))
        {
            return;
        }

        std::uint32_t count{0};
        if (!VKGC_ENSURE_VKSUCCESS(vkGetSwapchainImagesKHR(device_.handle(), swapchain_handle_, &count, nullptr)))
        {
            return;
        }

        swapchain_images_.resize(count);

        if (!VKGC_ENSURE_VKSUCCESS(vkGetSwapchainImagesKHR(
                device_.handle(),
                swapchain_handle_,
                &count,
                swapchain_images_.data())))
        {
            swapchain_images_.clear();
        }
    }

    bool vulkan_presenter::create_semaphores(
        std::uint32_t const image_acquired_count,
        std::uint32_t const present_wait_count)
    {
        image_acquired_semaphores_.reserve(image_acquired_count);
        for (std::uint32_t i = 0; i < image_acquired_count; ++i)
        {
            auto const debug_name = std::format("image acquired semaphore [#{}]", i);
            auto const semaphore = object_registry_.create_binary_semaphore(debug_name.c_str());
            if (!semaphore.is_valid())
            {
                return false;
            }

            image_acquired_semaphores_.push_back(semaphore);
        }

        present_semaphores_.reserve(present_wait_count);
        for (std::uint32_t i = 0; i < present_wait_count; ++i)
        {
            auto const debug_name = std::format("present wait semaphore [#{}]", i);
            auto const semaphore = object_registry_.create_binary_semaphore(debug_name.c_str());
            if (!semaphore.is_valid())
            {
                return false;
            }

            present_semaphores_.push_back(semaphore);
        }

        return true;
    }

    void vulkan_presenter::destroy_semaphores()
    {
        for (auto const& semaphores : {present_semaphores_, image_acquired_semaphores_})
        {
            for (auto const handle : semaphores)
            {
                object_registry_.destroy_immediate(handle);
            }
        }

        present_semaphores_.clear();
        image_acquired_semaphores_.clear();
    }
}
