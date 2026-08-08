include(FetchContent)

find_package(Vulkan ${COOKBOOK_VULKAN_API_VERSION_MAJOR}.${COOKBOOK_VULKAN_API_VERSION_MINOR} REQUIRED)

# === Slang === (shader compiler bundled in the Vulkan SDK; wrapped in the imported `Slang::Slang` target)
# Slang is neither fetched nor built from source: the SDK that find_package(Vulkan) above resolves already
# ships the compiler — headers under <sdk>/Include/slang, import libs under <sdk>/Lib, and the
# slang.dll / slangd.dll runtimes under <sdk>/Bin (kept on PATH by the SDK installer). CMake's FindVulkan
# exposes no `slang` component (only the glslang stack), so the pieces are located by hand here. The SDK
# carries split debug (`slangd`) and release (`slang`) artifacts; both are mapped so a Debug configure links
# the asserts-enabled build while RelWithDebInfo / Shipping fall through to the release one.
find_path(VKGC_SLANG_INCLUDE_DIR
        NAMES slang.h
        HINTS
            ${Vulkan_INCLUDE_DIR}
            $ENV{VULKAN_SDK}/Include
            $ENV{VULKAN_SDK}/include
        PATH_SUFFIXES slang
)

find_library(VKGC_SLANG_LIBRARY_RELEASE
        NAMES slang
        HINTS
            ${Vulkan_INCLUDE_DIR}/../Lib
            $ENV{VULKAN_SDK}/Lib
            $ENV{VULKAN_SDK}/lib
)

find_library(VKGC_SLANG_LIBRARY_DEBUG
        NAMES slangd
        HINTS
            ${Vulkan_INCLUDE_DIR}/../Lib
            $ENV{VULKAN_SDK}/Lib
            $ENV{VULKAN_SDK}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Slang
        REQUIRED_VARS VKGC_SLANG_LIBRARY_RELEASE VKGC_SLANG_INCLUDE_DIR
)

mark_as_advanced(VKGC_SLANG_INCLUDE_DIR VKGC_SLANG_LIBRARY_RELEASE VKGC_SLANG_LIBRARY_DEBUG)

# UNKNOWN IMPORTED keeps this cross-platform: on Windows the located file is slang's import .lib (the matching
# slang.dll resolves at runtime via the SDK's Bin dir on PATH); on Linux/macOS it's the shared object itself.
add_library(Slang::Slang UNKNOWN IMPORTED)
# IMPORTED_LOCATION is the all-config fallback; the maps funnel the non-Release build types onto the
# release artifact. The RELEASE configuration has to be declared unconditionally: once a non-empty
# MAP_IMPORTED_CONFIG_<CONFIG> exists, CMake commits to the mapping and will NOT fall back to the plain
# IMPORTED_LOCATION, so a map pointing at an undeclared configuration fails the generate step with
# "IMPORTED_LOCATION not set for imported target" once per consumer. The macOS SDK ships only
# libslang.dylib (no libslangd), which is exactly the case that leaves DEBUG mapped onto a Release
# configuration that the slangd-guarded block below would otherwise never have created.
set_target_properties(Slang::Slang PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${VKGC_SLANG_INCLUDE_DIR}"
        IMPORTED_LOCATION "${VKGC_SLANG_LIBRARY_RELEASE}"
        IMPORTED_CONFIGURATIONS "RELEASE"
        IMPORTED_LOCATION_RELEASE "${VKGC_SLANG_LIBRARY_RELEASE}"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release
        MAP_IMPORTED_CONFIG_DEBUG Release
)
# When the SDK carries the asserts-enabled build (Windows), add the DEBUG configuration and re-point
# the Debug map at it; otherwise every build type stays on the release artifact mapped above.
if (VKGC_SLANG_LIBRARY_DEBUG)
    set_target_properties(Slang::Slang PROPERTIES
            IMPORTED_CONFIGURATIONS "RELEASE;DEBUG"
            IMPORTED_LOCATION_DEBUG "${VKGC_SLANG_LIBRARY_DEBUG}"
            MAP_IMPORTED_CONFIG_DEBUG Debug
    )
endif ()

# slangc — the Slang command-line compiler, shipped next to the Slang runtime in the SDK's Bin dir.
# Located separately from the Slang::Slang link target above because it is a build tool, not a link
# dependency: cmake/Shaders.cmake drives it via add_custom_command to compile .slang sources to SPIR-V
# at build time. Found here (rather than in Shaders.cmake) so the SDK probing stays in one place.
find_program(VKGC_SLANGC_EXECUTABLE
        NAMES slangc
        HINTS
            ${Vulkan_INCLUDE_DIR}/../Bin
            $ENV{VULKAN_SDK}/Bin
            $ENV{VULKAN_SDK}/bin
)
mark_as_advanced(VKGC_SLANGC_EXECUTABLE)
if (VKGC_SLANGC_EXECUTABLE)
    message(STATUS "Found slangc: ${VKGC_SLANGC_EXECUTABLE}")
else ()
    message(WARNING
            "slangc not found in the Vulkan SDK — build-time shader compilation "
            "(vkgc_compile_shaders) will fail. Set VULKAN_SDK or VKGC_SLANGC_EXECUTABLE.")
endif ()

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

# === cgltf === (header-only glTF 2.0 loader; include-only)
# SOURCE_SUBDIR points at a directory that holds no CMakeLists.txt, so MakeAvailable populates the
# source tree (exposing cgltf.h via cgltf_SOURCE_DIR) but skips add_subdirectory — we never build the
# upstream test targets. Consumed through vkgc::dependencies::model_loading in cmake/Interfaces.cmake.
FetchContent_Declare(
        cgltf
        GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
        GIT_TAG        v1.15
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
        SYSTEM
        SOURCE_SUBDIR  do-not-build
)

# === stb === (header-only image loader; stb_image.h)
# Upstream ships no tags/releases, so this pins a master commit (unlike the all-real-tags convention
# noted above). GIT_SHALLOW is omitted on purpose: a depth-1 clone of an arbitrary SHA isn't reliable
# across Git servers. SOURCE_SUBDIR skips add_subdirectory (stb has no root CMakeLists anyway).
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        31c1ad37456438565541f4919958214b6e762fb4
        EXCLUDE_FROM_ALL
        SYSTEM
        SOURCE_SUBDIR  do-not-build
)

FetchContent_MakeAvailable(volk glm glfw taskflow glslang VulkanMemoryAllocator sigslot cgltf stb)
