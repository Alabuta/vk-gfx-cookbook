add_executable(chapter01_glfw main.cxx)

vkgc_attach_common_sources(chapter01_glfw)

target_link_libraries(chapter01_glfw
    PRIVATE
        vkgc::cxx_runtime
        vkgc::warnings
        vkgc::hardening
        vkgc::no_exceptions
        vkgc::diagnostics
        vkgc::platform_quirks

        vkgc::dependencies::vulkan
        vkgc::dependencies::windowing
        vkgc::dependencies::concurrency
        vkgc::dependencies::stdcxx_extras

        vkgc::config::cookbook_paths
)
