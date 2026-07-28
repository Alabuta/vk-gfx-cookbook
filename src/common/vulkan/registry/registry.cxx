module;

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

        drain_pool_in_destructor<vk_command_buffer_handle>();
        drain_pool_in_destructor<vk_command_pool_handle>();
        drain_pool_in_destructor<vk_pipeline_handle>();
        drain_pool_in_destructor<vk_pipeline_layout_handle>();
        drain_pool_in_destructor<vk_descriptor_set_handle>();
        drain_pool_in_destructor<vk_descriptor_pool_handle>();
        drain_pool_in_destructor<vk_descriptor_set_layout_handle>();
        drain_pool_in_destructor<vk_pipeline_cache_handle>();
        drain_pool_in_destructor<vk_image_view_handle>();
        drain_pool_in_destructor<vk_sampler_handle>();
        drain_pool_in_destructor<vk_image_handle>();
        drain_pool_in_destructor<vk_buffer_handle>();
        drain_pool_in_destructor<vk_allocation_handle>();
        drain_pool_in_destructor<vk_bin_semaphore_handle>();
        drain_pool_in_destructor<vk_timeline_semaphore_handle>();
        drain_pool_in_destructor<vk_fence_handle>();
        drain_pool_in_destructor<vk_shader_handle>();
    }

    gpu_image vulkan_object_registry::resolve_image(vk_image_handle const handle) const noexcept
    {
        vk_object_payload<vk_image_handle> const* p = slot_map<vk_image_handle>().try_get(handle);
        if (p == nullptr)
        {
            return {};
        }

        return gpu_image{
            .image{p->handle},
            .view{resolve_handle(p->default_view)},
            .allocation{p->allocation},
            .desc{p->desc},
            .steady_state_layout{p->steady_state_layout},
            .bindless_slot{p->bindless_slot}
        };
    }

    VkDeviceSize vulkan_object_registry::buffer_size(vk_buffer_handle handle) const noexcept
    {
        vk_object_payload<vk_buffer_handle> const* p = slot_map<vk_buffer_handle>().try_get(handle);
        return p != nullptr ? p->size : VkDeviceSize{0};
    }

    vk_allocation_handle vulkan_object_registry::bound_allocation(vk_buffer_handle handle) const noexcept
    {
        vk_object_payload<vk_buffer_handle> const* p = slot_map<vk_buffer_handle>().try_get(handle);
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
