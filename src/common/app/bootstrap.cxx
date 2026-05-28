module;

#include <functional>
#include <print>

#include <volk.h>

#include <GLFW/glfw3.h>

#include "diagnostic/assert.hxx"
#include "vulkan/assert.hxx"

#ifdef _CPPUNWIND
  #error exceptions are on
#endif
#ifdef __EXCEPTIONS
  #error exceptions are on
#endif
#ifdef _HAS_EXCEPTIONS
    #if _HAS_EXCEPTIONS != 0
      #error exceptions are on
    #endif
#endif


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

    void update_app(std::function<bool()> const& callback)
    {
        bool continue_app_run{true};
        while (continue_app_run)
        {
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
