# Compiler/platform dispatch via generator expressions (evaluated per-target at generation time).
# Using $<CXX_COMPILER_ID:...> rather than $<COMPILE_LANG_AND_ID:CXX,...> because the latter is
# only valid for compile properties; CXX_COMPILER_ID works in link-library and link-option contexts too.
#
# These aliases are consumed across cmake/Policies.cmake and cmake/Interfaces.cmake; defining them
# once here keeps every policy/interface target dispatched against the same compiler-family probes.

set(IS_GNU_LINUX  "$<AND:$<CXX_COMPILER_ID:GNU>,$<NOT:$<PLATFORM_ID:Windows>>>")
set(IS_MINGW      "$<AND:$<CXX_COMPILER_ID:GNU>,$<PLATFORM_ID:Windows>>")
set(IS_CLANG_MSYS "$<AND:$<CXX_COMPILER_ID:Clang>,$<PLATFORM_ID:Windows>,$<CXX_COMPILER_FRONTEND_VARIANT:GNU>>")
set(IS_CLANG_CL   "$<AND:$<CXX_COMPILER_ID:Clang>,$<PLATFORM_ID:Windows>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>")
set(IS_MSVC       "$<CXX_COMPILER_ID:MSVC>")
