module;

#include <format>
#include <source_location>
#include <string>
#include <string_view>

#include <volk.h>

module cookbook.vulkan_diagnostic;

import cookbook.diagnostic;
import cookbook.vulkan_format;

namespace vkgc::assert_detail
{
    namespace
    {
        std::string format_vk_message(VkResult const result, std::string_view const message)
        {
            return message.empty()
                ? std::format("[{}]", result)
                : std::format("[{}] {}", result, message);
        }
    }

    void vk_fatal(
        VkResult const result,
        std::string_view const expression,
        std::string_view const message,
        std::source_location const location) noexcept
    {
        fatal_log("Vulkan", expression, format_vk_message(result, message), location);
    }

    void vk_ensure_log(
        VkResult const result,
        std::string_view const expression,
        std::string_view const message,
        std::source_location const location) noexcept
    {
        ensure_log("Vulkan", expression, format_vk_message(result, message), location);
    }
}
