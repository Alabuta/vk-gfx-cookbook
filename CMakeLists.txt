cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(vk_gfx_cookbook VERSION 1.0.0 LANGUAGES CXX)

# --- Generated project headers ---
include(cmake/ProjectVersion.cmake)

# --- Third-party dependencies ---
include(cmake/Dependencies.cmake)

# --- Shared build configuration ---
include(cmake/ProjectConfig.cmake)

# --- Chapter sub-projects ---
add_subdirectory(src/chapter01/glfw)
add_subdirectory(src/chapter01/taskflow)
add_subdirectory(src/chapter01/glslang)
add_subdirectory(src/chapter02/swapchain)
