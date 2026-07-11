add_executable(chapter03_gltf main.cxx)

vkgc_configure_chapter_target(chapter03_gltf)
vkgc_attach_common_sources(chapter03_gltf)

vkgc_compile_shaders(chapter03_gltf
    PROFILE
        spirv_1_5
    CAPABILITIES
        spirv_1_5
        SPV_EXT_descriptor_indexing
        GL_EXT_nonuniform_qualifier
    GLOB
        chapter03/gltf/*.slang
)

target_link_libraries(chapter03_gltf
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
        vkgc::dependencies::model_loading
        vkgc::dependencies::concurrency
        vkgc::dependencies::shaders
        vkgc::dependencies::stdcxx_extras

        vkgc::config::cookbook_paths
)
