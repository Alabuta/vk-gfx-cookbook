module;

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <limits>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <volk.h>
#include "vulkan/format.hxx"

#include <GLFW/glfw3.h>

#include "assert.hxx"

#include "vk_mem_alloc.h"

module vkgc.vulkan_instance;

import vkgc.vulkan_device_features;

namespace
{
    std::array<char const*, 1> constexpr kVulkanDeviceRequiredExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    std::array<char const*, 1> constexpr kVulkanDeviceRequestedExtensions{
        VK_EXT_SHADER_OBJECT_EXTENSION_NAME
    };

    [[nodiscard]]
    bool check_device_extensions_support(
        VkPhysicalDevice physical_device,
        std::vector<char const*> extensions_to_check)
    {
        std::uint32_t extensions_count{0};
        if (!VKGC_ENSURE_VKSUCCESS(vkEnumerateDeviceExtensionProperties(
                physical_device,
                nullptr,
                &extensions_count, nullptr)))
        {
            return false;
        }

        std::vector<VkExtensionProperties> supported_extensions(extensions_count);
        if (!VKGC_ENSURE_VKSUCCESS(vkEnumerateDeviceExtensionProperties(
                physical_device,
                nullptr,
                &extensions_count,
                supported_extensions.data())))
        {
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
        std::println(stdout, "{} next device extensions are unsupported:", prefix);
        for (char const* extension : unsupported_extensions)
        {
            std::println(stdout, "{:{}} {}", "", prefix.size(), extension);
        }

        return false;
    }

    // The subset of `candidates` the device advertises. Optional extensions go through here
    // instead of check_device_extensions_support: a missing one is not a failure, it just
    // does not get enabled. Returned pointers alias `candidates`.
    [[nodiscard]]
    std::vector<char const*> filter_supported_extensions(
        VkPhysicalDevice physical_device,
        std::span<char const* const> const candidates)
    {
        std::uint32_t extensions_count{0};
        if (!VKGC_ENSURE_VKSUCCESS(vkEnumerateDeviceExtensionProperties(
                physical_device,
                nullptr,
                &extensions_count,
                nullptr)))
        {
            return {};
        }

        std::vector<VkExtensionProperties> supported_extensions(extensions_count);
        if (!VKGC_ENSURE_VKSUCCESS(vkEnumerateDeviceExtensionProperties(
                physical_device,
                nullptr,
                &extensions_count,
                supported_extensions.data())))
        {
            return {};
        }

        std::vector<char const*> supported;
        supported.reserve(candidates.size());

        std::ranges::copy_if(
            candidates,
            std::back_inserter(supported),
            [&supported_extensions](char const* candidate)
            {
                return std::ranges::any_of(
                    supported_extensions,
                    [candidate](char const* name) { return std::strcmp(name, candidate) == 0; },
                    &VkExtensionProperties::extensionName);
            });

        return supported;
    }

    [[nodiscard]]
    vkgc::vulkan_queue_families select_queue_families(
        VkInstance instance,
        VkPhysicalDevice physical_device,
        bool const presentation_required) noexcept
    {
        auto constexpr invalid_family_index = std::numeric_limits<std::uint32_t>::max();

        std::uint32_t families_count{0};
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &families_count, nullptr);

        std::vector<VkQueueFamilyProperties> families(families_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &families_count, families.data());

        vkgc::vulkan_queue_families queue_families;

        // Main: graphics (+ platform presentation if required)
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

            // Windowless presentation-capability check: queried at the platform level
            // (no surface needed). The surface-specific support is validated later when
            // the swapchain is built against the actual surface.
            if (presentation_required &&
                glfwGetPhysicalDevicePresentationSupport(instance, physical_device, family_index) != GLFW_TRUE)
            {
                continue;
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
        VkPhysicalDevice physical_device,
        VkPhysicalDeviceProperties const& properties,
        vkgc::vulkan_queue_families const& queue_families,
        std::vector<char const*> device_extensions
    ) noexcept
    {
        auto constexpr invalid_family_index = std::numeric_limits<std::uint32_t>::max();

        std::uint32_t score{0};

        if (check_device_extensions_support(physical_device, std::move(device_extensions)))
        {
            score += 500;
        }

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

    [[nodiscard]]
    char const* extension_name(std::string const& ext) noexcept
    {
        return ext.c_str();
    }

    [[nodiscard]]
    char const* extension_name(char const* ext) noexcept
    {
        return ext;
    }

    template <typename Range>
    concept extension_name_range =
        std::ranges::input_range<Range> &&
        std::ranges::sized_range<Range> &&
        requires(std::ranges::range_reference_t<Range> element) {
            { extension_name(element) } -> std::same_as<char const*>;
        };

    // Returned pointers alias the input ranges; do not mutate them before vkCreateDevice.
    template <extension_name_range... Exts>
    [[nodiscard]]
    std::vector<char const*> build_device_extensions(Exts&&... exts)
    {
        std::vector<char const*> extensions;
        extensions.reserve((std::ranges::size(exts) + ...));

        auto const append = [&extensions](auto&& ext_range)
        {
            std::ranges::copy_if(
                ext_range | std::views::transform([](auto&& ext) { return extension_name(ext); }),
                std::back_inserter(extensions),
                [&extensions](char const* ext)
                {
                    return !std::ranges::any_of(
                        extensions,
                        [ext](char const* e) { return std::strcmp(e, ext) == 0; });
                });
        };

        (append(std::forward<Exts>(exts)), ...);

        return extensions;
    }

    [[nodiscard]]
    std::pair<VkPhysicalDevice, vkgc::vulkan_queue_families> select_physical_device(
        VkInstance instance,
        std::span<char const* const> const required_extensions,
        std::span<char const* const> const requested_extensions,
        bool const presentation_required) noexcept
    {
        auto constexpr invalid_family_index = std::numeric_limits<std::uint32_t>::max();

        std::uint32_t physical_device_count{0};
        if (!VKGC_ENSURE_VKSUCCESS(vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr)))
        {
            return {VK_NULL_HANDLE, vkgc::vulkan_queue_families{}};
        }

        VKGC_CHECKF(physical_device_count > 0, "no Vulkan-capable physical devices found");

        std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
        if (!VKGC_ENSURE_VKSUCCESS(vkEnumeratePhysicalDevices(
                instance,
                &physical_device_count,
                physical_devices.data())))
        {
            return {VK_NULL_HANDLE, vkgc::vulkan_queue_families{}};
        }

        VkPhysicalDevice best_device{VK_NULL_HANDLE};
        vkgc::vulkan_queue_families best_queue_families{};
        std::uint32_t best_score{0};

        std::vector required_extensions_to_check{std::from_range, required_extensions};
        std::vector requested_extensions_to_check{std::from_range, requested_extensions};

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

            if (!check_device_extensions_support(physical_device, required_extensions_to_check))
            {
                continue;
            }

            if (!vkgc::check_device_features_support(physical_device, device_properties))
            {
                continue;
            }

            auto queue_families = select_queue_families(instance, physical_device, presentation_required);
            if (queue_families.main == invalid_family_index)
            {
                continue;
            }

            if (auto const score = score_physical_device(
                    physical_device,
                    device_properties.properties,
                    queue_families,
                    requested_extensions_to_check);
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
        if (!VKGC_ENSUREF_VKHANDLE(handle_, "cannot create device from null instance"))
        {
            return {};
        }

        auto const required_extensions = build_device_extensions(
            info.extensions, kVulkanDeviceRequiredExtensions);
        auto const requested_extensions = build_device_extensions(kVulkanDeviceRequestedExtensions);

        auto [physical_device, queue_families] = select_physical_device(
            handle_,
            required_extensions,
            requested_extensions,
            info.presentation_required);
        if (!VKGC_ENSUREF_VKHANDLE(physical_device, "failed to pick physical device"))
        {
            return {};
        }

        // Requested extensions never gate selection (they only score devices), so the winner may
        // support all, some, or none of them. Enable exactly the ones it advertises.
        auto const supported_requested_extensions = filter_supported_extensions(
            physical_device, requested_extensions);
        auto const enabled_extensions = build_device_extensions(
            required_extensions, supported_requested_extensions);

        auto const queue_create_infos = build_queue_create_infos(queue_families);
        assert(!queue_create_infos.empty());

        auto const device_features_chain = build_device_features_chain(supported_requested_extensions);

        VkDeviceCreateInfo const create_info{
            .sType{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO},
            .pNext{device_features_chain.as_pointer()},
            .flags{0},
            .queueCreateInfoCount{static_cast<std::uint32_t>(queue_create_infos.size())},
            .pQueueCreateInfos{queue_create_infos.data()},
            .enabledLayerCount{0},
            .ppEnabledLayerNames{nullptr},
            .enabledExtensionCount{static_cast<std::uint32_t>(enabled_extensions.size())},
            .ppEnabledExtensionNames{enabled_extensions.data()},
            .pEnabledFeatures{nullptr}
        };

        VkDevice device_handle{VK_NULL_HANDLE};
        if (!VKGC_ENSURE_VKSUCCESS(vkCreateDevice(physical_device, &create_info, nullptr, &device_handle)))
        {
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
