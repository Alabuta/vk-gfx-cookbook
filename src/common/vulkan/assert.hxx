#pragma once

// Vulkan-specific assert / verify / ensure macros for VkResult-returning
// calls and for non-null opaque handle checks.
//
// Gated by the same `VKGC_DO_CHECK` / `VKGC_DO_ENSURE` compile defines as
// the general macros in `diagnostic/assert.hxx`.

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

#define VKGC_VERIFY_VKSUCCESS(call)\
    do {\
        if (auto const result = (call); result != VK_SUCCESS) [[unlikely]]\
        {\
            ::vkgc::assert_detail::vk_fatal(result, #call, {}, std::source_location::current());\
            VKGC_BREAK_AND_FAIL();\
        }\
    } while (0)

#define VKGC_VERIFYF_VKSUCCESS(call, ...)\
    do {\
        if (auto const result = (call); result != VK_SUCCESS) [[unlikely]]\
        {\
            ::vkgc::assert_detail::vk_fatal(result, #call, std::format(__VA_ARGS__), std::source_location::current());\
            VKGC_BREAK_AND_FAIL();\
        }\
    } while (0)

#if VKGC_DO_CHECK
    #define VKGC_CHECK_VKSUCCESS(call)            VKGC_VERIFY_VKSUCCESS(call)
    #define VKGC_CHECKF_VKSUCCESS(call, ...)      VKGC_VERIFYF_VKSUCCESS(call, __VA_ARGS__)
#else
    #define VKGC_CHECK_VKSUCCESS(call)            ((void)0)
    #define VKGC_CHECKF_VKSUCCESS(call, ...)      ((void)0)
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

#define VKGC_VERIFY_VKHANDLE(handle)\
    do {\
        if ((handle) == VK_NULL_HANDLE) [[unlikely]]\
        {\
            ::vkgc::assert_detail::fatal_log("Vulkan", #handle " != VK_NULL_HANDLE", {}, std::source_location::current());\
            VKGC_BREAK_AND_FAIL();\
        }\
    } while (0)

#define VKGC_VERIFYF_VKHANDLE(handle, ...)\
    do {\
        if ((handle) == VK_NULL_HANDLE) [[unlikely]]\
        {\
            ::vkgc::assert_detail::fatal_log("Vulkan", #handle " != VK_NULL_HANDLE", std::format(__VA_ARGS__), std::source_location::current());\
            VKGC_BREAK_AND_FAIL();\
        }\
    } while (0)

#if VKGC_DO_CHECK
    #define VKGC_CHECK_VKHANDLE(handle)           VKGC_VERIFY_VKHANDLE(handle)
    #define VKGC_CHECKF_VKHANDLE(handle, ...)     VKGC_VERIFYF_VKHANDLE(handle, __VA_ARGS__)
#else
    #define VKGC_CHECK_VKHANDLE(handle)           ((void)0)
    #define VKGC_CHECKF_VKHANDLE(handle, ...)     ((void)0)
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
