module;

#include <atomic>

#include "diagnostic/assert.hxx"

module vkgc.vulkan_context;

namespace
{
    // volk's instance/device dispatch tables are process-global, so at most one
    // device may be active per process. The guard turns a second live context
    // into a loud failure instead of silent table clobbering.
    std::atomic g_context_alive{false};
}

namespace vkgc
{
    vulkan_context::vulkan_context(vulkan_context_info const& info) noexcept
        : instance_{info.instance},
          device_{instance_.create_device(info.device)}
    {
        VKGC_CHECKF(!g_context_alive.exchange(true), "only one vulkan_context may be alive at a time");
    }

    vulkan_context::~vulkan_context()
    {
        g_context_alive.store(false);
    }

    bool vulkan_context::is_valid() const noexcept
    {
        return instance_.is_valid() && device_.is_valid();
    }

    vulkan_instance const& vulkan_context::instance() const noexcept
    {
        return instance_;
    }

    vulkan_device const& vulkan_context::device() const noexcept
    {
        return device_;
    }
}
