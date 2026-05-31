module;

#include <algorithm>
#include <array>
#include <print>

#include <volk.h>

module vkgc.vulkan_device_features;

import vkgc.vulkan_helpers;

namespace
{
    std::array constexpr kRequiredVk10Features{
        &VkPhysicalDeviceFeatures::imageCubeArray,
        &VkPhysicalDeviceFeatures::geometryShader,
        &VkPhysicalDeviceFeatures::tessellationShader,
        &VkPhysicalDeviceFeatures::sampleRateShading,
        &VkPhysicalDeviceFeatures::multiDrawIndirect,
        &VkPhysicalDeviceFeatures::drawIndirectFirstInstance,
        &VkPhysicalDeviceFeatures::depthClamp,
        &VkPhysicalDeviceFeatures::fillModeNonSolid,
        &VkPhysicalDeviceFeatures::samplerAnisotropy,
        &VkPhysicalDeviceFeatures::shaderUniformBufferArrayDynamicIndexing,
        &VkPhysicalDeviceFeatures::shaderSampledImageArrayDynamicIndexing,
        &VkPhysicalDeviceFeatures::shaderStorageBufferArrayDynamicIndexing,
        &VkPhysicalDeviceFeatures::shaderStorageImageArrayDynamicIndexing,
        &VkPhysicalDeviceFeatures::shaderInt16,
        &VkPhysicalDeviceFeatures::sparseBinding
    };

    // None required today
    std::array constexpr kRequiredVk11Features{
        &VkPhysicalDeviceVulkan11Features::storageBuffer16BitAccess,
        &VkPhysicalDeviceVulkan11Features::samplerYcbcrConversion,
        &VkPhysicalDeviceVulkan11Features::shaderDrawParameters
    };

    std::array constexpr kRequiredVk12Features{
        &VkPhysicalDeviceVulkan12Features::drawIndirectCount,
        &VkPhysicalDeviceVulkan12Features::descriptorIndexing,
        &VkPhysicalDeviceVulkan12Features::shaderSampledImageArrayNonUniformIndexing,
        &VkPhysicalDeviceVulkan12Features::descriptorBindingSampledImageUpdateAfterBind,
        &VkPhysicalDeviceVulkan12Features::descriptorBindingStorageImageUpdateAfterBind,
        &VkPhysicalDeviceVulkan12Features::descriptorBindingUpdateUnusedWhilePending,
        &VkPhysicalDeviceVulkan12Features::descriptorBindingPartiallyBound,
        &VkPhysicalDeviceVulkan12Features::descriptorBindingVariableDescriptorCount,
        &VkPhysicalDeviceVulkan12Features::runtimeDescriptorArray,
        &VkPhysicalDeviceVulkan12Features::scalarBlockLayout,
        &VkPhysicalDeviceVulkan12Features::timelineSemaphore,
        &VkPhysicalDeviceVulkan12Features::bufferDeviceAddress
    };

    std::array constexpr kRequiredVk13Features{
        &VkPhysicalDeviceVulkan13Features::subgroupSizeControl,
        &VkPhysicalDeviceVulkan13Features::synchronization2,
        &VkPhysicalDeviceVulkan13Features::dynamicRendering,
        &VkPhysicalDeviceVulkan13Features::maintenance4
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
        vkgc::vulkan_device_features_chain features_chain{
            VkPhysicalDeviceFeatures2{.sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}},
            VkPhysicalDeviceVulkan11Features{.sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES}},
            VkPhysicalDeviceVulkan12Features{.sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES}},
            VkPhysicalDeviceVulkan13Features{.sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES}},
            VkPhysicalDeviceVulkan14Features{.sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES}}
        };
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

        return vkgc::vulkan_device_features_chain{features_chain};
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
}

namespace vkgc
{
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

    vulkan_device_features_chain build_device_features_chain()
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

        return vulkan_device_features_chain{features_chain};
    }
}
