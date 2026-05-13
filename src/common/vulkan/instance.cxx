module;

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <print>
#include <ranges>
#include <vector>

#include "config.hxx"

#include "vulkan_format.hxx"

module cookbook.vulkan_instance;

namespace
{
    VkApplicationInfo constexpr application_info{
        .sType{VK_STRUCTURE_TYPE_APPLICATION_INFO},
        .pNext{nullptr},
        .pApplicationName{"vk-gfx-cookbook"},
        .applicationVersion{VK_MAKE_VERSION(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH)},
        .pEngineName{nullptr},
        .engineVersion{0},
        .apiVersion{VK_MAKE_API_VERSION(0, COOKBOOK_VULKAN_API_VERSION_MAJOR, COOKBOOK_VULKAN_API_VERSION_MINOR, 0)}
    };

    std::array constexpr kVulkanInstanceDefaultExtensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
        VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_METAL_EXT)
        VK_EXT_METAL_SURFACE_EXTENSION_NAME
#else
#error Unsupported OS
#endif
    };

    VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT const message_severity,
        VkDebugUtilsMessageTypeFlagsEXT const message_types,
        VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
        void*)
    {
        char const* const id_name = callback_data->pMessageIdName ? callback_data->pMessageIdName : "";
        std::int32_t const id = callback_data->messageIdNumber;
        char const* const message = callback_data->pMessage ? callback_data->pMessage : "";

        if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            std::println(stderr, "[Vulkan] : Error : [#{}:{}] {}", id, id_name, message);
        }
        else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
            {
                std::println(stderr, "[Vulkan] : Perf : [#{}:{}] {}", id, id_name, message);
            }
            else
            {
                std::println(stderr, "[Vulkan] : Warning : [#{}:{}] {}", id, id_name, message);
            }
        }
        else
        {
            std::println(stdout, "[Vulkan] : Log : [#{}:{}] {}", id, id_name, message);
        }

        return VK_FALSE;
    }

    VkDebugUtilsMessengerCreateInfoEXT constexpr kDebugUtilsMessengerCreateInfo{
        .sType{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT},
        .pNext{nullptr},
        .flags{0},
        .messageSeverity{
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT /*|
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT*/
        },
        .messageType{
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
        },
        .pfnUserCallback{debug_utils_messenger_callback},
        .pUserData{nullptr}
    };
}

namespace cookbook
{
    vulkan_instance::vulkan_instance(vulkan_instance_info const& info) noexcept
    {
        {
            std::uint32_t supported_api_version = 0;
            if (auto const result = vkEnumerateInstanceVersion(&supported_api_version); result != VK_SUCCESS)
            {
                std::println(stderr, "[Vulkan] : Fatal : failed to retrieve Vulkan API version ({})", result);
                return;
            }

            if (supported_api_version < application_info.apiVersion)
            {
                std::println(
                    stderr,
                    "[Vulkan] : Fatal : supported Vulkan API version is {}.{}, minimum required is {}.{}",
                    VK_VERSION_MAJOR(supported_api_version),
                    VK_VERSION_MINOR(supported_api_version),
                    VK_VERSION_MAJOR(application_info.apiVersion),
                    VK_VERSION_MINOR(application_info.apiVersion));
                return;
            }
        }

        // extensions/validation_layers name pointers alias info.extensions/validation_layers; do not mutate before vkCreate
        std::vector<char const*> extensions;
        std::vector<char const*> validation_layers;

        if (info.enable_validation)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            validation_layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        {
            extensions.reserve(extensions.size() + info.extensions.size() + kVulkanInstanceDefaultExtensions.size());

            std::ranges::copy_if(
                info.extensions | std::views::transform(&std::string::c_str),
                std::back_inserter(extensions),
                [&extensions](char const* ext)
                {
                    return !std::ranges::any_of(extensions, [ext](char const* e) { return std::strcmp(e, ext) == 0; });
                });

            std::ranges::copy_if(
                kVulkanInstanceDefaultExtensions,
                std::back_inserter(extensions),
                [&extensions](char const* ext)
                {
                    return !std::ranges::any_of(extensions, [ext](char const* e) { return std::strcmp(e, ext) == 0; });
                });
        }

        {
            validation_layers.reserve(validation_layers.size() + info.validation_layers.size());

            std::ranges::copy_if(
                info.validation_layers | std::views::transform(&std::string::c_str),
                std::back_inserter(validation_layers),
                [&validation_layers](char const* layer)
                {
                    return !std::ranges::any_of(
                        validation_layers,
                        [layer](char const* l) { return std::strcmp(l, layer) == 0; });
                });
        }

        VkInstanceCreateInfo const create_info{
            .sType{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO},
            .pNext{info.enable_validation ? &kDebugUtilsMessengerCreateInfo : nullptr},
            .flags{0},
            .pApplicationInfo{&application_info},
            .enabledLayerCount{static_cast<std::uint32_t>(validation_layers.size())},
            .ppEnabledLayerNames{validation_layers.data()},
            .enabledExtensionCount{static_cast<std::uint32_t>(extensions.size())},
            .ppEnabledExtensionNames{extensions.data()}
        };

        if (auto const result = vkCreateInstance(&create_info, nullptr, &handle_); result != VK_SUCCESS)
        {
            std::println(stderr, "[Vulkan] : Fatal : failed to create instance ({})", result);
            handle_ = VK_NULL_HANDLE;
            return;
        }

        volkLoadInstance(handle_);

        if (info.enable_validation)
        {
            if (auto const result = vkCreateDebugUtilsMessengerEXT(
                    handle_,
                    &kDebugUtilsMessengerCreateInfo,
                    nullptr,
                    &debug_utils_messenger_);
                result != VK_SUCCESS)
            {
                std::println(stderr, "[Vulkan] : Fatal : debug utils messenger create request failed ({})", result);
                debug_utils_messenger_ = VK_NULL_HANDLE;
            }
        }
    }

    vulkan_instance::~vulkan_instance()
    {
        if (handle_ == VK_NULL_HANDLE)
        {
            return;
        }

        if (debug_utils_messenger_ != VK_NULL_HANDLE)
        {
            vkDestroyDebugUtilsMessengerEXT(handle_, debug_utils_messenger_, nullptr);
            debug_utils_messenger_ = VK_NULL_HANDLE;
        }

        vkDestroyInstance(handle_, nullptr);
        handle_ = VK_NULL_HANDLE;
    }

    vulkan_instance::operator bool() const noexcept
    {
        return handle_ != VK_NULL_HANDLE;
    }

    VkInstance vulkan_instance::handle() const noexcept
    {
        return handle_;
    }

    vulkan_surface vulkan_instance::create_surface(window const& w) noexcept
    {
        return vulkan_surface{handle_, w.create_vulkan_surface(handle_)};
    }
}
