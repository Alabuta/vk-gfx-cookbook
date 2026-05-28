# Dependency and runtime-config INTERFACE targets. Each wraps an upstream library (or a set of
# closely-coupled ones) plus the defines/includes its consumers need. Chapters link only the subsets
# they use, so a chapter that doesn't need (e.g.) the shader compiler doesn't pay for it.
#
# Compiler-family dispatch via IS_* aliases defined in cmake/CompilerDispatch.cmake.

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(CheckIncludeFile)

    check_include_file("wayland-client.h" VKGC_HAVE_WAYLAND)
    check_include_file("xcb/xcb.h" VKGC_HAVE_XCB)

    if (NOT VKGC_HAVE_WAYLAND AND NOT VKGC_HAVE_XCB)
        message(FATAL_ERROR
                "No Vulkan WSI backend found on Linux. "
                "Install libwayland-dev (Wayland) or libxcb1-dev (X11/XCB).")
    endif ()
endif ()

# === vkgc::dependencies::vulkan ===
# volk dynamic loader + VMA allocator. Vulkan::Headers is excluded: volk ships its own vulkan.h, and
# mixing both multiply-defines the VK prototypes; VK_NO_PROTOTYPES avoids that conflict.
# VK_USE_PLATFORM_* lives here (not a platform target) because the Vulkan SDK headers consume it —
# forgetting it breaks Vulkan, not "platform" abstractly.

add_library(vkgc_dependencies_vulkan INTERFACE)
add_library(vkgc::dependencies::vulkan ALIAS vkgc_dependencies_vulkan)

target_link_libraries(vkgc_dependencies_vulkan
    INTERFACE
        volk::volk
        GPUOpen::VulkanMemoryAllocator
)

target_compile_definitions(vkgc_dependencies_vulkan
    INTERFACE
        VK_NO_PROTOTYPES

        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1

        "$<$<AND:$<PLATFORM_ID:Linux>,$<BOOL:${VKGC_HAVE_WAYLAND}>>:"
            VK_USE_PLATFORM_WAYLAND_KHR
        ">"
        "$<$<AND:$<PLATFORM_ID:Linux>,$<BOOL:${VKGC_HAVE_XCB}>>:"
            VK_USE_PLATFORM_XCB_KHR
        ">"

        # Darwin: VK_EXT_metal_surface is the current path (used by GLFW on macOS).
        # VK_MVK_macos_surface is the deprecated MoltenVK-specific extension; not defined.
        "$<$<PLATFORM_ID:Darwin>:"
            VK_USE_PLATFORM_METAL_EXT
        ">"

        "$<$<PLATFORM_ID:Windows>:"
            VK_USE_PLATFORM_WIN32_KHR
        ">"
)


# === vkgc::dependencies::windowing ===
# GLFW window/input/surface management. GLFW_INCLUDE_NONE stops the GLFW header pulling in any
# OpenGL/ES header. GLFW_EXPOSE_NATIVE_WIN32 + GLFW_NATIVE_INCLUDE_NONE expose the native handles
# we need for Win32 surface creation without dragging in <windows.h> via GLFW.

add_library(vkgc_dependencies_windowing INTERFACE)
add_library(vkgc::dependencies::windowing ALIAS vkgc_dependencies_windowing)

target_link_libraries(vkgc_dependencies_windowing
    INTERFACE
        glfw
)

target_compile_definitions(vkgc_dependencies_windowing
    INTERFACE
        GLFW_INCLUDE_NONE

        "$<$<AND:$<PLATFORM_ID:Linux>,$<BOOL:${VKGC_HAVE_WAYLAND}>>:"
            GLFW_EXPOSE_NATIVE_WAYLAND
        ">"
        "$<$<AND:$<PLATFORM_ID:Linux>,$<BOOL:${VKGC_HAVE_XCB}>>:"
            GLFW_EXPOSE_NATIVE_X11
        ">"

        "$<$<PLATFORM_ID:Windows>:"
            GLFW_EXPOSE_NATIVE_WIN32
            GLFW_NATIVE_INCLUDE_NONE
        ">"
)


# === vkgc::dependencies::math ===
# GLM header-only linear algebra.

add_library(vkgc_dependencies_math INTERFACE)
add_library(vkgc::dependencies::math ALIAS vkgc_dependencies_math)

target_link_libraries(vkgc_dependencies_math
    INTERFACE
        glm::glm
)


# === vkgc::dependencies::concurrency ===
# Signal/slot library (Pal::Sigslot) and task-graph runtime (Taskflow). Grouped as concurrency
# primitives — chapters that use one tend to need the other.

add_library(vkgc_dependencies_concurrency INTERFACE)
add_library(vkgc::dependencies::concurrency ALIAS vkgc_dependencies_concurrency)

target_link_libraries(vkgc_dependencies_concurrency
    INTERFACE
        Pal::Sigslot
        Taskflow::Taskflow
)


# === vkgc::dependencies::shaders ===
# glslang's GLSL→SPIR-V compiler stack; the three targets link together: glslang is the
# parser/frontend, SPIRV emits SPIR-V from its AST, and glslang-default-resource-limits provides
# the default TBuiltInResource for parsing.

add_library(vkgc_dependencies_shaders INTERFACE)
add_library(vkgc::dependencies::shaders ALIAS vkgc_dependencies_shaders)

target_link_libraries(vkgc_dependencies_shaders
    INTERFACE
        SPIRV
        glslang
        glslang-default-resource-limits
)


# === vkgc::dependencies::stdcxx_extras ===
# libstdc++ filesystem and experimental-TS link libs. These archives exist only for GNU libstdc++, so
# gate on the active standard library, not the frontend: an IS_CLANG_MSYS toolchain may be MSYS2
# ucrt64-clang (libstdc++) or clang64 (libc++), and under libc++ they don't exist ("unable to find
# library -lstdc++fs/exp"). Detect libstdc++ via __GLIBCXX__; inert on libc++ and MSVC-STL
# (clang-cl/MSVC), where <print>/<filesystem> live in the main runtime.
#   * stdc++fs is a no-op since GCC 9 (filesystem in main libstdc++) — kept for older toolchains.
#   * stdc++exp carries the C++23 <print>/<stacktrace> runtime. Whether those symbols sit in the main
#     library or in exp varies by version/distro, so probe by linking std::println and add stdc++exp
#     only when the default link fails. Auto-corrects if a future libstdc++ moves them to the main
#     library (where adding -lstdc++exp would duplicate symbols).
#
# Both probes force -std=c++23 so <version>/std::println resolve under the project's standard, not the
# compiler's lower default; gated to GNU-driver compilers where -std=c++23 is valid (clang-cl/MSVC use
# /std: and never have __GLIBCXX__).

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
       AND "x${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "xGNU"))
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS_SAVED "${CMAKE_REQUIRED_FLAGS}")
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++23")
    check_cxx_source_compiles([[
        #include <version>
        #ifndef __GLIBCXX__
        #error not libstdc++
        #endif
        int main() {}
    ]] VKGC_USING_LIBSTDCXX)
    if(VKGC_USING_LIBSTDCXX)
        check_cxx_source_compiles([[
            #include <print>
            int main() { std::println("probe"); }
        ]] VKGC_LIBSTDCXX_PRINT_SELF_CONTAINED)
    endif()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS_SAVED}")
    unset(CMAKE_REQUIRED_FLAGS_SAVED)
endif()

add_library(vkgc_dependencies_stdcxx_extras INTERFACE)
add_library(vkgc::dependencies::stdcxx_extras ALIAS vkgc_dependencies_stdcxx_extras)

target_link_libraries(vkgc_dependencies_stdcxx_extras
    INTERFACE
        "$<$<BOOL:${VKGC_USING_LIBSTDCXX}>:stdc++fs>"
        "$<$<AND:$<BOOL:${VKGC_USING_LIBSTDCXX}>,$<NOT:$<BOOL:${VKGC_LIBSTDCXX_PRINT_SELF_CONTAINED}>>>:stdc++exp>"
)


# === vkgc::config::cookbook_paths ===
# Source-tree paths as string-literal compile defines so chapter binaries resolve
# shader/cache/loader-settings files relative to the repo root rather than the cwd.
# Also propagates the build-tree include directory holding generated headers (config.hxx
# from cmake/ProjectVersion.cmake) so chapters can `#include "config.hxx"` uniformly.

add_library(vkgc_config_cookbook_paths INTERFACE)
add_library(vkgc::config::cookbook_paths ALIAS vkgc_config_cookbook_paths)

target_include_directories(vkgc_config_cookbook_paths
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/include
)

target_compile_definitions(vkgc_config_cookbook_paths
    INTERFACE
        COOKBOOK_SHADER_DIR_STRING="${CMAKE_SOURCE_DIR}/shaders/"
        COOKBOOK_CACHE_DIR_STRING="${CMAKE_SOURCE_DIR}/.cache/"
        COOKBOOK_LOADER_SETTINGS_FILE_STRING="${CMAKE_SOURCE_DIR}/vk_loader_settings.json"
)
