module;

#include <algorithm>
#include <bit>
#include <cstdint>
#include <print>
#include <ranges>
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
            shaders.push_back(slot_map<vk_shader_handle>().emplace(handle, info.stage));
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

        return finalize_created_object<vk_descriptor_set_layout_handle>(
            VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, handle, debug_name);
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

        return finalize_created_object<vk_descriptor_pool_handle>(
            VK_OBJECT_TYPE_DESCRIPTOR_POOL, handle, debug_name, info.flags);
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

        vk_object_payload<vk_descriptor_pool_handle> const* pool_payload = slot_map<vk_descriptor_pool_handle>().try_get(pool);
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

            handles.push_back(slot_map<vk_descriptor_set_handle>().emplace(raw_handle, pool_handle, can_free));
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

        return finalize_created_object<vk_pipeline_layout_handle>(VK_OBJECT_TYPE_PIPELINE_LAYOUT, handle, debug_name);
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

        return finalize_created_object<vk_pipeline_cache_handle>(VK_OBJECT_TYPE_PIPELINE_CACHE, handle, debug_name);
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

        return finalize_created_object<vk_pipeline_cache_handle>(VK_OBJECT_TYPE_PIPELINE_CACHE, handle, debug_name);
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

        return finalize_created_object<vk_pipeline_handle>(VK_OBJECT_TYPE_PIPELINE, handle, debug_name);
    }
}
