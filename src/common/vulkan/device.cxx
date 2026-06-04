module;

#include <cstdint>
#include <limits>
#include <print>

#include <volk.h>
#include "vulkan/format.hxx"
#include "vk_mem_alloc.h"

module vkgc.vulkan_device;


namespace vkgc
{
    vulkan_device::vulkan_device(
        VkPhysicalDevice physical_device,
        VkDevice handle,
        vulkan_queue_families queue_families,
        VmaAllocator vma_allocator) noexcept
        : physical_device_{physical_device},
          handle_{handle},
          queue_families_{queue_families},
          vma_allocator_{vma_allocator}
    {
        VkPhysicalDeviceDriverProperties driver_properties{
            .sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES},
            .pNext{nullptr},
            .driverID{},
            .driverName{},
            .driverInfo{},
            .conformanceVersion{}
        };

        device_properties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        device_properties_.pNext = &driver_properties;
        vkGetPhysicalDeviceProperties2(physical_device_, &device_properties_);

        memory_properties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        vkGetPhysicalDeviceMemoryProperties2(physical_device_, &memory_properties_);

        auto const& device_memory_properties = memory_properties_.memoryProperties;

        {
            // ReBAR: device-local + host-visible heap larger than the standard 256 MiB BAR window.
            static VkDeviceSize constexpr kRebarMinHeap{256ULL * 1024ULL * 1024ULL};

            auto const& [memory_types_count, memory_types, _, memory_heaps] = device_memory_properties;

            for (std::uint32_t i = 0; i < memory_types_count; ++i)
            {
                auto const [property_flags, heap_index] = memory_types[i];

                auto constexpr kRequiredMemProp =
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

                if ((property_flags & kRequiredMemProp) != kRequiredMemProp)
                {
                    continue;
                }

                auto const [heap_size, heap_flags] = memory_heaps[heap_index];
                if ((heap_flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0 || heap_size <= kRebarMinHeap)
                {
                    continue;
                }

                // Prefer HOST_COHERENT among candidates (avoids explicit flushes).
                bool const is_coherent = (property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
                bool const have_candidate  = rebar_memory_type_index_ != std::numeric_limits<std::uint32_t>::max();
                if (!have_candidate || is_coherent)
                {
                    rebar_memory_type_index_ = i;
                }
            }
        }

        auto constexpr invalid_queue_family_index = std::numeric_limits<std::uint32_t>::max();

        vkGetDeviceQueue(handle_, queue_families_.main, 0, &main_queue_);

        if (queue_families_.dedicated_compute != invalid_queue_family_index)
        {
            vkGetDeviceQueue(handle_, queue_families_.dedicated_compute, 0, &dedicated_compute_queue_);
        }

        if (queue_families_.dedicated_transfer != invalid_queue_family_index)
        {
            vkGetDeviceQueue(handle_, queue_families_.dedicated_transfer, 0, &dedicated_transfer_queue_);
        }

        std::println("[Vulkan] : Log : physical device name is {}", device_properties_.properties.deviceName);
        std::println(
            "[Vulkan] : Log : driver name and info {} {}", driver_properties.driverName, driver_properties.driverInfo);

        std::println("[Vulkan] : Log : main queue family index is {}", queue_families_.main);

        if (queue_families_.dedicated_compute != invalid_queue_family_index)
        {
            std::println(
                "[Vulkan] : Log : dedicated async compute queue family index is {}",
                queue_families_.dedicated_compute);
        }
        else
        {
            std::println("[Vulkan] : Log : no dedicated async compute queue; main queue must be used for compute");
        }

        if (queue_families_.dedicated_transfer != invalid_queue_family_index)
        {
            std::println(
                "[Vulkan] : Log : dedicated DMA transfer queue family index is {}",
                queue_families_.dedicated_transfer);
        }
        else
        {
            std::println("[Vulkan] : Log : no dedicated DMA transfer queue; main queue must be used for transfer");
        }

        if (rebar_memory_type_index_ != std::numeric_limits<std::uint32_t>::max())
        {
            auto const heap_index = device_memory_properties.memoryTypes[rebar_memory_type_index_].heapIndex;
            auto const property_flags = device_memory_properties.memoryTypes[rebar_memory_type_index_].propertyFlags;
            auto const heap_mib = device_memory_properties.memoryHeaps[heap_index].size / (1024ULL * 1024ULL);

            bool const is_coherent = (property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

            std::println(
                "[Vulkan] : Log : ReBAR available - memory type index {} (heap {} MiB{})",
                rebar_memory_type_index_,
                heap_mib,
                is_coherent ? ", coherent" : "");
        }
        else
        {
            std::println("[Vulkan] : Log : ReBAR not available; no large device-local host-visible heap detected");
        }
    }

    vulkan_device::~vulkan_device()
    {
        if (handle_ == VK_NULL_HANDLE)
        {
            return;
        }

        if (auto const result = vkDeviceWaitIdle(handle_); result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Warning : an error encountered on 'vkDeviceWaitIdle' call ({})", result);
        }

        if (vma_allocator_ != VK_NULL_HANDLE)
        {
            vmaDestroyAllocator(vma_allocator_);
        }

        vkDestroyDevice(handle_, nullptr);
    }

    bool vulkan_device::is_valid() const noexcept
    {
        return physical_device_ != VK_NULL_HANDLE && handle_ != VK_NULL_HANDLE;
    }

    VkPhysicalDevice vulkan_device::physical_device() const noexcept
    {
        return physical_device_;
    }

    VkDevice vulkan_device::handle() const noexcept
    {
        return handle_;
    }

    VkPhysicalDeviceProperties const& vulkan_device::properties() const noexcept
    {
        return device_properties_.properties;
    }

    VkPhysicalDeviceMemoryProperties const& vulkan_device::memory_properties() const noexcept
    {
        return memory_properties_.memoryProperties;
    }

    bool vulkan_device::has_rebar() const noexcept
    {
        return rebar_memory_type_index_ != std::numeric_limits<std::uint32_t>::max();
    }

    std::uint32_t vulkan_device::rebar_memory_type_index() const noexcept
    {
        return rebar_memory_type_index_;
    }

    VmaAllocator vulkan_device::vma_allocator() const noexcept
    {
        return vma_allocator_;
    }

    VkQueue vulkan_device::main_queue() const noexcept
    {
        return main_queue_;
    }

    std::uint32_t vulkan_device::main_queue_family_index() const noexcept
    {
        return queue_families_.main;
    }

    VkQueue vulkan_device::dedicated_compute_queue() const noexcept
    {
        return dedicated_compute_queue_;
    }

    std::uint32_t vulkan_device::dedicated_compute_queue_family_index() const noexcept
    {
        return queue_families_.dedicated_compute;
    }

    VkQueue vulkan_device::dedicated_transfer_queue() const noexcept
    {
        return dedicated_transfer_queue_;
    }

    std::uint32_t vulkan_device::dedicated_transfer_queue_family_index() const noexcept
    {
        return queue_families_.dedicated_transfer;
    }

    VkResult vulkan_device::set_debug_object_name(
        [[maybe_unused]] VkObjectType type,
        [[maybe_unused]] std::uint64_t handle,
        [[maybe_unused]] char const* name) const
    {
#if VKGC_DEBUG_VULKAN
        VkDebugUtilsObjectNameInfoEXT const name_info{
            .sType{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT},
            .pNext{nullptr},
            .objectType{type},
            .objectHandle{handle},
            .pObjectName{name},
          };

        return vkSetDebugUtilsObjectNameEXT(handle_, &name_info);
#else
        return VK_SUCCESS;
#endif
    }
}
