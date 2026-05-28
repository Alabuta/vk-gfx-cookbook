# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Implementations following **"Vulkan 3D Graphics Rendering Cookbook — Second Edition"** (Kosarevsky, Medvedev, Latypov; Packt 2025). Organized as progressive chapters teaching Vulkan concepts. Written in modern C++23. Currently in early development (chapter 1 in progress).

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

CMake 3.30+ with the single-config `Ninja` generator. Dependencies fetched via `FetchContent` (no manual installs needed except system-level X11 on Linux). `CMakePresets.json` ships nine visible configure presets — `Debug`, `Development`, `Shipping`, `Debug ASan`, `Development ASan`, `Shipping ASan`, `Debug UBSan`, `Development UBSan`, `Shipping UBSan` — plus hidden `_base`, `_asan`, and `_ubsan` mixins. Each preset pins `CMAKE_BUILD_TYPE` (Debug / RelWithDebInfo / Release) and the relevant `VKGC_ENABLE_ASAN` / `VKGC_ENABLE_UBSAN`; none pin a compiler. The two sanitizers compose (`-fsanitize=address,undefined`); the shipped presets enable one at a time, so multi-inherit `_asan` + `_ubsan` in `CMakeUserPresets.json` to run both. UBSan is **Clang-only** (clang-cl trap mode, Clang-MSYS print mode) — the configure refuses MSVC native and the GCC family, since GCC ICEs instrumenting this project's C++20 module units. Compiler choice is the developer's, applied externally (env, CLI, or `CMakeUserPresets.json`). No `buildPresets` section — single-config makes them redundant, and dropping them keeps CLion's profile names clean (CLion uses the configure preset's `name` field directly).

Preset names contain spaces so the CLion profile UI shows them verbatim; quote them on the CLI (`cmake --preset "Debug ASan"`). Build directories follow the same names (`build/Debug ASan/` etc.) — same quoting rule applies.

### Compiler selection

`CMakePresets.json` doesn't set `CMAKE_C_COMPILER` / `CMAKE_CXX_COMPILER`. Pick one of:

1. **Environment** — launch the configure from a shell where your toolchain is on PATH (vcvars64 for `cl` / `clang-cl`; MSYS2 UCRT64 for `g++` / `clang++`; system shell on Linux). CMake autodetects.
2. **`CC` / `CXX` env vars** — `CXX=clang-cl cmake --preset Debug`.
3. **`CMakeUserPresets.json`** (local, gitignored) — define a hidden compiler-base and multi-inherit it alongside the committed build-type presets:
   ```json
   {
       "version": 6,
       "configurePresets": [
           { "name": "_msvc",     "hidden": true, "cacheVariables": { "CMAKE_C_COMPILER": "cl",       "CMAKE_CXX_COMPILER": "cl" } },
           { "name": "_clang-cl", "hidden": true, "cacheVariables": { "CMAKE_C_COMPILER": "clang-cl", "CMAKE_CXX_COMPILER": "clang-cl" } },

           { "name": "MSVC Debug",             "inherits": ["_msvc", "Debug"] },
           { "name": "MSVC Development",       "inherits": ["_msvc", "Development"] },
           { "name": "MSVC Shipping",          "inherits": ["_msvc", "Shipping"] },
           { "name": "MSVC Debug ASan",        "inherits": ["_msvc", "Debug ASan"] },
           { "name": "MSVC Development ASan",  "inherits": ["_msvc", "Development ASan"] },
           { "name": "MSVC Shipping ASan",     "inherits": ["_msvc", "Shipping ASan"] }
       ]
   }
   ```
   Each named variant gets its own `build/<name>/` via the inherited `${presetName}` binaryDir.
4. **CLI override** — `cmake --preset Debug -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl`.

### Usage

```bash
# Configure (each preset gets its own build/<name>/ dir; quote names with spaces):
cmake --preset Debug
cmake --preset "Development ASan"

# Build (single-config — no --preset for build; just point ninja at the build dir):
cmake --build build/Debug
cmake --build build/Debug --target chapter01_glfw
cmake --build "build/Development ASan" --target chapter02_swapchain

# Discover:
cmake --list-presets configure
```

For one-off configures bypassing presets entirely, `cmake -B <dir> -G Ninja -DCMAKE_BUILD_TYPE=Debug [-DVKGC_ENABLE_ASAN=ON] [-DVKGC_ENABLE_UBSAN=ON]` still works.

## Architecture

- `src/chapterNN/<topic>/` — each chapter exercise is a self-contained CMake sub-project with its own `CMakeLists.txt` and `main.cxx`
- `src/common/` — shared sources and C++23 module units (`app/`, `platform/`, `vulkan/`) attached to every chapter executable via `vkgc_attach_common_sources()`; transitional pending extraction into a real `vkgc::common` library
- `shaders/chapterNN/<topic>/` — GLSL sources consumed by chapter targets at runtime; located via the `COOKBOOK_SHADER_DIR_STRING` compile define carried by `vkgc::config::cookbook_paths`
- `.cache/` — runtime output for compiled SPIR-V and other generated artifacts; located via `COOKBOOK_CACHE_DIR_STRING` (same source); gitignored
- Root `CMakeLists.txt` — toolchain-shaping invariants, non-transitive target-property defaults, and `include()`s for the modules below
- `cmake/CompilerDispatch.cmake` — `IS_*` generator-expression aliases (`IS_GCC_POSIX`, `IS_CLANG_POSIX`, `IS_MINGW`, `IS_CLANG_MSYS`, `IS_CLANG_CL`, `IS_MSVC`) consumed by every policy/interface target
- `cmake/Dependencies.cmake` — `FetchContent_Declare` for all third-party libs
- `cmake/Policies.cmake` — `vkgc::cxx_runtime`, `vkgc::warnings`, `vkgc::hardening`, `vkgc::no_exceptions`, `vkgc::diagnostics`, `vkgc::platform_quirks` INTERFACE targets
- `cmake/Interfaces.cmake` — `vkgc::dependencies::*` (vulkan, windowing, math, concurrency, shaders, stdcxx_extras) and `vkgc::config::cookbook_paths` INTERFACE targets
- `cmake/ProjectConfig.cmake` — per-chapter helpers `vkgc_configure_chapter_target()` (sets `DEBUG_POSTFIX`) and `vkgc_attach_common_sources()` (attaches common TUs + module units)
- Chapter `CMakeLists.txt` files are minimal: define the executable, call `vkgc_configure_chapter_target()` and `vkgc_attach_common_sources()`, then `target_link_libraries` against the `vkgc::*` targets they need

## Dependencies

| Library | Version | Source |
|---------|---------|--------|
| Vulkan SDK | required | `find_package(Vulkan)` — version from `COOKBOOK_VULKAN_API_VERSION_*` |
| volk | 1.4.304 | FetchContent (GitHub) — Vulkan function loader |
| VulkanMemoryAllocator | 3.3.0 | FetchContent (GitHub) — exposes `GPUOpen::VulkanMemoryAllocator` |
| GLFW | 3.4 | FetchContent (GitHub) |
| GLM | 1.0.2 | FetchContent (GitHub) |
| Taskflow | 4.0.0 | FetchContent (GitHub) |
| sigslot | 1.2.2 | FetchContent (GitHub) — exposes `Pal::Sigslot` |
| glslang | 16.2.0 | FetchContent (GitHub) — `ENABLE_OPT=OFF` to skip the SPIRV-Tools bootstrap |
| X11 | system | Required on Linux only |

## Compiler Configuration

Warnings are treated as errors (`-Werror` / `/WX`). `cmake/Policies.cmake` defines `vkgc::warnings`, dispatched via the `IS_*` generator-expression aliases in `cmake/CompilerDispatch.cmake` (`IS_GCC_POSIX`, `IS_CLANG_POSIX`, `IS_MINGW`, `IS_CLANG_MSYS`, `IS_CLANG_CL`, `IS_MSVC`):

- **Shared GNU-driver core** (GCC + Clang on every platform): `-Wpedantic -Wall -Wextra -Werror -Wconversion` plus the usual quality block (`-Wold-style-cast`, `-Wsign-conversion`, `-Wnull-dereference`, `-Wformat=2`, …) and the `-Wno-switch-enum` / `-Wno-switch-default` / `-Wno-padded` Vulkan-specific suppressions GCC accepts
- **Clang-only** (`IS_CLANG_POSIX` + `IS_CLANG_MSYS` + `IS_CLANG_CL`): the `-Wno-*` flags GCC doesn't recognize (`-Wno-c++98-compat`, `-Wno-pre-c++17-compat`, `-Wno-braced-scalar-init`, `-Wno-missing-designated-field-initializers`). Kept separate because GCC errors on unrecognized `-Wno-*` whenever the category would have fired — under `-Werror`, fatal
- **GCC family only** (`IS_GCC_POSIX` + `IS_MINGW`): additional GCC-only warnings (`-Wduplicated-cond`, `-Wduplicated-branches`, `-Wlogical-op`, `-Wuseless-cast`) plus `-pipe` / `-fasynchronous-unwind-tables`
- **Clang-cl**: extra `-Wno-*` suppressions that keep the CLion compiler-info probe happy under `-Weverything`
- **MSVC native**: `/W4 /WX` plus per-warning `/wNNNNN` opt-ins for specific narrowing/conversion/lifetime issues

Exception model and codegen flags are factored out into their own INTERFACE targets (`vkgc::no_exceptions`, `vkgc::hardening`) rather than living alongside the warning flags.

## Delegation

- **Codebase reconnaissance** — prefer `Agent(subagent_type: "scout", ...)` over inline `Glob`/`Grep` when (a) the search needs ≥3 queries, (b) you're scanning unknown territory ("where does the swapchain rebuild happen?"), or (c) results would dump >50 lines of raw matches into the main context. For single targeted lookups (one file, one symbol), keep using `Grep`/`Read` directly — delegation has its own overhead.
- **Scout vs. Explore** — `scout` (Haiku, citation-only contract) for cheap location work; reserve the built-in `Explore` for broader read-with-judgment passes that need a stronger model.
- **Never delegate understanding to scout.** It reports `path:line` citations; you Read and reason. Don't write prompts like "scout this and then fix the bug" — pull the citations back, then decide.
- **Commit grouping** — when the working tree has accumulated multiple unrelated-ish changes that need to land as separate commits, call `Agent(subagent_type: "committer", ...)` in plan mode first. Show the returned plan to the user for approval (it's their git history), then re-invoke with `mode: execute` and the approved plan pasted back. Skip the agent for trivial single-theme commits — just run `git add` / `git commit` inline.
- **Never let committer decide whether to commit.** The agent plans and executes; the user approves the plan in between. Don't chain plan→execute in one parent turn without user confirmation.
- **Build error triage** — after a failed local build, call `Agent(subagent_type: "build-doctor", log: "<path>")` rather than reading the raw log yourself. The agent dedupes `-Werror` cascades, template-instantiation chains, and ninja `FAILED:` noise, then returns ranked `path:line — code — message` citations. Pass `filter` to narrow (e.g., `filter: "configure"`, `filter: "chapter02_swapchain"`). You then Read the cited source and decide on fixes — the agent never proposes them. Skip the agent for one-shot errors visible in <20 lines of log; just read directly.
- **Never auto-build from the harness.** `build-doctor` consumes logs only — it does not run cmake or ninja. The user builds locally and gives you the log path. Same applies to any future build-related agent.

## Conventions

- Source file extension: `.cxx`
- C++ standard: C++23 (`CXX_STANDARD 23`, extensions off)
- Uses `std::print` (C++23 `<print>` header) instead of `iostream`
- Debug builds get `.dbg` suffix on binaries
- Target naming: `chapterNN_<topic>` (e.g., `chapter01_glfw`)
