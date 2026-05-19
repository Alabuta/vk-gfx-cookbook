module;

#include <functional>
#include <print>

#include <volk.h>

#include <GLFW/glfw3.h>

#include "diagnostic/assert.hxx"
#include "vulkan/assert.hxx"

module vkgc.bootstrap;

namespace vkgc
{
    void bootstrap_app()
    {
        glfwSetErrorCallback(
            [](int const error, char const* description)
            {
                std::println("GLFW Error ({:#06x}): {}", error, description);
            });

        VKGC_VERIFYF(glfwInit() == GLFW_TRUE, "GLFW initialization failed");
        VKGC_VERIFYF_VKSUCCESS(volkInitialize(), "'volk' meta-loader initialization failed");
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
