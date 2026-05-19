module;

#include <algorithm>
#include <cstdint>
#include <limits>
#include <print>
#include <ranges>
#include <vector>

#include <volk.h>

module vkgc.vulkan_frame_ring;

import vkgc.vulkan_format;
import vkgc.vulkan_handle;
import vkgc.vulkan_object_registry;

namespace vkgc
{
    vulkan_frame_ring::vulkan_frame_ring(vulkan_object_registry& object_registry, std::uint32_t const frames_in_flight)
        : object_registry_{object_registry}
    {
        frame_fences_.resize(frames_in_flight);
        slot_pending_.resize(frames_in_flight);

        std::ranges::generate(
            frame_fences_,
            [this]
            {
                return object_registry_.create_fence({
                    .sType{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO},
                    .pNext{nullptr},
                    .flags{VK_FENCE_CREATE_SIGNALED_BIT}
                });
            });

    }

    vulkan_frame_ring::~vulkan_frame_ring()
    {
        if (VkDevice device_handle = object_registry_.device().handle();
            device_handle != VK_NULL_HANDLE && !frame_fences_.empty())
        {
            std::vector<VkFence> raw_fences;
            raw_fences.reserve(frame_fences_.size());

            auto projection = frame_fences_ | std::ranges::views::transform([this](vk_fence_handle const handle)
            {
                return object_registry_.resolve_handle(handle);
            });
            std::ranges::copy_if(
                projection,
                std::back_inserter(raw_fences),
                [](VkFence const fence) { return fence != VK_NULL_HANDLE; });

            if (!raw_fences.empty())
            {
                if (auto const result = vkWaitForFences(
                        device_handle,
                        static_cast<std::uint32_t>(raw_fences.size()),
                        raw_fences.data(),
                        VK_TRUE,
                        std::numeric_limits<std::uint64_t>::max());
                    result != VK_SUCCESS)
                {
                    std::println(
                        stderr,
                        "[Vulkan] : Warning : ~vulkan_frame_ring encountered an error on 'vkWaitForFences' ({})",
                        result);
                }
            }
        }

        for (std::uint32_t i = 0; i < slot_pending_.size(); ++i)
        {
            drain_slot(i);
        }

        for (auto const fence : frame_fences_)
        {
            object_registry_.destroy_immediate(fence);
        }

        frame_fences_.clear();
        slot_pending_.clear();
    }

    std::uint32_t vulkan_frame_ring::begin_frame()
    {
        VkDevice device_handle = object_registry_.device().handle();
        VkFence frame_fence = object_registry_.resolve_handle(current_frame_fence());

        if (device_handle != VK_NULL_HANDLE && frame_fence != VK_NULL_HANDLE)
        {
            if (auto const result = vkWaitForFences(
                    device_handle,
                    1,
                    &frame_fence,
                    VK_TRUE,
                    std::numeric_limits<std::uint64_t>::max());
                result != VK_SUCCESS)
            {
                std::println(
                    stderr,
                    "[Vulkan] : Warning : vulkan_frame_ring::begin_frame fence wait failed ({})",
                    result);
            }

            // Destroying previously enqueued objects
            drain_slot(current_frame_index_);

            if (auto const result = vkResetFences(device_handle, 1, &frame_fence); result != VK_SUCCESS)
            {
                std::println(
                    stderr,
                    "[Vulkan] : Warning : vulkan_frame_ring::begin_frame fence reset failed ({})",
                    result);
            }
        }

        return current_frame_index_;
    }

    void vulkan_frame_ring::end_frame()
    {
        if (frame_fences_.empty())
        {
            return;
        }

        current_frame_index_ = (current_frame_index_ + 1) % static_cast<std::uint32_t>(frame_fences_.size());
    }

    vk_fence_handle vulkan_frame_ring::current_frame_fence() const noexcept
    {
        if (frame_fences_.empty())
        {
            return {};
        }

        return frame_fences_[current_frame_index_];
    }

    std::uint32_t vulkan_frame_ring::current_frame_index() const noexcept
    {
        return current_frame_index_;
    }

    std::uint32_t vulkan_frame_ring::frames_in_flight() const noexcept
    {
        return static_cast<std::uint32_t>(frame_fences_.size());
    }

    void vulkan_frame_ring::drain_slot(std::uint32_t const frame_index)
    {
        if (frame_index >= slot_pending_.size())
        {
            return;
        }

        pending& slot_pending = slot_pending_[frame_index];

        // Match the destruction order used by ~vulkan_object_registry.
        drain_one<vk_object_tags::command_buffer>(slot_pending);
        drain_one<vk_object_tags::command_pool>(slot_pending);
        drain_one<vk_object_tags::image_view>(slot_pending);
        drain_one<vk_object_tags::image>(slot_pending);
        drain_one<vk_object_tags::buffer>(slot_pending);
        drain_one<vk_object_tags::allocation>(slot_pending);
        drain_one<vk_object_tags::semaphore>(slot_pending);
        drain_one<vk_object_tags::fence>(slot_pending);
    }
}
