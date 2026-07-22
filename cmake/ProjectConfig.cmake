# Per-chapter configuration helpers.
#
# `vkgc_configure_chapter_target` sets non-transitive target properties that INTERFACE libraries
# can't carry and that executables don't inherit from CMAKE_* variables (e.g. DEBUG_POSTFIX, which
# CMAKE_<CONFIG>_POSTFIX only initializes on non-executable targets).
#
# `vkgc_attach_common_sources` is transitional: Phase A attaches the shared src/common/ sources to
# each chapter executable as TUs; Phase B extracts them to a vkgc::common OBJECT/STATIC library,
# turning call sites into `target_link_libraries(<target> PRIVATE vkgc::common)`.

# Standalone-binary support: clang64/libc++ links libc++.dll dynamically, so a binary launched
# outside a CLANG64 shell can't find it. Locate the runtime DLL next to the compiler so
# vkgc_configure_chapter_target can stage it beside each executable. Clang-MSYS only (the GNU-
# frontend clang that defaults to -stdlib=libc++); sanitized builds are dev-only and run from
# the shell, so skip them.
if (WIN32
        AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"
        AND NOT VKGC_ENABLE_ASAN
        AND NOT VKGC_ENABLE_UBSAN)

    get_filename_component(_vkgc_cxx_bindir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(VKGC_LIBCXX_DLL
            NAMES libc++.dll
            PATHS "${_vkgc_cxx_bindir}"
            NO_DEFAULT_PATH)
    unset(_vkgc_cxx_bindir)

    if (NOT VKGC_LIBCXX_DLL)
        message(STATUS
                "vkgc: libc++.dll not found beside the compiler; executables will need it on PATH")
    endif ()
endif ()

function(vkgc_configure_chapter_target TARGET_NAME)

    if (NOT TARGET "${TARGET_NAME}")
        message(FATAL_ERROR
                "vkgc_configure_chapter_target: target '${TARGET_NAME}' does not exist")
    endif ()

    set_target_properties(${TARGET_NAME}
        PROPERTIES
            DEBUG_POSTFIX .dbg
    )

    if (VKGC_LIBCXX_DLL)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${VKGC_LIBCXX_DLL}" "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                COMMENT "Staging libc++.dll next to ${TARGET_NAME}"
                VERBATIM)
    endif ()

endfunction()

function(vkgc_attach_common_sources TARGET_NAME)

    if (NOT TARGET "${TARGET_NAME}")
        message(FATAL_ERROR
                "vkgc_attach_common_sources: target '${TARGET_NAME}' does not exist")
    endif ()

    set(COMMON_DIR ${CMAKE_SOURCE_DIR}/src/common)

    target_include_directories(${TARGET_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${COMMON_DIR}
    )

    target_sources(${TARGET_NAME}
        PRIVATE
            # app
            ${COMMON_DIR}/app/bootstrap.cxx

            # io
            ${COMMON_DIR}/io/file_io.cxx

            # platform
            ${COMMON_DIR}/platform/window.cxx

            # vulkan core
            ${COMMON_DIR}/vulkan/instance.cxx
            ${COMMON_DIR}/vulkan/device.cxx
            ${COMMON_DIR}/vulkan/device_creation.cxx
            ${COMMON_DIR}/vulkan/device_features.cxx
            ${COMMON_DIR}/vulkan/context.cxx
            ${COMMON_DIR}/vulkan/frame_ring.cxx
            ${COMMON_DIR}/vulkan/vma_implementation.cxx
            ${COMMON_DIR}/vulkan/vertex_layout.cxx

            # vulkan/presentation
            ${COMMON_DIR}/vulkan/presentation/presenter.cxx
            ${COMMON_DIR}/vulkan/presentation/swapchain.cxx

            # vulkan/registry
            ${COMMON_DIR}/vulkan/registry/registry.cxx
            ${COMMON_DIR}/vulkan/registry/memory_resources.cxx
            ${COMMON_DIR}/vulkan/registry/synchronization.cxx
            ${COMMON_DIR}/vulkan/registry/command_buffers.cxx
            ${COMMON_DIR}/vulkan/registry/pipeline_objects.cxx
            ${COMMON_DIR}/vulkan/registry/destructors.cxx

            # vulkan/helpers
            ${COMMON_DIR}/vulkan/helpers/buffers.cxx
            ${COMMON_DIR}/vulkan/helpers/images.cxx
            ${COMMON_DIR}/vulkan/helpers/memory_binding.cxx

            # geometry
            ${COMMON_DIR}/geometry/box.cxx

        PRIVATE
            FILE_SET cookbook_modules TYPE CXX_MODULES
                BASE_DIRS ${COMMON_DIR}
                FILES
                    # app
                    ${COMMON_DIR}/app/bootstrap.cxxm

                    # config
                    ${COMMON_DIR}/config/cookbook_paths.cxxm

                    # io
                    ${COMMON_DIR}/io/file_io.cxxm

                    # platform
                    ${COMMON_DIR}/platform/window.cxxm

                    # utility
                    ${COMMON_DIR}/utility/scope_guard.cxxm

                    # vulkan core
                    ${COMMON_DIR}/vulkan/instance.cxxm
                    ${COMMON_DIR}/vulkan/device.cxxm
                    ${COMMON_DIR}/vulkan/device_features.cxxm
                    ${COMMON_DIR}/vulkan/context.cxxm
                    ${COMMON_DIR}/vulkan/frame_ring.cxxm
                    ${COMMON_DIR}/vulkan/vertex_layout.cxxm

                    # vulkan/presentation
                    ${COMMON_DIR}/vulkan/presentation/presenter.cxxm

                    # vulkan/registry
                    ${COMMON_DIR}/vulkan/registry/registry.cxxm
                    ${COMMON_DIR}/vulkan/registry/object_handle.cxxm
                    ${COMMON_DIR}/vulkan/registry/payload.cxxm

                    # vulkan/helpers
                    ${COMMON_DIR}/vulkan/helpers/resource_helpers.cxxm

                    # vulkan/utility
                    ${COMMON_DIR}/vulkan/utility/ext_structs_chain.cxxm
                    ${COMMON_DIR}/vulkan/utility/slot_map.cxxm

                    # geometry
                    ${COMMON_DIR}/geometry/geometry.cxxm
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            vkgc::dependencies::vulkan
            vkgc::dependencies::windowing
            vkgc::dependencies::math
            vkgc::dependencies::concurrency
            vkgc::dependencies::stdcxx_extras
            vkgc::config::cookbook_paths
    )

endfunction()
