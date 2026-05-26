cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(vk_gfx_cookbook VERSION 1.0.0 LANGUAGES CXX)

# --- User-facing options ---

option(VKGC_ENABLE_ASAN
    "Enable AddressSanitizer (compile + link flags via vkgc::sanitizers::address)" OFF)

# --- Toolchain-shaping invariants ---

# AddressSanitizer on Windows is only supported with MSVC-style frontends (MSVC native or
# clang-cl). MinGW GCC and Clang-MSYS use the GNU driver against MSYS2/UCRT64; the GCC build
# of libasan isn't packaged for that runtime and linking yields undefined __asan_* symbols.
# WSL targets CMAKE_SYSTEM_NAME=Linux and is unaffected. Fail fast at configure time rather
# than during link of the first chapter target.
if (VKGC_ENABLE_ASAN AND CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT MSVC)
    message(FATAL_ERROR
        "VKGC_ENABLE_ASAN=ON on Windows requires an MSVC-style compiler (MSVC or clang-cl). "
        "Detected ${CMAKE_CXX_COMPILER_ID} with frontend variant "
        "'${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}'. Use vcvars64 + cl/clang-cl, or build under WSL."
    )
endif()

# CMake injects /EHsc into CMAKE_CXX_FLAGS for MSVC-style frontends. Strip it so the exception
# model is chosen per-target via vkgc::no_exceptions (which sets /EHs-c-) instead of fighting
# the injected default.
if (MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/EHs"  "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# MSVC-style frontends (MSVC + clang-cl) inject /RTC1 into CMAKE_CXX_FLAGS_DEBUG. /RTC* is
# mutually exclusive with /fsanitize=address (cl emits a hard error, clang-cl warns and
# ignores ASan). Strip from the Debug flag strings when ASan is on so chapter targets can
# pick up vkgc::sanitizers::address without per-config gymnastics.
if (MSVC AND VKGC_ENABLE_ASAN)
    string(REGEX REPLACE "/RTC[1csu]*" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE "/RTC[1csu]*" "" CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}")
endif()

# MSVC-style frontends default to /MDd for Debug builds. clang-cl's bundled compiler-rt
# rejects /MDd with /fsanitize=address ("AddressSanitizer doesn't support linking with debug
# runtime libraries yet"); MSVC's own ASan accepts /MDd, but Microsoft also recommends /MD
# with ASan for portability. Force the release-DLL CRT when ASan is on. Must be set before
# targets are added, hence its placement here rather than in a policy interface target.
if (MSVC AND VKGC_ENABLE_ASAN)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
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
