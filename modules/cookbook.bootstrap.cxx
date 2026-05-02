module;

#include <functional>
#include <print>

#include <volk.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

module cookbook.bootstrap;

namespace cookbook
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

    void tick_app(std::function<void()>&& callback)
    {
        glfwPollEvents();

        if (callback)
        {
            callback();
        }
    }

    void terminate_app()
    {
        glfwTerminate();
    }
}
