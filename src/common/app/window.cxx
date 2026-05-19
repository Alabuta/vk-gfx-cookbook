module;

#include <print>
#include <string>

#include <volk.h>
#include "vulkan/format.hxx"
#include <GLFW/glfw3.h>

#include "diagnostic/assert.hxx"
#include "vulkan/assert.hxx"

module vkgc.window;


namespace vkgc
{
    window::window(std::string const& title, uint32_t const width, uint32_t const height) noexcept
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        handle_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.c_str(), nullptr, nullptr);
        if (!VKGC_ENSURE(handle_ != nullptr))
        {
            return;
        }

        glfwSetKeyCallback(
            handle_,
            [](GLFWwindow* w, int const key, int, int const action, int)
            {
                if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
                {
                    glfwSetWindowShouldClose(w, GLFW_TRUE);
                }
            });

        glfwSetWindowUserPointer(handle_, this);
    }

    window::~window()
    {
        if (VKGC_ENSURE(handle_ != nullptr))
        {
            glfwDestroyWindow(handle_);
            handle_ = nullptr;
        }
    }

    bool window::is_valid() const noexcept
    {
        return handle_ != nullptr;
    }

    GLFWwindow* window::handle() const noexcept
    {
        return handle_;
    }

    bool window::should_close() const noexcept
    {
        return handle_ == nullptr || glfwWindowShouldClose(handle_) == GLFW_TRUE;
    }

    VkSurfaceKHR window::create_vulkan_surface(VkInstance instance) const noexcept
    {
        VkSurfaceKHR surface{VK_NULL_HANDLE};
        if (!VKGC_ENSURE_VKSUCCESS(glfwCreateWindowSurface(instance, handle_, nullptr, &surface)))
        {
            return VK_NULL_HANDLE;
        }

        return surface;
    }
}
