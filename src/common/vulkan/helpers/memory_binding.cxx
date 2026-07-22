module;

#include <print>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vk_mem_alloc.h"
#include "vulkan/assert.hxx"

module vkgc.vulkan_resource_helpers;

import vkgc.vulkan_handle;
import vkgc.vulkan_device;
import vkgc.vulkan_context;
import vkgc.vulkan_object_registry;

namespace vkgc
{
    bool bind_image_memory(
        vulkan_object_registry const& vk_object_registry,
        vk_image_handle const image,
        vk_allocation_handle const memory)
    {
        VkImage image_handle = vk_object_registry.resolve_handle(image);
        if (!VKGC_ENSURE_VKHANDLE(image_handle))
        {
            return false;
        }

        VmaAllocation vma_allocation_handle = vk_object_registry.resolve_handle(memory);
        if (!VKGC_ENSURE_VKHANDLE(vma_allocation_handle))
        {
            return false;
        }

        if (auto const result = vmaBindImageMemory(
                vk_object_registry.device().vma_allocator(),
                vma_allocation_handle,
                image_handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to bind image memory ({})", result);
            return false;
        }

        return true;
    }

    bool bind_buffer_memory(
        vulkan_object_registry const& vk_object_registry,
        vk_buffer_handle const buffer,
        vk_allocation_handle const memory)
    {
        VkBuffer buffer_handle = vk_object_registry.resolve_handle(buffer);
        if (!VKGC_ENSURE_VKHANDLE(buffer_handle))
        {
            return false;
        }

        VmaAllocation vma_allocation_handle = vk_object_registry.resolve_handle(memory);
        if (!VKGC_ENSURE_VKHANDLE(vma_allocation_handle))
        {
            return false;
        }

        if (auto const result = vmaBindBufferMemory(
                vk_object_registry.device().vma_allocator(),
                vma_allocation_handle,
                buffer_handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to bind buffer memory ({})", result);
            return false;
        }

        return true;
    }

    VkMemoryPropertyFlags get_allocation_memory_properties(
        vulkan_context const& vk_context,
        vulkan_object_registry const& vk_object_registry,
        vk_allocation_handle const allocation)
    {
        VkMemoryPropertyFlags memory_property_flags;
        vmaGetAllocationMemoryProperties(
            vk_context.device().vma_allocator(),
            vk_object_registry.resolve_handle(allocation),
            &memory_property_flags);

        return memory_property_flags;
    }

    VmaAllocationInfo2 get_vma_allocation_info(
        vulkan_context const& vk_context,
        vulkan_object_registry const& vk_object_registry,
        vk_allocation_handle const allocation)
    {
        VmaAllocationInfo2 allocation_info;
        vmaGetAllocationInfo2(
            vk_context.device().vma_allocator(),
            vk_object_registry.resolve_handle(allocation),
            &allocation_info);

        return allocation_info;
    }
}
