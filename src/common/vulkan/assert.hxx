#pragma once

// Vulkan-specific assert / verify / ensure macros for VkResult-returning
// calls and for non-null opaque handle checks.
//
// Macros:
//   VKGC_CHECK_VKSUCCESS(call)               // VK_SUCCESS check; abort on failure
//   VKGC_CHECKF_VKSUCCESS(call, fmt, ...)    // same + std::format message
//   VKGC_VERIFY_VKSUCCESS(call)              // alias for CHECK_VKSUCCESS
//   VKGC_VERIFYF_VKSUCCESS(call, fmt, ...)   // alias for CHECKF_VKSUCCESS
//   VKGC_ENSURE_VKSUCCESS(call)              // log-once on failure; returns bool
//   VKGC_ENSUREF_VKSUCCESS(call, fmt, ...)   // same + std::format message
//   VKGC_CHECK_VKHANDLE(handle)              // != VK_NULL_HANDLE; abort on failure
//   VKGC_CHECKF_VKHANDLE(handle, fmt, ...)   // same + std::format message
//   VKGC_VERIFY_VKHANDLE(handle)             // alias for CHECK_VKHANDLE
//   VKGC_VERIFYF_VKHANDLE(handle, fmt, ...)  // alias for CHECKF_VKHANDLE
//   VKGC_ENSURE_VKHANDLE(handle)             // log-once on failure; returns bool
//   VKGC_ENSUREF_VKHANDLE(handle, fmt, ...)  // same + std::format message
//
// Gated by the same `VKGC_DO_CHECK` / `VKGC_DO_ENSURE` compile defines as
// the general macros in `diagnostic/assert.hxx`.
//
// Header-only: `vk_fatal` / `vk_ensure_log` are defined inline below and
// delegate to `fatal_log` / `ensure_log` from `diagnostic/assert.hxx`
// after stringifying the VkResult. The VKHANDLE macros call the general
// `fatal_log` / `ensure_log` directly with the "Vulkan" category.
//
// Prerequisites — the including TU must already have visible:
//   - `<volk.h>` (for `VkResult` / `VK_SUCCESS` / `VK_NULL_HANDLE` and
//     the opaque handle typedefs)
//   - `"vulkan/format.hxx"` (only for the VKSUCCESS variants, which need
//     the `std::formatter<VkResult>` specialization)
// This header deliberately does NOT pull them in, so consumers have a
// single canonical place where the Vulkan and formatter headers enter
// the include graph. The conventional spot in a module implementation
// is the global module fragment:
//
//   module;
//   #include <volk.h>
//   #include "vulkan/format.hxx"
//   #include "vulkan/assert.hxx"
//   module vkgc.foo;
//   ...
//
// In a non-module TU, include them anywhere at file scope before any
// expansion of a VKGC_*_VKSUCCESS macro.

#include <format>
#include <source_location>
#include <string>
#include <string_view>

#include "diagnostic/assert.hxx"
#include "format.hxx"

namespace vkgc::assert_detail
{
    namespace detail
    {
        inline std::string format_vk_message(VkResult const result, std::string_view const message)
        {
            return message.empty()
                ? std::format("[{}]", result)
                : std::format("[{}] {}", result, message);
        }
    }

    inline void vk_fatal(
        VkResult const result,
        std::string_view const expression,
        std::string_view const message,
        std::source_location const location) noexcept
    {
        fatal_log("Vulkan", expression, detail::format_vk_message(result, message), location);
    }

    inline void vk_ensure_log(
        VkResult const result,
        std::string_view const expression,
        std::string_view const message,
        std::source_location const location) noexcept
    {
        ensure_log("Vulkan", expression, detail::format_vk_message(result, message), location);
    }
}

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

#if VKGC_DO_CHECK
    #define VKGC_CHECK_VKHANDLE(handle)\
        do {\
            if ((handle) == VK_NULL_HANDLE) [[unlikely]]\
            {\
                ::vkgc::assert_detail::fatal_log("Vulkan", #handle " != VK_NULL_HANDLE", {}, std::source_location::current());\
                VKGC_BREAK_AND_FAIL();\
            }\
        } while (0)

    #define VKGC_CHECKF_VKHANDLE(handle, ...)\
        do {\
            if ((handle) == VK_NULL_HANDLE) [[unlikely]]\
            {\
                ::vkgc::assert_detail::fatal_log("Vulkan", #handle " != VK_NULL_HANDLE", std::format(__VA_ARGS__), std::source_location::current());\
                VKGC_BREAK_AND_FAIL();\
            }\
        } while (0)

    #define VKGC_VERIFY_VKHANDLE(handle)          VKGC_CHECK_VKHANDLE(handle)
    #define VKGC_VERIFYF_VKHANDLE(handle, ...)    VKGC_CHECKF_VKHANDLE(handle, __VA_ARGS__)
#else
    #define VKGC_CHECK_VKHANDLE(handle)           ((void)0)
    #define VKGC_CHECKF_VKHANDLE(handle, ...)     ((void)0)
    #define VKGC_VERIFY_VKHANDLE(handle)          ((void)(handle))
    #define VKGC_VERIFYF_VKHANDLE(handle, ...)    ((void)(handle))
#endif

#if VKGC_DO_ENSURE
    #define VKGC_ENSURE_VKHANDLE(handle)\
        ([&]() -> bool {\
            if ((handle) != VK_NULL_HANDLE) [[likely]] return true;\
            static std::atomic_flag fired;\
            if (!fired.test_and_set(std::memory_order_relaxed)) [[unlikely]]\
            {\
                ::vkgc::assert_detail::ensure_log(\
                    "Vulkan", #handle " != VK_NULL_HANDLE", {}, std::source_location::current());\
                VKGC_DEBUG_BREAK();\
            }\
            return false;\
        }())

    #define VKGC_ENSUREF_VKHANDLE(handle, ...)\
        ([&]() -> bool {\
            if ((handle) != VK_NULL_HANDLE) [[likely]] return true;\
            static std::atomic_flag fired;\
            if (!fired.test_and_set(std::memory_order_relaxed)) [[unlikely]]\
            {\
                ::vkgc::assert_detail::ensure_log(\
                    "Vulkan", #handle " != VK_NULL_HANDLE", std::format(__VA_ARGS__), std::source_location::current());\
                VKGC_DEBUG_BREAK();\
            }\
            return false;\
        }())
#else
    #define VKGC_ENSURE_VKHANDLE(handle)          ((handle) != VK_NULL_HANDLE)
    #define VKGC_ENSUREF_VKHANDLE(handle, ...)    ((handle) != VK_NULL_HANDLE)
#endif
