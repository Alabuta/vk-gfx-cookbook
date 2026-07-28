module;

#include <format>
#include <print>
#include <string>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vk_mem_alloc.h"
#include "vulkan/assert.hxx"

module vkgc.vulkan_object_registry;

import vkgc.vulkan_handle;
import vkgc.vulkan_device;
import vkgc.vulkan_payload;

namespace
{
    [[nodiscard]]
    vkgc::image_desc to_image_desc(VkImageCreateInfo const& info)
    {
        return vkgc::image_desc{
            .format{info.format},
            .extent{info.extent},
            .mip_levels{info.mipLevels},
            .array_layers{info.arrayLayers},
            .type{info.imageType},
            .samples{info.samples},
            .usage{info.usage},
            .flags{info.flags},
            .aspect{vkgc::format_to_image_aspect(info.format)}
        };
    }

    // A default view spans the whole resource, so its type follows the image's own geometry.
    [[nodiscard]]
    VkImageViewType to_default_view_type(vkgc::image_desc const& desc)
    {
        bool const is_layered = desc.array_layers > 1;

        switch (desc.type)
        {
        case VK_IMAGE_TYPE_1D:
            return is_layered ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        case VK_IMAGE_TYPE_3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        default:
            break;
        }

        if ((desc.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0 && desc.array_layers % 6u == 0u)
        {
            return desc.array_layers > 6u ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        }

        return is_layered ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    }

    // Usage bits that make a view meaningful at all. Transfer-only and staging images get none,
    // and paying for one would just be a handle nobody ever resolves.
    [[nodiscard]]
    bool usage_needs_default_view(VkImageUsageFlags const usage)
    {
        static VkImageUsageFlags constexpr kViewableUsage{
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
        };

        return (usage & kViewableUsage) != 0;
    }
}

namespace vkgc
{
    vk_image_handle vulkan_object_registry::create_image(
        VkImageCreateInfo const& info,
        image_registration const& registration,
        char const* debug_name)
    {
        VkImage handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateImage(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create image ({})", result);
            return {};
        }

        return finalize_created_image(handle, vk_allocation_handle{}, info, registration, debug_name);
    }

    vk_image_handle vulkan_object_registry::create_memory_bound_image(
        VkImageCreateInfo const& image_info,
        VmaAllocationCreateInfo const& allocation_info,
        image_registration const& registration,
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

        auto const allocation_handle = slot_map<vk_allocation_handle>().emplace(allocation);
        return finalize_created_image(image, allocation_handle, image_info, registration, debug_name);
    }

    // Shared tail of both image-creation paths: derive the immutable description, eagerly create the
    // default view, and register the finished payload. The view is a dependent of the image, so a
    // failure here unwinds the image (and its allocation) rather than handing back a half-built
    // resource whose default_view silently stays invalid.
    vk_image_handle vulkan_object_registry::finalize_created_image(
        VkImage const image,
        vk_allocation_handle const allocation,
        VkImageCreateInfo const& info,
        image_registration const& registration,
        char const* const debug_name)
    {
        auto const desc = to_image_desc(info);

        vk_image_view_handle default_view{};
        if (registration.create_default_view && usage_needs_default_view(desc.usage))
        {
            VkImageViewCreateInfo const view_create_info{
                .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .image{image},
                .viewType{to_default_view_type(desc)},
                .format{desc.format},
                .components{
                    .r{VK_COMPONENT_SWIZZLE_IDENTITY},
                    .g{VK_COMPONENT_SWIZZLE_IDENTITY},
                    .b{VK_COMPONENT_SWIZZLE_IDENTITY},
                    .a{VK_COMPONENT_SWIZZLE_IDENTITY}
                },
                .subresourceRange{
                    .aspectMask{
                        registration.view_aspect != VK_IMAGE_ASPECT_NONE ? registration.view_aspect : desc.aspect
                    },
                    .baseMipLevel{0},
                    .levelCount{desc.mip_levels},
                    .baseArrayLayer{0},
                    .layerCount{desc.array_layers}
                }
            };

            auto const view_debug_name = debug_name != nullptr && *debug_name != '\0'
                ? std::format("{} default view", debug_name)
                : std::string{};

            default_view = create_image_view(view_create_info, view_debug_name.c_str());
            if (!default_view.is_valid())
            {
                vkDestroyImage(device_.handle(), image, nullptr);
                if (allocation.is_valid())
                {
                    destroy_immediate(allocation);
                }

                return {};
            }
        }

        return finalize_created_payload<vk_image_handle>(
            VK_OBJECT_TYPE_IMAGE,
            debug_name,
            vk_object_payload<vk_image_handle>{
                .handle{image},
                .desc{desc},
                .allocation{allocation},
                .default_view{default_view},
                .steady_state_layout{registration.steady_state_layout},
                .bindless_slot{registration.bindless_slot}
            });
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
