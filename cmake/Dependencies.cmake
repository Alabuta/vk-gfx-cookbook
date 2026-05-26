# Share the populated dependency tree across configure presets. Default would be
# ${CMAKE_BINARY_DIR}/_deps, i.e. each `build/<preset>/_deps/` re-clones from scratch when the
# user switches presets. Pinning to a fixed location under build/ (gitignored) means the second
# preset's first configure populates from cache and runs near-instant. Override with
# -DFETCHCONTENT_BASE_DIR=... to point at a system-wide mirror.
#
# Must precede include(FetchContent): the module's body does its own
# `set(FETCHCONTENT_BASE_DIR ... CACHE PATH ...)`, and `set(... CACHE ...)` without FORCE is a
# no-op when the cache slot is already filled. Setting it here populates the slot first so the
# module's default becomes the no-op; `-D` overrides still win because command-line cache vars
# are written before any CMakeLists code runs.
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/build/_deps/${CMAKE_SYSTEM_PROCESSOR}"
    CACHE PATH "Shared FetchContent cache across all configure presets")

include(FetchContent)

find_package(Vulkan ${COOKBOOK_VULKAN_API_VERSION_MAJOR}.${COOKBOOK_VULKAN_API_VERSION_MINOR} REQUIRED)

# === volk === (header-only)
FetchContent_Declare(
        volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG        1.4.304
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS
)

# === GLM === (header-only)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.2
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS
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
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS
)

# === Taskflow === (header-only, parallel task programming)
set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        taskflow
        GIT_REPOSITORY https://github.com/taskflow/taskflow
        GIT_TAG v4.0.0
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS
)

# === glslang === (GLSL to SPIR-V compiler; produces `glslang`, `SPIRV`, and `glslang-default-resource-limits` targets)
# ENABLE_OPT=OFF avoids the SPIRV-Tools dependency that glslang's optimizer pulls in; the
# update_glslang_sources.py bootstrap step is only needed when the optimizer is on.
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG 16.2.0
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS
)

# === VulkanMemoryAllocator === (header-only; exposes `GPUOpen::VulkanMemoryAllocator`)
FetchContent_Declare(
        VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG        v3.3.0
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS
)

# === sigslot === (header-only signal/slot library; exposes `Pal::Sigslot`)
set(SIGSLOT_COMPILE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SIGSLOT_COMPILE_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        sigslot
        GIT_REPOSITORY https://github.com/palacaze/sigslot.git
        GIT_TAG        v1.2.2
        EXCLUDE_FROM_ALL
        SYSTEM
        FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(volk glm glfw taskflow glslang VulkanMemoryAllocator sigslot)
