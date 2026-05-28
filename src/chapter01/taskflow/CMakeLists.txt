add_executable(chapter01_taskflow main.cxx)

vkgc_configure_chapter_target(chapter01_taskflow)
vkgc_attach_common_sources(chapter01_taskflow)

target_link_libraries(chapter01_taskflow
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
        vkgc::dependencies::concurrency
        vkgc::dependencies::stdcxx_extras

        vkgc::config::cookbook_paths
)
