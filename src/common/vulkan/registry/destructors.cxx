module;

#include <volk.h>
#include "vk_mem_alloc.h"

module vkgc.vulkan_object_registry;

import vkgc.vulkan_device;
import vkgc.vulkan_payload;

namespace vkgc
{
    void vulkan_object_registry::destroy_payload(image_payload const& payload) noexcept
    {
        vkDestroyImage(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(image_view_payload const& payload) noexcept
    {
        vkDestroyImageView(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(buffer_payload const& payload) noexcept
    {
        vkDestroyBuffer(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(allocation_payload const& payload) noexcept
    {
        vmaFreeMemory(device_.vma_allocator(), payload.handle);
    }

    void vulkan_object_registry::destroy_payload(fence_payload const& payload) noexcept
    {
        vkDestroyFence(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(semaphore_payload const& payload) noexcept
    {
        vkDestroySemaphore(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(command_pool_payload const& payload) noexcept
    {
        vkDestroyCommandPool(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(command_buffer_payload const& payload) noexcept
    {
        vkFreeCommandBuffers(device_.handle(), payload.source_pool, 1, &payload.handle);
    }
}
