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
# Linux/Darwin/MinGW/Clang-MSYS only — MSVC link.exe doesn't accept these.

add_library(vkgc_hardening INTERFACE)
add_library(vkgc::hardening ALIAS vkgc_hardening)

target_link_options(vkgc_hardening
    INTERFACE
        "$<$<PLATFORM_ID:Linux>:"
            LINKER:-z,defs                          # Detect and reject underlinking
            LINKER:-z,now                           # Disable lazy binding
            LINKER:-z,relro                         # Read-only segments after relocation
        ">"

        "$<$<PLATFORM_ID:Darwin>:"
            LINKER:-bind_at_load                    # Disable lazy binding
        ">"

        "$<$<OR:${IS_GNU_LINUX},${IS_MINGW},${IS_CLANG_MSYS}>:"
            LINKER:-no-undefined                    # Report unresolved symbol references from regular object files
            LINKER:-no-allow-shlib-undefined        # Disallows undefined symbols in shared libraries
            LINKER:-unresolved-symbols=report-all
        ">"
)


# === vkgc::no_exceptions ===
# Disable C++ exceptions. clang-cl rejects -fno-exceptions (-Wunknown-argument, fatal under
# -Werror) and follows MSVC convention; everything else takes the GNU-driver spelling.
# `_HAS_EXCEPTIONS=0` routes MSVC-STL throw expressions through std::terminate() and removes
# APIs like vector::at() — required because MSVC STL still contains throw statements even with
# /EHs-c-.

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
        VKGC_DO_CHECK=$<IF:$<CONFIG:Debug>,1,0>

        VKGC_DEBUG_VULKAN=$<IF:$<CONFIG:Debug>,1,0>
)


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
