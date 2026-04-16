# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Implementations following **"Vulkan 3D Graphics Rendering Cookbook — Second Edition"** (Kosarevsky, Medvedev, Latypov; Packt 2025). Organized as progressive chapters teaching Vulkan concepts. Written in modern C++26. Currently in early development (chapter 1 in progress).

### Book Chapters

1. **Establishing a Build Environment** — CMake, GLFW, Taskflow, GLSLang, BC7 compression
2. **Getting Started with Vulkan** — Swapchain, HelloTriangle, GLM
3. **Working with Vulkan Objects** — Assimp, STB image loading, buffers, textures
4. **Adding User Interaction and Productivity Tools** — ImGui, Tracy profiler, FPS counter, cubemap, camera
5. **Working with Geometry Data** — MeshOptimizer, vertex pulling, instanced meshes, tessellation, compute
6. **PBR Using glTF 2.0 Shading Model** — Unlit, BRDF LUT, environment filtering, metallic-roughness
7. **Advanced PBR Extensions** — Clearcoat, sheen, transmission, volume, IOR, specular, emissive
8. **Graphics Rendering Pipeline** — Descriptor indexing, scene graph, large scene rendering
9. **glTF Animations** — Skinning, morphing, animation blending, lights, cameras
10. **Image-based Techniques** — Offscreen rendering, shadow mapping, MSAA, SSAO, HDR
11. **Advanced Rendering and Optimizations** — CPU/GPU culling, directional shadows, OIT, lazy loading

## Build System

CMake 4.0+ with Ninja generator. Dependencies fetched via `FetchContent` (no manual installs needed except system-level X11 on Linux).

```bash
# Configure and build (from project root)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Build a specific chapter target
cmake --build build --target chapter01_glfw
```

## Architecture

- `chapters/chapterNN/<topic>/` — each chapter exercise is a self-contained CMake sub-project with its own `CMakeLists.txt` and `src/main.cxx`
- `cmake/CookbookConfig.cmake` — shared `configure_cookbook_target()` function applied to all chapter targets
- Root `CMakeLists.txt` — project-wide settings, dependency declarations, compiler detection, and chapter subdirectory includes
- Chapter CMakeLists files are minimal; they define the executable target and source, then call `configure_cookbook_target()`

## Dependencies

| Library | Version | Source |
|---------|---------|--------|
| GLFW | 3.4 | FetchContent (GitHub) |
| GLM | 1.0.2 | FetchContent (GitHub) |
| X11 | system | Required on Linux only |

## Compiler Configuration

Warnings are treated as errors (`-Werror` / MSVC equivalents). `cmake/CookbookConfig.cmake` defines comprehensive warning flags for four compiler families:
- **GNU/MinGW**: `-Wall -Wextra -Werror -Wpedantic -Wconversion` plus many specific warnings
- **Clang-cl**: `/EHa` with clang-specific warning suppressions
- **MSVC**: Individual `/wNNNNN` warning enables instead of `/W4 /WX`

Platform detection uses `CXX_FLAGS_STYLE_*` variables and CMake generator expressions.

## Conventions

- Source file extension: `.cxx`
- C++ standard: C++26 (`CXX_STANDARD 26`, extensions off)
- Uses `std::print` (C++23 `<print>` header) instead of `iostream`
- Debug builds get `.d` suffix on binaries
- Target naming: `chapterNN_<topic>` (e.g., `chapter01_glfw`)
