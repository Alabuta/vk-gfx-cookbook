module;

#include <cstring>
#include <print>
#include <functional>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vk_mem_alloc.h"
#include "vulkan/assert.hxx"

module vkgc.vulkan_resource_helpers;

import vkgc.vulkan_handle;
import vkgc.vulkan_device;
import vkgc.vulkan_context;
import vkgc.vulkan_object_registry;
import vkgc.scope_guard;

namespace vkgc
{
    bool bind_image_memory(
        vulkan_object_registry const& vk_object_registry,
        vk_image_handle const image,
        vk_allocation_handle const memory)
    {
        VkImage image_handle = vk_object_registry.resolve_handle(image);
        if (!VKGC_ENSURE_VKHANDLE(image_handle))
        {
            return false;
        }

        VmaAllocation vma_allocation_handle = vk_object_registry.resolve_handle(memory);
        if (!VKGC_ENSURE_VKHANDLE(vma_allocation_handle))
        {
            return false;
        }

        if (auto const result = vmaBindImageMemory(
                vk_object_registry.device().vma_allocator(),
                vma_allocation_handle,
                image_handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to bind image memory ({})", result);
            return false;
        }

        return true;
    }

    bool bind_buffer_memory(
        vulkan_object_registry const& vk_object_registry,
        vk_buffer_handle const buffer,
        vk_allocation_handle const memory)
    {
        VkBuffer buffer_handle = vk_object_registry.resolve_handle(buffer);
        if (!VKGC_ENSURE_VKHANDLE(buffer_handle))
        {
            return false;
        }

        VmaAllocation vma_allocation_handle = vk_object_registry.resolve_handle(memory);
        if (!VKGC_ENSURE_VKHANDLE(vma_allocation_handle))
        {
            return false;
        }

        if (auto const result = vmaBindBufferMemory(
                vk_object_registry.device().vma_allocator(),
                vma_allocation_handle,
                buffer_handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Error : failed to bind buffer memory ({})", result);
            return false;
        }

        return true;
    }

    bool submit_once(
        vulkan_context const& vk_context,
        vulkan_object_registry& vk_object_registry,
        vk_command_pool_handle const command_pool,
        std::function<void(VkCommandBuffer)> const& record_commands)
    {
        auto const one_time_command_buffers = vk_object_registry.allocate_command_buffers(command_pool, 1, true);
        if (!VKGC_ENSURE(!one_time_command_buffers.empty()))
        {
            return false;
        }

        auto const command_buffer = one_time_command_buffers.front();

        scope_guard const free_command_buffer{
            [&] { vk_object_registry.destroy_immediate(command_buffer); }
        };

        auto const command_buffer_handle = vk_object_registry.resolve_handle(command_buffer);
        if (!VKGC_ENSURE_VKHANDLE(command_buffer_handle))
        {
            return false;
        }

        VkCommandBufferBeginInfo constexpr begin_info{
            .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO},
            .pNext{nullptr},
            .flags{VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT},
            .pInheritanceInfo{nullptr}
        };

        if (!VKGC_ENSURE_VKSUCCESS(vkBeginCommandBuffer(command_buffer_handle, &begin_info)))
        {
            return false;
        }

        record_commands(command_buffer_handle);

        if (!VKGC_ENSURE_VKSUCCESS(vkEndCommandBuffer(command_buffer_handle)))
        {
            return false;
        }

        auto const submit_fence = vk_object_registry.create_fence(false, "one-shot submit fence");
        if (!VKGC_ENSURE(submit_fence.is_valid()))
        {
            return false;
        }

        scope_guard const free_fence{[&] { vk_object_registry.destroy_immediate(submit_fence); }};

        auto const submit_fence_handle = vk_object_registry.resolve_handle(submit_fence);
        if (!VKGC_ENSURE_VKHANDLE(submit_fence_handle))
        {
            return false;
        }

        VkCommandBufferSubmitInfo const command_buffer_submit_info{
            .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO},
            .pNext{nullptr},
            .commandBuffer{command_buffer_handle},
            .deviceMask{0}
        };

        VkSubmitInfo2 const submit_info{
            .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO_2},
            .pNext{nullptr},
            .flags{0},
            .waitSemaphoreInfoCount{0},
            .pWaitSemaphoreInfos{nullptr},
            .commandBufferInfoCount{1},
            .pCommandBufferInfos{&command_buffer_submit_info},
            .signalSemaphoreInfoCount{0},
            .pSignalSemaphoreInfos{nullptr}
        };

        if (!VKGC_ENSURE_VKSUCCESS(
                vkQueueSubmit2(vk_context.device().main_queue(), 1, &submit_info, submit_fence_handle)))
        {
            return false;
        }

        if (!VKGC_ENSURE_VKSUCCESS(
                vkWaitForFences(
                    vk_context.device().handle(),
                    1,
                    &submit_fence_handle,
                    VK_TRUE,
                    std::numeric_limits<std::uint64_t>::max())))
        {
            return false;
        }

        return true;
    }

    // Writes `bytes` into `allocation`'s mapping (mapping it temporarily if needed) and flushes
    // when the memory isn't host-coherent. Caller must have already confirmed host-visibility.
    bool upload_host_mapped_buffer(
        vulkan_context const& vk_context,
        vulkan_object_registry const& vk_object_registry,
        vk_buffer_handle const buffer,
        std::span<std::byte const> const bytes,
        bool const flush_memory_after_upload)
    {
        if (!VKGC_ENSURE(buffer.is_valid()))
        {
            return false;
        }

        auto const allocation = vk_object_registry.bound_allocation(buffer);
        if (!VKGC_ENSURE(allocation.is_valid()))
        {
            return false;
        }

        if (!VKGC_ENSUREF(
                is_host_visible_allocation(vk_context, vk_object_registry, allocation),
                "non-host-visible memory type cannot be accessed from host"))
        {
            return false;
        }

        auto const allocation_info = get_vma_allocation_info(vk_context, vk_object_registry, allocation);
        auto const is_mapped_allocation = allocation_info.allocationInfo.pMappedData != nullptr;

        void* mapped_ptr;
        if (is_mapped_allocation)
        {
            mapped_ptr = allocation_info.allocationInfo.pMappedData;
        }
        else if (!VKGC_ENSURE_VKSUCCESS(vkMapMemory(
            vk_context.device().handle(),
            allocation_info.allocationInfo.deviceMemory,
            allocation_info.allocationInfo.offset,
            bytes.size(),
            0,
            &mapped_ptr
        )))
        {
            return false;
        }

        std::memcpy(mapped_ptr, bytes.data(), bytes.size());

        if (flush_memory_after_upload && !is_host_coherent_allocation(vk_context, vk_object_registry, allocation))
        {
            VkMappedMemoryRange const mapped_range{
                VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                nullptr,
                allocation_info.allocationInfo.deviceMemory,
                allocation_info.allocationInfo.offset,
                bytes.size()
            };

            vkFlushMappedMemoryRanges(vk_context.device().handle(), 1, &mapped_range);
        }

        if (!is_mapped_allocation)
        {
            vkUnmapMemory(vk_context.device().handle(), allocation_info.allocationInfo.deviceMemory);
        }

        return true;
    }

    bool upload_buffer(
        vulkan_context const& vk_context,
        vulkan_object_registry& vk_object_registry,
        vk_command_pool_handle const command_pool,
        vk_buffer_handle const buffer,
        std::span<std::byte const> const bytes)
    {
        auto const allocation = vk_object_registry.bound_allocation(buffer);
        if (!VKGC_ENSURE(allocation.is_valid()))
        {
            return false;
        }

        if (is_host_visible_allocation(vk_context, vk_object_registry, allocation))
        {
            if (upload_host_mapped_buffer(vk_context, vk_object_registry, buffer, bytes, true))
            {
                return true;
            }
        }

        // Non-mappable destination memory: route the data through a staging buffer and a
        // one-shot transfer submission.
        auto const staging_buffer = create_staging_buffer(vk_object_registry, bytes.size());
        if (!staging_buffer.is_valid())
        {
            return false;
        }

        scope_guard const free_staging_buffer{[&] { vk_object_registry.destroy_immediate(staging_buffer); }};

        if (!upload_host_mapped_buffer(vk_context, vk_object_registry, staging_buffer, bytes, true))
        {
            return false;
        }

        auto const src_buffer_handle = vk_object_registry.resolve_handle(staging_buffer);
        if (!VKGC_ENSURE_VKHANDLE(src_buffer_handle))
        {
            return false;
        }

        auto const dst_buffer_handle = vk_object_registry.resolve_handle(buffer);
        if (!VKGC_ENSURE_VKHANDLE(dst_buffer_handle))
        {
            return false;
        }

        auto const record_buffer_copy = [&](VkCommandBuffer command_buffer)
        {
            VkBufferCopy2 const copy_region{
                .sType{VK_STRUCTURE_TYPE_BUFFER_COPY_2},
                .pNext{nullptr},
                .srcOffset{0},
                .dstOffset{0},
                .size{static_cast<VkDeviceSize>(bytes.size())}
            };

            VkCopyBufferInfo2 const copy_buffer_info{
                .sType{VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2},
                .pNext{nullptr},
                .srcBuffer{src_buffer_handle},
                .dstBuffer{dst_buffer_handle},
                .regionCount{1},
                .pRegions{&copy_region}
            };

            vkCmdCopyBuffer2(command_buffer, &copy_buffer_info);

            // The fence wait only synchronizes with the host; vertex/index fetch in later
            // submissions needs its own memory dependency on the copy.
            VkBufferMemoryBarrier2 const to_vertex_input{
                .sType{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2},
                .pNext{nullptr},
                .srcStageMask{VK_PIPELINE_STAGE_2_COPY_BIT},
                .srcAccessMask{VK_ACCESS_2_TRANSFER_WRITE_BIT},
                .dstStageMask{VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT},
                .dstAccessMask{VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT},
                .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .buffer{dst_buffer_handle},
                .offset{0},
                .size{VK_WHOLE_SIZE}
            };

            VkDependencyInfo const dependency_info{
                .sType{VK_STRUCTURE_TYPE_DEPENDENCY_INFO},
                .pNext{nullptr},
                .dependencyFlags{},
                .memoryBarrierCount{0},
                .pMemoryBarriers{nullptr},
                .bufferMemoryBarrierCount{1},
                .pBufferMemoryBarriers{&to_vertex_input},
                .imageMemoryBarrierCount{0},
                .pImageMemoryBarriers{nullptr},
            };

            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        };

        return submit_once(vk_context, vk_object_registry, command_pool, record_buffer_copy);
    }

    VkMemoryPropertyFlags get_allocation_memory_properties(
        vulkan_context const& vk_context,
        vulkan_object_registry const& vk_object_registry,
        vk_allocation_handle const allocation)
    {
        VkMemoryPropertyFlags memory_property_flags;
        vmaGetAllocationMemoryProperties(
            vk_context.device().vma_allocator(),
            vk_object_registry.resolve_handle(allocation),
            &memory_property_flags);

        return memory_property_flags;
    }

    VmaAllocationInfo2 get_vma_allocation_info(
        vulkan_context const& vk_context,
        vulkan_object_registry const& vk_object_registry,
        vk_allocation_handle const allocation)
    {
        VmaAllocationInfo2 allocation_info;
        vmaGetAllocationInfo2(
            vk_context.device().vma_allocator(),
            vk_object_registry.resolve_handle(allocation),
            &allocation_info);

        return allocation_info;
    }

    bool is_host_visible_allocation(
        vulkan_context const& vk_context,
        vulkan_object_registry const& vk_object_registry,
        vk_allocation_handle const allocation)
    {
        auto const memory_property_flags = get_allocation_memory_properties(vk_context, vk_object_registry, allocation);
        return (memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    }

    bool is_host_coherent_allocation(
        vulkan_context const& vk_context,
        vulkan_object_registry const& vk_object_registry,
        vk_allocation_handle const allocation)
    {
        auto const memory_property_flags = get_allocation_memory_properties(vk_context, vk_object_registry, allocation);
        return (memory_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
    }
}
