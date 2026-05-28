# Compiler/platform dispatch via generator expressions (evaluated per-target at generation time).
# Uses $<CXX_COMPILER_ID:...> not $<COMPILE_LANG_AND_ID:CXX,...>: the latter is compile-only, while
# CXX_COMPILER_ID also works in link-library and link-option contexts.
#
# Consumed across cmake/Policies.cmake and cmake/Interfaces.cmake; defining them once keeps every
# policy/interface target dispatched against the same compiler-family probes.
#
# POSIX = any non-Windows platform (Linux, macOS, *BSD). Naming `_POSIX` rather than `_LINUX`
# avoids the macOS-GCC and macOS-Clang blind spots that an `IS_GNU_LINUX` would silently include.
set(IS_GCC_POSIX   "$<AND:$<CXX_COMPILER_ID:GNU>,$<NOT:$<PLATFORM_ID:Windows>>>")
set(IS_CLANG_POSIX "$<AND:$<CXX_COMPILER_ID:Clang>,$<NOT:$<PLATFORM_ID:Windows>>>")
set(IS_MINGW       "$<AND:$<CXX_COMPILER_ID:GNU>,$<PLATFORM_ID:Windows>>")
set(IS_CLANG_MSYS  "$<AND:$<CXX_COMPILER_ID:Clang>,$<PLATFORM_ID:Windows>,$<CXX_COMPILER_FRONTEND_VARIANT:GNU>>")
set(IS_CLANG_CL    "$<AND:$<CXX_COMPILER_ID:Clang>,$<PLATFORM_ID:Windows>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>")
set(IS_MSVC        "$<CXX_COMPILER_ID:MSVC>")
