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

#include "config.hxx"
#include "vulkan_format.hxx"
#include "GLFW/glfw3.h"

module cookbook.vulkan_instance;

import cookbook.vulkan_helpers;

namespace
{
    std::uint32_t constexpr kRequiredApiVersion{
        VK_MAKE_API_VERSION(0, COOKBOOK_VULKAN_API_VERSION_MAJOR, COOKBOOK_VULKAN_API_VERSION_MINOR, 0)
    };

    std::array constexpr kVulkanDeviceDefaultExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    std::array constexpr kRequiredVk10Features{
        &VkPhysicalDeviceFeatures::samplerAnisotropy
    };

    // None required today
    std::array<VkBool32 VkPhysicalDeviceVulkan11Features::*, 0> constexpr kRequiredVk11Features{
    };

    std::array constexpr kRequiredVk12Features{
        &VkPhysicalDeviceVulkan12Features::descriptorIndexing,
        &VkPhysicalDeviceVulkan12Features::shaderSampledImageArrayNonUniformIndexing,
        &VkPhysicalDeviceVulkan12Features::descriptorBindingVariableDescriptorCount,
        &VkPhysicalDeviceVulkan12Features::runtimeDescriptorArray,
        &VkPhysicalDeviceVulkan12Features::bufferDeviceAddress
    };

    std::array constexpr kRequiredVk13Features{
        &VkPhysicalDeviceVulkan13Features::synchronization2,
        &VkPhysicalDeviceVulkan13Features::dynamicRendering
    };

    std::array constexpr kRequiredVk14Features{
        &VkPhysicalDeviceVulkan14Features::pushDescriptor
    };

    [[nodiscard]]
    auto make_features_chain() noexcept
    {
        static_assert(
            VK_HEADER_VERSION_COMPLETE < VK_MAKE_API_VERSION(0, 1, 5, 0),
            "Vulkan 1.5 headers available - extend make_features_chain and visit_required_feature_sets");

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
        cookbook::vulkan_extending_structs_chain features_chain{
            VkPhysicalDeviceFeatures2{.sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}},
            VkPhysicalDeviceVulkan11Features{.sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES}},
            VkPhysicalDeviceVulkan12Features{.sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES}},
            VkPhysicalDeviceVulkan13Features{.sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES}},
            VkPhysicalDeviceVulkan14Features{.sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES}}
        };
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

        return cookbook::vulkan_extending_structs_chain{features_chain};
    }

    template <class C, class V>
    void visit_required_feature_sets(C& features_chain, V&& visit)
    {
        static_assert(C::Size == 5, "Chain shape changed - update visit_required_feature_sets");

        visit(features_chain.template get<0>().features, kRequiredVk10Features, "VkPhysicalDeviceFeatures");
        visit(features_chain.template get<1>(), kRequiredVk11Features, "VkPhysicalDeviceVulkan11Features");
        visit(features_chain.template get<2>(), kRequiredVk12Features, "VkPhysicalDeviceVulkan12Features");
        visit(features_chain.template get<3>(), kRequiredVk13Features, "VkPhysicalDeviceVulkan13Features");
        visit(features_chain.template get<4>(), kRequiredVk14Features, "VkPhysicalDeviceVulkan14Features");
    }

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
    bool check_device_features_support(
        VkPhysicalDevice physical_device,
        VkPhysicalDeviceProperties2 const& device_properties)
    {
        auto features_chain = make_features_chain();
        vkGetPhysicalDeviceFeatures2(physical_device, features_chain.as_pointer());

        bool all_supported{true};

        visit_required_feature_sets(
            features_chain,
            [&](auto const& features, auto const& required, char const* set_name)
            {
                if (!all_supported)
                {
                    return;
                }

                bool const ok = std::ranges::all_of(
                    required,
                    [&features](auto m) { return features.*m == VK_TRUE; });

                if (!ok)
                {
                    std::println(
                        stderr,
                        "[Vulkan] : Warning : required feature set '{}' not supported by selected physical device '{}'",
                        set_name,
                        device_properties.properties.deviceName);
                    all_supported = false;
                }
            });

        return all_supported;
    }

    [[nodiscard]]
    auto build_device_features_chain()
    {
        auto features_chain = make_features_chain();

        visit_required_feature_sets(
            features_chain,
            [](auto& features, auto const& required, char const*)
            {
                for (auto m : required)
                {
                    features.*m = VK_TRUE;
                }
            });

        return cookbook::vulkan_extending_structs_chain{features_chain};
    }

    [[nodiscard]]
    cookbook::vulkan_queue_families select_queue_families(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface) noexcept
    {
        auto constexpr invalid_family_index = std::numeric_limits<std::uint32_t>::max();

        std::uint32_t families_count{0};
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &families_count, nullptr);

        std::vector<VkQueueFamilyProperties> families(families_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &families_count, families.data());

        bool const presentation_support_required = surface != VK_NULL_HANDLE;

        cookbook::vulkan_queue_families queue_families;

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
        cookbook::vulkan_queue_families const& queue_families) noexcept
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

    [[nodiscard]]
    std::pair<VkPhysicalDevice, cookbook::vulkan_queue_families> select_physical_device(
        VkInstance instance,
        VkSurfaceKHR surface,
        std::span<char const*> const required_extensions) noexcept
    {
        auto constexpr invalid_family_index = std::numeric_limits<std::uint32_t>::max();

        std::uint32_t physical_device_count{0};
        if (auto const result = vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to enumerate physical devices ({})", result);
            return {VK_NULL_HANDLE, cookbook::vulkan_queue_families{}};
        }

        if (physical_device_count == 0)
        {
            std::println(stderr, "[Vulkan] : Fatal : no Vulkan-capable physical devices found");
            return {VK_NULL_HANDLE, cookbook::vulkan_queue_families{}};
        }

        std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
        if (auto const result = vkEnumeratePhysicalDevices(
                instance,
                &physical_device_count,
                physical_devices.data());
            result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to enumerate physical devices ({})", result);
            return {VK_NULL_HANDLE, cookbook::vulkan_queue_families{}};
        }

        VkPhysicalDevice best_device{VK_NULL_HANDLE};
        cookbook::vulkan_queue_families best_queue_families{};
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

            if (device_properties.properties.apiVersion < kRequiredApiVersion)
            {
                continue;
            }

            if (!check_device_extensions_support(physical_device, extensions_to_check))
            {
                continue;
            }

            if (!check_device_features_support(physical_device, device_properties))
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
}

namespace cookbook
{
    vulkan_device vulkan_instance::create_device(vulkan_device_info const& info) noexcept
    {
        if (handle_ == VK_NULL_HANDLE)
        {
            std::println(stderr, "[Vulkan] : Fatal : cannot create device from null instance");
            return {};
        }

        // extensions name pointers alias info.extensions; do not mutate before vkCreate
        std::vector<char const*> extensions;
        {
            extensions.reserve(info.extensions.size() + kVulkanDeviceDefaultExtensions.size());

            std::ranges::copy_if(
                info.extensions | std::views::transform(&std::string::c_str),
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
        }

        auto [physical_device, queue_families] = select_physical_device(handle_, info.surface, extensions);
        if (physical_device == VK_NULL_HANDLE)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to pick physical device");
            return {};
        }

        auto constexpr invalid_family_index = std::numeric_limits<std::uint32_t>::max();
        assert(queue_families.main != invalid_family_index);

        float constexpr queue_priority{1.f};

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        {
            queue_create_infos.reserve(3);

            auto const push_queue = [&](std::uint32_t family_index)
            {
                queue_create_infos.push_back({
                    .sType{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO},
                    .pNext{nullptr},
                    .flags{0},
                    .queueFamilyIndex{family_index},
                    .queueCount{1},
                    .pQueuePriorities{&queue_priority}
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
        }

        auto device_features_chain = build_device_features_chain();

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

        return vulkan_device{physical_device, device_handle, queue_families};
    }
}
