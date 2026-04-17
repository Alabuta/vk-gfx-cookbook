cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(vk_gfx_cookbook VERSION 1.0.0 LANGUAGES CXX)

# --- Third-party dependencies ---
include(cmake/Dependencies.cmake)

# --- Shared build configuration ---
include(cmake/CookbookConfig.cmake)

# --- Chapter sub-projects ---
add_subdirectory(chapters/chapter01/glfw)
add_subdirectory(chapters/chapter01/taskflow)
