#include <print>
#include <tuple>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#  define GLFW_EXPOSE_NATIVE_WIN32
#  define GLFW_EXPOSE_NATIVE_WGL
#elif __APPLE__
#  define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#else
#  error Unsupported OS
#endif

#include <GLFW/glfw3native.h>

int main()
{
    glfwSetErrorCallback(
        [](int error, const char* description)
        {
            std::println("GLFW Error ({0}): {1}", error, description);
        });

    if (const auto result = glfwInit(); result != GLFW_TRUE)
    {
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    const auto [w, h] = std::tuple{1280, 800};

    auto* window = glfwCreateWindow(w, h, "GLFW example", nullptr, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return -1;
    }

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int, int action, int)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    });

    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
