# Build-time shader compilation.
#
# The cookbook authors its runtime shaders in Slang (the book's primary shader language). They are
# compiled to SPIR-V by the SDK's `slangc` — located as VKGC_SLANGC_EXECUTABLE in cmake/Dependencies.cmake
# — and the .spv lands in the repo's .cache/ dir, where chapter binaries load it at runtime via the
# COOKBOOK_CACHE_DIR_STRING define (see cmake/Interfaces.cmake). This replaces hand-running slangc.
#
# Note: chapter01/glslang's GLSL sources are deliberately NOT compiled here — they are inputs to the
# chapter01_glslang runtime exercise, which compiles them in-process via the glslang C API.
#
#   vkgc_compile_shaders(<chapter_target>
#       PROFILE       <slang-profile>          # required, e.g. spirv_1_5
#       [TARGET       <slang-target>]          # default: spirv
#       [SHADERS      <src> [<src> ...]]       # explicit sources, each relative to <repo>/shaders/
#       [GLOB         <pat> [<pat> ...]]       # glob patterns relative to shaders/ (non-recursive)
#       [GLOB_RECURSE <pat> [<pat> ...]]       # glob patterns relative to shaders/ (recursive)
#       [INCLUDE_DIRS <dir> ...]               # -I; each relative to <repo>/shaders/ or absolute
#       [DEFINES      <name>[=<value>] ...]    # -D
#       [CAPABILITIES <cap> ...])              # -capability; optional target features to assume
#
# At least one of SHADERS / GLOB / GLOB_RECURSE must yield a file; the three combine (de-duplicated)
# into the compiled set. Glob patterns are evaluated relative to shaders/ and use CONFIGURE_DEPENDS,
# so adding or removing a matching shader file re-triggers a CMake regeneration on the next build —
# no manual re-configure needed. (CMake still discourages globbing build inputs; the trade-off here
# is a cheap glob check each build in exchange for not maintaining an explicit list.)
#
# Output layout mirrors the source tree under .cache/: a source's path relative to shaders/ has its
# extension swapped to .spv and is written at the same relative path. So
#   shaders/chapter02/hello_triangle/chapter02_triangle.slang
#       -> .cache/chapter02/hello_triangle/chapter02_triangle.spv
# This keeps the output namespace collision-free as chapters multiply.
#
# shaders/common/ is auto-added as an -I include path when it exists, so chapters can `#include` /
# `import` shared shader code without restating the path.
#
# Incremental rebuilds: each shader is compiled with `-depfile`, fed to CMake's DEPFILE so Ninja (the
# project's generator) recompiles a shader when ANY file it pulls in via #include/import changes — not
# just when its own top file does. The compiled outputs are gathered into a generated
# <target>_shaders target that <target> depends on, so a chapter build always brings its SPIR-V up to
# date before the binary runs.

function(vkgc_compile_shaders TARGET_NAME)

    if (NOT TARGET "${TARGET_NAME}")
        message(FATAL_ERROR
                "vkgc_compile_shaders: target '${TARGET_NAME}' does not exist")
    endif ()

    if (NOT VKGC_SLANGC_EXECUTABLE)
        message(FATAL_ERROR
                "vkgc_compile_shaders: slangc not found (VKGC_SLANGC_EXECUTABLE is unset). "
                "Ensure the Vulkan SDK is installed and VULKAN_SDK points at it.")
    endif ()

    cmake_parse_arguments(ARG ""
            "PROFILE;TARGET"
            "SHADERS;GLOB;GLOB_RECURSE;INCLUDE_DIRS;DEFINES;CAPABILITIES"
            ${ARGN})

    if (NOT ARG_PROFILE)
        message(FATAL_ERROR
                "vkgc_compile_shaders: no PROFILE listed for '${TARGET_NAME}'")
    endif ()

    if (NOT ARG_TARGET)
        set(ARG_TARGET spirv)
    endif ()

    set(shader_src_dir ${CMAKE_SOURCE_DIR}/shaders)
    set(shader_out_dir ${CMAKE_SOURCE_DIR}/.cache)

    # --- Resolve the shader set: explicit SHADERS plus any GLOB / GLOB_RECURSE pattern matches,
    #     all as paths relative to shaders/. CONFIGURE_DEPENDS re-globs at build time so newly added
    #     or removed shader files are noticed without a manual re-configure. ---
    set(shader_list ${ARG_SHADERS})

    foreach (pattern IN LISTS ARG_GLOB)
        file(GLOB matched CONFIGURE_DEPENDS
                RELATIVE ${shader_src_dir} ${shader_src_dir}/${pattern})
        list(APPEND shader_list ${matched})
    endforeach ()

    foreach (pattern IN LISTS ARG_GLOB_RECURSE)
        file(GLOB_RECURSE matched CONFIGURE_DEPENDS
                RELATIVE ${shader_src_dir} ${shader_src_dir}/${pattern})
        list(APPEND shader_list ${matched})
    endforeach ()

    list(REMOVE_DUPLICATES shader_list)

    if (NOT shader_list)
        message(FATAL_ERROR
                "vkgc_compile_shaders: no shaders for '${TARGET_NAME}' "
                "(SHADERS/GLOB/GLOB_RECURSE matched nothing)")
    endif ()

    # --- Include search paths (-I): the shared shaders/common dir (when present) plus any explicit
    #     INCLUDE_DIRS, with relative entries resolved against shaders/. ---
    set(include_flags)
    if (IS_DIRECTORY ${shader_src_dir}/common)
        list(APPEND include_flags -I${shader_src_dir}/common)
    endif ()

    foreach (inc IN LISTS ARG_INCLUDE_DIRS)
        if (IS_ABSOLUTE ${inc})
            list(APPEND include_flags -I${inc})
        else ()
            list(APPEND include_flags -I${shader_src_dir}/${inc})
        endif ()
    endforeach ()

    # --- Preprocessor defines (-D). ---
    set(define_flags)
    foreach (def IN LISTS ARG_DEFINES)
        list(APPEND define_flags -D${def})
    endforeach ()

    # --- Optional target capabilities (-capability): features slangc may assume the target supports
    #     and generate code accordingly (e.g. spvShaderClockKHR, spvRayTracingMotionBlurNV). ---
    set(capability_flags)
    foreach (cap IN LISTS ARG_CAPABILITIES)
        list(APPEND capability_flags -capability ${cap})
    endforeach ()

    set(spv_outputs)
    foreach (src_rel IN LISTS shader_list)
        set(src_abs ${shader_src_dir}/${src_rel})

        if (NOT EXISTS ${src_abs})
            message(FATAL_ERROR
                    "vkgc_compile_shaders: shader source '${src_abs}' does not exist")
        endif ()

        # Mirror the source-relative path under .cache/, swapping the extension to .spv.
        get_filename_component(rel_dir ${src_rel} DIRECTORY)
        get_filename_component(stem ${src_rel} NAME_WE)
        if (rel_dir STREQUAL "")
            set(out_rel ${stem}.spv)
        else ()
            set(out_rel ${rel_dir}/${stem}.spv)
        endif ()

        set(out_abs ${shader_out_dir}/${out_rel})
        get_filename_component(out_dir ${out_abs} DIRECTORY)
        set(depfile ${out_abs}.d)

        add_custom_command(
            OUTPUT ${out_abs}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${out_dir}
            COMMAND ${VKGC_SLANGC_EXECUTABLE}
                    ${src_abs}
                    -profile ${ARG_PROFILE}
                    -target ${ARG_TARGET}
                    -std 2026
                    # 41012 reports the automatic profile widening that fragment-stage texture
                    # sampling always triggers under a bare spirv_* profile; the escalation is the
                    # desired behavior and does not change the emitted SPIR-V.
                    -warnings-disable 41012
                    ${include_flags}
                    ${define_flags}
                    ${capability_flags}
                    -depfile ${depfile}
                    -o ${out_abs}
            DEPENDS ${src_abs}
            DEPFILE ${depfile}
            COMMENT "Compiling shader: ${src_rel} -> .cache/${out_rel}"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )

        list(APPEND spv_outputs ${out_abs})
    endforeach ()

    add_custom_target(${TARGET_NAME}_shaders DEPENDS ${spv_outputs})
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_shaders)

endfunction()
