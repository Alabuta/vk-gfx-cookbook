module;

#include <functional>
#include <print>

#include <volk.h>

#include <GLFW/glfw3.h>

module vkgc.bootstrap;

namespace vkgc
{
    bool bootstrap_app()
    {
        glfwSetErrorCallback(
            [](int const error, char const* description)
            {
                std::println("GLFW Error ({:#06x}): {}", error, description);
            });

        if (glfwInit() != GLFW_TRUE)
        {
            std::println("GLFW initialization failed");
            return false;
        }

        if (volkInitialize() != VK_SUCCESS)
        {
            std::println("'volk' meta-loader initialization failed");
            return false;
        }

        return true;
    }

    void run_app(std::function<bool()> const& callback)
    {
        bool continue_app_run{true};
        while (continue_app_run)
        {
            glfwPollEvents();

            if (callback)
            {
                continue_app_run = callback();
            }
            else
            {
                continue_app_run = false;
            }
        }
    }

    void terminate_app()
    {
        glfwTerminate();
    }
}
