module;

#include <cstdio>
#include <format>
#include <print>
#include <source_location>
#include <string>
#include <string_view>

module cookbook.diagnostic;

namespace vkgc::assert_detail
{
    namespace
    {
        void emit(
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

    void fatal_log(
        std::string_view const log_category,
        std::string_view const expression,
        std::string_view const message,
        std::source_location const location) noexcept
    {
        emit("Fatal", log_category, expression, message, location);
    }

    void ensure_log(
        std::string_view const log_category,
        std::string_view const expression,
        std::string_view const message,
        std::source_location const location) noexcept
    {
        emit("Ensure", log_category, expression, message, location);
    }
}
