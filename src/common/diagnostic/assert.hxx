#pragma once

// General-purpose assert / verify / ensure macros.
//
// Macros forward to `vkgc::assert_detail::fatal_log` /
// `vkgc::assert_detail::ensure_log_once`, which are exported by the
// `cookbook.diagnostic` module — imported below.
//
// Include this header at file scope of a non-module TU, or inside the
// purview of a module unit. Do NOT include from a module's global module
// fragment (the embedded `import` declaration is not allowed there).

#include <atomic>
#include <cstdlib>
#include <format>
#include <source_location>

import cookbook.diagnostic;

#if defined(__cpp_lib_debugging)
    import <debugging>;
    #define VKGC_DEBUG_BREAK() std::breakpoint()
#elif defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
    #define VKGC_DEBUG_BREAK() __builtin_debugtrap()
#elif defined(_MSC_VER)
    #define VKGC_DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
    #if defined(__aarch64__)
        #define VKGC_DEBUG_BREAK() __builtin_trap()
    #elif defined(__i386__) || defined(__x86_64__)
        #define VKGC_DEBUG_BREAK() (__extension__ ({ __asm__ volatile("int $0x03"); }))
    #else
        #define VKGC_DEBUG_BREAK() __builtin_trap()
    #endif
#else
    #error Unsupported compiler
#endif

#define VKGC_BREAK_AND_FAIL() do { VKGC_DEBUG_BREAK(); std::abort(); } while (0)

#if VKGC_DO_CHECK
    #define VKGC_CHECK(expr)\
        do {\
            if (!(expr)) [[unlikely]]\
            {\
                ::vkgc::assert_detail::fatal_log("General", #expr, {}, std::source_location::current());\
                VKGC_BREAK_AND_FAIL();\
            }\
        } while (0)

    #define VKGC_CHECKF(expr, ...)\
        do {\
            if (!(expr)) [[unlikely]]\
            {\
                ::vkgc::assert_detail::fatal_log("General", #expr, std::format(__VA_ARGS__), std::source_location::current());\
                VKGC_BREAK_AND_FAIL();\
            }\
        } while (0)

    #define VKGC_VERIFY(expr)               VKGC_CHECK(expr)
    #define VKGC_VERIFYF(expr, ...)         VKGC_CHECKF(expr, __VA_ARGS__)
#else
    #define VKGC_CHECK(expr)                ((void)0)
    #define VKGC_CHECKF(expr, ...)          ((void)0)
    #define VKGC_VERIFY(expr)               ((void)(expr))
    #define VKGC_VERIFYF(expr, ...)         ((void)(expr))
#endif

#if VKGC_DO_ENSURE
    #define VKGC_ENSURE(expr)\
        ((expr)\
         || ([&] {\
                static std::atomic_flag fired;\
                if (fired.test_and_set(std::memory_order_relaxed)) [[likely]] return false;\
                ::vkgc::assert_detail::ensure_log("General", #expr, {}, std::source_location::current());\
                return true;\
            }() ? (VKGC_DEBUG_BREAK(), false) : false))

    #define VKGC_ENSUREF(expr, ...)\
        ((expr)\
         || ([&] {\
                static std::atomic_flag fired;\
                if (fired.test_and_set(std::memory_order_relaxed)) [[likely]] return false;\
                ::vkgc::assert_detail::ensure_log("General", #expr, std::format(__VA_ARGS__), std::source_location::current());\
                return true;\
            }() ? (VKGC_DEBUG_BREAK(), false) : false))
#else
    #define VKGC_ENSURE(expr)               (static_cast<bool>(expr))
    #define VKGC_ENSUREF(expr, ...)         (static_cast<bool>(expr))
#endif
