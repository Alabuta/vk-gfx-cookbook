module;

#include <algorithm>
#include <cstdint>
#include <print>
#include <ranges>
#include <vector>

#include <volk.h>
#include "vk_mem_alloc.h"

module vkgc.vulkan_object_registry;

import vkgc.vulkan_format;
import vkgc.vulkan_handle;
import vkgc.vulkan_device;
import vkgc.vulkan_payload;

namespace vkgc
{
    vulkan_object_registry::vulkan_object_registry(vulkan_device const& device) noexcept
        : device_{device}
    {}

    vulkan_object_registry::~vulkan_object_registry()
    {
        VkDevice device_handle = device_.handle();
        if (device_handle == VK_NULL_HANDLE)
        {
            return;
        }

        if (auto const result = vkDeviceWaitIdle(device_handle); result != VK_SUCCESS)
        {
            std::println(
                stderr,
                "[Vulkan] : Warning : ~vulkan_object_registry encountered an error on 'vkDeviceWaitIdle' ({})",
                result);
        }

        // Destruction order: command_buffers -> command_pools -> image_views ->
        // images -> buffers -> allocations -> semaphores -> fences.
        drain_pool_in_destructor<vk_object_tags::command_buffer>();
        drain_pool_in_destructor<vk_object_tags::command_pool>();
        drain_pool_in_destructor<vk_object_tags::image_view>();
        drain_pool_in_destructor<vk_object_tags::image>();
        drain_pool_in_destructor<vk_object_tags::buffer>();
        drain_pool_in_destructor<vk_object_tags::allocation>();
        drain_pool_in_destructor<vk_object_tags::semaphore>();
        drain_pool_in_destructor<vk_object_tags::fence>();
    }

    vk_image_handle vulkan_object_registry::create_image(VkImageCreateInfo const& info)
    {
        VkImage handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateImage(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to create image ({})", result);
            return {};
        }

        return slot_map<vk_object_tags::image>().insert(image_payload{
            .handle{handle},
            .format{info.format},
            .extent{info.extent},
            .mip_levels{info.mipLevels},
            .array_layers{info.arrayLayers}
        });
    }

    vk_allocation_handle vulkan_object_registry::allocate_image_memory(
        vk_image_handle const image,
        VmaAllocationCreateInfo const& alloc_info)
    {
        VkImage const image_handle = resolve_handle(image);
        if (image_handle == VK_NULL_HANDLE)
        {
            std::println(stderr, "[Vulkan] : Fatal : allocate_image_memory called with stale image handle");
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
            std::println(stderr, "[Vulkan] : Fatal : failed to allocate memory for image ({})", result);
            return {};
        }

        return slot_map<vk_object_tags::allocation>().insert(allocation_payload{.handle{allocation}});
    }

    vk_buffer_handle vulkan_object_registry::create_buffer(VkBufferCreateInfo const& info)
    {
        VkBuffer handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateBuffer(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to create buffer ({})", result);
            return {};
        }

        return slot_map<vk_object_tags::buffer>().insert(buffer_payload{
            .handle{handle},
            .size{info.size},
            .usage{info.usage}
        });
    }

    vk_allocation_handle vulkan_object_registry::allocate_buffer_memory(
        vk_buffer_handle buffer,
        VmaAllocationCreateInfo const& alloc_info)
    {
        VkBuffer buffer_handle = resolve_handle(buffer);
        if (buffer_handle == VK_NULL_HANDLE)
        {
            std::println(stderr, "[Vulkan] : Fatal : allocate_buffer_memory called with stale buffer handle");
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
            std::println(stderr, "[Vulkan] : Fatal : failed to allocate memory for buffer ({})", result);
            return {};
        }

        return slot_map<vk_object_tags::allocation>().insert(allocation_payload{.handle{allocation}});
    }

    vk_image_view_handle vulkan_object_registry::create_image_view(VkImageViewCreateInfo const& info)
    {
        VkImageView handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateImageView(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to create image view ({})", result);
            return {};
        }

        return slot_map<vk_object_tags::image_view>().insert(image_view_payload{
            .handle{handle},
            .format{info.format}
        });
    }

    vk_fence_handle vulkan_object_registry::create_fence(VkFenceCreateInfo const& info)
    {
        VkFence handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateFence(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to create fence ({})", result);
            return {};
        }

        return slot_map<vk_object_tags::fence>().insert(fence_payload{.handle{handle}});
    }

    vk_semaphore_handle vulkan_object_registry::create_semaphore(VkSemaphoreCreateInfo const& info)
    {
        VkSemaphore handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateSemaphore(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to create semaphore ({})", result);
            return {};
        }

        return slot_map<vk_object_tags::semaphore>().insert(semaphore_payload{.handle{handle}});
    }

    vk_command_pool_handle vulkan_object_registry::command_pool_create(VkCommandPoolCreateInfo const& info)
    {
        VkCommandPool handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateCommandPool(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to create command pool ({})", result);
            return {};
        }

        return slot_map<vk_object_tags::command_pool>().insert(command_pool_payload{
            .handle{handle},
            .queue_family_index{info.queueFamilyIndex}
        });
    }

    std::vector<vk_command_buffer_handle> vulkan_object_registry::allocate_command_buffers(
        vk_command_pool_handle const pool,
        std::uint32_t const count,
        bool const is_primary)
    {
        VkCommandPool command_pool_handle = resolve_handle(pool);
        if (command_pool_handle == VK_NULL_HANDLE)
        {
            std::println(stderr, "[Vulkan] : Fatal : allocate_command_buffers called with stale pool handle");
            return {};
        }

        VkCommandBufferAllocateInfo const allocate_info{
            .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO},
            .pNext{nullptr},
            .commandPool{command_pool_handle},
            .level{is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY},
            .commandBufferCount{count}
        };

        std::vector<VkCommandBuffer> raw_handles(count, VK_NULL_HANDLE);
        if (auto const result = vkAllocateCommandBuffers(device_.handle(), &allocate_info, raw_handles.data());
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to allocate command buffers ({})", result);
            return {};
        }

        std::vector<vk_command_buffer_handle> handles;
        handles.reserve(count);

        auto&& vulkan_slot_map = slot_map<vk_object_tags::command_buffer>();

        std::ranges::transform(
            raw_handles,
            std::back_inserter(handles),
            [command_pool_handle, is_primary, &vulkan_slot_map](VkCommandBuffer raw_handle)
            {
                return vulkan_slot_map.insert(command_buffer_payload{
                    .handle{raw_handle},
                    .source_pool{command_pool_handle},
                    .is_primary{is_primary}
                });
            });

        return handles;
    }

    VkFormat vulkan_object_registry::image_format(vk_image_handle handle) const noexcept
    {
        image_payload const* p = slot_map<vk_object_tags::image>().try_get(handle);
        return p != nullptr ? p->format : VK_FORMAT_UNDEFINED;
    }

    VkExtent3D vulkan_object_registry::image_extent(vk_image_handle handle) const noexcept
    {
        image_payload const* p = slot_map<vk_object_tags::image>().try_get(handle);
        return p != nullptr ? p->extent : VkExtent3D{};
    }

    VkDeviceSize vulkan_object_registry::buffer_size(vk_buffer_handle handle) const noexcept
    {
        buffer_payload const* p = slot_map<vk_object_tags::buffer>().try_get(handle);
        return p != nullptr ? p->size : VkDeviceSize{0};
    }

}
