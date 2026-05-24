# Common-source attachment helper (transitional).
#
# Phase A keeps the shared sources under src/common/ as TU-level sources attached to each chapter
# executable. Phase B will extract them to a real vkgc::common OBJECT/STATIC library; the call
# sites become `target_link_libraries(<target> PRIVATE vkgc::common)` and this helper goes away.
# Until then this is the one piece of build configuration each chapter still invokes by name.

function(vkgc_attach_common_sources TARGET_NAME)

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

            # platform
            ${COMMON_DIR}/platform/window.cxx

            # vulkan core
            ${COMMON_DIR}/vulkan/instance.cxx
            ${COMMON_DIR}/vulkan/device.cxx
            ${COMMON_DIR}/vulkan/device_creation.cxx
            ${COMMON_DIR}/vulkan/device_features.cxx
            ${COMMON_DIR}/vulkan/frame_ring.cxx
            ${COMMON_DIR}/vulkan/vma_implementation.cxx

            # vulkan/presentation
            ${COMMON_DIR}/vulkan/presentation/surface.cxx
            ${COMMON_DIR}/vulkan/presentation/presenter.cxx

            # vulkan/registry
            ${COMMON_DIR}/vulkan/registry/registry.cxx
            ${COMMON_DIR}/vulkan/registry/destructors.cxx

            # vulkan/helpers
            ${COMMON_DIR}/vulkan/helpers/resource_helpers.cxx

        PRIVATE
            FILE_SET cookbook_modules TYPE CXX_MODULES
                BASE_DIRS ${COMMON_DIR}
                FILES
                    # app
                    ${COMMON_DIR}/app/bootstrap.cxxm

                    # platform
                    ${COMMON_DIR}/platform/window.cxxm

                    # vulkan core
                    ${COMMON_DIR}/vulkan/instance.cxxm
                    ${COMMON_DIR}/vulkan/device.cxxm
                    ${COMMON_DIR}/vulkan/device_features.cxxm
                    ${COMMON_DIR}/vulkan/frame_ring.cxxm

                    # vulkan/presentation
                    ${COMMON_DIR}/vulkan/presentation/surface.cxxm
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
    )

endfunction()
