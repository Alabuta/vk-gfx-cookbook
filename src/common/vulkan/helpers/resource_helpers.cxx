module;

#include <cstddef>
#include <cstdint>
#include <format>
#include <print>
#include <span>
#include <vector>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vk_mem_alloc.h"
#include "vulkan/assert.hxx"

module vkgc.vulkan_resource_helpers;

import vkgc.vulkan_handle;
import vkgc.vulkan_device;
import vkgc.vulkan_object_registry;
import vkgc.scope_guard;

namespace vkgc
{
    vk_buffer_handle create_host_writable_buffer(
        vulkan_object_registry& resources,
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
            .usage{VMA_MEMORY_USAGE_AUTO},
            .requiredFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            .preferredFlags{0},
            .memoryTypeBits{0},
            .pool{VK_NULL_HANDLE},
            .pUserData{nullptr},
            .priority{0}
        };

        return resources.create_memory_bound_buffer(buffer_create_info, buffer_allocation_info);
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
            .usage{VMA_MEMORY_USAGE_AUTO},
            .requiredFlags{0},
            .preferredFlags{0},
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

    std::vector<vk_image_view_handle> create_swapchain_image_views(
        vulkan_object_registry& vk_object_registry,
        std::span<VkImage const> const images,
        VkFormat const format)
    {
        std::vector<vk_image_view_handle> image_view_handles;
        image_view_handles.reserve(images.size());

        for (std::uint32_t i{0}; auto image_handle : images)
        {
            VkImageViewCreateInfo const create_info{
                .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .image{image_handle},
                .viewType{VK_IMAGE_VIEW_TYPE_2D},
                .format{format},
                .components{
                    .r{VK_COMPONENT_SWIZZLE_IDENTITY},
                    .g{VK_COMPONENT_SWIZZLE_IDENTITY},
                    .b{VK_COMPONENT_SWIZZLE_IDENTITY},
                    .a{VK_COMPONENT_SWIZZLE_IDENTITY}
                },
                .subresourceRange{
                    .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                    .baseMipLevel{0},
                    .levelCount{1},
                    .baseArrayLayer{0},
                    .layerCount{1}
                }
            };

            auto const debug_name = std::format("swapchain image [#{}]", i++);
            if (auto const view = vk_object_registry.create_image_view(create_info, debug_name.c_str()); view.is_valid())
            {
                image_view_handles.push_back(view);
                continue;
            }

            for (auto const image_view_handle : image_view_handles)
            {
                vk_object_registry.destroy_immediate(image_view_handle);
            }

            return {};
        }

        return image_view_handles;
    }

    bool create_depth_attachment(
        vulkan_object_registry& vk_object_registry,
        VkFormat const image_format,
        VkExtent3D const image_extent,
        vk_image_handle& image,
        vk_image_view_handle& image_view)
    {
        VkImageCreateInfo const image_create_info{
            .sType{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .imageType{VK_IMAGE_TYPE_2D},
            .format{image_format},
            .extent{image_extent},
            .mipLevels{1},
            .arrayLayers{1},
            .samples{VK_SAMPLE_COUNT_1_BIT},
            .tiling{VK_IMAGE_TILING_OPTIMAL},
            .usage{VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
            .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount{0},
            .pQueueFamilyIndices{nullptr},
            .initialLayout{VK_IMAGE_LAYOUT_UNDEFINED}
        };

        VmaAllocationCreateInfo constexpr allocation_create_info{
            .flags{VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT},
            .usage{VMA_MEMORY_USAGE_AUTO},
            .requiredFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            .preferredFlags{0},
            .memoryTypeBits{0},
            .pool{VK_NULL_HANDLE},
            .pUserData{nullptr},
            .priority{0}
        };

        image = vk_object_registry.create_memory_bound_image(
            image_create_info,
            allocation_create_info,
            "depth attachment image");
        if (!image.is_valid())
        {
            return false;
        }

        VkImageViewCreateInfo const depth_view_create_info{
            .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .image{vk_object_registry.resolve_handle(image)},
            .viewType{VK_IMAGE_VIEW_TYPE_2D},
            .format{image_format},
            .components{
                .r{VK_COMPONENT_SWIZZLE_IDENTITY},
                .g{VK_COMPONENT_SWIZZLE_IDENTITY},
                .b{VK_COMPONENT_SWIZZLE_IDENTITY},
                .a{VK_COMPONENT_SWIZZLE_IDENTITY}
            },
            .subresourceRange{
                .aspectMask{VK_IMAGE_ASPECT_DEPTH_BIT},
                .baseMipLevel{0},
                .levelCount{1},
                .baseArrayLayer{0},
                .layerCount{1}
            }
        };

        image_view = vk_object_registry.create_image_view(depth_view_create_info, "depth attachment image view");
        if (!image_view.is_valid())
        {
            vk_object_registry.destroy_immediate(image);
            return false;
        }

        return true;
    }

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
} // namespace vkgc
