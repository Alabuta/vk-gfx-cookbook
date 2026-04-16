#include <tuple>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

int main()
{
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
