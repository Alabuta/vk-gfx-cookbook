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
                    .aspectMask{format_to_image_aspect(format)},
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

    vk_image_handle create_depth_attachment(
        vulkan_object_registry& vk_object_registry,
        VkFormat const image_format,
        VkExtent3D const image_extent)
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

        // view_aspect stays zero, so the default view keeps the full DEPTH|STENCIL aspect the
        // packed depth-stencil formats report. That is what a depth attachment wants; sampling
        // such an image needs a separate single-aspect view.
        image_registration constexpr registration{
            .bindless_slot{kInvalidBindlessSlot},
            .steady_state_layout{VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
            .view_aspect{VK_IMAGE_ASPECT_NONE},
            .create_default_view{true}
        };

        return vk_object_registry.create_memory_bound_image(
            image_create_info,
            allocation_create_info,
            registration,
            "depth attachment image");
    }

    vk_image_handle create_sampled_texture(
        vulkan_object_registry& vk_object_registry,
        VkFormat const image_format,
        VkExtent3D const image_extent)
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

        // The texture is uploaded through a transfer and then read by shaders, so it rests in
        // SHADER_READ_ONLY_OPTIMAL — which is also the layout a descriptor write must declare.
        image_registration constexpr registration{
            .bindless_slot{kInvalidBindlessSlot},
            .steady_state_layout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            .view_aspect{VK_IMAGE_ASPECT_NONE},
            .create_default_view{true}
        };

        return vk_object_registry.create_memory_bound_image(
            image_create_info,
            allocation_create_info,
            registration,
            "sampled texture image");
    }

}
