include(FetchContent)

# === GLM === (header-only)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.2
        EXCLUDE_FROM_ALL
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
        EXCLUDE_FROM_ALL
        SYSTEM
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
)

# === glslang === (GLSL to SPIR-V compiler; produces `glslang`, `SPIRV`, and `glslang-default-resource-limits` targets)
#
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
)

FetchContent_MakeAvailable(glm glfw taskflow glslang)
