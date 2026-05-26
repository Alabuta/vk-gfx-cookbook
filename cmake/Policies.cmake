# Core policy INTERFACE targets. Each carries one axis of build policy as transitive
# compile/link options or definitions; chapters link the targets they want.
#
# Compiler-family dispatch via IS_* aliases defined in cmake/CompilerDispatch.cmake.


# === vkgc::cxx_runtime ===
# C++ language standard and position-independent code. cxx_std_23 propagates as a transitive
# compile feature (modern equivalent of CXX_STANDARD=23 which is non-transitive through INTERFACE).

add_library(vkgc_cxx_runtime INTERFACE)
add_library(vkgc::cxx_runtime ALIAS vkgc_cxx_runtime)

target_compile_features(vkgc_cxx_runtime
    INTERFACE
        cxx_std_23)

set_target_properties(vkgc_cxx_runtime
    PROPERTIES
        INTERFACE_POSITION_INDEPENDENT_CODE ON
)


# === vkgc::warnings ===
# Strict warnings + warnings-as-errors across all four compiler families. GNU-driver compilers
# (GCC + MinGW + Clang-MSYS) and clang-cl share `-W*` spellings (clang-cl is "bilingual"); MSVC
# native uses `/W4 /WX` plus per-warning `/wNNNNN` opt-ins. Also carries `-pipe` /
# `-fasynchronous-unwind-tables` codegen helpers for GCC-family — too small a set to deserve a
# separate target.

add_library(vkgc_warnings INTERFACE)
add_library(vkgc::warnings ALIAS vkgc_warnings)

target_compile_options(vkgc_warnings
    INTERFACE
        "$<$<OR:${IS_GNU_LINUX},${IS_MINGW},${IS_CLANG_MSYS},${IS_CLANG_CL}>:"
            -Wpedantic
            -Wall
            -Wextra
            -Werror
            -Wconversion

            -Wold-style-cast
            -Wnon-virtual-dtor
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wsign-conversion
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wmisleading-indentation

            -Wno-braced-scalar-init

            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic

            -Wno-pre-c++17-compat

            # Project is C++23-only; the -Wc++NN-compat family flags C++20+ features
            # as "won't work in older standards" — pure noise for forward-only code.
            -Wno-c++20-compat

            # Vulkan-struct designated init (`{.sType{X}}`) deliberately omits pNext and
            # payload members; C++20 value-initializes them to {} / nullptr so it's
            # idiomatic and safe. Clang 19+ raises -Wmissing-designated-field-initializers
            # on every such site, which adds nothing for this codebase. GCC's separate
            # -Wmissing-field-initializers is still active.
            -Wno-missing-designated-field-initializers

            # Vulkan enums (VkResult, VkDebugReportObjectType, ...) carry sentinel
            # *_MAX_ENUM values and grow with every SDK update; enumerating every case
            # in formatter switches is a treadmill. -Wswitch (no default + missing
            # cases) is still active, so genuinely-uncovered switches still error out.
            -Wno-switch-enum

            # Exhaustive switches over project enums (e.g. present_status) deliberately
            # omit `default:` so that adding a new enumerator triggers -Wswitch on every
            # call site. -Wswitch-default enforces the opposite preference (always add
            # a fallback) and would mask exactly the kind of churn we want surfaced.
            -Wno-switch-default

            # Vulkan payload structs hit -Wpadded because the natural field layout
            # (pointer-sized handle + 4-byte enum / 1-byte bool / etc.) leaves
            # unavoidable trailing padding to satisfy 8-byte alignment. Reordering
            # can't dissolve the padding without filler fields, which is worse than
            # the diagnostic. The warning is off by default upstream; suppress so
            # the implicit enable doesn't break us.
            -Wno-padded
        ">"

        "$<$<OR:${IS_GNU_LINUX},${IS_MINGW}>:"
            -fasynchronous-unwind-tables                # Increased reliability of backtraces

            -pipe

            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wuseless-cast
        ">"

        "$<${IS_CLANG_CL}:"
            -Wno-unknown-pragmas
            -Wno-unknown-warning-option

            -Wno-shadow-field-in-constructor

            # CMake's Windows-Clang-CXX module injects `-TP` (force C++) on every CXX
            # compile rule. For .cxxm module interface units, clang already infers C++
            # from the extension, so `-TP` is redundant and `-Wunused-command-line-argument`
            # flags it — fatal under `-Werror`. Suppress project-wide; we don't rely on
            # this warning to police flag drift.
            -Wno-unused-command-line-argument

            # For CLion's compiler-info probe: it runs clang-cl with -Weverything
            # on a synthetic TU full of reserved identifiers (`___CIDR_*`); without
            # these, -Werror kills the probe and IDE intelligence breaks. Inert
            # for real builds (not in -Wall/-Wextra).
            -Wno-reserved-macro-identifier
            -Wno-reserved-identifier
            -Wno-unused-macros
        ">"

        "$<${IS_MSVC}:"
            /W4
            /WX
            /w14242 # 'identifier': conversion from 'type1' to 'type1', possible loss of data
            /w14254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
            /w14263 # 'function': member function does not override any base class virtual member function
            /w14265 # 'classname': class has virtual functions, but destructor is not virtual
            /w14287 # 'operator': unsigned/negative constant mismatch
            /we4289 # 'variable': loop control variable declared in the for-loop is used outside the for-loop scope
            /w14296 # 'operator': expression is always 'boolean_value'
            /w14311 # 'variable': pointer truncation from 'type1' to 'type2'
            /w14545 # expression before comma evaluates to a function which is missing an argument list
            /w14546 # function call before comma missing argument list
            /w14547 # 'operator': operator before comma has no effect; expected operator with side-effect
            /w14549 # 'operator': operator before comma has no effect; did you intend 'operator'?
            /w14555 # expression has no effect; expected expression with side-effect
            /w14619 # pragma warning: there is no warning number 'number'
            /w14640 # Enable warning on thread un-safe static member initialization
            /w14826 # Conversion from 'type1' to 'type_2' is sign-extended. This may cause unexpected runtime behavior.
            /w14905 # wide string literal cast to 'LPSTR'
            /w14906 # string literal cast to 'LPWSTR'
            /w14928 # illegal copy-initialization; more than one user-defined conversion has been implicitly applied
        ">"
)


# === vkgc::hardening ===
# Link-time hardening: detect underlinking, disable lazy binding, enforce read-only-after-relocation.
# ELF-only concepts — PE/COFF (Windows) already requires every import to be resolved against an
# import library at link time, so MinGW/Clang-MSYS need nothing here. MSVC link.exe wouldn't accept
# these spellings anyway.

add_library(vkgc_hardening INTERFACE)
add_library(vkgc::hardening ALIAS vkgc_hardening)

target_link_options(vkgc_hardening
    INTERFACE
        "$<$<PLATFORM_ID:Linux>:"
            LINKER:-z,defs                          # Detect and reject underlinking (== --no-undefined)
            LINKER:-z,now                           # Disable lazy binding
            LINKER:-z,relro                         # Read-only segments after relocation
        ">"

        "$<$<PLATFORM_ID:Darwin>:"
            LINKER:-bind_at_load                    # Disable lazy binding
        ">"
)


# === vkgc::no_exceptions ===
# Disable C++ exceptions. clang-cl rejects -fno-exceptions (-Wunknown-argument, fatal under
# -Werror) and follows MSVC convention; everything else takes the GNU-driver spelling.
#
# `_HAS_EXCEPTIONS=0` on MSVC / clang-cl is a best-effort cover, not airtight. It rewrites the
# inline `_THROW(...)` macro to terminate and adds `_Doraise()` overrides on the std exception
# classes, so throws written directly into STL headers redirect through std::terminate. It does
# NOT remove APIs like `vector::at()` and does NOT recompile the runtime: out-of-line throw
# helpers (`_Xout_of_range`, `_Xlength_error`, `_Xbad_alloc`, …) are declared `_CRTIMP2_PURE`
# in <xutility> and live in msvcp140.dll, which ships built with exceptions enabled. Calling
# `vector::at()` out of range still hits a real `throw out_of_range` inside the DLL; with
# /EHs-c- the user frames carry no unwind tables, so the throw rides through to std::terminate
# instead of being catchable. Treat throwing STL accessors (`at`, `map::at`, `stoi`/`stoX`, …)
# as effectively banned in this project rather than relying on this target to neutralize them.

add_library(vkgc_no_exceptions INTERFACE)
add_library(vkgc::no_exceptions ALIAS vkgc_no_exceptions)

target_compile_options(vkgc_no_exceptions
    INTERFACE
        "$<$<OR:${IS_GNU_LINUX},${IS_MINGW},${IS_CLANG_MSYS}>:"
            -fno-exceptions
        ">"

        "$<$<OR:${IS_MSVC},${IS_CLANG_CL}>:"
            /EHs-c-
        ">"
)

target_compile_definitions(vkgc_no_exceptions
    INTERFACE
        "$<$<OR:${IS_MSVC},${IS_CLANG_CL}>:"
            _HAS_EXCEPTIONS=0
        ">"
)


# === vkgc::diagnostics ===
# Project-defined runtime check toggles. VKGC_DO_ENSURE is always on (hard preconditions);
# VKGC_DO_CHECK and VKGC_DEBUG_VULKAN gate softer checks and validation-layer messaging on
# Debug-only.

add_library(vkgc_diagnostics INTERFACE)
add_library(vkgc::diagnostics ALIAS vkgc_diagnostics)

target_compile_definitions(vkgc_diagnostics
    INTERFACE
        VKGC_DO_ENSURE=1
        VKGC_DO_CHECK=$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,1,0>

        VKGC_DEBUG_VULKAN=$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,1,0>
)


# === vkgc::sanitizers::address ===
# AddressSanitizer (compile + link). Gated by VKGC_ENABLE_ASAN; when OFF this target is empty
# and inert, so chapters can link it unconditionally. -fno-omit-frame-pointer keeps ASan stack
# traces useful on the GNU-driver side. MSVC-style frontends use /fsanitize=address and require
# /INCREMENTAL:NO at link (CMake injects /INCREMENTAL into CMAKE_EXE_LINKER_FLAGS_DEBUG); /RTC1
# stripping for Debug is handled in the root CMakeLists.txt where the flag string lives.
#
# Composition notes: combines with vkgc::no_exceptions (/EHs-c- + _HAS_EXCEPTIONS=0 stay valid
# under ASan) and with vkgc::hardening (ASan's runtime resolves its own symbols, so -z,defs and
# -no-undefined do not flag false positives on the injected interceptors).

add_library(vkgc_sanitizers_address INTERFACE)
add_library(vkgc::sanitizers::address ALIAS vkgc_sanitizers_address)

if (VKGC_ENABLE_ASAN)
    target_compile_options(vkgc_sanitizers_address
        INTERFACE
            "$<$<OR:${IS_GNU_LINUX},${IS_MINGW},${IS_CLANG_MSYS}>:"
                -fsanitize=address
                -fno-omit-frame-pointer
            ">"

            "$<$<OR:${IS_MSVC},${IS_CLANG_CL}>:"
                /fsanitize=address
            ">"
    )

    target_link_options(vkgc_sanitizers_address
        INTERFACE
            "$<$<OR:${IS_GNU_LINUX},${IS_MINGW},${IS_CLANG_MSYS}>:"
                -fsanitize=address
            ">"

            "$<$<OR:${IS_MSVC},${IS_CLANG_CL}>:"
                /INCREMENTAL:NO
            ">"
    )

    # clang-cl embeds /defaultlib:clang_rt.asan_*.lib pragmas into instrumented objects and
    # normally relies on the clang-cl driver to inject -libpath:<resource>/lib/windows at
    # link time. CMake invokes lld-link.exe directly through vs_link_exe (no driver), so the
    # path never reaches the linker and the symbols come back undefined. Add the compiler-rt
    # lib directory to the search path ourselves. Native MSVC ships compiler-rt alongside the
    # toolset on the default LIB so this isn't needed there; clang-MSYS uses the GNU driver
    # which auto-resolves its runtime on the GCC-style link line.
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
            OUTPUT_VARIABLE _vkgc_clang_resource_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
        )
        target_link_directories(vkgc_sanitizers_address
            INTERFACE "${_vkgc_clang_resource_dir}/lib/windows"
        )
        unset(_vkgc_clang_resource_dir)
    endif()
endif()


# === vkgc::platform_quirks ===
# Truly platform-level defines that aren't tied to any specific dependency. Currently just
# NOMINMAX on Windows to suppress the min/max macros from <windows.h>. Dependency-coupled
# platform defines (VK_USE_PLATFORM_*, GLFW_EXPOSE_NATIVE_*) belong with their respective
# vkgc::dependencies::* targets.

add_library(vkgc_platform_quirks INTERFACE)
add_library(vkgc::platform_quirks ALIAS vkgc_platform_quirks)

target_compile_definitions(vkgc_platform_quirks
    INTERFACE
        "$<$<PLATFORM_ID:Windows>:"
            NOMINMAX
        ">"
)
