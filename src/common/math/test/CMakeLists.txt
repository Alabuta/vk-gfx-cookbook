# Standalone round-trip exercise for the header-only math/pack_unpack.hxx.
#
# Deliberately does NOT call vkgc_attach_common_sources(): the header pulls in
# nothing but GLM, so the test links only vkgc::dependencies::math and the
# policy/codegen interfaces. That keeps the Vulkan/windowing TUs out of the
# build and makes this a tight compile-and-run check of the header against the
# project's -Werror -Wconversion wall.
#
# stdcxx_extras is the one non-math link: it carries -lstdc++exp, where
# libstdc++ parks the C++23 <print> terminal runtime (__open_terminal et al.).
# Inert on MSVC-STL / libc++, so it stays portable.

add_executable(vkgc_math_pack_unpack_test pack_unpack_test.cxx)

vkgc_configure_chapter_target(vkgc_math_pack_unpack_test)

target_include_directories(vkgc_math_pack_unpack_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(vkgc_math_pack_unpack_test
    PRIVATE
        vkgc::cxx_runtime
        vkgc::warnings
        vkgc::hardening
        vkgc::no_exceptions
        vkgc::sanitizers::address
        vkgc::sanitizers::undefined
        vkgc::diagnostics
        vkgc::platform_quirks

        vkgc::dependencies::math
        vkgc::dependencies::stdcxx_extras
)

# Sibling exercise for math/transforms.hxx — same header-only, math-only link
# surface as the pack_unpack test above.
add_executable(vkgc_math_transforms_test transforms_test.cxx)

vkgc_configure_chapter_target(vkgc_math_transforms_test)

target_include_directories(vkgc_math_transforms_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(vkgc_math_transforms_test
    PRIVATE
        vkgc::cxx_runtime
        vkgc::warnings
        vkgc::hardening
        vkgc::no_exceptions
        vkgc::sanitizers::address
        vkgc::sanitizers::undefined
        vkgc::diagnostics
        vkgc::platform_quirks

        vkgc::dependencies::math
        vkgc::dependencies::stdcxx_extras
)
