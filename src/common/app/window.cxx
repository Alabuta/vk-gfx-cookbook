module;

#include <print>
#include <string>

#include <volk.h>
#include <GLFW/glfw3.h>

module cookbook.window;

import cookbook.vulkan_format;

namespace vkgc
{
    window::window(std::string const& title, uint32_t const width, uint32_t const height) noexcept
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        handle_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.c_str(), nullptr, nullptr);
        if (handle_ == nullptr)
        {
            std::println(stderr, "[GLFW] : Fatal : failed to create window");
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
        if (handle_ != nullptr)
        {
            glfwDestroyWindow(handle_);
            handle_ = nullptr;
        }
    }

    window::operator bool() const noexcept
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

        if (auto const result = glfwCreateWindowSurface(instance, handle_, nullptr, &surface);
            result != VK_SUCCESS)
        {
            std::println(stderr, "[GLFW] : Fatal : failed to create Vulkan window surface ({})", result);
            return VK_NULL_HANDLE;
        }

        return surface;
    }
}
