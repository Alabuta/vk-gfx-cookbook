module;

#include <algorithm>
#include <bit>
#include <cstdint>
#include <print>
#include <vector>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vulkan/assert.hxx"

module vkgc.vulkan_object_registry;

import vkgc.vulkan_handle;
import vkgc.vulkan_device;
import vkgc.vulkan_payload;

namespace vkgc
{
    vk_command_pool_handle vulkan_object_registry::create_command_pool(
        VkCommandPoolCreateInfo const& info,
        char const* debug_name)
    {
        VkCommandPool handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateCommandPool(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create command pool ({})", result);
            return {};
        }

        device_.set_debug_object_name(VK_OBJECT_TYPE_COMMAND_POOL, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_command_pool_handle>().emplace(handle, info.queueFamilyIndex);
    }

    std::vector<vk_command_buffer_handle> vulkan_object_registry::allocate_command_buffers(
        vk_command_pool_handle const pool,
        std::uint32_t const count,
        bool const is_primary)
    {
        auto const command_pool_handle = resolve_handle(pool);
        if (!VKGC_ENSURE_VKHANDLE(command_pool_handle))
        {
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
            std::println(stderr, "[Vulkan] : Error : failed to allocate command buffers ({})", result);
            return {};
        }

        std::vector<vk_command_buffer_handle> handles;
        handles.reserve(count);

        auto&& vulkan_slot_map = slot_map<vk_command_buffer_handle>();

        std::ranges::transform(
            raw_handles,
            std::back_inserter(handles),
            [command_pool_handle, is_primary, &vulkan_slot_map](VkCommandBuffer raw_handle)
            {
                return vulkan_slot_map.emplace(raw_handle, command_pool_handle, is_primary);
            });

        return handles;
    }
}
