#pragma once

// General-purpose assert / verify / ensure macros.
//
// Macros:
//   VKGC_CHECK(expr)               // truthiness check; abort on failure
//   VKGC_CHECKF(expr, fmt, ...)    // same + std::format message
//   VKGC_VERIFY(expr)              // alias for CHECK
//   VKGC_VERIFYF(expr, fmt, ...)   // alias for CHECKF
//   VKGC_ENSURE(expr)              // log-once on failure; returns bool
//   VKGC_ENSUREF(expr, fmt, ...)   // same + std::format message
//   VKGC_DEBUG_BREAK()             // platform breakpoint trap
//   VKGC_BREAK_AND_FAIL()          // debug-break then std::abort()
//
// Compile-time gates (set in cmake/ProjectConfig.cmake):
//   VKGC_DO_CHECK   = 1  ->  CHECK/CHECKF/VERIFY/VERIFYF active; 0 strips them
//   VKGC_DO_ENSURE  = 1  ->  ENSURE/ENSUREF active; 0 reduces them to bool cast
//
// Header-only: `fatal_log` / `ensure_log` are inline below; no companion
// module. A module would force consumers to juggle the [module.import]/9
// contiguity rule — all `import`s must form one block right after the
// `module X;` declaration, and the stdlib `#include`s this header pulls
// in would break that block.
//
// Include this header anywhere at file scope of a non-module TU, or from
// the global module fragment of a module unit. The macros and helpers
// have no external prerequisites beyond what's `#include`d below.

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <print>
#include <source_location>
#include <string>
#include <string_view>

namespace vkgc::assert_detail
{
    namespace detail
    {
        inline void emit(
            std::string_view const severity,
            std::string_view const log_category,
            std::string_view const expression,
            std::string_view const message,
            std::source_location const location) noexcept
        {
            std::string out = std::format(
                "[{}] : {} : {}:{} ({}) : `{}`",
                log_category,
                severity,
                location.file_name(),
                location.line(),
                location.function_name(),
                expression);

            if (!message.empty())
            {
                out = std::format("{} -- {}", out, message);
            }

            std::println(stderr, "{}", out);
            std::fflush(stderr);
        }
    }

    inline void fatal_log(
        std::string_view const log_category,
        std::string_view const expression,
        std::string_view const message,
        std::source_location const location) noexcept
    {
        detail::emit("Fatal", log_category, expression, message, location);
    }

    inline void ensure_log(
        std::string_view const log_category,
        std::string_view const expression,
        std::string_view const message,
        std::source_location const location) noexcept
    {
        detail::emit("Ensure", log_category, expression, message, location);
    }
}

#ifndef __has_builtin
    #define __has_builtin(x) 0
#endif

#if defined(__cpp_lib_debugging)
    import <debugging>;
    #define VKGC_DEBUG_BREAK() std::breakpoint()
#elif __has_builtin(__builtin_debugtrap)
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
