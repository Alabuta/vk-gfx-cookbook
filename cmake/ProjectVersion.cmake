set(COOKBOOK_VULKAN_API_VERSION_MAJOR 1)
set(COOKBOOK_VULKAN_API_VERSION_MINOR 4)

configure_file(
    "${CMAKE_SOURCE_DIR}/config/config.hxx.in"
    "${CMAKE_SOURCE_DIR}/src/config.hxx"
)
