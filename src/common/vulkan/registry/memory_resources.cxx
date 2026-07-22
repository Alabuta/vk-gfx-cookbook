module;

#include <print>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vk_mem_alloc.h"
#include "vulkan/assert.hxx"

module vkgc.vulkan_object_registry;

import vkgc.vulkan_handle;
import vkgc.vulkan_device;
import vkgc.vulkan_payload;

namespace vkgc
{
    vk_image_handle vulkan_object_registry::create_image(VkImageCreateInfo const& info, char const* debug_name)
    {
        VkImage handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateImage(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create image ({})", result);
            return {};
        }

        return finalize_created_object<vk_image_handle>(
            VK_OBJECT_TYPE_IMAGE, handle, debug_name,
            info.format, info.extent, info.mipLevels, info.arrayLayers, vk_allocation_handle{});
    }

    vk_image_handle vulkan_object_registry::create_memory_bound_image(
        VkImageCreateInfo const& image_info,
        VmaAllocationCreateInfo const& allocation_info,
        char const* debug_name)
    {
        VkImage image{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};

        if (auto const result = vmaCreateImage(
                device_.vma_allocator(),
                &image_info,
                &allocation_info,
                &image,
                &allocation,
                nullptr);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create memory bound image ({})", result);
            return {};
        }

        auto allocation_handle = slot_map<vk_allocation_handle>().emplace(allocation);
        return finalize_created_object<vk_image_handle>(
            VK_OBJECT_TYPE_IMAGE, image, debug_name,
            image_info.format, image_info.extent, image_info.mipLevels, image_info.arrayLayers, allocation_handle);
    }

    vk_allocation_handle vulkan_object_registry::allocate_image_memory(
        vk_image_handle const image,
        VmaAllocationCreateInfo const& alloc_info)
    {
        auto const image_handle = resolve_handle(image);
        if (!VKGC_ENSURE_VKHANDLE(image_handle))
        {
            return {};
        }

        VmaAllocation allocation{VK_NULL_HANDLE};
        if (auto const result = vmaAllocateMemoryForImage(
                device_.vma_allocator(),
                image_handle,
                &alloc_info,
                &allocation,
                nullptr);
            result != VK_SUCCESS)
        {
            // Just error logging
            std::println(stderr, "[Vulkan] : Error : failed to allocate memory for image ({})", result);
            return {};
        }

        return slot_map<vk_allocation_handle>().emplace(allocation);
    }

    vk_buffer_handle vulkan_object_registry::create_buffer(VkBufferCreateInfo const& info, char const* debug_name)
    {
        VkBuffer handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateBuffer(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create buffer ({})", result);
            return {};
        }

        return finalize_created_object<vk_buffer_handle>(
            VK_OBJECT_TYPE_BUFFER, handle, debug_name,
            info.size, info.usage, vk_allocation_handle{});
    }

    vk_buffer_handle vulkan_object_registry::create_memory_bound_buffer(
        VkBufferCreateInfo const& buffer_info,
        VmaAllocationCreateInfo const& allocation_info,
        char const* debug_name)
    {
        VkBuffer buffer{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};

        if (auto const result = vmaCreateBuffer(
                device_.vma_allocator(),
                &buffer_info,
                &allocation_info,
                &buffer,
                &allocation,
                nullptr);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create memory bound buffer ({})", result);
            return {};
        }

        auto allocation_handle = slot_map<vk_allocation_handle>().emplace(allocation);
        return finalize_created_object<vk_buffer_handle>(
            VK_OBJECT_TYPE_BUFFER, buffer, debug_name,
            buffer_info.size, buffer_info.usage, allocation_handle);
    }

    vk_allocation_handle vulkan_object_registry::allocate_buffer_memory(
        vk_buffer_handle buffer,
        VmaAllocationCreateInfo const& alloc_info)
    {
        auto const buffer_handle = resolve_handle(buffer);
        if (!VKGC_ENSURE_VKHANDLE(buffer_handle))
        {
            return {};
        }

        VmaAllocation allocation{VK_NULL_HANDLE};
        if (auto const result = vmaAllocateMemoryForBuffer(
                device_.vma_allocator(),
                buffer_handle,
                &alloc_info,
                &allocation,
                nullptr);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to allocate memory for buffer ({})", result);
            return {};
        }

        return slot_map<vk_allocation_handle>().emplace(allocation);
    }

    vk_image_view_handle vulkan_object_registry::create_image_view(
        VkImageViewCreateInfo const& info,
        char const* debug_name)
    {
        VkImageView handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateImageView(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create image view ({})", result);
            return {};
        }

        return finalize_created_object<vk_image_view_handle>(VK_OBJECT_TYPE_IMAGE_VIEW, handle, debug_name, info.format);
    }

    vk_sampler_handle vulkan_object_registry::create_sampler(VkSamplerCreateInfo const& info, char const* debug_name)
    {
        VkSampler handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateSampler(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create sampler ({})", result);
            return {};
        }

        return finalize_created_object<vk_sampler_handle>(VK_OBJECT_TYPE_SAMPLER, handle, debug_name);
    }
}
