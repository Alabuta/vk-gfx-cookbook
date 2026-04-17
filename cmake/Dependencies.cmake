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

# === LightweightVK === (Vulkan rendering library; deploys its own deps via deploy_deps.py at configure time — requires Python3)
set(LVK_WITH_GLFW OFF CACHE BOOL "" FORCE)
set(LVK_WITH_SAMPLES OFF CACHE BOOL "" FORCE)
set(LVK_DEPLOY_SCREENSHOT_TESTS OFF CACHE BOOL "" FORCE)
set(LVK_WITH_TRACY OFF CACHE BOOL "" FORCE)
set(LVK_WITH_IMPLOT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        lvk
        GIT_REPOSITORY https://github.com/corporateshark/lightweightvk
        GIT_TAG 1.4.0
        EXCLUDE_FROM_ALL
        SYSTEM
)

FetchContent_MakeAvailable(glm glfw taskflow lvk)
