include(FetchContent)

find_package(Vulkan ${COOKBOOK_VULKAN_API_VERSION_MAJOR}.${COOKBOOK_VULKAN_API_VERSION_MINOR} REQUIRED)

# Every declaration carries:
#   * GIT_SHALLOW TRUE — depth-1 clone; all GIT_TAGs are real tags (not SHAs), so shallow is valid
#                       and cuts cold-configure time meaningfully for large repos (glslang especially).
#   * FIND_PACKAGE_ARGS <version> — version floor on the system-fallback path, so an older
#                       system install with diverging APIs can't silently satisfy the find.
# Where a system package's CMake config differs from the FetchContent name, an explicit `NAMES`
# selector points find_package at the right config (e.g. `NAMES glfw3` — GLFW installs as
# `glfw3Config.cmake` exporting target `glfw`, so no alias is needed once the find succeeds).

# === volk === (header-only)
FetchContent_Declare(
        volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG        1.4.304
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS 1.4.304 NAMES volk
)

# === GLM === (header-only)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.2
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS 1.0.2 NAMES glm
)

# === GLFW ===
# `NAMES glfw3` because GLFW installs `glfw3Config.cmake`, not `glfwConfig.cmake`; without it the
# system-fallback path is dead code. The installed package exports the bare `glfw` target name,
# so the link line stays uniform between FetchContent and system paths — no alias needed.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS 3.4 NAMES glfw3
)

# === Taskflow === (header-only, parallel task programming)
set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        taskflow
        GIT_REPOSITORY https://github.com/taskflow/taskflow
        GIT_TAG        v4.0.0
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS 4.0.0
)

# === glslang === (GLSL to SPIR-V compiler; produces `glslang`, `SPIRV`, and `glslang-default-resource-limits` targets)
# ENABLE_OPT=OFF avoids the SPIRV-Tools dependency the optimizer pulls in (the
# update_glslang_sources.py bootstrap is only needed with the optimizer on).
#
# No FIND_PACKAGE_ARGS: the installed `glslangConfig.cmake` exports namespaced targets
# (`glslang::glslang`, `glslang::SPIRV`, `glslang::glslang-default-resource-limits`), so a
# successful system find would break the bare-name link line in vkgc::dependencies::shaders.
# The cookbook tracks the book's pinned glslang version anyway — system substitution isn't wanted.
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG        16.2.0
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
        SYSTEM
)

# === VulkanMemoryAllocator === (header-only; exposes `GPUOpen::VulkanMemoryAllocator`)
FetchContent_Declare(
        VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG        v3.3.0
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS 3.3.0
)

# === sigslot === (header-only signal/slot library; exposes `Pal::Sigslot`)
set(SIGSLOT_COMPILE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SIGSLOT_COMPILE_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        sigslot
        GIT_REPOSITORY https://github.com/palacaze/sigslot.git
        GIT_TAG        v1.2.2
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS 1.2.2
)

FetchContent_MakeAvailable(volk glm glfw taskflow glslang VulkanMemoryAllocator sigslot)
