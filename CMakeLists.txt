cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(vk_gfx_cookbook VERSION 1.0.0 LANGUAGES CXX)

# --- Single-config build-type default ---
# Multi-config generators (VS, Xcode) drive CMAKE_CONFIGURATION_TYPES; on Ninja/Makefiles an
# unset CMAKE_BUILD_TYPE makes $<CONFIG> evaluate to "", which silently zeroes VKGC_DO_CHECK /
# VKGC_DEBUG_VULKAN and produces a stealth Release build with no debug checks. Presets in
# CMakePresets.json always pin a build type, so this guard only fires on bypass-presets configures.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
        "Build type (Debug/Release/RelWithDebInfo/MinSizeRel)" FORCE)
    message(STATUS "No CMAKE_BUILD_TYPE specified — defaulting to Debug")
endif()

# --- User-facing options ---

option(VKGC_ENABLE_ASAN
    "Enable AddressSanitizer (compile + link flags via vkgc::sanitizers::address)" OFF)

option(VKGC_ENABLE_UBSAN
    "Enable UndefinedBehaviorSanitizer (compile + link flags via vkgc::sanitizers::undefined)" OFF)

# --- Early toolchain-shaping (must precede Dependencies.cmake / target creation) ---
include(cmake/ToolchainShaping.cmake)

# --- Non-transitive target-property defaults ---
# Properties that don't propagate through INTERFACE libraries — set as initializer variables so
# chapter executables (and FetchContent libs, where they're inert) inherit them without boilerplate.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VS_DEBUGGER_WORKING_DIRECTORY "$<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}>")
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)
set(CMAKE_XCODE_SCHEME_WORKING_DIRECTORY "$<$<PLATFORM_ID:Darwin>:${CMAKE_SOURCE_DIR}>")

# --- Generated project headers ---
include(cmake/ProjectVersion.cmake)

# --- Compiler-family dispatch (IS_* generator expressions) ---
include(cmake/CompilerDispatch.cmake)

# --- Third-party dependencies ---
include(cmake/Dependencies.cmake)

# --- Composable policy + interface targets ---
include(cmake/Policies.cmake)
include(cmake/Interfaces.cmake)

# --- Common sources helper (transitional; Phase B will replace with a vkgc::common library) ---
include(cmake/ProjectConfig.cmake)

# --- Build-time shader compilation (vkgc_compile_shaders) ---
include(cmake/Shaders.cmake)

# --- Chapter sub-projects ---
add_subdirectory(src/chapter01/glfw)
add_subdirectory(src/chapter01/taskflow)
add_subdirectory(src/chapter01/glslang)
add_subdirectory(src/chapter02/swapchain)
add_subdirectory(src/chapter02/hello_triangle)
add_subdirectory(src/chapter02/glm)

# --- Common-code tests ---
add_subdirectory(src/common/math/test)
