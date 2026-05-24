# Dependency and runtime-config INTERFACE targets. Each wraps an upstream library (or set of
# closely-coupled libraries) along with the defines/includes its consumers need. Chapters link
# only the subsets they actually use, so future chapters that don't need (e.g.) the shader
# compiler don't pay for it.
#
# Compiler-family dispatch via IS_* aliases defined in cmake/CompilerDispatch.cmake.


# === vkgc::dependencies::vulkan ===
# volk dynamic loader + VMA allocator. Vulkan::Headers deliberately excluded: volk provides its
# own vulkan.h; mixing both would multiply-define VK function prototypes.
# VK_NO_PROTOTYPES avoids symbolic conflicts between volk.h and vulkan/vulkan.h.
# VK_USE_PLATFORM_* lives here (not in a platform target) because it's the Vulkan SDK headers
# that consume it — forgetting it breaks Vulkan, not "platform" abstractly.

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

        "$<$<PLATFORM_ID:Linux>:"
            VK_USE_PLATFORM_WAYLAND_KHR
            VK_USE_PLATFORM_XCB_KHR
        ">"

        "$<$<PLATFORM_ID:Darwin>:"
            VK_USE_PLATFORM_MACOS_MVK
        ">"

        "$<$<PLATFORM_ID:Windows>:"
            VK_USE_PLATFORM_WIN32_KHR
        ">"
)


# === vkgc::dependencies::windowing ===
# GLFW window/input/surface management. GLFW_INCLUDE_NONE prevents the GLFW header from pulling
# in any OpenGL/ES API header. GLFW_EXPOSE_NATIVE_WIN32 + GLFW_NATIVE_INCLUDE_NONE enable the
# native-handle access we need for Win32 surface creation without including <windows.h> via GLFW.

add_library(vkgc_dependencies_windowing INTERFACE)
add_library(vkgc::dependencies::windowing ALIAS vkgc_dependencies_windowing)

target_link_libraries(vkgc_dependencies_windowing
    INTERFACE
        glfw
)

target_compile_definitions(vkgc_dependencies_windowing
    INTERFACE
        GLFW_INCLUDE_NONE

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
# Signal/slot library (Pal::Sigslot) and task-graph runtime (Taskflow). Grouped because both
# are concurrency primitives and chapters that use one tend to need the other.

add_library(vkgc_dependencies_concurrency INTERFACE)
add_library(vkgc::dependencies::concurrency ALIAS vkgc_dependencies_concurrency)

target_link_libraries(vkgc_dependencies_concurrency
    INTERFACE
        Pal::Sigslot
        Taskflow::Taskflow
)


# === vkgc::dependencies::shaders ===
# glslang's GLSL→SPIR-V compiler stack. The three targets must be linked together: SPIRV
# emits SPIR-V from glslang's AST, glslang is the parser/frontend, and
# glslang-default-resource-limits provides the default TBuiltInResource for parsing.

add_library(vkgc_dependencies_shaders INTERFACE)
add_library(vkgc::dependencies::shaders ALIAS vkgc_dependencies_shaders)

target_link_libraries(vkgc_dependencies_shaders
    INTERFACE
        SPIRV
        glslang
        glslang-default-resource-limits
)


# === vkgc::dependencies::stdcxx_extras ===
# libstdc++ filesystem and experimental TS link libs. GCC and clang-with-libstdc++ need these
# linked explicitly for std::filesystem and certain <experimental/*> components. Inert on
# MSVC-STL toolchains (clang-cl + MSVC).

add_library(vkgc_dependencies_stdcxx_extras INTERFACE)
add_library(vkgc::dependencies::stdcxx_extras ALIAS vkgc_dependencies_stdcxx_extras)

target_link_libraries(vkgc_dependencies_stdcxx_extras
    INTERFACE
        "$<$<OR:${IS_GNU_LINUX},${IS_MINGW},${IS_CLANG_MSYS}>:"
            stdc++fs
            stdc++exp
        ">"
)


# === vkgc::config::cookbook_paths ===
# Source-tree paths exposed as string-literal compile defines so the chapter binaries can
# resolve shader/cache/loader-settings files relative to the repo root rather than the cwd.

add_library(vkgc_config_cookbook_paths INTERFACE)
add_library(vkgc::config::cookbook_paths ALIAS vkgc_config_cookbook_paths)

target_compile_definitions(vkgc_config_cookbook_paths
    INTERFACE
        COOKBOOK_SHADER_DIR_STRING="${CMAKE_SOURCE_DIR}/shaders/"
        COOKBOOK_CACHE_DIR_STRING="${CMAKE_SOURCE_DIR}/.cache/"
        COOKBOOK_LOADER_SETTINGS_FILE_STRING="${CMAKE_SOURCE_DIR}/vk_loader_settings.json"
)
