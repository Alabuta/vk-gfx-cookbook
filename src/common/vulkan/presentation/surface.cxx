module;

#include <volk.h>

#include "vulkan/assert.hxx"

module vkgc.vulkan_surface;

namespace vkgc
{
    vulkan_surface::vulkan_surface(VkInstance instance, VkSurfaceKHR handle) noexcept
        : instance_{instance}, handle_{handle}
    {
    }

    vulkan_surface::~vulkan_surface()
    {
        if (!VKGC_ENSURE_VKHANDLE(instance_) || !VKGC_ENSURE_VKHANDLE(handle_))
        {
            return;
        }

        vkDestroySurfaceKHR(instance_, handle_, nullptr);
        handle_ = VK_NULL_HANDLE;
    }

    vulkan_surface::operator bool() const noexcept
    {
        return handle_ != VK_NULL_HANDLE;
    }

    VkSurfaceKHR vulkan_surface::handle() const noexcept
    {
        return handle_;
    }
}
