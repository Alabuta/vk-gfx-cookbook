# Vulkan 3D Graphics Rendering Cookbook

Hands-on implementations following **"Vulkan 3D Graphics Rendering Cookbook — Second Edition"** by Sergey Kosarevsky, Alexey Medvedev, and Viktor Latypov (Packt, 2025).

## Chapter Roadmap

| # | Chapter | Key Topics | Status |
|---|---------|------------|--------|
| 1 | Establishing a Build Environment | CMake, GLFW, Taskflow, GLSLang, BC7 compression | WIP |
| 2 | Getting Started with Vulkan | Swapchain, HelloTriangle, GLM | — |
| 3 | Working with Vulkan Objects | Assimp, STB image loading, buffers, textures | — |
| 4 | Adding User Interaction and Productivity Tools | ImGui, Tracy profiler, FPS counter, cubemap, camera | — |
| 5 | Working with Geometry Data | MeshOptimizer, vertex pulling, instanced meshes, tessellation, compute | — |
| 6 | PBR Using glTF 2.0 Shading Model | Unlit, BRDF LUT, environment filtering, metallic-roughness, specular-glossiness | — |
| 7 | Advanced PBR Extensions | Clearcoat, sheen, transmission, volume, IOR, specular, emissive | — |
| 8 | Graphics Rendering Pipeline | Descriptor indexing, scene graph, large scene rendering | — |
| 9 | glTF Animations | Skinning, morphing, animation blending, lights, cameras | — |
| 10 | Image-based Techniques | Offscreen rendering, shadow mapping, MSAA, SSAO, HDR | — |
| 11 | Advanced Rendering and Optimizations | CPU/GPU culling, directional shadows, OIT, lazy loading | — |

## Prerequisites

- C++23 compiler (MSVC 19.40+, GCC 14+, or Clang 18+)
- [CMake 4.0+](https://cmake.org/download/)
- [Ninja](https://ninja-build.org/) (recommended generator)
- [Vulkan SDK](https://vulkan.lunarg.com/) (pulled in by LightweightVK)
- Python 3 — LightweightVK runs `deploy_deps.py` at CMake configure time to fetch its transitive third-party libraries
- Linux only: X11 development libraries (`libx11-dev`, `libxrandr-dev`, etc.)

## Building

```bash
# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build everything
cmake --build build

# Build a specific chapter target
cmake --build build --target chapter01_glfw
```

## Project Structure

```
vk-gfx-cookbook/
    chapters/
        chapter01/
            glfw/           # GLFW window setup
            taskflow/       # Taskflow hello-world
        chapter02/          # (upcoming)
        ...
    cmake/
        CookbookConfig.cmake    # Shared target configuration (warnings, C++ standard, linker flags)
        Dependencies.cmake      # Third-party FetchContent declarations
    CMakeLists.txt              # Root build file
```

## Target Naming

Each exercise is a self-contained executable: `chapterNN_<topic>` (e.g. `chapter01_glfw`).

## Dependencies

All libraries are fetched automatically via CMake `FetchContent` — no manual installs required (except system-level X11 on Linux).

| Library | Version | Purpose |
|---------|---------|---------|
| GLFW | 3.4 | Windowing and input |
| GLM | 1.0.2 | Mathematics |
| Taskflow | v4.0.0 | Parallel task programming |
| LightweightVK | 1.4.0 | Vulkan rendering abstraction (pulls VMA, Tracy, minilog, KTX-Software, and others via its own `deploy_deps.py`) |

Additional dependencies will be added as chapters progress (Assimp, ImGui, MeshOptimizer, etc.).

## License

Educational / personal use. Based on the book's code examples.
