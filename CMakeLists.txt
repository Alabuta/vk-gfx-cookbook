cmake_minimum_required(VERSION 4.0 FATAL_ERROR)
include(FetchContent)

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

# === GLM === (header-only)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.2
        SYSTEM
)

# === GLFW ===
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        SYSTEM
)

# === Taskflow === (header-only, parallel task programming)
set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        taskflow
        GIT_REPOSITORY git@github.com:taskflow/taskflow.git
        GIT_TAG v4.0.0
        SYSTEM
)

FetchContent_MakeAvailable(glm glfw taskflow)

# --- Shared build configuration ---
include(cmake/CookbookConfig.cmake)

# --- Chapter sub-projects ---
add_subdirectory(chapters/chapter01/glfw)
add_subdirectory(chapters/chapter01/taskflow)
