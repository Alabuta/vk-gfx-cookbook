add_executable(chapter02_swapchain main.cxx)

vkgc_configure_chapter_target(chapter02_swapchain)
vkgc_attach_common_sources(chapter02_swapchain)

target_link_libraries(chapter02_swapchain
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
        vkgc::dependencies::stdcxx_extras

        vkgc::config::cookbook_paths
)
