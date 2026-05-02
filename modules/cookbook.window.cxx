module;

#include <print>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

module cookbook.window;

namespace cookbook
{
    window::window(std::string const& title, int const width, int const height) noexcept
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        handle_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (handle_ == nullptr)
        {
            std::println("GLFW window creation failed");
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
    }

    window::~window()
    {
        if (handle_ != nullptr)
        {
            glfwDestroyWindow(handle_);
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
}
