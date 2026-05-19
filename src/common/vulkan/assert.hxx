#pragma once

// Vulkan-specific assert / verify / ensure macros for VkResult-returning calls.
//
// Macros forward to `vkgc::assert_detail::vk_fatal` /
// `vkgc::assert_detail::vk_ensure_log`, exported by the
// `cookbook.vulkan_diagnostic` module. The VkResult-to-string formatting
// happens inside those functions (the module implementation imports
// `cookbook.vulkan_format`), so the macro expansion site does NOT need a
// VkResult formatter visible. Consumers who want to format VkResult in
// their own `std::format` / `std::println` calls should
// `import cookbook.vulkan_format;` separately.
//
// Same include-site constraints as `diagnostic/assert.hxx`: include at file
// scope of a non-module TU or inside a module purview, not inside a global
// module fragment.

#include <atomic>
#include <format>
#include <source_location>

#include <volk.h>

#include "diagnostic/assert.hxx"

import cookbook.vulkan_diagnostic;

#if VKGC_DO_CHECK
    #define VKGC_CHECK_VKSUCCESS(call)\
        do {\
            if (auto const result = (call); result != VK_SUCCESS) [[unlikely]]\
            {\
                ::vkgc::assert_detail::vk_fatal(result, #call, {}, std::source_location::current());\
                VKGC_BREAK_AND_FAIL();\
            }\
        } while (0)

    #define VKGC_CHECKF_VKSUCCESS(call, ...)\
        do {\
            if (auto const result = (call); result != VK_SUCCESS) [[unlikely]]\
            {\
                ::vkgc::assert_detail::vk_fatal(result, #call, std::format(__VA_ARGS__), std::source_location::current());\
                VKGC_BREAK_AND_FAIL();\
            }\
        } while (0)

    #define VKGC_VERIFY_VKSUCCESS(call)           VKGC_CHECK_VKSUCCESS(call)
    #define VKGC_VERIFYF_VKSUCCESS(call, ...)     VKGC_CHECKF_VKSUCCESS(call, __VA_ARGS__)
#else
    #define VKGC_CHECK_VKSUCCESS(call)            ((void)0)
    #define VKGC_CHECKF_VKSUCCESS(call, ...)      ((void)0)
    #define VKGC_VERIFY_VKSUCCESS(call)           ((void)(call))
    #define VKGC_VERIFYF_VKSUCCESS(call, ...)     ((void)(call))
#endif

#if VKGC_DO_ENSURE
    #define VKGC_ENSURE_VKSUCCESS(call)\
        ([&]() -> bool {\
            auto const result = (call);\
            if (result == VK_SUCCESS) [[likely]] return true;\
            static std::atomic_flag fired;\
            if (!fired.test_and_set(std::memory_order_relaxed)) [[unlikely]]\
            {\
                ::vkgc::assert_detail::vk_ensure_log(\
                    result, #call, {}, std::source_location::current());\
                VKGC_DEBUG_BREAK();\
            }\
            return false;\
        }())

    #define VKGC_ENSUREF_VKSUCCESS(call, ...)\
        ([&]() -> bool {\
            auto const result = (call);\
            if (result == VK_SUCCESS) [[likely]] return true;\
            static std::atomic_flag fired;\
            if (!fired.test_and_set(std::memory_order_relaxed)) [[unlikely]]\
            {\
                ::vkgc::assert_detail::vk_ensure_log(\
                    result, #call, std::format(__VA_ARGS__), std::source_location::current());\
                VKGC_DEBUG_BREAK();\
            }\
            return false;\
        }())
#else
    #define VKGC_ENSURE_VKSUCCESS(call)           ((call) == VK_SUCCESS)
    #define VKGC_ENSUREF_VKSUCCESS(call, ...)     ((call) == VK_SUCCESS)
#endif
