module;

#include <cstdint>

#include <volk.h>
#include "vk_mem_alloc.h"

module vkgc.vulkan_object_registry;

import vkgc.vulkan_device;
import vkgc.vulkan_payload;

namespace vkgc
{
    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_image_handle> const& payload) noexcept
    {
        vkDestroyImage(device_.handle(), payload.handle, nullptr);
        if (payload.allocation.is_valid())
        {
            destroy_immediate(payload.allocation);
        }
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_image_view_handle> const& payload) noexcept
    {
        vkDestroyImageView(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_sampler_handle> const& payload) noexcept
    {
        vkDestroySampler(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_buffer_handle> const& payload) noexcept
    {
        vkDestroyBuffer(device_.handle(), payload.handle, nullptr);
        if (payload.allocation.is_valid())
        {
            destroy_immediate(payload.allocation);
        }
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_allocation_handle> const& payload) noexcept
    {
        vmaFreeMemory(device_.vma_allocator(), payload.handle);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_fence_handle> const& payload) noexcept
    {
        vkDestroyFence(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_bin_semaphore_handle> const& payload) noexcept
    {
        vkDestroySemaphore(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_timeline_semaphore_handle> const& payload) noexcept
    {
        vkDestroySemaphore(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_command_pool_handle> const& payload) noexcept
    {
        vkDestroyCommandPool(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_command_buffer_handle> const& payload) noexcept
    {
        vkFreeCommandBuffers(device_.handle(), payload.source_pool, 1, &payload.handle);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_shader_handle> const& payload) noexcept
    {
        vkDestroyShaderEXT(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_descriptor_set_layout_handle> const& payload) noexcept
    {
        vkDestroyDescriptorSetLayout(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_descriptor_pool_handle> const& payload) noexcept
    {
        vkDestroyDescriptorPool(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_descriptor_set_handle> const& payload) noexcept
    {
        if (payload.can_free)
        {
            vkFreeDescriptorSets(device_.handle(), payload.source_pool, 1, &payload.handle);
        }
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_pipeline_layout_handle> const& payload) noexcept
    {
        vkDestroyPipelineLayout(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_pipeline_handle> const& payload) noexcept
    {
        vkDestroyPipeline(device_.handle(), payload.handle, nullptr);
    }

    void vulkan_object_registry::destroy_payload(vk_object_payload<vk_pipeline_cache_handle> const& payload) noexcept
    {
        vkDestroyPipelineCache(device_.handle(), payload.handle, nullptr);
    }
}
