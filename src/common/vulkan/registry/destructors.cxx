module;

#include <cstdint>

#include <volk.h>
#include "vk_mem_alloc.h"

module vkgc.vulkan_object_registry;

import vkgc.vulkan_device;
import vkgc.vulkan_payload;

namespace vkgc
{
    void vulkan_object_registry::destroy_payload(image_payload const& payload) noexcept
    {
        if (payload.handle != VK_NULL_HANDLE)
        {
            vkDestroyImage(device_.handle(), payload.handle, nullptr);
        }
    }

    void vulkan_object_registry::destroy_payload(image_view_payload const& payload) noexcept
    {
        if (payload.handle != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device_.handle(), payload.handle, nullptr);
        }
    }

    void vulkan_object_registry::destroy_payload(buffer_payload const& payload) noexcept
    {
        if (payload.handle != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device_.handle(), payload.handle, nullptr);
        }
    }

    void vulkan_object_registry::destroy_payload(allocation_payload const& payload) noexcept
    {
        if (payload.handle != VK_NULL_HANDLE && device_.vma_allocator() != VK_NULL_HANDLE)
        {
            vmaFreeMemory(device_.vma_allocator(), payload.handle);
        }
    }

    void vulkan_object_registry::destroy_payload(fence_payload const& payload) noexcept
    {
        if (payload.handle != VK_NULL_HANDLE)
        {
            vkDestroyFence(device_.handle(), payload.handle, nullptr);
        }
    }

    void vulkan_object_registry::destroy_payload(semaphore_payload const& payload) noexcept
    {
        if (payload.handle != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device_.handle(), payload.handle, nullptr);
        }
    }

    void vulkan_object_registry::destroy_payload(command_pool_payload const& payload) noexcept
    {
        if (payload.handle != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device_.handle(), payload.handle, nullptr);
        }
    }

    void vulkan_object_registry::destroy_payload(command_buffer_payload const& payload) noexcept
    {
        if (payload.handle != VK_NULL_HANDLE && payload.source_pool != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(device_.handle(), payload.source_pool, 1, &payload.handle);
        }
    }
}
