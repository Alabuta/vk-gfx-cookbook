# Core policy INTERFACE targets. Each carries one axis of build policy as transitive
# compile/link options or definitions; chapters link the targets they want.
#
# Compiler-family dispatch via IS_* aliases defined in cmake/CompilerDispatch.cmake.


# === vkgc::cxx_runtime ===
# C++ standard and position-independent code. cxx_std_23 propagates as a transitive compile feature
# (the modern equivalent of CXX_STANDARD=23, which is non-transitive through INTERFACE).

add_library(vkgc_cxx_runtime INTERFACE)
add_library(vkgc::cxx_runtime ALIAS vkgc_cxx_runtime)

target_compile_features(vkgc_cxx_runtime
    INTERFACE
        cxx_std_23)

# INTERFACE_POSITION_INDEPENDENT_CODE propagates through to chapter executables, making each
# chapter binary PIE. Intentional security-hardening default — pairs with the link-time
# hardening flags in vkgc::hardening. Carries a small overhead on some ELF architectures
# (PLT indirection, slight code-size inflation) which is accepted for a teaching project.
set_target_properties(vkgc_cxx_runtime
    PROPERTIES
        INTERFACE_POSITION_INDEPENDENT_CODE ON
)


# === vkgc::warnings ===
# Strict warnings + warnings-as-errors across all compiler families. GNU-driver compilers (GCC-POSIX,
# Clang-POSIX, MinGW, Clang-MSYS) and "bilingual" clang-cl share `-W*` spellings; MSVC native uses
# `/W4 /WX` plus per-warning `/wNNNNN` opt-ins. Also carries the GCC-family `-pipe` /
# `-fasynchronous-unwind-tables` codegen helpers — too small a set to deserve a separate target.

add_library(vkgc_warnings INTERFACE)
add_library(vkgc::warnings ALIAS vkgc_warnings)

target_compile_options(vkgc_warnings
    INTERFACE
        # Shared GNU-driver core: GCC + Clang on all platforms (POSIX/MSYS/clang-cl).
        # Only flags GCC actually accepts go here; Clang-only `-Wno-*` lives in the next block
        # because GCC errors on unrecognized `-Wno-*` whenever the underlying category would fire
        # (with -Werror, that's a build break — even if today's GCC happens not to raise it).
        "$<$<OR:${IS_GCC_POSIX},${IS_CLANG_POSIX},${IS_MINGW},${IS_CLANG_MSYS},${IS_CLANG_CL}>:"
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

            # Vulkan enums (VkResult, VkDebugReportObjectType, ...) carry sentinel *_MAX_ENUM values
            # and grow every SDK update; enumerating every case in formatter switches is a treadmill.
            # -Wswitch (no default + missing cases) stays active, so truly-uncovered switches error.
            -Wno-switch-enum

            # Exhaustive switches over project enums (e.g. present_status) omit `default:` so a new
            # enumerator trips -Wswitch at every call site. -Wswitch-default wants the opposite (always
            # add a fallback) and would mask exactly the churn we want surfaced.
            -Wno-switch-default

            # Vulkan payload structs hit -Wpadded: the natural layout (pointer-sized handle +
            # 4-byte enum / 1-byte bool / ...) leaves unavoidable trailing padding for 8-byte
            # alignment, and reordering can't dissolve it without filler fields (worse than the
            # diagnostic). Off by default upstream; suppress so an implicit enable can't break us.
            -Wno-padded
        ">"

        # Clang-only `-Wno-*`. Kept separate from the shared block: GCC silently accepts unknown
        # `-Wno-*` at parse time, but reports `unrecognized command-line option` if the
        # corresponding diagnostic would have fired — and with -Werror, that report is fatal.
        "$<$<OR:${IS_CLANG_POSIX},${IS_CLANG_MSYS},${IS_CLANG_CL}>:"
            -Wno-braced-scalar-init

            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic

            -Wno-pre-c++17-compat

            # Project is C++23-only; the -Wc++NN-compat family flags C++20+ features as
            # "won't work in older standards" — noise for forward-only code.
            -Wno-c++20-compat

            # Vulkan-struct designated init (`{.sType{X}}`) omits pNext and payload members;
            # C++20 value-initializes them to {} / nullptr, so it's idiomatic and safe. Clang 19+
            # raises -Wmissing-designated-field-initializers on every such site for no benefit here.
            # GCC's separate -Wmissing-field-initializers stays active.
#            -Wno-missing-designated-field-initializers
        ">"

        "$<$<OR:${IS_GCC_POSIX},${IS_MINGW}>:"
            -fasynchronous-unwind-tables                # Increased reliability of backtraces

            -pipe

            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
        ">"

        "$<${IS_CLANG_CL}:"
            -Wno-unknown-pragmas
            -Wno-unknown-warning-option

            -Wno-shadow-field-in-constructor

            # CMake's Windows-Clang-CXX module injects `-TP` (force C++) on every CXX compile rule.
            # For .cxxm units clang already infers C++ from the extension, so `-TP` is redundant and
            # `-Wunused-command-line-argument` flags it — fatal under `-Werror`. Suppress project-wide;
            # we don't rely on this warning to police flag drift.
            -Wno-unused-command-line-argument

            # CLion's compiler-info probe runs clang-cl with -Weverything on a synthetic TU full of
            # reserved identifiers (`___CIDR_*`); without these, -Werror kills the probe and IDE
            # intelligence breaks. Inert for real builds (not in -Wall/-Wextra).
            -Wno-reserved-macro-identifier
            -Wno-reserved-identifier
            -Wno-unused-macros
        ">"

        # `/WX` already promotes every warning to an error, so the `/w14NNNN` form (enable at L1)
        # is sufficient — `/we4NNNN` (treat-as-error) would be redundant. Kept uniform across the
        # list for that reason.
        "$<${IS_MSVC}:"
            /W4
            /WX
            /w14242 # 'identifier': conversion from 'type1' to 'type1', possible loss of data
            /w14254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
            /w14263 # 'function': member function does not override any base class virtual member function
            /w14265 # 'classname': class has virtual functions, but destructor is not virtual
            /w14287 # 'operator': unsigned/negative constant mismatch
            /w14289 # 'variable': loop control variable declared in the for-loop is used outside the for-loop scope
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
# ELF-only. PE/COFF (Windows) already resolves every import against an import library at link time,
# so MinGW/Clang-MSYS need nothing here, and MSVC link.exe wouldn't accept these spellings anyway.
# Mach-O (macOS) ships the equivalents by default: two-level-namespace links are `-undefined error`
# (underlinking fails the link), chained fixups (macOS 12+ deployment targets) have dyld bind every
# symbol at load with no lazy-binding stubs — which is why Xcode 15+ ld warns that `-bind_at_load`
# is deprecated — and dyld remaps `__DATA_CONST` read-only once fixups are applied (RELRO's analog).

add_library(vkgc_hardening INTERFACE)
add_library(vkgc::hardening ALIAS vkgc_hardening)

target_link_options(vkgc_hardening
    INTERFACE
        "$<$<PLATFORM_ID:Linux>:"
            LINKER:-z,defs                          # Detect and reject underlinking (== --no-undefined)
            LINKER:-z,now                           # Disable lazy binding
            LINKER:-z,relro                         # Read-only segments after relocation
        ">"
)


# === vkgc::no_exceptions ===
# Disable C++ exceptions. clang-cl rejects -fno-exceptions (-Wunknown-argument, fatal under -Werror)
# and follows MSVC convention; everything else takes the GNU-driver spelling.
#
# `_HAS_EXCEPTIONS=0` on MSVC / clang-cl is best-effort, not airtight. It rewrites the inline
# `_THROW(...)` macro to terminate and adds `_Doraise()` overrides on the std exception classes, so
# throws written into STL headers redirect through std::terminate. It does NOT remove APIs like
# `vector::at()` or recompile the runtime: out-of-line throw helpers (`_Xout_of_range`,
# `_Xlength_error`, …) are `_CRTIMP2_PURE` in <xutility> and live in msvcp140.dll, built with
# exceptions on. An out-of-range `vector::at()` still throws inside the DLL; under /EHs-c- the user
# frames carry no unwind tables, so it rides through to std::terminate instead of being catchable.
# Treat throwing STL accessors (`at`, `map::at`, `stoi`/`stoX`, …) as banned rather than relying on
# this target to neutralize them.

add_library(vkgc_no_exceptions INTERFACE)
add_library(vkgc::no_exceptions ALIAS vkgc_no_exceptions)

target_compile_options(vkgc_no_exceptions
    INTERFACE
        "$<$<OR:${IS_GCC_POSIX},${IS_CLANG_POSIX},${IS_MINGW},${IS_CLANG_MSYS}>:"
            -fno-exceptions
        ">"

        "$<$<OR:${IS_MSVC},${IS_CLANG_CL}>:"
            /EHs-c-
            /GR-
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
# VKGC_DO_CHECK and VKGC_DEBUG_VULKAN gate softer checks and validation-layer messaging to
# Debug/RelWithDebInfo.

add_library(vkgc_diagnostics INTERFACE)
add_library(vkgc::diagnostics ALIAS vkgc_diagnostics)

target_compile_definitions(vkgc_diagnostics
    INTERFACE
        VKGC_DO_ENSURE=1
        VKGC_DO_CHECK=$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,1,0>

        VKGC_DEBUG_VULKAN=$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,1,0>
)


# === vkgc::sanitizers::address ===
# AddressSanitizer (compile + link). Gated by VKGC_ENABLE_ASAN; when OFF the target is empty and
# inert, so chapters link it unconditionally. -fno-omit-frame-pointer keeps stack traces useful on
# the GNU-driver side. MSVC-style frontends use /fsanitize=address and need /INCREMENTAL:NO at link
# (CMake injects /INCREMENTAL into CMAKE_EXE_LINKER_FLAGS_DEBUG); /RTC1 stripping lives in the root
# CMakeLists.txt with the flag string.
#
# Composition: combines with vkgc::no_exceptions (/EHs-c- + _HAS_EXCEPTIONS=0 stay valid under ASan)
# and vkgc::hardening (ASan resolves its own runtime symbols, so -z,defs / -no-undefined don't
# false-positive on the injected interceptors).

add_library(vkgc_sanitizers_address INTERFACE)
add_library(vkgc::sanitizers::address ALIAS vkgc_sanitizers_address)

if (VKGC_ENABLE_ASAN)
    target_compile_options(vkgc_sanitizers_address
        INTERFACE
            "$<$<OR:${IS_GCC_POSIX},${IS_CLANG_POSIX},${IS_MINGW},${IS_CLANG_MSYS}>:"
                -fsanitize=address
                -fno-omit-frame-pointer
            ">"

            "$<$<OR:${IS_MSVC},${IS_CLANG_CL}>:"
                /fsanitize=address
            ">"
    )

    target_link_options(vkgc_sanitizers_address
        INTERFACE
            "$<$<OR:${IS_GCC_POSIX},${IS_CLANG_POSIX},${IS_MINGW},${IS_CLANG_MSYS}>:"
                -fsanitize=address
            ">"

            "$<$<OR:${IS_MSVC},${IS_CLANG_CL}>:"
                /INCREMENTAL:NO
            ">"
    )

    # clang-cl embeds /defaultlib:clang_rt.asan_*.lib pragmas into instrumented objects and relies on
    # the driver to inject -libpath:<resource>/lib/windows at link. CMake calls lld-link.exe directly
    # via vs_link_exe (no driver), so that path never reaches the linker and the symbols come back
    # undefined — add the compiler-rt lib dir to the search path ourselves. Not needed for native MSVC
    # (compiler-rt is on the default LIB) or clang-MSYS (GNU driver auto-resolves its runtime).
    # The probe result is cached so a re-configure doesn't fork the compiler again.
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
            AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")

        if (NOT DEFINED CACHE{VKGC_CLANG_CL_RESOURCE_DIR})
            execute_process(
                    COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
                    OUTPUT_VARIABLE _vkgc_clang_resource_dir
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    COMMAND_ERROR_IS_FATAL ANY
            )
            set(VKGC_CLANG_CL_RESOURCE_DIR "${_vkgc_clang_resource_dir}"
                    CACHE INTERNAL "clang-cl compiler-rt resource dir (cached `clang -print-resource-dir`)")
            unset(_vkgc_clang_resource_dir)
        endif ()

        target_link_directories(vkgc_sanitizers_address
                INTERFACE "${VKGC_CLANG_CL_RESOURCE_DIR}/lib/windows"
        )
    endif ()
endif ()


# === vkgc::sanitizers::undefined ===
# UndefinedBehaviorSanitizer (compile + link). Gated by VKGC_ENABLE_UBSAN; when OFF the target is
# empty and inert, so chapters link it unconditionally — symmetric with vkgc::sanitizers::address,
# and the two stack (-fsanitize=address,undefined is supported).
#
# Clang-only: GCC ICEs on this project's C++20 module units and MSVC native has no UBSan, so both are
# refused at configure (see the root CMakeLists.txt guard) and need no branch here.
#   * Clang-MSYS uses -fsanitize=undefined against compiler-rt's ubsan runtime, with
#     -fno-omit-frame-pointer for readable traces and default print-and-continue (no
#     -fno-sanitize-recover) so all findings in a run are reported.
#   * clang-cl instruments in trap mode (-fsanitize-trap=undefined): no ubsan runtime needed, which is
#     the point — vs_link_exe bypasses the clang driver and can't resolve the compiler-rt libpath (the
#     ASan issue above). Tradeoff: UB aborts via int3/ud2 with no diagnostic text rather than a report.
#
# Composition: combines with vkgc::no_exceptions (/EHs-c-, -fno-exceptions) and vkgc::hardening; the
# instrumented checks don't trip -z,defs / -no-undefined.

add_library(vkgc_sanitizers_undefined INTERFACE)
add_library(vkgc::sanitizers::undefined ALIAS vkgc_sanitizers_undefined)

if (VKGC_ENABLE_UBSAN)
    target_compile_options(vkgc_sanitizers_undefined
        INTERFACE
            "$<$<OR:${IS_CLANG_POSIX},${IS_CLANG_MSYS}>:"
                -fsanitize=undefined
                -fno-omit-frame-pointer
            ">"

            "$<${IS_CLANG_CL}:"
                -fsanitize=undefined
                -fsanitize-trap=undefined
            ">"
    )

    target_link_options(vkgc_sanitizers_undefined
        INTERFACE
            "$<$<OR:${IS_CLANG_POSIX},${IS_CLANG_MSYS}>:"
                -fsanitize=undefined
            ">"
    )
endif()


# === vkgc::platform_quirks ===
# Platform-level defines not tied to any specific dependency. On Windows: NOMINMAX suppresses the
# min/max macros from <windows.h>, and WIN32_LEAN_AND_MEAN trims the surface to core Win32 (no
# winsock / OLE / COM by default). Both are inert in TUs that never include <windows.h>;
# GLFW_NATIVE_INCLUDE_NONE keeps GLFW out of that path, so this currently only affects user TUs
# that include <windows.h> directly. Dependency-coupled platform defines (VK_USE_PLATFORM_*,
# GLFW_EXPOSE_NATIVE_*) belong with their vkgc::dependencies::* targets.

add_library(vkgc_platform_quirks INTERFACE)
add_library(vkgc::platform_quirks ALIAS vkgc_platform_quirks)

target_compile_definitions(vkgc_platform_quirks
    INTERFACE
        "$<$<PLATFORM_ID:Windows>:"
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            VC_EXTRA_LEAN   # optional, even more aggressive
        ">"
)
