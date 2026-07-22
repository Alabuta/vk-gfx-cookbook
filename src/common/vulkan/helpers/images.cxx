module;

#include <cstdint>
#include <format>
#include <span>
#include <vector>

#include <volk.h>
#include "vk_mem_alloc.h"

module vkgc.vulkan_resource_helpers;

import vkgc.vulkan_handle;
import vkgc.vulkan_object_registry;

namespace vkgc
{
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

    bool create_sampled_texture(
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
            .usage{VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT},
            .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount{0},
            .pQueueFamilyIndices{nullptr},
            .initialLayout{VK_IMAGE_LAYOUT_UNDEFINED}
        };

        VmaAllocationCreateInfo constexpr allocation_create_info{
            .flags{0},
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
            "sampled texture image");
        if (!image.is_valid())
        {
            return false;
        }

        VkImageViewCreateInfo const texture_view_create_info{
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
                .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                .baseMipLevel{0},
                .levelCount{1},
                .baseArrayLayer{0},
                .layerCount{1}
            }
        };

        image_view = vk_object_registry.create_image_view(texture_view_create_info, "sampled texture image view");
        if (!image_view.is_valid())
        {
            vk_object_registry.destroy_immediate(image);
            return false;
        }

        return true;
    }
}
