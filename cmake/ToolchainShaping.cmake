# Early toolchain-shaping invariants. Runs before Dependencies.cmake (and any
# add_subdirectory) because every block here either fails the configure or
# mutates a global CMAKE_*_FLAGS variable, and those mutations must be in effect
# before any target is created — FetchContent deps included.
#
# Separate from cmake/Policies.cmake on purpose: Policies.cmake defines INTERFACE
# targets that downstream targets opt into (link order, transitive properties).
# This file edits CMake's own per-config flag variables, which is a different
# axis of control — global and order-sensitive rather than per-target.

# --- Sanitizer fail-fast guards ---

# ASan on Windows needs an MSVC-style frontend (MSVC or clang-cl). MinGW/Clang-MSYS use the
# GNU driver against MSYS2/UCRT64, where libasan isn't packaged and __asan_* come back undefined
# at link. WSL is CMAKE_SYSTEM_NAME=Linux and unaffected. Fail fast here.
if (VKGC_ENABLE_ASAN AND CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT MSVC)
    message(FATAL_ERROR
            "VKGC_ENABLE_ASAN=ON on Windows requires an MSVC-style compiler (MSVC or clang-cl). "
            "Detected ${CMAKE_CXX_COMPILER_ID} with frontend variant "
            "'${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}'. Use vcvars64 + cl/clang-cl, or build under WSL.")
endif ()

# UBSan is Clang-only here. MSVC native has no -fsanitize=undefined; GCC (incl. MinGW) accepts it
# but ICEs instrumenting C++20 module interface units (cp/module.cc, confirmed GCC 15.2), and every
# chapter attaches common module units — so no usable GCC path. Clang handles modules + UBSan (see
# vkgc::sanitizers::undefined). Fail fast rather than surfacing a cl flag error or GCC ICE mid-build.
if (VKGC_ENABLE_UBSAN AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
            "VKGC_ENABLE_UBSAN=ON requires a Clang frontend (Clang-MSYS or clang-cl). Detected "
            "${CMAKE_CXX_COMPILER_ID}: MSVC native has no UndefinedBehaviorSanitizer, and GCC ICEs "
            "instrumenting this project's C++20 module units. Use clang/clang++ or clang-cl.")
endif ()

# --- MSVC exception-model / RTTI flag stripping ---

# CMake injects /EHsc into CMAKE_CXX_FLAGS for MSVC-style frontends. Strip it so the exception
# model is set per-target via vkgc::no_exceptions (/EHs-c-) instead of fighting the default.
# `/EHs[c]?` consumes both spellings in one pass (order-independent); `/GR-?` swallows an optional
# trailing dash so an externally-injected `/GR-` doesn't leave a stray `-` in the flag string.
if (MSVC)
    string(REGEX REPLACE "/EHs[c]?" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/GR-?" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif ()

# --- ASan-driven MSVC flag / runtime mutations ---

# MSVC-style frontends inject /RTC1 into CMAKE_CXX_FLAGS_DEBUG, which is mutually exclusive with
# /fsanitize=address (cl errors; clang-cl warns and drops ASan). Strip it when ASan is on so
# chapter targets pick up vkgc::sanitizers::address without per-config gymnastics.
if (MSVC AND VKGC_ENABLE_ASAN)
    string(REGEX REPLACE "/RTC[1csu]*" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE "/RTC[1csu]*" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
endif ()

# MSVC-style frontends default to /MDd for Debug. clang-cl's compiler-rt rejects /MDd with
# /fsanitize=address ("doesn't support linking with debug runtime libraries yet"); MSVC's ASan
# accepts it but Microsoft recommends /MD anyway. Force the release-DLL CRT when ASan is on.
# Must precede target creation, hence here rather than in a policy target.
if (MSVC AND VKGC_ENABLE_ASAN)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif ()

# --- Clang-MSYS linker selection ---

# Clang-MSYS only: BFD ld emits unsuppressable "duplicate section ... has different size" warnings
# against libstdc++exp.a(print.o) from a header/precompiled-lib size mismatch in MSYS2 UCRT64; LLD
# silently merges those COMDATs. Applied globally (LLD is fine for FetchContent deps too).
# CMAKE_LINKER_TYPE is 3.29+; we require 3.30.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        AND CMAKE_SYSTEM_NAME STREQUAL "Windows"
        AND "${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "GNU")
    set(CMAKE_LINKER_TYPE LLD)
endif ()
