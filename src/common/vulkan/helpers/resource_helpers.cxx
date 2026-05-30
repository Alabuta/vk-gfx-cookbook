module;

#include <print>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vk_mem_alloc.h"
#include "vulkan/assert.hxx"

module vkgc.vulkan_resource_helpers;

import vkgc.vulkan_handle;
import vkgc.vulkan_device;
import vkgc.vulkan_object_registry;

namespace vkgc
{
    bool bind_image_memory(
        vulkan_object_registry const& resources,
        vk_image_handle const image,
        vk_allocation_handle const memory)
    {
        VkImage image_handle = resources.resolve_handle(image);
        if (!VKGC_ENSURE_VKHANDLE(image_handle))
        {
            return false;
        }

        VmaAllocation vma_allocation_handle = resources.resolve_handle(memory);
        if (!VKGC_ENSURE_VKHANDLE(vma_allocation_handle))
        {
            return false;
        }

        if (auto const result = vmaBindImageMemory(
                resources.device().vma_allocator(),
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
        vulkan_object_registry const& resources,
        vk_buffer_handle const buffer,
        vk_allocation_handle const memory)
    {
        VkBuffer buffer_handle = resources.resolve_handle(buffer);
        if (!VKGC_ENSURE_VKHANDLE(buffer_handle))
        {
            return false;
        }

        VmaAllocation vma_allocation_handle = resources.resolve_handle(memory);
        if (!VKGC_ENSURE_VKHANDLE(vma_allocation_handle))
        {
            return false;
        }

        if (auto const result = vmaBindBufferMemory(
                resources.device().vma_allocator(),
                vma_allocation_handle,
                buffer_handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to bind buffer memory ({})", result);
            return false;
        }

        return true;
    }

    bool create_memory_bound_image(
        vulkan_object_registry& resources,
        VkImageCreateInfo const& create_info,
        VmaAllocationCreateInfo const& alloc_info,
        vk_image_handle& out_image,
        vk_allocation_handle& out_memory)
    {
        auto const image = resources.create_image(create_info);
        if (!image.is_valid())
        {
            return false;
        }

        auto const memory = resources.allocate_image_memory(image, alloc_info);
        if (!memory.is_valid())
        {
            resources.destroy_immediate(image);
            return false;
        }

        if (!bind_image_memory(resources, image, memory))
        {
            resources.destroy_immediate(image);
            resources.destroy_immediate(memory);
            return false;
        }

        out_image = image;
        out_memory = memory;
        return true;
    }

    bool create_memory_bound_buffer(
        vulkan_object_registry& resources,
        VkBufferCreateInfo const& create_info,
        VmaAllocationCreateInfo const& alloc_info,
        vk_buffer_handle& out_buffer,
        vk_allocation_handle& out_memory)
    {
        auto const buffer = resources.create_buffer(create_info);
        if (!buffer.is_valid())
        {
            return false;
        }

        auto const memory = resources.allocate_buffer_memory(buffer, alloc_info);
        if (!memory.is_valid())
        {
            resources.destroy_immediate(buffer);
            return false;
        }

        if (!bind_buffer_memory(resources, buffer, memory))
        {
            resources.destroy_immediate(buffer);
            resources.destroy_immediate(memory);
            return false;
        }

        out_buffer = buffer;
        out_memory = memory;
        return true;
    }
}
