module;

#include <cstddef>

#include <volk.h>
#include "vk_mem_alloc.h"
#include "diagnostic/assert.hxx"

module vkgc.vulkan_resource_helpers;

import vkgc.vulkan_handle;
import vkgc.vulkan_object_registry;

namespace vkgc
{
    vk_buffer_handle create_host_writable_buffer(
        vulkan_object_registry& vk_object_registry,
        std::size_t const size,
        VkBufferUsageFlags const usage)
    {
        if (!VKGC_ENSURE(size > 0))
        {
            return {};
        }

        VkBufferCreateInfo const buffer_create_info{
            .sType{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .size{static_cast<VkDeviceSize>(size)},
            .usage{usage},
            .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount{0},
            .pQueueFamilyIndices{nullptr},
        };

        VmaAllocationCreateInfo constexpr buffer_allocation_info{
            .flags{
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
            },
            .usage{VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE},
            .requiredFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            .preferredFlags{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT},
            .memoryTypeBits{0},
            .pool{VK_NULL_HANDLE},
            .pUserData{nullptr},
            .priority{0}
        };

        return vk_object_registry.create_memory_bound_buffer(buffer_create_info, buffer_allocation_info);
    }

    vk_buffer_handle create_staging_buffer(vulkan_object_registry& vk_object_registry, std::size_t const size)
    {
        if (!VKGC_ENSURE(size > 0))
        {
            return {};
        }

        VkBufferCreateInfo const buffer_create_info{
            .sType{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .size{static_cast<VkDeviceSize>(size)},
            .usage{VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
            .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount{0},
            .pQueueFamilyIndices{nullptr},
        };

        VmaAllocationCreateInfo constexpr buffer_allocation_info{
            .flags{
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT
            },
            .usage{VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE},
            .requiredFlags{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT},
            .preferredFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT},
            .memoryTypeBits{0},
            .pool{VK_NULL_HANDLE},
            .pUserData{nullptr},
            .priority{0}
        };

        return vk_object_registry.create_memory_bound_buffer(
            buffer_create_info,
            buffer_allocation_info,
            "staging buffer");
    }

    vk_buffer_handle create_vertex_buffer(vulkan_object_registry& vk_object_registry, std::size_t const size)
    {
        return create_host_writable_buffer(
            vk_object_registry,
            size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }

    vk_buffer_handle create_index_buffer(vulkan_object_registry& vk_object_registry, std::size_t const size)
    {
        return create_host_writable_buffer(
            vk_object_registry,
            size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }

}
