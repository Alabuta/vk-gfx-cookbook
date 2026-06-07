add_executable(chapter02_hello_triangle main.cxx)

vkgc_configure_chapter_target(chapter02_hello_triangle)
vkgc_attach_common_sources(chapter02_hello_triangle)

target_link_libraries(chapter02_hello_triangle
    PRIVATE
        vkgc::cxx_runtime
        vkgc::warnings
        vkgc::hardening
        vkgc::no_exceptions
        vkgc::sanitizers::address
        vkgc::sanitizers::undefined
        vkgc::diagnostics
        vkgc::platform_quirks

        vkgc::dependencies::vulkan
        vkgc::dependencies::windowing
        vkgc::dependencies::math
        vkgc::dependencies::concurrency
        vkgc::dependencies::shaders
        vkgc::dependencies::stdcxx_extras

        vkgc::config::cookbook_paths
)
