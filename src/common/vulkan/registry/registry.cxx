module;

#include <algorithm>
#include <bit>
#include <cstdint>
#include <print>
#include <ranges>
#include <vector>

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
        drain_pool_in_destructor<vk_object_tags::bin_semaphore>();
        drain_pool_in_destructor<vk_object_tags::timeline_semaphore>();
        drain_pool_in_destructor<vk_object_tags::fence>();
        drain_pool_in_destructor<vk_object_tags::shader>();
        drain_pool_in_destructor<vk_object_tags::shader_module>();
    }

    vk_image_handle vulkan_object_registry::create_image(VkImageCreateInfo const& info, char const* debug_name)
    {
        VkImage handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateImage(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create image ({})", result);
            return {};
        }

        device_.set_debug_object_name(VK_OBJECT_TYPE_IMAGE, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_object_tags::image>().emplace(
            handle,
            info.format,
            info.extent,
            info.mipLevels,
            info.arrayLayers
        );
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

        return slot_map<vk_object_tags::allocation>().emplace(allocation);
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

        device_.set_debug_object_name(VK_OBJECT_TYPE_BUFFER, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_object_tags::buffer>().emplace(handle, info.size, info.usage);
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

        return slot_map<vk_object_tags::allocation>().emplace(allocation);
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

        device_.set_debug_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_object_tags::image_view>().emplace(handle, info.format);
    }

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

        return slot_map<vk_object_tags::fence>().emplace(handle);
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

        return slot_map<vk_object_tags::bin_semaphore>().emplace(handle);
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

        return slot_map<vk_object_tags::timeline_semaphore>().emplace(handle);

    }

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

        return slot_map<vk_object_tags::command_pool>().emplace(handle, info.queueFamilyIndex);
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

        auto&& vulkan_slot_map = slot_map<vk_object_tags::command_buffer>();

        std::ranges::transform(
            raw_handles,
            std::back_inserter(handles),
            [command_pool_handle, is_primary, &vulkan_slot_map](VkCommandBuffer raw_handle)
            {
                return vulkan_slot_map.emplace(raw_handle, command_pool_handle, is_primary);
            });

        return handles;
    }

    std::vector<vk_shader_handle> vulkan_object_registry::create_shaders(
        std::span<shader_create_info const> const infos,
        std::span<std::byte const> const shader_code)
    {
        VKGC_CHECKF(
            !shader_code.empty() && shader_code.size() % sizeof(std::uint32_t) == 0, "invalid byte code buffer size");

        VKGC_CHECK((reinterpret_cast<std::uintptr_t>(shader_code.data()) & (alignof(std::uint32_t) - 1)) == 0);

        std::vector<VkShaderCreateInfoEXT> shader_create_infos;
        shader_create_infos.reserve(infos.size());

        std::ranges::transform(
            infos,
            std::back_inserter(shader_create_infos),
            [shader_code](shader_create_info const& info)
        {
            return VkShaderCreateInfoEXT{
                .sType{VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT},
                .pNext{nullptr},
                .flags{0},
                .stage{info.stage},
                .nextStage{info.next_stage},
                .codeType{VK_SHADER_CODE_TYPE_SPIRV_EXT}, // :TODO: cache VK_SHADER_CODE_TYPE_BINARY_EXT
                .codeSize{static_cast<std::uint32_t>(shader_code.size())},
                .pCode{reinterpret_cast<std::uint32_t const*>(shader_code.data())},
                .pName{info.entry_point},
                .setLayoutCount{0},
                .pSetLayouts{nullptr},
                .pushConstantRangeCount{0},
                .pPushConstantRanges{nullptr},
                .pSpecializationInfo{nullptr}
            };
        });

        std::vector<VkShaderEXT> shader_handles(shader_create_infos.size());

        if (auto const result = vkCreateShadersEXT(
                device_.handle(),
                static_cast<std::uint32_t>(shader_create_infos.size()),
                shader_create_infos.data(),
                nullptr,
                shader_handles.data());
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create shader(s) ({})", result);
            return {};
        }

        std::vector<vk_shader_handle> shaders;
        shaders.reserve(shader_create_infos.size());

        for (auto [handle, info] : std::views::zip(shader_handles, infos))
        {
            shaders.push_back(slot_map<vk_object_tags::shader>().emplace(handle, info.stage));
            device_.set_debug_object_name(
                VK_OBJECT_TYPE_SHADER_EXT,
                std::bit_cast<std::uint64_t>(handle),
                info.debug_name);
        }

        return shaders;
    }

    std::vector<vk_shader_handle> vulkan_object_registry::create_shader_modules(
        std::span<shader_module_create_info const> infos,
        std::span<std::byte const> shader_code)
    {
        VKGC_CHECKF(
            !shader_code.empty() && shader_code.size() % sizeof(std::uint32_t) == 0, "invalid byte code buffer size");

        VKGC_CHECK((reinterpret_cast<std::uintptr_t>(shader_code.data()) & (alignof(std::uint32_t) - 1)) == 0);

        std::vector<VkShaderModuleCreateInfo> shader_module_create_infos;
        shader_module_create_infos.reserve(infos.size());

        std::ranges::transform(
            infos,
            std::back_inserter(shader_module_create_infos),
            [shader_code](shader_module_create_info const& info)
        {
            return VkShaderCreateInfoEXT{
                .sType{VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT},
                .pNext{nullptr},
                .flags{0},
                .stage{info.stage},
                .nextStage{info.next_stage},
                .codeType{VK_SHADER_CODE_TYPE_SPIRV_EXT}, // :TODO: cache VK_SHADER_CODE_TYPE_BINARY_EXT
                .codeSize{static_cast<std::uint32_t>(shader_code.size())},
                .pCode{reinterpret_cast<std::uint32_t const*>(shader_code.data())},
                .pName{info.entry_point},
                .setLayoutCount{0},
                .pSetLayouts{nullptr},
                .pushConstantRangeCount{0},
                .pPushConstantRanges{nullptr},
                .pSpecializationInfo{nullptr}
            };
        });

        std::vector<VkShaderModule> shader_module_handles(shader_module_create_infos.size());

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
