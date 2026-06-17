add_executable(chapter02_glm main.cxx)

vkgc_configure_chapter_target(chapter02_glm)
vkgc_attach_common_sources(chapter02_glm)

vkgc_compile_shaders(chapter02_glm
    PROFILE
        spirv_1_5
    CAPABILITIES
        spirv_1_5
    GLOB
        chapter02/glm/*.slang
)

target_link_libraries(chapter02_glm
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
