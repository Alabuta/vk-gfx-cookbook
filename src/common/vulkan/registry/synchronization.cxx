module;

#include <bit>
#include <cstdint>
#include <print>

#include <volk.h>
#include "vulkan/format.hxx"

module vkgc.vulkan_object_registry;

import vkgc.vulkan_handle;
import vkgc.vulkan_device;
import vkgc.vulkan_payload;

namespace vkgc
{
    vk_fence_handle vulkan_object_registry::create_fence(bool const create_signaled, char const* debug_name)
    {
        VkFenceCreateInfo const create_info
        {
            .sType{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO},
            .pNext{nullptr},
            .flags{create_signaled ? static_cast<VkFenceCreateFlags>(VK_FENCE_CREATE_SIGNALED_BIT) : VkFenceCreateFlags{}}
        };

        VkFence handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateFence(device_.handle(), &create_info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create fence ({})", result);
            return {};
        }

        device_.set_debug_object_name(VK_OBJECT_TYPE_FENCE, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_fence_handle>().emplace(handle);
    }

    vk_bin_semaphore_handle vulkan_object_registry::create_binary_semaphore(char const* debug_name)
    {
        VkSemaphoreTypeCreateInfo constexpr type_create_info{
            .sType{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO},
            .pNext{nullptr},
            .semaphoreType{VK_SEMAPHORE_TYPE_BINARY},
            .initialValue{0}
        };

        VkSemaphoreCreateInfo const create_info{
            .sType{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO},
            .pNext{&type_create_info},
            .flags{0}
        };

        VkSemaphore handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateSemaphore(device_.handle(), &create_info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create binary semaphore ({})", result);
            return {};
        }

        device_.set_debug_object_name(VK_OBJECT_TYPE_SEMAPHORE, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_bin_semaphore_handle>().emplace(handle);
    }

    vk_timeline_semaphore_handle vulkan_object_registry::create_timeline_semaphore(
        std::uint64_t const initial_value,
        char const* debug_name)
    {
        VkSemaphoreTypeCreateInfo const type_create_info{
            .sType{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO},
            .pNext{nullptr},
            .semaphoreType{VK_SEMAPHORE_TYPE_TIMELINE},
            .initialValue{initial_value}
        };

        VkSemaphoreCreateInfo const create_info{
            .sType{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO},
            .pNext{&type_create_info},
            .flags{0}
        };

        VkSemaphore handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateSemaphore(device_.handle(), &create_info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create timeline semaphore ({})", result);
            return {};
        }

        device_.set_debug_object_name(VK_OBJECT_TYPE_SEMAPHORE, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_timeline_semaphore_handle>().emplace(handle);

    }
}
