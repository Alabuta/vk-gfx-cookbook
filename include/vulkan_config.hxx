#pragma once

#include <volk.h>

#include "config.hxx"


namespace cfg
{
    VkApplicationInfo constexpr kApplicationInfo{
        .sType{VK_STRUCTURE_TYPE_APPLICATION_INFO},
        .pNext{nullptr},
        .pApplicationName{"vkcookbook"},
        .applicationVersion{VK_MAKE_VERSION(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH)},
        .pEngineName{nullptr},
        .engineVersion{0},
        .apiVersion{VK_API_VERSION_1_4}
    };
}
