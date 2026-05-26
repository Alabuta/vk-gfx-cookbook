add_executable(chapter01_glslang main.cxx)

vkgc_configure_chapter_target(chapter01_glslang)
vkgc_attach_common_sources(chapter01_glslang)

target_link_libraries(chapter01_glslang
    PRIVATE
        vkgc::cxx_runtime
        vkgc::warnings
        vkgc::hardening
        vkgc::no_exceptions
        vkgc::sanitizers::address
        vkgc::diagnostics
        vkgc::platform_quirks

        vkgc::dependencies::vulkan
        vkgc::dependencies::windowing
        vkgc::dependencies::concurrency
        vkgc::dependencies::shaders
        vkgc::dependencies::stdcxx_extras

        vkgc::config::cookbook_paths
)
