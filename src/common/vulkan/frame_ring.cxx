module;

#include <cstdint>
#include <limits>
#include <vector>

#include <volk.h>

#include "assert.hxx"

module vkgc.vulkan_frame_ring;

import vkgc.vulkan_handle;
import vkgc.vulkan_object_registry;

namespace vkgc
{
    vulkan_frame_ring::vulkan_frame_ring(vulkan_object_registry& object_registry, std::uint32_t const frames_in_flight)
        : object_registry_{object_registry}, kFramesInFlight_{frames_in_flight}
    {
        VKGC_CHECK(kFramesInFlight_ > 1);

        pending_queue_slots_.resize(kFramesInFlight_);

        frame_slot_semaphore_ = object_registry_.create_timeline_semaphore(0, "frame slot semaphore");
        VKGC_VERIFY(frame_slot_semaphore_.is_valid());
    }

    vulkan_frame_ring::~vulkan_frame_ring()
    {
        auto const device_handle = object_registry_.device().handle();
        if (!VKGC_ENSURE_VKHANDLE(device_handle))
        {
            return;
        }

        if (auto const wait_semaphore = object_registry_.resolve_handle(frame_slot_semaphore_);
            VKGC_ENSURE_VKHANDLE(wait_semaphore))
        {
            VkSemaphoreWaitInfo const wait_info{
                .sType{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO},
                .pNext{nullptr},
                .flags{0},
                .semaphoreCount{1},
                .pSemaphores{&wait_semaphore},
                .pValues{&ended_frame_index_},
            };

            VKGC_ENSURE_VKSUCCESS(
                vkWaitSemaphores(device_handle, &wait_info, std::numeric_limits<std::uint64_t>::max()));
        }

        for (std::uint32_t slot_index = 0; slot_index < pending_queue_slots_.size(); ++slot_index)
        {
            drain_slot(slot_index);
        }

        pending_queue_slots_.clear();

        object_registry_.destroy_immediate(frame_slot_semaphore_);
    }

    std::uint32_t vulkan_frame_ring::begin_frame(std::uint64_t const frame_index)
    {
        VKGC_CHECKF(started_frame_index_ == ended_frame_index_, "starting a frame without properly ending the last one");
        VKGC_CHECKF(frame_index >= kFramesInFlight_, "frame index has to be greater or equal to frames-in-flight number");

        auto const device_handle = object_registry_.device().handle();
        auto const wait_semaphore = object_registry_.resolve_handle(frame_slot_semaphore_);

        if (!VKGC_ENSURE_VKHANDLE(device_handle) || !VKGC_ENSURE_VKHANDLE(wait_semaphore))
        {
            return std::numeric_limits<std::uint32_t>::max();
        }

        frame_slot_index_ = static_cast<std::uint32_t>(frame_index % kFramesInFlight_);

        {
            std::uint64_t const wait_value = frame_index - kFramesInFlight_;
            VKGC_CHECK(wait_value < std::numeric_limits<std::uint32_t>::max());

            VkSemaphoreWaitInfo const wait_info{
                .sType{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO},
                .pNext{nullptr},
                .flags{0},
                .semaphoreCount{1},
                .pSemaphores{&wait_semaphore},
                .pValues{&wait_value},
            };

            if (!VKGC_ENSURE_VKSUCCESS(
                    vkWaitSemaphores(device_handle, &wait_info, std::numeric_limits<std::uint64_t>::max())))
            {
                return std::numeric_limits<std::uint32_t>::max();
            }
        }

        // Destroying previously enqueued objects
        drain_slot(frame_slot_index_);

        started_frame_index_ = frame_index;

        return frame_slot_index_;
    }

    void vulkan_frame_ring::end_frame()
    {
        ended_frame_index_ = started_frame_index_;
    }

    vk_timeline_semaphore_handle vulkan_frame_ring::frame_slot_semaphore() const noexcept
    {
        return frame_slot_semaphore_;
    }

    std::uint32_t vulkan_frame_ring::frames_in_flight() const noexcept
    {
        return kFramesInFlight_;
    }

    void vulkan_frame_ring::drain_slot(std::uint32_t const slot_index)
    {
        if (!VKGC_ENSURE(slot_index < pending_queue_slots_.size()))
        {
            return;
        }

        auto& queue = pending_queue_slots_[slot_index];

        using T = std::remove_reference_t<decltype(queue.buckets)>;

        [this, &queue]<std::size_t... I>(std::index_sequence<I...>)
        {
            ((drain_one<typename std::tuple_element_t<I, T>::value_type>(queue)), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>{});
    }
}
