# Workaround: CMake 4.1 (and earlier) does not configure C++20 module dependency
# scanning for clang-cl (Clang's MSVC frontend variant). Modules/Compiler/Clang-CXX.cmake
# only wires CMAKE_CXX_SCANDEP_SOURCE for the GNU frontend; the MSVC frontend branch
# is empty. With a CXX_MODULES file set, CMake then aborts generation with:
#   "...the compiler does not provide a way to discover the import graph dependencies..."
#
# Mirror the GNU-frontend block, but call clang-scan-deps explicitly. The depfile-gen
# flags (-MT/-MD/-MF) are NOT bilingual: in clang-cl driver mode -MF is unknown, and
# -MT/-MD get parsed as the /MT//MD CRT switches (boolean, no argument), so the path
# following -MT is mistakenly consumed as a positional input file and clang errors out
# with "cannot specify -o when generating multiple output files". Forward each dep-gen
# arg through the /clang: pass-through prefix so they reach the GCC-frontend Args parser.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND "x${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "xMSVC"
   AND NOT DEFINED CMAKE_CXX_SCANDEP_SOURCE
   AND CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)

    if(CMAKE_CXX_COMPILER_CLANG_RESOURCE_DIR)
        set(_clang_scan_deps_resource_dir
            " -resource-dir \"${CMAKE_CXX_COMPILER_CLANG_RESOURCE_DIR}\"")
    else()
        set(_clang_scan_deps_resource_dir "")
    endif()

    string(CONCAT CMAKE_CXX_SCANDEP_SOURCE
        "\"${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS}\""
        " -format=p1689"
        " --"
        " <CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS>"
        " -x c++ <SOURCE> -c -o <OBJECT>"
        "${_clang_scan_deps_resource_dir}"
        " /clang:-MT /clang:<DYNDEP_FILE>"
        " /clang:-MD /clang:-MF /clang:<DEP_FILE>"
        " > <DYNDEP_FILE>.tmp"
        " && \"${CMAKE_COMMAND}\" -E rename <DYNDEP_FILE>.tmp <DYNDEP_FILE>")
    unset(_clang_scan_deps_resource_dir)

    set(CMAKE_CXX_MODULE_MAP_FORMAT "clang")
    set(CMAKE_CXX_MODULE_MAP_FLAG "@<MODULE_MAP_FILE>")
    set(CMAKE_CXX_MODULE_BMI_ONLY_FLAG "--precompile")
endif()
