cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(vk_gfx_cookbook VERSION 1.0.0 LANGUAGES CXX)

# --- Toolchain-shaping invariants ---

# CMake injects /EHsc into CMAKE_CXX_FLAGS for MSVC-style frontends. Strip it so the exception
# model is chosen per-target via vkgc::no_exceptions (which sets /EHs-c-) instead of fighting
# the injected default.
if (MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/EHs"  "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# Clang-MSYS only: BFD ld emits unsuppressable "duplicate section ... has different size"
# warnings against libstdc++exp.a(print.o) due to a header/precompiled-library size mismatch
# in MSYS2 UCRT64. LLD silently merges those COMDATs. Applied globally because LLD is fine for
# FetchContent deps too; CMAKE_LINKER_TYPE is CMake 3.29+ (we require 3.30).
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
    AND CMAKE_SYSTEM_NAME STREQUAL "Windows"
    AND "${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "GNU")
    set(CMAKE_LINKER_TYPE LLD)
endif()

# --- Non-transitive target-property defaults ---
# Properties that don't propagate through INTERFACE libraries — set as initializer variables so
# chapter executables (and FetchContent libs, where they're inert) inherit without per-target
# boilerplate.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_DEBUG_POSTFIX .dbg)
set(CMAKE_VS_DEBUGGER_WORKING_DIRECTORY "$<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}>")
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)
set(CMAKE_XCODE_SCHEME_WORKING_DIRECTORY "$<$<PLATFORM_ID:Darwin>:${CMAKE_SOURCE_DIR}>")

# --- Generated project headers ---
include(cmake/ProjectVersion.cmake)

# --- Toolchain quirks ---
include(cmake/ClangClModules.cmake)

# --- Compiler-family dispatch (IS_* generator expressions) ---
include(cmake/CompilerDispatch.cmake)

# --- Third-party dependencies ---
include(cmake/Dependencies.cmake)

# --- Composable policy + interface targets ---
include(cmake/Policies.cmake)
include(cmake/Interfaces.cmake)

# --- Common sources helper (transitional; Phase B will replace with a vkgc::common library) ---
include(cmake/ProjectConfig.cmake)

# --- Chapter sub-projects ---
add_subdirectory(src/chapter01/glfw)
add_subdirectory(src/chapter01/taskflow)
add_subdirectory(src/chapter01/glslang)
add_subdirectory(src/chapter02/swapchain)
