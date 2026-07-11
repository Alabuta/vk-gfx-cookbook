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

        drain_pool_in_destructor<vk_object_tags::command_buffer>();
        drain_pool_in_destructor<vk_object_tags::command_pool>();
        drain_pool_in_destructor<vk_object_tags::pipeline>();
        drain_pool_in_destructor<vk_object_tags::pipeline_layout>();
        drain_pool_in_destructor<vk_object_tags::descriptor_set>();
        drain_pool_in_destructor<vk_object_tags::descriptor_pool>();
        drain_pool_in_destructor<vk_object_tags::descriptor_set_layout>();
        drain_pool_in_destructor<vk_object_tags::pipeline_cache>();
        drain_pool_in_destructor<vk_object_tags::image_view>();
        drain_pool_in_destructor<vk_object_tags::sampler>();
        drain_pool_in_destructor<vk_object_tags::image>();
        drain_pool_in_destructor<vk_object_tags::buffer>();
        drain_pool_in_destructor<vk_object_tags::allocation>();
        drain_pool_in_destructor<vk_object_tags::bin_semaphore>();
        drain_pool_in_destructor<vk_object_tags::timeline_semaphore>();
        drain_pool_in_destructor<vk_object_tags::fence>();
        drain_pool_in_destructor<vk_object_tags::shader>();
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
            info.arrayLayers,
            vk_allocation_handle{}
        );
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

        device_.set_debug_object_name(VK_OBJECT_TYPE_IMAGE, std::bit_cast<std::uint64_t>(image), debug_name);

        auto allocation_handle = slot_map<vk_object_tags::allocation>().emplace(allocation);
        return slot_map<vk_object_tags::image>().emplace(
            image,
            image_info.format,
            image_info.extent,
            image_info.mipLevels,
            image_info.arrayLayers,
            allocation_handle);
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

        return slot_map<vk_object_tags::buffer>().emplace(handle, info.size, info.usage, vk_allocation_handle{});
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

        device_.set_debug_object_name(VK_OBJECT_TYPE_BUFFER, std::bit_cast<std::uint64_t>(buffer), debug_name);

        auto allocation_handle = slot_map<vk_object_tags::allocation>().emplace(allocation);
        return slot_map<vk_object_tags::buffer>().emplace(
            buffer,
            buffer_info.size,
            buffer_info.usage,
            allocation_handle);
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

    vk_sampler_handle vulkan_object_registry::create_sampler(VkSamplerCreateInfo const& info, char const* debug_name)
    {
        VkSampler handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateSampler(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create sampler ({})", result);
            return {};
        }

        device_.set_debug_object_name(VK_OBJECT_TYPE_SAMPLER, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_object_tags::sampler>().emplace(handle);
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

    vk_descriptor_set_layout_handle vulkan_object_registry::create_descriptor_set_layout(
        VkDescriptorSetLayoutCreateInfo const& info,
        char const* debug_name)
    {
        VkDescriptorSetLayout handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateDescriptorSetLayout(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create descriptor set layout ({})", result);
            return {};
        }

        device_.set_debug_object_name(
            VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
            std::bit_cast<std::uint64_t>(handle),
            debug_name);

        return slot_map<vk_object_tags::descriptor_set_layout>().emplace(handle);
    }

    vk_descriptor_pool_handle vulkan_object_registry::create_descriptor_pool(
        VkDescriptorPoolCreateInfo const& info,
        char const* debug_name)
    {
        VkDescriptorPool handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateDescriptorPool(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create descriptor pool ({})", result);
            return {};
        }

        device_.set_debug_object_name(
            VK_OBJECT_TYPE_DESCRIPTOR_POOL,
            std::bit_cast<std::uint64_t>(handle),
            debug_name);

        return slot_map<vk_object_tags::descriptor_pool>().emplace(handle, info.flags);
    }

    std::vector<vk_descriptor_set_handle> vulkan_object_registry::allocate_descriptor_sets(
        vk_descriptor_pool_handle const pool,
        std::span<vk_descriptor_set_layout_handle const> const layouts,
        std::span<std::uint32_t const> const variable_descriptor_counts,
        char const* debug_name)
    {
        VKGC_CHECKF(
            variable_descriptor_counts.empty() || variable_descriptor_counts.size() == layouts.size(),
            "variable descriptor counts must be empty or match the layout count");

        if (!VKGC_ENSUREF(!layouts.empty(), "no descriptor set layouts provided"))
        {
            return {};
        }

        descriptor_pool_payload const* pool_payload = slot_map<vk_object_tags::descriptor_pool>().try_get(pool);
        if (!VKGC_ENSUREF(pool_payload != nullptr, "stale descriptor pool handle"))
        {
            return {};
        }

        std::vector<VkDescriptorSetLayout> raw_layouts;
        raw_layouts.reserve(layouts.size());

        std::ranges::transform(
            layouts,
            std::back_inserter(raw_layouts),
            [this](vk_descriptor_set_layout_handle const layout) { return resolve_handle(layout); });

        if (!VKGC_ENSUREF(
                !std::ranges::contains(raw_layouts, VkDescriptorSetLayout{VK_NULL_HANDLE}),
                "stale descriptor set layout handle"))
        {
            return {};
        }

        VkDescriptorSetVariableDescriptorCountAllocateInfo const variable_count_info{
            .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO},
            .pNext{nullptr},
            .descriptorSetCount{static_cast<std::uint32_t>(variable_descriptor_counts.size())},
            .pDescriptorCounts{variable_descriptor_counts.data()}
        };

        VkDescriptorSetAllocateInfo const allocate_info{
            .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO},
            .pNext{variable_descriptor_counts.empty() ? nullptr : &variable_count_info},
            .descriptorPool{pool_payload->handle},
            .descriptorSetCount{static_cast<std::uint32_t>(raw_layouts.size())},
            .pSetLayouts{raw_layouts.data()}
        };

        std::vector<VkDescriptorSet> raw_handles(raw_layouts.size(), VK_NULL_HANDLE);
        if (auto const result = vkAllocateDescriptorSets(device_.handle(), &allocate_info, raw_handles.data());
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to allocate descriptor set(s) ({})", result);
            return {};
        }

        VkDescriptorPool const pool_handle = pool_payload->handle;
        bool const can_free = (pool_payload->flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) != 0;

        std::vector<vk_descriptor_set_handle> handles;
        handles.reserve(raw_handles.size());

        for (VkDescriptorSet raw_handle : raw_handles)
        {
            device_.set_debug_object_name(
                VK_OBJECT_TYPE_DESCRIPTOR_SET,
                std::bit_cast<std::uint64_t>(raw_handle),
                debug_name);

            handles.push_back(slot_map<vk_object_tags::descriptor_set>().emplace(raw_handle, pool_handle, can_free));
        }

        return handles;
    }

    vk_pipeline_layout_handle vulkan_object_registry::create_pipeline_layout(
        VkPipelineLayoutCreateInfo const& info,
        char const* debug_name)
    {
        VkPipelineLayout handle{VK_NULL_HANDLE};
        if (auto const result = vkCreatePipelineLayout(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create pipeline layout ({})", result);
            return {};
        }

        device_.set_debug_object_name(VK_OBJECT_TYPE_PIPELINE_LAYOUT, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_object_tags::pipeline_layout>().emplace(handle);
    }

    vk_pipeline_cache_handle vulkan_object_registry::create_pipeline_cache_from_data(
        std::span<std::byte const> const cache_data,
        char const* debug_name)
    {
        if (!VKGC_ENSUREF(!cache_data.empty(), "pipeline cache is empty"))
        {
            return {};
        }

        VkPipelineCacheCreateInfo const info{
            .sType{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .initialDataSize{static_cast<std::uint32_t>(cache_data.size())},
            .pInitialData{reinterpret_cast<std::uint32_t const*>(cache_data.data())},
        };

        VkPipelineCache handle{VK_NULL_HANDLE};
        if (auto const result = vkCreatePipelineCache(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create pipeline cache ({})", result);
            return {};
        }

        device_.set_debug_object_name(VK_OBJECT_TYPE_PIPELINE_CACHE, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_object_tags::pipeline_cache>().emplace(handle);
    }

    vk_pipeline_cache_handle vulkan_object_registry::create_pipeline_cache_empty(char const* debug_name)
    {
        VkPipelineCacheCreateInfo const info{
            .sType{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .initialDataSize{0},
            .pInitialData{nullptr},
        };

        VkPipelineCache handle{VK_NULL_HANDLE};
        if (auto const result = vkCreatePipelineCache(device_.handle(), &info, nullptr, &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create pipeline cache ({})", result);
            return {};
        }

        device_.set_debug_object_name(VK_OBJECT_TYPE_PIPELINE_CACHE, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_object_tags::pipeline_cache>().emplace(handle);
    }

    vk_pipeline_handle vulkan_object_registry::create_graphics_pipeline(
        VkGraphicsPipelineCreateInfo const& info,
        vk_pipeline_cache_handle const cache,
        char const* debug_name)
    {
        VkPipeline handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateGraphicsPipelines(
                device_.handle(),
                resolve_handle(cache),
                1,
                &info,
                nullptr,
                &handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to create graphics pipeline ({})", result);
            return {};
        }

        device_.set_debug_object_name(VK_OBJECT_TYPE_PIPELINE, std::bit_cast<std::uint64_t>(handle), debug_name);

        return slot_map<vk_object_tags::pipeline>().emplace(handle);
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

    vk_allocation_handle vulkan_object_registry::bound_allocation(vk_buffer_handle handle) const noexcept
    {
        buffer_payload const* p = slot_map<vk_object_tags::buffer>().try_get(handle);
        return p != nullptr ? p->allocation : vk_allocation_handle{};
    }

    std::vector<std::byte> vulkan_object_registry::pipeline_cache_data(vk_pipeline_cache_handle const handle) const
    {
        auto const pipeline_cache = resolve_handle(handle);
        if (!VKGC_ENSUREF_VKHANDLE(pipeline_cache, "invalid pipeline cache"))
        {
            return {};
        }

        std::size_t size{0};
        if (auto const result = vkGetPipelineCacheData(device_.handle(), pipeline_cache, &size, nullptr);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to query pipeline cache data size ({})", result);
            return {};
        }

        std::vector<std::byte> data(size);
        if (auto const result = vkGetPipelineCacheData(device_.handle(), pipeline_cache, &size, data.data());
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to retrieve pipeline cache data ({})", result);
            return {};
        }

        // The second query may report fewer bytes than the first; trim to the actual size.
        data.resize(size);
        return data;
    }

}
