cmake_minimum_required(VERSION 4.0 FATAL_ERROR)

project(vk_gfx_cookbook VERSION 1.0.0 LANGUAGES CXX)

# --- Compiler family detection ---
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT MINGW)
    set(CXX_FLAGS_STYLE_GNU TRUE)
elseif(MINGW)
    set(CXX_FLAGS_STYLE_MINGW_WINDOWS TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND MSVC)
    set(CXX_FLAGS_STYLE_CLANGCL TRUE)
elseif(MSVC)
    set(CXX_FLAGS_STYLE_MSVC TRUE)
endif()

# --- Third-party dependencies ---
include(cmake/Dependencies.cmake)

# --- Shared build configuration ---
include(cmake/CookbookConfig.cmake)

# --- Chapter sub-projects ---
add_subdirectory(chapters/chapter01/glfw)
add_subdirectory(chapters/chapter01/taskflow)
