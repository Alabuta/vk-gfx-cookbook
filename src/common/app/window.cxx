module;

#include <print>
#include <string>
#include <utility>

#include <volk.h>
#include "vulkan/format.hxx"
#include <GLFW/glfw3.h>

#include <sigslot/signal.hpp>

#include "diagnostic/assert.hxx"
#include "vulkan/assert.hxx"

module vkgc.window;


namespace vkgc
{
    struct window::signals
    {
        sigslot::signal<std::uint32_t, std::uint32_t> resize;
    };

    struct window::slot_connection::connection_state
    {
        sigslot::scoped_connection connection;
    };

    window::slot_connection::slot_connection() noexcept = default;

    window::slot_connection::slot_connection(connection_state* const state) noexcept
        : state_{state}
    {}

    window::slot_connection::slot_connection(slot_connection&& other) noexcept
        : state_{std::exchange(other.state_, nullptr)}
    {}

    window::slot_connection& window::slot_connection::operator=(slot_connection&& other) noexcept
    {
        if (this != &other)
        {
            delete state_;
            state_ = std::exchange(other.state_, nullptr);
        }
        return *this;
    }

    window::slot_connection::~slot_connection()
    {
        delete state_;
    }

    void window::slot_connection::disconnect() noexcept
    {
        delete state_;
        state_ = nullptr;
    }

    window::window(std::string const& title, std::uint32_t const width, std::uint32_t const height) noexcept
        : size_{std::make_pair(width, height)},
          signals_{new signals{}}
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        handle_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.c_str(), nullptr, nullptr);
        if (!VKGC_ENSURE(handle_ != nullptr))
        {
            return;
        }

        glfwSetWindowUserPointer(handle_, this);

        glfwSetKeyCallback(
            handle_,
            [](GLFWwindow* handle, int const key, int, int const action, int)
            {
                if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
                {
                    glfwSetWindowShouldClose(handle, GLFW_TRUE);
                }
            });

        glfwSetFramebufferSizeCallback(handle_, [] (GLFWwindow* handle, int const new_width, int const new_height)
        {
            if (auto* instance = static_cast<window*>(glfwGetWindowUserPointer(handle)); instance != nullptr)
            {
                instance->handle_resize_event(
                    static_cast<std::uint32_t>(new_width),
                    static_cast<std::uint32_t>(new_height));
            }
        });
    }

    window::~window()
    {
        if (signals_ != nullptr)
        {
            signals_->resize.disconnect_all();
            delete signals_;
            signals_ = nullptr;
        }

        if (VKGC_ENSURE(handle_ != nullptr))
        {
            glfwDestroyWindow(handle_);
            handle_ = nullptr;
        }
    }

    GLFWwindow* window::handle() const noexcept
    {
        return handle_;
    }

    std::pair<std::uint32_t, std::uint32_t> window::get_size() const noexcept
    {
        return size_;
    }

    bool window::is_valid() const noexcept
    {
        return handle_ != nullptr;
    }

    bool window::should_close() const noexcept
    {
        return handle_ == nullptr || glfwWindowShouldClose(handle_) == GLFW_TRUE;
    }

    VkSurfaceKHR window::get_vulkan_surface(VkInstance instance) const noexcept
    {
        if (surface_ == VK_NULL_HANDLE)
        {
            if (!VKGC_ENSURE_VKSUCCESS(glfwCreateWindowSurface(instance, handle_, nullptr, &surface_)))
            {
                surface_ = VK_NULL_HANDLE;
                return VK_NULL_HANDLE;
            }
        }

        return surface_;
    }

    window::slot_connection window::connect_on_resize(resize_slot_t slot)
    {
        return slot_connection{new slot_connection::connection_state{signals_->resize.connect(std::move(slot))}};
    }

    void window::handle_resize_event(std::uint32_t const new_width, std::uint32_t const new_height)
    {
        if (size_.first == new_width && size_.second == new_height)
        {
            return;
        }

        size_ = std::make_pair(new_width, new_height);

        signals_->resize(new_width, new_height);
    }
}
