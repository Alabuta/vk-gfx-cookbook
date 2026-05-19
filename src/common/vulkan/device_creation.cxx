module;

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <print>
#include <ranges>
#include <vector>

#include <volk.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "GLFW/glfw3.h"

module vkgc.vulkan_instance;

import vkgc.vulkan_format;
import vkgc.vulkan_device_features;

namespace
{
    std::array constexpr kVulkanDeviceDefaultExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    [[nodiscard]]
    bool check_device_extensions_support(
        VkPhysicalDevice physical_device,
        std::vector<char const*> extensions_to_check)
    {
        std::uint32_t extensions_count{0};
        if (auto const result = vkEnumerateDeviceExtensionProperties(
                physical_device,
                nullptr,
                &extensions_count, nullptr);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Warning : failed to retrieve device extensions count ({})", result);
            return false;
        }

        std::vector<VkExtensionProperties> supported_extensions(extensions_count);
        if (auto const result = vkEnumerateDeviceExtensionProperties(
                physical_device,
                nullptr,
                &extensions_count,
                supported_extensions.data());
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Warning : failed to retrieve device extensions ({})", result);
            return false;
        }

        auto comp = [](char const* lhs, char const* rhs)
        {
            return std::strcmp(lhs, rhs) < 0;
        };

        std::ranges::sort(extensions_to_check, comp);
        std::ranges::sort(supported_extensions, comp, &VkExtensionProperties::extensionName);

        std::vector<char const*> unsupported_extensions;
        unsupported_extensions.reserve(extensions_to_check.size());

        std::ranges::set_difference(
            extensions_to_check,
            supported_extensions | std::views::transform(&VkExtensionProperties::extensionName),
            std::back_inserter(unsupported_extensions),
            comp);

        if (unsupported_extensions.empty())
        {
            return true;
        }

        std::string_view constexpr prefix{"[Vulkan] : Warning :"};
        std::println(stderr, "{} next device extensions are unsupported:", prefix);
        for (auto&& extension : unsupported_extensions)
        {
            std::println(stderr, "{:{}} {}", "", prefix.size(), extension);
        }

        return false;
    }

    [[nodiscard]]
    vkgc::vulkan_queue_families select_queue_families(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface) noexcept
    {
        auto constexpr invalid_family_index = std::numeric_limits<std::uint32_t>::max();

        std::uint32_t families_count{0};
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &families_count, nullptr);

        std::vector<VkQueueFamilyProperties> families(families_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &families_count, families.data());

        bool const presentation_support_required = surface != VK_NULL_HANDLE;

        vkgc::vulkan_queue_families queue_families;

        // Main: graphics (+ presentation if a surface is provided)
        for (std::uint32_t family_index{0}; family_index < families_count; ++family_index)
        {
            if (families[family_index].queueCount == 0)
            {
                continue;
            }

            if ((families[family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            {
                continue;
            }

            if (presentation_support_required)
            {
                VkBool32 surface_supported{VK_FALSE};
                if (auto const result = vkGetPhysicalDeviceSurfaceSupportKHR(
                        physical_device,
                        family_index,
                        surface,
                        &surface_supported);
                    result != VK_SUCCESS || surface_supported != VK_TRUE)
                {
                    continue;
                }
            }

            queue_families.main = family_index;
            break;
        }

        if (queue_families.main == invalid_family_index)
        {
            return queue_families;
        }

        // Dedicated async compute: compute, no graphics
        for (std::uint32_t family_index{0}; family_index < families_count; ++family_index)
        {
            if (families[family_index].queueCount == 0)
            {
                continue;
            }

            auto const flags = families[family_index].queueFlags;
            if ((flags & VK_QUEUE_COMPUTE_BIT) == 0)
            {
                continue;
            }

            if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                continue;
            }

            queue_families.dedicated_compute = family_index;
            break;
        }

        // Dedicated DMA transfer: transfer, no graphics, no compute
        for (std::uint32_t family_index{0}; family_index < families_count; ++family_index)
        {
            if (families[family_index].queueCount == 0)
            {
                continue;
            }

            auto const flags = families[family_index].queueFlags;
            if ((flags & VK_QUEUE_TRANSFER_BIT) == 0)
            {
                continue;
            }

            if ((flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != 0)
            {
                continue;
            }

            queue_families.dedicated_transfer = family_index;
            break;
        }

        return queue_families;
    }

    [[nodiscard]]
    std::uint32_t score_physical_device(
        VkPhysicalDeviceProperties const& properties,
        vkgc::vulkan_queue_families const& queue_families) noexcept
    {
        auto constexpr invalid_family_index = std::numeric_limits<std::uint32_t>::max();

        std::uint32_t score{0};

        switch (properties.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 1'000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 100;
            break;
        default:
            break;
        }

        if (queue_families.dedicated_compute != invalid_family_index)
        {
            score += 50;
        }

        if (queue_families.dedicated_transfer != invalid_family_index)
        {
            score += 50;
        }

        return score;
    }

    // Returned pointers alias `requested`; do not mutate `requested` before vkCreateDevice.
    [[nodiscard]]
    std::vector<char const*> build_device_extensions(std::vector<std::string> const& requested)
    {
        std::vector<char const*> extensions;
        extensions.reserve(requested.size() + kVulkanDeviceDefaultExtensions.size());

        std::ranges::copy_if(
            requested | std::views::transform(&std::string::c_str),
            std::back_inserter(extensions),
            [&extensions](char const* ext)
            {
                return !std::ranges::any_of(extensions, [ext](char const* e) { return std::strcmp(e, ext) == 0; });
            });

        std::ranges::copy_if(
            kVulkanDeviceDefaultExtensions,
            std::back_inserter(extensions),
            [&extensions](char const* ext)
            {
                return !std::ranges::any_of(extensions, [ext](char const* e) { return std::strcmp(e, ext) == 0; });
            });

        return extensions;
    }

    [[nodiscard]]
    std::pair<VkPhysicalDevice, vkgc::vulkan_queue_families> select_physical_device(
        VkInstance instance,
        VkSurfaceKHR surface,
        std::span<char const* const> const required_extensions) noexcept
    {
        auto constexpr invalid_family_index = std::numeric_limits<std::uint32_t>::max();

        std::uint32_t physical_device_count{0};
        if (auto const result = vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to enumerate physical devices ({})", result);
            return {VK_NULL_HANDLE, vkgc::vulkan_queue_families{}};
        }

        if (physical_device_count == 0)
        {
            std::println(stderr, "[Vulkan] : Fatal : no Vulkan-capable physical devices found");
            return {VK_NULL_HANDLE, vkgc::vulkan_queue_families{}};
        }

        std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
        if (auto const result = vkEnumeratePhysicalDevices(
                instance,
                &physical_device_count,
                physical_devices.data());
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to enumerate physical devices ({})", result);
            return {VK_NULL_HANDLE, vkgc::vulkan_queue_families{}};
        }

        VkPhysicalDevice best_device{VK_NULL_HANDLE};
        vkgc::vulkan_queue_families best_queue_families{};
        std::uint32_t best_score{0};

        std::vector extensions_to_check{std::from_range, required_extensions};

        for (VkPhysicalDevice physical_device : physical_devices)
        {
            VkPhysicalDeviceProperties2 device_properties{
                .sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2},
                .pNext{nullptr},
                .properties{}
            };
            vkGetPhysicalDeviceProperties2(physical_device, &device_properties);

            if (device_properties.properties.apiVersion < vkgc::kVulkanApiVersion)
            {
                continue;
            }

            if (!check_device_extensions_support(physical_device, extensions_to_check))
            {
                continue;
            }

            if (!vkgc::check_device_features_support(physical_device, device_properties))
            {
                continue;
            }

            auto queue_families = select_queue_families(physical_device, surface);
            if (queue_families.main == invalid_family_index)
            {
                continue;
            }

            if (auto const score = score_physical_device(device_properties.properties, queue_families);
                best_device == VK_NULL_HANDLE || score > best_score)
            {
                best_device = physical_device;
                best_queue_families = queue_families;
                best_score = score;
            }
        }

        return {best_device, best_queue_families};
    }

    [[nodiscard]]
    std::vector<VkDeviceQueueCreateInfo> build_queue_create_infos(vkgc::vulkan_queue_families const& queue_families)
    {
        auto constexpr invalid_family_index = std::numeric_limits<std::uint32_t>::max();
        assert(queue_families.main != invalid_family_index);

        static float constexpr KQueuePriority{1.f};

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        queue_create_infos.reserve(3);

        auto const push_queue = [&](std::uint32_t family_index)
        {
            queue_create_infos.push_back({
                .sType{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .queueFamilyIndex{family_index},
                .queueCount{1},
                .pQueuePriorities{&KQueuePriority}
            });
        };

        push_queue(queue_families.main);

        if (queue_families.dedicated_compute != invalid_family_index)
        {
            push_queue(queue_families.dedicated_compute);
        }

        if (queue_families.dedicated_transfer != invalid_family_index)
        {
            push_queue(queue_families.dedicated_transfer);
        }

        return queue_create_infos;
    }
}

namespace vkgc
{
    vulkan_device vulkan_instance::create_device(vulkan_device_info const& info) noexcept
    {
        if (handle_ == VK_NULL_HANDLE)
        {
            std::println(stderr, "[Vulkan] : Fatal : cannot create device from null instance");
            return {};
        }

        auto const extensions = build_device_extensions(info.extensions);

        auto [physical_device, queue_families] = select_physical_device(handle_, info.surface, extensions);
        if (physical_device == VK_NULL_HANDLE)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to pick physical device");
            return {};
        }

        auto const queue_create_infos = build_queue_create_infos(queue_families);
        assert(!queue_create_infos.empty());

        auto const device_features_chain = build_device_features_chain();

        VkDeviceCreateInfo const create_info{
            .sType{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO},
            .pNext{device_features_chain.as_pointer()},
            .flags{0},
            .queueCreateInfoCount{static_cast<std::uint32_t>(queue_create_infos.size())},
            .pQueueCreateInfos{queue_create_infos.data()},
            .enabledLayerCount{0},
            .ppEnabledLayerNames{nullptr},
            .enabledExtensionCount{static_cast<std::uint32_t>(extensions.size())},
            .ppEnabledExtensionNames{extensions.data()},
            .pEnabledFeatures{nullptr}
        };

        VkDevice device_handle{VK_NULL_HANDLE};
        if (auto const result = vkCreateDevice(physical_device, &create_info, nullptr, &device_handle);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to create logical device ({})", result);
            return {};
        }

        volkLoadDevice(device_handle);

        VmaVulkanFunctions vk_functions{};
        vk_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vk_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo const allocator_create_info{
            .flags{VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT},
            .physicalDevice{physical_device},
            .device{device_handle},
            .preferredLargeHeapBlockSize{0},
            .pAllocationCallbacks{nullptr},
            .pDeviceMemoryCallbacks{nullptr},
            .pHeapSizeLimit{nullptr},
            .pVulkanFunctions{&vk_functions},
            .instance{handle_},
            .vulkanApiVersion{kVulkanApiVersion},
            .pTypeExternalMemoryHandleTypes{nullptr}
        };

        VmaAllocator vma_allocator;
        vmaCreateAllocator(&allocator_create_info, &vma_allocator);

        return vulkan_device{physical_device, device_handle, queue_families, vma_allocator};
    }
}
