module;

#include <volk.h>

module vkgc.vulkan_surface;

namespace vkgc
{
    vulkan_surface::vulkan_surface(VkInstance instance, VkSurfaceKHR handle) noexcept
        : instance_{instance}, handle_{handle}
    {
    }

    vulkan_surface::~vulkan_surface()
    {
        if (instance_ != VK_NULL_HANDLE && handle_ != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(instance_, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
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
