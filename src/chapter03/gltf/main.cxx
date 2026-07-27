#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <array>
#include <filesystem>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <volk.h>
#include <vk_mem_alloc.h>

#include "math/glm.hxx"
#include "math/transforms.hxx"

#include "GLFW/glfw3.h"

#include "vulkan/assert.hxx"
#include "vulkan/format.hxx"

std::string_view constexpr kCacheDir{COOKBOOK_CACHE_DIR_STRING};
std::string_view constexpr kAssetDir{COOKBOOK_ASSET_DIR_STRING};

import vkgc.bootstrap;
import vkgc.scope_guard;
import vkgc.file_io;
import vkgc.window;
import vkgc.vulkan_instance;
import vkgc.vulkan_context;
import vkgc.vulkan_device;
import vkgc.vulkan_handle;
import vkgc.vulkan_object_registry;
import vkgc.vulkan_resource_helpers;
import vkgc.vulkan_presenter;
import vkgc.vulkan_frame_ring;
import vkgc.vulkan_helpers;
import vkgc.vulkan_vertex_layout;

namespace vkgc
{
    std::uint32_t constexpr kFramesInFlight{2}; // belongs to renderer settings
}

namespace
{
    // Interleaved vertex the chapter03 shader consumes. Field order and sizes mirror the
    // vertex_layout declared in run_app; the static_assert pins the stride those two share.
    struct gltf_vertex
    {
        float position[3];
        float normal[3];
        std::uint8_t color[4];
        float uv[2];
    };

    static_assert(sizeof(gltf_vertex) == 36, "gltf_vertex must match the 36-byte vertex_layout stride");

    struct loaded_mesh
    {
        std::vector<gltf_vertex> vertices;
        std::vector<std::uint32_t> indices;
        std::filesystem::path base_color_texture_path;
        bool valid{false};
    };

    // Load every triangle primitive of every mesh in `gltf_path` into one interleaved vertex
    // buffer plus a 32-bit index buffer, packing each vertex as a gltf_vertex. Missing
    // attributes fall back to sensible defaults (normal +Z, color white, uv 0). Also resolves
    // the first material's base-color image path (consumed CPU-side only this lesson).
    loaded_mesh load_gltf_mesh(std::filesystem::path const& gltf_path)
    {
        loaded_mesh result;

        std::string const path_str = gltf_path.string();

        cgltf_options options{};
        cgltf_data* data{nullptr};

        if (cgltf_parse_file(&options, path_str.c_str(), &data) != cgltf_result_success)
        {
            std::println(stderr, "[glTF] : Error : failed to parse [{}]", path_str);
            return result;
        }

        vkgc::scope_guard const free_data{[&] { cgltf_free(data); }};

        if (cgltf_load_buffers(&options, data, path_str.c_str()) != cgltf_result_success)
        {
            std::println(stderr, "[glTF] : Error : failed to load buffers for [{}]", path_str);
            return result;
        }

        for (cgltf_size mesh_idx = 0; mesh_idx < data->meshes_count; ++mesh_idx)
        {
            cgltf_mesh const& mesh = data->meshes[mesh_idx];

            for (cgltf_size primitive_idx = 0; primitive_idx < mesh.primitives_count; ++primitive_idx)
            {
                cgltf_primitive const& primitive = mesh.primitives[primitive_idx];
                if (primitive.type != cgltf_primitive_type_triangles)
                {
                    continue;
                }

                cgltf_accessor const* positions{nullptr};
                cgltf_accessor const* normals{nullptr};
                cgltf_accessor const* colors{nullptr};
                cgltf_accessor const* texcoords{nullptr};

                for (cgltf_size attribute_idx = 0; attribute_idx < primitive.attributes_count; ++attribute_idx)
                {
                    cgltf_attribute const& attribute = primitive.attributes[attribute_idx];
                    switch (attribute.type)
                    {
                    case cgltf_attribute_type_position:
                        positions = attribute.data;
                        break;
                    case cgltf_attribute_type_normal:
                        normals = attribute.data;
                        break;
                    case cgltf_attribute_type_color:
                        if (colors == nullptr)
                        {
                            colors = attribute.data;
                        }
                        break;
                    case cgltf_attribute_type_texcoord:
                        if (texcoords == nullptr)
                        {
                            texcoords = attribute.data;
                        }
                        break;
                    default:
                        break;
                    }
                }

                if (positions == nullptr)
                {
                    continue;
                }

                auto const base_vertex = static_cast<std::uint32_t>(result.vertices.size());
                cgltf_size const vertex_count = positions->count;

                for (cgltf_size vertex_idx = 0; vertex_idx < vertex_count; ++vertex_idx)
                {
                    gltf_vertex vertex{};

                    cgltf_accessor_read_float(positions, vertex_idx, vertex.position, 3);

                    if (normals != nullptr)
                    {
                        cgltf_accessor_read_float(normals, vertex_idx, vertex.normal, 3);
                    }
                    else
                    {
                        vertex.normal[0] = 0.f;
                        vertex.normal[1] = 0.f;
                        vertex.normal[2] = 1.f;
                    }

                    float rgba[4]{1.f, 1.f, 1.f, 1.f};
                    if (colors != nullptr)
                    {
                        cgltf_size const components = cgltf_num_components(colors->type);
                        cgltf_accessor_read_float(colors, vertex_idx, rgba, components);
                        if (components < 4)
                        {
                            rgba[3] = 1.f;
                        }
                    }

                    for (std::size_t k = 0; k < 4; ++k)
                    {
                        float const clamped = std::clamp(rgba[k], 0.f, 1.f);
                        vertex.color[k] = static_cast<std::uint8_t>(std::lround(clamped * 255.f));
                    }

                    if (texcoords != nullptr)
                    {
                        cgltf_accessor_read_float(texcoords, vertex_idx, vertex.uv, 2);
                    }
                    else
                    {
                        vertex.uv[0] = 0.f;
                        vertex.uv[1] = 0.f;
                    }

                    result.vertices.push_back(vertex);
                }

                if (primitive.indices != nullptr)
                {
                    cgltf_size const index_count = primitive.indices->count;
                    for (cgltf_size i = 0; i < index_count; ++i)
                    {
                        cgltf_size const index = cgltf_accessor_read_index(primitive.indices, i);
                        result.indices.push_back(base_vertex + static_cast<std::uint32_t>(index));
                    }
                }
                else
                {
                    for (cgltf_size i = 0; i < vertex_count; ++i)
                    {
                        result.indices.push_back(base_vertex + static_cast<std::uint32_t>(i));
                    }
                }
            }
        }

        if (data->materials_count > 0)
        {
            cgltf_material const& material = data->materials[0];
            cgltf_texture const* base_color_texture = material.has_pbr_metallic_roughness
                                                          ? material.pbr_metallic_roughness.base_color_texture.texture
                                                          : nullptr;

            if (base_color_texture != nullptr
                && base_color_texture->image != nullptr
                && base_color_texture->image->uri != nullptr)
            {
                std::string uri{base_color_texture->image->uri};
                cgltf_decode_uri(uri.data());
                uri.resize(std::strlen(uri.c_str()));
                result.base_color_texture_path = gltf_path.parent_path() / uri;
            }
        }

        result.valid = !result.vertices.empty() && !result.indices.empty();
        return result;
    }

    struct loaded_rgba8_image
    {
        std::uint32_t width{0};
        std::uint32_t height{0};
        std::vector<std::byte> pixels;
        bool valid{false};
    };

    // Decodes `image_path` into 8-bit RGBA pixels via stb_image, forcing 4 channels
    // regardless of the source format (matches the VK_FORMAT_R8G8B8A8_SRGB sampled image
    // run_app creates for it). Generic decode, not glTF-specific, even though the sole
    // current caller sources the path from a glTF material.
    loaded_rgba8_image load_rgba8_image(std::filesystem::path const& image_path)
    {
        loaded_rgba8_image result;

        std::string const path_str = image_path.string();

        int width{0};
        int height{0};
        int channels{0};

        stbi_uc* const pixels = stbi_load(path_str.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (pixels == nullptr)
        {
            std::println(stderr, "[stb] : Warning : failed to decode base-color texture [{}]", path_str);
            return result;
        }

        vkgc::scope_guard const free_pixels{[&] { stbi_image_free(pixels); }};

        std::println(
            "[stb] : decoded base-color texture [{}] : {}x{}, {} source channels",
            path_str,
            width,
            height,
            channels);

        result.width = static_cast<std::uint32_t>(width);
        result.height = static_cast<std::uint32_t>(height);

        // stbi_image_free fires when this function returns; copy the bytes out since the
        // caller can no longer hold a span over stb's buffer once it's freed.
        auto const image_size_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
        auto const pixel_bytes = std::as_bytes(std::span{pixels, image_size_bytes});
        result.pixels.assign(pixel_bytes.begin(), pixel_bytes.end());

        result.valid = true;
        return result;
    }
}

static bool run_app(
    vkgc::vulkan_context& vk_context,
    vkgc::vulkan_object_registry& vk_object_registry,
    vkgc::window& window)
{
    auto [width, height] = window.get_size();

    vkgc::swapchain_params swapchain_params{
        .preferred_surface_formats{
            // display settings
            {.format{VK_FORMAT_B8G8R8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
            {.format{VK_FORMAT_R8G8B8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}
        },
        // display settings
        .preferred_present_modes{
#if (VKGC_DEBUG_VULKAN == 0)
            VK_PRESENT_MODE_MAILBOX_KHR,
#endif
            VK_PRESENT_MODE_FIFO_RELAXED_KHR,
            VK_PRESENT_MODE_IMMEDIATE_KHR
        },
        .framebuffer_extent{.width{width}, .height{height}}, // display settings
        .min_image_count{2}, // renderer settings
        .image_usage_flags{
            // renderer settings
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        }
    };

    vkgc::vulkan_presenter presenter{
        vk_context.device(),
        vk_object_registry,
        window.surface(),
        vkgc::kFramesInFlight,
        swapchain_params};
    VKGC_VERIFY(presenter.is_valid());

    auto connect_on_resize = window.connect_on_resize(
        [&width, &height, &presenter](std::uint32_t const new_width, std::uint32_t const new_height)
        {
            width = new_width;
            height = new_height;
            presenter.request_rebuild();
        });

    auto swapchain_image_view_handles =
        vkgc::create_swapchain_image_views(vk_object_registry, presenter.images(), presenter.surface_format().format);
    if (swapchain_image_view_handles.empty())
    {
        std::println(stderr, "[Vulkan] : Error : failed to create swapchain image views");
        return false;
    }

    auto const main_queue_command_pool = vk_object_registry.create_command_pool(
        VkCommandPoolCreateInfo{
            .sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
            .pNext{nullptr},
            .flags{VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT},
            .queueFamilyIndex{vk_context.device().main_queue_family_index()}
        }
    );
    if (!VKGC_ENSURE(main_queue_command_pool.is_valid()))
    {
        return false;
    }

    auto const command_buffers =
        vk_object_registry.allocate_command_buffers(main_queue_command_pool, vkgc::kFramesInFlight, true);
    if (!VKGC_ENSURE(!command_buffers.empty()))
    {
        return false;
    }

    VkFormat depth_attachment_format{VK_FORMAT_UNDEFINED};
    for (auto preferred_depth_format : {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}) // from renderer
    {
        VkFormatProperties2 fmt_properties{
            .sType{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2},
            .pNext{nullptr},
            .formatProperties{}
        };

        vkGetPhysicalDeviceFormatProperties2(
            vk_context.device().physical_device(),
            preferred_depth_format,
            &fmt_properties);

        auto const optimal_tiling_features = fmt_properties.formatProperties.optimalTilingFeatures;
        if ((optimal_tiling_features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        {
            depth_attachment_format = preferred_depth_format;
            break;
        }
    }

    if (!VKGC_ENSUREF(depth_attachment_format != VK_FORMAT_UNDEFINED, "failed to find supported depth format"))
    {
        return false;
    }

    vkgc::vk_image_handle depth_attachment_image;
    vkgc::vk_image_view_handle depth_attachment_image_view;

    if (!vkgc::create_depth_attachment(
            vk_object_registry,
            depth_attachment_format,
            {.width{presenter.surface_extent().width}, .height{presenter.surface_extent().height}, .depth{1}},
            depth_attachment_image,
            depth_attachment_image_view))
    {
        std::println(stderr, "[Vulkan] : Error : failed to create depth attachment");
        return false;
    }

    auto rebuild_swapchain_resources = [&]() -> bool
    {
        if (width == 0 || height == 0)
        {
            return true;
        }

        VKGC_VERIFY_VKSUCCESS(vkDeviceWaitIdle(vk_context.device().handle()));

        vk_object_registry.destroy_immediate(depth_attachment_image_view);
        vk_object_registry.destroy_immediate(depth_attachment_image);

        for (auto const image_view_handle : swapchain_image_view_handles)
        {
            vk_object_registry.destroy_immediate(image_view_handle);
        }
        swapchain_image_view_handles.clear();

        swapchain_params.framebuffer_extent = {.width{width}, .height{height}};
        presenter.recreate_swapchain(window.surface(), swapchain_params);
        VKGC_VERIFYF(presenter.is_valid(), "unexpected error occurred while recreating the swapchain");

        swapchain_image_view_handles = vkgc::create_swapchain_image_views(
            vk_object_registry,
            presenter.images(),
            presenter.surface_format().format);
        if (swapchain_image_view_handles.empty())
        {
            std::println(stderr, "[Vulkan] : Error : failed to create swapchain image views");
            return false;
        }

        if (!vkgc::create_depth_attachment(
                vk_object_registry,
                depth_attachment_format,
                {.width{presenter.surface_extent().width}, .height{presenter.surface_extent().height}, .depth{1}},
                depth_attachment_image,
                depth_attachment_image_view))
        {
            std::println(stderr, "[Vulkan] : Error : failed to create depth attachment");
            return false;
        }

        return true;
    };

    std::filesystem::path const path{std::filesystem::path{kCacheDir} / "chapter03/gltf/chapter03_gltf.spv"};
    auto spirv_words = vkgc::load_binary_file<std::uint32_t>(path);
    if (!VKGC_ENSUREF(!spirv_words.empty(), "failed to load shader [{}]", path.string()))
    {
        return false;
    }

    auto const shader_code = std::as_bytes(std::span{spirv_words});
    VKGC_CHECKF(
        !shader_code.empty() && shader_code.size() % sizeof(std::uint32_t) == 0,
        "invalid byte code buffer size");

    vkgc::vulkan_extending_structs_chain shader_vertex_stage{
        VkPipelineShaderStageCreateInfo{
            .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .stage{VK_SHADER_STAGE_VERTEX_BIT},
            .module{VK_NULL_HANDLE},
            .pName{"main"},
            .pSpecializationInfo{nullptr}
        },
        VkShaderModuleCreateInfo{
            .sType{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .codeSize{static_cast<std::uint32_t>(shader_code.size())},
            .pCode{reinterpret_cast<std::uint32_t const*>(shader_code.data())}
        }
    };

    vkgc::vulkan_extending_structs_chain shader_fragment_stage{
        VkPipelineShaderStageCreateInfo{
            .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .stage{VK_SHADER_STAGE_FRAGMENT_BIT},
            .module{VK_NULL_HANDLE},
            .pName{"main"},
            .pSpecializationInfo{nullptr}
        },
        VkShaderModuleCreateInfo{
            .sType{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .codeSize{static_cast<std::uint32_t>(shader_code.size())},
            .pCode{reinterpret_cast<std::uint32_t const*>(shader_code.data())}
        }
    };

    std::array const shader_stages{*shader_vertex_stage.as_pointer(), *shader_fragment_stage.as_pointer()};

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
        .primitiveRestartEnable{VK_FALSE}
    };

    // Interleaved layout the glTF shader consumes: position + normal (float32x3), color
    // (unorm rgba8), texcoord (float32x2). Stride matches gltf_vertex (36 bytes).
    vkgc::vertex_layout const vertex_layout{
        .stride{static_cast<std::uint32_t>(sizeof(gltf_vertex))},
        .attributes{
            {vkgc::attribute_semantic::position, VK_FORMAT_R32G32B32_SFLOAT, 0u, 0u},
            {vkgc::attribute_semantic::normal, VK_FORMAT_R32G32B32_SFLOAT, 12u, 0u},
            {vkgc::attribute_semantic::color, VK_FORMAT_R8G8B8A8_UNORM, 24u, 0u},
            {vkgc::attribute_semantic::texcoord, VK_FORMAT_R32G32_SFLOAT, 28u, 0u}
        }
    };

    std::filesystem::path const gltf_path{std::filesystem::path{kAssetDir} / "chapter03/rubber_duck/scene.gltf"};
    auto const mesh = load_gltf_mesh(gltf_path);
    if (!mesh.valid)
    {
        std::println(stderr, "[glTF] : Error : no renderable mesh in [{}]", gltf_path.string());
        return false;
    }

    std::println(
        "[glTF] : loaded {} vertices, {} indices from [{}]",
        mesh.vertices.size(),
        mesh.indices.size(),
        gltf_path.string());

    // Decode the base-color image CPU-side, move the pixels through a staging buffer into
    // a device-local sampled image, and create the sampler that will read it.
    vkgc::vk_image_handle base_color_texture_image;
    vkgc::vk_image_view_handle base_color_texture_image_view;
    vkgc::vk_sampler_handle base_color_texture_sampler;

    if (!mesh.base_color_texture_path.empty())
    {
        auto const base_color_image = load_rgba8_image(mesh.base_color_texture_path);
        if (base_color_image.valid)
        {
            VkExtent3D const texture_extent{
                .width{base_color_image.width},
                .height{base_color_image.height},
                .depth{1}
            };

            if (!vkgc::create_sampled_texture(
                    vk_object_registry,
                    VK_FORMAT_R8G8B8A8_SRGB,
                    texture_extent,
                    base_color_texture_image,
                    base_color_texture_image_view))
            {
                std::println(stderr, "[Vulkan] : Error : failed to create base-color texture");
                return false;
            }

            base_color_texture_sampler = vk_object_registry.create_sampler(
                VkSamplerCreateInfo{
                    .sType{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO},
                    .pNext{nullptr},
                    .flags{0},
                    .magFilter{VK_FILTER_LINEAR},
                    .minFilter{VK_FILTER_LINEAR},
                    .mipmapMode{VK_SAMPLER_MIPMAP_MODE_LINEAR},
                    .addressModeU{VK_SAMPLER_ADDRESS_MODE_REPEAT},
                    .addressModeV{VK_SAMPLER_ADDRESS_MODE_REPEAT},
                    .addressModeW{VK_SAMPLER_ADDRESS_MODE_REPEAT},
                    .mipLodBias{0.f},
                    .anisotropyEnable{VK_FALSE},
                    .maxAnisotropy{1.f},
                    .compareEnable{VK_FALSE},
                    .compareOp{VK_COMPARE_OP_NEVER},
                    .minLod{0.f},
                    .maxLod{VK_LOD_CLAMP_NONE},
                    .borderColor{VK_BORDER_COLOR_INT_OPAQUE_BLACK},
                    .unnormalizedCoordinates{VK_FALSE}
                },
                "gltf base-color sampler");

            if (!VKGC_ENSURE(base_color_texture_sampler.is_valid()))
            {
                return false;
            }

            auto const texture_data = std::span{base_color_image.pixels};

            auto const staging_buffer = vkgc::create_staging_buffer(vk_object_registry, texture_data.size());
            if (!staging_buffer.is_valid())
            {
                return false;
            }

            vkgc::scope_guard const free_staging_buffer{
                [&] { vk_object_registry.destroy_immediate(staging_buffer); }
            };

            if (!upload_buffer(vk_context, vk_object_registry, main_queue_command_pool, staging_buffer, texture_data))
            {
                return false;
            }

            auto const texture_image_handle = vk_object_registry.resolve_handle(base_color_texture_image);
            if (!VKGC_ENSURE_VKHANDLE(texture_image_handle))
            {
                return false;
            }

            auto const staging_buffer_handle = vk_object_registry.resolve_handle(staging_buffer);
            if (!VKGC_ENSURE_VKHANDLE(staging_buffer_handle))
            {
                return false;
            }

            auto const record_texture_upload = [&](VkCommandBuffer command_buffer)
            {
                {
                    VkImageMemoryBarrier2 const to_transfer_dst{
                        .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2},
                        .pNext{nullptr},
                        .srcStageMask{VK_PIPELINE_STAGE_2_NONE},
                        .srcAccessMask{VK_ACCESS_2_NONE},
                        .dstStageMask{VK_PIPELINE_STAGE_2_COPY_BIT},
                        .dstAccessMask{VK_ACCESS_2_TRANSFER_WRITE_BIT},
                        .oldLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                        .newLayout{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
                        .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                        .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                        .image{texture_image_handle},
                        .subresourceRange{
                            .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                            .baseMipLevel{0},
                            .levelCount{1},
                            .baseArrayLayer{0},
                            .layerCount{1}
                        }
                    };

                    VkDependencyInfo const dependency_info{
                        .sType{VK_STRUCTURE_TYPE_DEPENDENCY_INFO},
                        .pNext{nullptr},
                        .dependencyFlags{},
                        .memoryBarrierCount{0},
                        .pMemoryBarriers{nullptr},
                        .bufferMemoryBarrierCount{0},
                        .pBufferMemoryBarriers{nullptr},
                        .imageMemoryBarrierCount{1},
                        .pImageMemoryBarriers{&to_transfer_dst},
                    };

                    vkCmdPipelineBarrier2(command_buffer, &dependency_info);
                }

                VkBufferImageCopy2 const copy_region{
                    .sType{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2},
                    .pNext{nullptr},
                    .bufferOffset{0},
                    .bufferRowLength{0},
                    .bufferImageHeight{0},
                    .imageSubresource{
                        .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                        .mipLevel{0},
                        .baseArrayLayer{0},
                        .layerCount{1}
                    },
                    .imageOffset{.x{0}, .y{0}, .z{0}},
                    .imageExtent{texture_extent}
                };

                VkCopyBufferToImageInfo2 const copy_buffer_to_image_info{
                    .sType{VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2},
                    .pNext{nullptr},
                    .srcBuffer{staging_buffer_handle},
                    .dstImage{texture_image_handle},
                    .dstImageLayout{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
                    .regionCount{1},
                    .pRegions{&copy_region}
                };

                vkCmdCopyBufferToImage2(command_buffer, &copy_buffer_to_image_info);

                {
                    VkImageMemoryBarrier2 const to_shader_read{
                        .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2},
                        .pNext{nullptr},
                        .srcStageMask{VK_PIPELINE_STAGE_2_COPY_BIT},
                        .srcAccessMask{VK_ACCESS_2_TRANSFER_WRITE_BIT},
                        .dstStageMask{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT},
                        .dstAccessMask{VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
                        .oldLayout{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
                        .newLayout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                        .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                        .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                        .image{texture_image_handle},
                        .subresourceRange{
                            .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                            .baseMipLevel{0},
                            .levelCount{1},
                            .baseArrayLayer{0},
                            .layerCount{1}
                        }
                    };

                    VkDependencyInfo const dependency_info{
                        .sType{VK_STRUCTURE_TYPE_DEPENDENCY_INFO},
                        .pNext{nullptr},
                        .dependencyFlags{},
                        .memoryBarrierCount{0},
                        .pMemoryBarriers{nullptr},
                        .bufferMemoryBarrierCount{0},
                        .pBufferMemoryBarriers{nullptr},
                        .imageMemoryBarrierCount{1},
                        .pImageMemoryBarriers{&to_shader_read},
                    };

                    vkCmdPipelineBarrier2(command_buffer, &dependency_info);
                }
            };

            bool const texture_uploaded = submit_once(
                vk_context,
                vk_object_registry,
                main_queue_command_pool,
                record_texture_upload);
            if (!VKGC_ENSURE(texture_uploaded))
            {
                return false;
            }
        }
    }
    else
    {
        std::println("[stb] : no base-color texture referenced by the glTF material");
    }

    VkIndexType constexpr index_type{VK_INDEX_TYPE_UINT32};
    auto const index_count = static_cast<std::uint32_t>(mesh.indices.size());

    auto const vertex_data = std::as_bytes(std::span{mesh.vertices});
    auto const index_data = std::as_bytes(std::span{mesh.indices});

    auto const vertex_buffer = create_vertex_buffer(vk_object_registry, vertex_data.size());
    if (!vertex_buffer.is_valid())
    {
        return false;
    }

    auto const index_buffer = create_index_buffer(vk_object_registry, index_data.size());
    if (!index_buffer.is_valid())
    {
        return false;
    }

    if (!upload_buffer(vk_context, vk_object_registry, main_queue_command_pool, vertex_buffer, vertex_data))
    {
        return false;
    }

    if (!upload_buffer(vk_context, vk_object_registry, main_queue_command_pool, index_buffer, index_data))
    {
        return false;
    }

    auto const binding_descriptions = vkgc::to_binding_descriptions(vertex_layout);
    auto const attribute_descriptions = vkgc::to_attribute_descriptions(vertex_layout);

    VkPipelineVertexInputStateCreateInfo vertex_input_state{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .vertexBindingDescriptionCount{static_cast<std::uint32_t>(binding_descriptions.size())},
        .pVertexBindingDescriptions{binding_descriptions.data()},
        .vertexAttributeDescriptionCount{static_cast<std::uint32_t>(attribute_descriptions.size())},
        .pVertexAttributeDescriptions{attribute_descriptions.data()}
    };

    struct per_frame_shader_data
    {
        glm::mat4 mvp{};
        glm::mat4 normal{};
        std::uint32_t texture_id{0};
    };

    VkPushConstantRange constexpr push_constant_range{
        .stageFlags{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        .offset{0},
        .size{sizeof(per_frame_shader_data)}
    };

    // Bindless combined image-sampler array the fragment shader indexes (binding 0, set 0).
    // The layout only fixes the upper bound; update-after-bind + partially-bound +
    // variable-count let the descriptor set carry however many textures actually load.
    std::uint32_t constexpr max_bindless_samplers{64};

    VkDescriptorSetLayoutBinding constexpr bindless_samplers_binding{
        .binding{0},
        .descriptorType{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        .descriptorCount{max_bindless_samplers},
        .stageFlags{VK_SHADER_STAGE_FRAGMENT_BIT},
        .pImmutableSamplers{nullptr}
    };

    VkDescriptorBindingFlags constexpr bindless_samplers_binding_flags{
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo const binding_flags_create_info{
        .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO},
        .pNext{nullptr},
        .bindingCount{1},
        .pBindingFlags{&bindless_samplers_binding_flags}
    };

    auto const descriptor_set_layout = vk_object_registry.create_descriptor_set_layout(
        VkDescriptorSetLayoutCreateInfo{
            .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
            .pNext{&binding_flags_create_info},
            .flags{VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT},
            .bindingCount{1},
            .pBindings{&bindless_samplers_binding}
        },
        "gltf bindless sampler set layout");
    if (!VKGC_ENSURE(descriptor_set_layout.is_valid()))
    {
        return false;
    }

    auto const descriptor_set_layout_handle = vk_object_registry.resolve_handle(descriptor_set_layout);
    if (!VKGC_ENSURE_VKHANDLE(descriptor_set_layout_handle))
    {
        return false;
    }

    VkDescriptorPoolSize constexpr bindless_samplers_pool_size{
        .type{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        .descriptorCount{max_bindless_samplers}
    };

    auto const descriptor_pool = vk_object_registry.create_descriptor_pool(
        VkDescriptorPoolCreateInfo{
            .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO},
            .pNext{nullptr},
            .flags{VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT},
            .maxSets{1},
            .poolSizeCount{1},
            .pPoolSizes{&bindless_samplers_pool_size}
        },
        "gltf bindless descriptor pool");
    if (!VKGC_ENSURE(descriptor_pool.is_valid()))
    {
        return false;
    }

    std::array const bindless_set_layouts{descriptor_set_layout};
    std::array constexpr bindless_variable_counts{max_bindless_samplers};

    auto const bindless_descriptor_sets = vk_object_registry.allocate_descriptor_sets(
        descriptor_pool,
        bindless_set_layouts,
        bindless_variable_counts,
        "gltf bindless descriptor set");
    if (!VKGC_ENSURE(!bindless_descriptor_sets.empty()))
    {
        return false;
    }

    // The fragment shader indexes kSamplers2D[0]; write the duck's base color there. With no
    // texture loaded the slot stays unwritten and sampling it is undefined (partially-bound
    // only makes unwritten descriptors legal to *leave* unwritten, not to read).
    if (base_color_texture_image_view.is_valid() && base_color_texture_sampler.is_valid())
    {
        auto const base_color_image_view_handle = vk_object_registry.resolve_handle(base_color_texture_image_view);
        if (!VKGC_ENSURE_VKHANDLE(base_color_image_view_handle))
        {
            return false;
        }

        auto const base_color_sampler_handle = vk_object_registry.resolve_handle(base_color_texture_sampler);
        if (!VKGC_ENSURE_VKHANDLE(base_color_sampler_handle))
        {
            return false;
        }

        auto const bindless_descriptor_set_handle =
            vk_object_registry.resolve_handle(bindless_descriptor_sets.front());
        if (!VKGC_ENSURE_VKHANDLE(bindless_descriptor_set_handle))
        {
            return false;
        }

        VkDescriptorImageInfo const base_color_image_info{
            .sampler{base_color_sampler_handle},
            .imageView{base_color_image_view_handle},
            .imageLayout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
        };

        VkWriteDescriptorSet const bindless_sampler_write{
            .sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
            .pNext{nullptr},
            .dstSet{bindless_descriptor_set_handle},
            .dstBinding{0},
            .dstArrayElement{0},
            .descriptorCount{1},
            .descriptorType{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
            .pImageInfo{&base_color_image_info},
            .pBufferInfo{nullptr},
            .pTexelBufferView{nullptr}
        };

        vkUpdateDescriptorSets(vk_context.device().handle(), 1, &bindless_sampler_write, 0, nullptr);
    }

    auto const pipeline_layout = vk_object_registry.create_pipeline_layout(
        VkPipelineLayoutCreateInfo{
            .sType{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .setLayoutCount{1},
            .pSetLayouts{&descriptor_set_layout_handle},
            .pushConstantRangeCount{1},
            .pPushConstantRanges{&push_constant_range}
        },
        "gltf pipeline layout");
    if (!VKGC_ENSURE(pipeline_layout.is_valid()))
    {
        return false;
    }

    VkPipelineViewportStateCreateInfo constexpr viewport_state{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .viewportCount{1},
        .pViewports{nullptr},
        .scissorCount{1},
        .pScissors{nullptr},
    };

    std::array constexpr dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo const pipeline_dynamic_state{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .dynamicStateCount{static_cast<std::uint32_t>(dynamic_states.size())},
        .pDynamicStates{dynamic_states.data()}
    };

    VkPipelineDepthStencilStateCreateInfo constexpr depth_stencil_state{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .depthTestEnable{VK_TRUE},
        .depthWriteEnable{VK_TRUE},
        .depthCompareOp{VK_COMPARE_OP_GREATER},
        .depthBoundsTestEnable{VK_FALSE},
        .stencilTestEnable{VK_FALSE},
        .front{},
        .back{},
        .minDepthBounds{0.f},
        .maxDepthBounds{1.f}
    };

    std::array const color_attachment_formats{presenter.surface_format().format};

    VkPipelineRenderingCreateInfo pipeline_rendering_create_info{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO},
        .pNext{nullptr},
        .viewMask{0},
        .colorAttachmentCount{static_cast<std::uint32_t>(color_attachment_formats.size())},
        .pColorAttachmentFormats{color_attachment_formats.data()},
        .depthAttachmentFormat{depth_attachment_format},
        .stencilAttachmentFormat{VK_FORMAT_UNDEFINED}
    };

    VkPipelineColorBlendAttachmentState constexpr color_blend_attachment_state{
        .blendEnable{VK_FALSE},
        .srcColorBlendFactor{},
        .dstColorBlendFactor{},
        .colorBlendOp{},
        .srcAlphaBlendFactor{},
        .dstAlphaBlendFactor{},
        .alphaBlendOp{},
        .colorWriteMask{
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        }
    };

    VkPipelineColorBlendStateCreateInfo const color_blend_state{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .logicOpEnable{VK_FALSE},
        .logicOp{},
        .attachmentCount{1},
        .pAttachments{&color_blend_attachment_state},
        .blendConstants{}
    };

    VkPipelineRasterizationStateCreateInfo constexpr rasterization_state{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .depthClampEnable{VK_TRUE},
        .rasterizerDiscardEnable{VK_FALSE},
        .polygonMode{VK_POLYGON_MODE_FILL},
        .cullMode{VK_CULL_MODE_BACK_BIT},
        .frontFace{VK_FRONT_FACE_COUNTER_CLOCKWISE},
        .depthBiasEnable{VK_FALSE},
        .depthBiasConstantFactor{0.f},
        .depthBiasClamp{0.f},
        .depthBiasSlopeFactor{0.f},
        .lineWidth{1.f}
    };

    VkPipelineMultisampleStateCreateInfo multisample_state{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .rasterizationSamples{VK_SAMPLE_COUNT_1_BIT},
        .sampleShadingEnable{VK_FALSE},
        .minSampleShading{1},
        .pSampleMask{nullptr},
        .alphaToCoverageEnable{VK_FALSE},
        .alphaToOneEnable{VK_FALSE}
    };

    std::filesystem::path const pipeline_cache_path{std::filesystem::path{kCacheDir} / "chapter03_gltf.pipeline"};

    vkgc::vk_pipeline_cache_handle pipeline_cache;
    if (auto const cache_data = vkgc::load_binary_file<std::byte>(pipeline_cache_path); !cache_data.empty())
    {
        pipeline_cache = vk_object_registry.create_pipeline_cache_from_data(cache_data, "gltf pipeline cache");
    }

    if (pipeline_cache.is_valid())
    {
        std::println("[Vulkan] : loaded pipeline cache from [{}]", pipeline_cache_path.string());
    }
    else
    {
        std::println("[Vulkan] : no pipeline cache found at [{}], creating a new one", pipeline_cache_path.string());
        pipeline_cache = vk_object_registry.create_pipeline_cache_empty("gltf pipeline cache");
    }

    VKGC_VERIFY(pipeline_cache.is_valid());

    VkGraphicsPipelineCreateInfo const pipeline_create_info{
        .sType{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO},
        .pNext{&pipeline_rendering_create_info},
        .flags{0}, // VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT
        .stageCount{static_cast<std::uint32_t>(shader_stages.size())},
        .pStages{shader_stages.data()},
        .pVertexInputState{&vertex_input_state},
        .pInputAssemblyState{&input_assembly_state},
        .pTessellationState{nullptr},
        .pViewportState{&viewport_state},
        .pRasterizationState{&rasterization_state},
        .pMultisampleState{&multisample_state},
        .pDepthStencilState{&depth_stencil_state},
        .pColorBlendState{&color_blend_state},
        .pDynamicState{&pipeline_dynamic_state},
        .layout{vk_object_registry.resolve_handle(pipeline_layout)},
        .renderPass{VK_NULL_HANDLE},
        .subpass{0},
        .basePipelineHandle{VK_NULL_HANDLE},
        .basePipelineIndex{-1}
    };

    auto const pipeline = vk_object_registry.create_graphics_pipeline(
        pipeline_create_info,
        pipeline_cache,
        "gltf pipeline");
    if (!VKGC_ENSURE(pipeline.is_valid()))
    {
        return false;
    }

    std::uint32_t frame_index{vkgc::kFramesInFlight}; // renderer state

    vkgc::vulkan_frame_ring frame_ring{vk_object_registry, vkgc::kFramesInFlight};

    vkgc::update_loop([&]
    {
        // Begin render loop

        VKGC_CHECKF(
            frame_index >= vkgc::kFramesInFlight,
            "frame index has to be greater or equal to frames-in-flight number");

        if (window.should_close())
        {
            return false;
        }

        if (width == 0 || height == 0)
        {
            glfwWaitEvents();
            return true;
        }

        if (presenter.consume_rebuild_request())
        {
            if (!rebuild_swapchain_resources())
            {
                return false;
            }

            return true;
        }

        // Spin the mesh so all sides become visible over time.
        auto const time = static_cast<float>(glfwGetTime());
        glm::mat4 model = glm::rotate(
            glm::identity<glm::mat4>(),
            glm::radians(-90.f),
            glm::normalize(glm::vec3{1.f, 0.f, 0.f}));
        model = glm::rotate(
            model,
            time * glm::radians(45.f),
            glm::normalize(glm::vec3{0.f, 0.f, 1.f}));

        auto const aspect = static_cast<float>(width) / static_cast<float>(height);
        auto proj = vkgc::math::rperspective(glm::radians(90.f), aspect, .01f, 1'000.f);

        glm::mat4 constexpr view_point{glm::translate(glm::identity<glm::mat4>(), glm::vec3{0.f, 1.f, 2.f})};

        auto const view = glm::inverse(view_point);

        /*glm::mat4 const view{
            glm::lookAt(glm::vec3{0.f, 1.f, 2.f}, glm::vec3{0.f, 0.f, 0.f}, glm::vec3{0.f, 1.f, 0.f})
        };*/

        auto normal = glm::inverseTranspose(view * model);

        per_frame_shader_data const shader_data{
            .mvp{proj * view * model},
            .normal{normal},
            .texture_id{0}
        };

        std::uint32_t const frame_slot_index = frame_ring.begin_frame(frame_index);
        if (!VKGC_ENSUREF(
                frame_slot_index != std::numeric_limits<std::uint32_t>::max(),
                "failed to begin [#{}] frame slot",
                frame_slot_index))
        {
            return false;
        }

        auto const frame_slot_semaphore = vk_object_registry.resolve_handle(frame_ring.frame_slot_semaphore());
        if (!VKGC_ENSURE_VKHANDLE(frame_slot_semaphore))
        {
            return false;
        }

        auto const acquired = presenter.acquire_image(frame_slot_index);
        if (!acquired.has_value())
        {
            VKGC_VERIFYF(
                acquired.error() != vkgc::present_status::out_of_date,
                "vulkan surface no longer compatible with the swapchain, unexpected on this stage of the frame");

            VKGC_VERIFYF(
                acquired.error() != vkgc::present_status::error,
                "unexpected error occurred while acquiring the swapchain image");
        }

        auto [
            swapchain_image_acquired_semaphore,
            present_wait_semaphore,
            swapchain_image,
            swapchain_image_index] = acquired.value();

        auto const swapchain_image_acquired_semaphore_raw =
            vk_object_registry.resolve_handle(swapchain_image_acquired_semaphore);
        if (!VKGC_ENSURE_VKHANDLE(swapchain_image_acquired_semaphore_raw))
        {
            return false;
        }

        auto const present_wait_semaphore_raw = vk_object_registry.resolve_handle(present_wait_semaphore);
        if (!VKGC_ENSURE_VKHANDLE(present_wait_semaphore_raw))
        {
            return false;
        }

        auto const command_buffer = vk_object_registry.resolve_handle(command_buffers[frame_slot_index]);
        if (!VKGC_ENSURE_VKHANDLE(command_buffer))
        {
            return false;
        }

        if (!VKGC_ENSUREF_VKSUCCESS(
                vkResetCommandBuffer(command_buffer, 0),
                "failed to reset [#{}] frame render command buffer",
                frame_slot_index))
        {
            return false;
        }

        {
            VkCommandBufferBeginInfo constexpr begin_info{
                .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO},
                .pNext{nullptr},
                .flags{VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT},
                .pInheritanceInfo{nullptr}
            };

            if (!VKGC_ENSUREF_VKSUCCESS(
                    vkBeginCommandBuffer(command_buffer, &begin_info),
                    "failed to begin [#{}] frame render command buffer record",
                    frame_slot_index))
            {
                return false;
            }
        }

        auto const depth_attachment_image_handle = vk_object_registry.resolve_handle(depth_attachment_image);
        if (!VKGC_ENSURE_VKHANDLE(depth_attachment_image_handle))
        {
            return false;
        }

        {
            std::array images_transition_barriers{
                VkImageMemoryBarrier2{
                    .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2},
                    .pNext{nullptr},
                    .srcStageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                    .srcAccessMask{0},
                    .dstStageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                    .dstAccessMask{VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
                    .oldLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                    .newLayout{VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                    .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .image{swapchain_image},
                    .subresourceRange{
                        .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                        .baseMipLevel{0},
                        .levelCount{1},
                        .baseArrayLayer{0},
                        .layerCount{1}
                    }
                },
                VkImageMemoryBarrier2{
                    .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2},
                    .pNext{nullptr},
                    .srcStageMask{VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT},
                    .srcAccessMask{VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
                    .dstStageMask{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT},
                    .dstAccessMask{VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
                    .oldLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                    .newLayout{VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                    .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .image{depth_attachment_image_handle},
                    .subresourceRange{
                        .aspectMask{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT},
                        .baseMipLevel{0},
                        .levelCount{1},
                        .baseArrayLayer{0},
                        .layerCount{1}
                    }
                }
            };

            VkDependencyInfo const dependency_info{
                .sType{VK_STRUCTURE_TYPE_DEPENDENCY_INFO},
                .pNext{nullptr},
                .dependencyFlags{},
                .memoryBarrierCount{0},
                .pMemoryBarriers{nullptr},
                .bufferMemoryBarrierCount{0},
                .pBufferMemoryBarriers{nullptr},
                .imageMemoryBarrierCount{static_cast<std::uint32_t>(images_transition_barriers.size())},
                .pImageMemoryBarriers{images_transition_barriers.data()},
            };

            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }

        auto const swapchain_image_view_handle =
            vk_object_registry.resolve_handle(swapchain_image_view_handles[swapchain_image_index]);
        if (!VKGC_ENSURE_VKHANDLE(swapchain_image_view_handle))
        {
            return false;
        }

        auto const depth_attachment_image_view_handle = vk_object_registry.resolve_handle(depth_attachment_image_view);
        if (!VKGC_ENSURE_VKHANDLE(depth_attachment_image_view_handle))
        {
            return false;
        }

        VkRenderingAttachmentInfo const color_attachment_info{
            .sType{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},
            .pNext{nullptr},
            .imageView{swapchain_image_view_handle},
            .imageLayout{VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
            .resolveMode{VK_RESOLVE_MODE_NONE},
            .resolveImageView{VK_NULL_HANDLE},
            .resolveImageLayout{VK_IMAGE_LAYOUT_UNDEFINED},
            .loadOp{VK_ATTACHMENT_LOAD_OP_CLEAR},
            .storeOp{VK_ATTACHMENT_STORE_OP_STORE},
            .clearValue{.color{.float32{.64f, .64f, .64f, 1.f}}}
        };

        VkRenderingAttachmentInfo const depth_attachment_info{
            .sType{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},
            .pNext{nullptr},
            .imageView{depth_attachment_image_view_handle},
            .imageLayout{VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
            .resolveMode{VK_RESOLVE_MODE_NONE},
            .resolveImageView{VK_NULL_HANDLE},
            .resolveImageLayout{VK_IMAGE_LAYOUT_UNDEFINED},
            .loadOp{VK_ATTACHMENT_LOAD_OP_CLEAR},
            .storeOp{VK_ATTACHMENT_STORE_OP_DONT_CARE},
            .clearValue{.depthStencil{0.f, 0}}
        };

        auto const [render_width, render_height] = presenter.surface_extent();

        VkRenderingInfo const renderingInfo{
            .sType{VK_STRUCTURE_TYPE_RENDERING_INFO},
            .pNext{nullptr},
            .flags{0},
            .renderArea{
                .offset{0, 0},
                .extent{.width{render_width}, .height{render_height}}
            },
            .layerCount{1},
            .viewMask{0},
            .colorAttachmentCount{1},
            .pColorAttachments{&color_attachment_info},
            .pDepthAttachment{&depth_attachment_info},
            .pStencilAttachment{nullptr}
        };
        vkCmdBeginRendering(command_buffer, &renderingInfo);

        VkViewport const viewport{
            .x{0},
            .y{static_cast<float>(render_height)},
            .width{static_cast<float>(render_width)},
            .height{-static_cast<float>(render_height)},
            .minDepth{0},
            .maxDepth{1},
        };
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D const scissor{.offset{0, 0}, .extent{render_width, render_height}};
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        auto const pipeline_handle = vk_object_registry.resolve_handle(pipeline);
        if (!VKGC_ENSURE_VKHANDLE(pipeline_handle))
        {
            return false;
        }

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_handle);

        auto const bindless_descriptor_set_handle =
            vk_object_registry.resolve_handle(bindless_descriptor_sets.front());
        if (!VKGC_ENSURE_VKHANDLE(bindless_descriptor_set_handle))
        {
            return false;
        }

        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vk_object_registry.resolve_handle(pipeline_layout),
            0,
            1,
            &bindless_descriptor_set_handle,
            0,
            nullptr);

        // Update shader data
        vkCmdPushConstants(
            command_buffer,
            vk_object_registry.resolve_handle(pipeline_layout),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(shader_data),
            &shader_data);

        // Record command buffer

        VkDeviceSize buffer_offset{0};

        auto const vertex_buffer_handle = vk_object_registry.resolve_handle(vertex_buffer);
        if (!VKGC_ENSURE_VKHANDLE(vertex_buffer_handle))
        {
            return false;
        }

        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer_handle, &buffer_offset);

        auto const index_buffer_handle = vk_object_registry.resolve_handle(index_buffer);
        if (!VKGC_ENSURE_VKHANDLE(index_buffer_handle))
        {
            return false;
        }

        vkCmdBindIndexBuffer(command_buffer, index_buffer_handle, 0, index_type);

        vkCmdDrawIndexed(command_buffer, index_count, 1, 0, 0, 0);

        vkCmdEndRendering(command_buffer);

        {
            // Swapchain image transition for following presentation
            VkImageMemoryBarrier2 const to_present{
                .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2},
                .pNext{nullptr},
                .srcStageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                .srcAccessMask{VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
                .dstStageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                .dstAccessMask{0},
                .oldLayout{VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                .newLayout{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
                .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .image{swapchain_image},
                .subresourceRange{
                    .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                    .baseMipLevel{0},
                    .levelCount{1},
                    .baseArrayLayer{0},
                    .layerCount{1}
                }
            };

            VkDependencyInfo const dependency_info{
                .sType{VK_STRUCTURE_TYPE_DEPENDENCY_INFO},
                .pNext{nullptr},
                .dependencyFlags{},
                .memoryBarrierCount{0},
                .pMemoryBarriers{nullptr},
                .bufferMemoryBarrierCount{0},
                .pBufferMemoryBarriers{nullptr},
                .imageMemoryBarrierCount{1},
                .pImageMemoryBarriers{&to_present},
            };

            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }

        if (!VKGC_ENSUREF_VKSUCCESS(
                vkEndCommandBuffer(command_buffer),
                "failed to end [#{}] frame render command buffer record",
                frame_slot_index))
        {
            return false;
        }

        {
            std::array const wait_semaphores_submit_info{
                VkSemaphoreSubmitInfo{
                    .sType{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},
                    .pNext{nullptr},
                    .semaphore{swapchain_image_acquired_semaphore_raw},
                    .value{0},
                    .stageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                    .deviceIndex{0}
                }
            };

            std::array const command_buffer_submit_info{
                VkCommandBufferSubmitInfo{
                    .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO},
                   .pNext{nullptr},
                   .commandBuffer{command_buffer},
                   .deviceMask{0}
               }
            };

            std::array const signal_semaphores_info{
                VkSemaphoreSubmitInfo{
                    .sType{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},
                    .pNext{nullptr},
                    .semaphore{frame_slot_semaphore},
                    .value{frame_index},
                    .stageMask{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT},
                    .deviceIndex{0}
                },
                VkSemaphoreSubmitInfo{
                    .sType{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},
                    .pNext{nullptr},
                    .semaphore{present_wait_semaphore_raw},
                    .value{0},
                    .stageMask{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                    .deviceIndex{0}
                }
            };

            VkSubmitInfo2 const submit_info{
                .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO_2},
                .pNext{nullptr},
                .flags{0},
                .waitSemaphoreInfoCount{static_cast<std::uint32_t>(wait_semaphores_submit_info.size())},
                .pWaitSemaphoreInfos{wait_semaphores_submit_info.data()},
                .commandBufferInfoCount{static_cast<std::uint32_t>(command_buffer_submit_info.size())},
                .pCommandBufferInfos{command_buffer_submit_info.data()},
                .signalSemaphoreInfoCount{static_cast<std::uint32_t>(signal_semaphores_info.size())},
                .pSignalSemaphoreInfos{signal_semaphores_info.data()}
            };

            VKGC_VERIFYF_VKSUCCESS(
                vkQueueSubmit2(vk_context.device().main_queue(), 1, &submit_info, VK_NULL_HANDLE),
                "failed to submit [#{}] frame render command buffer",
                frame_slot_index);
        }

        frame_ring.end_frame();

        ++frame_index;

        // End render loop

        switch (presenter.request_presentation(vk_context.device().main_queue()))
        {
        case vkgc::present_status::ok:
            break;

        case vkgc::present_status::suboptimal:
            [[fallthrough]];
        case vkgc::present_status::out_of_date:
            if (!rebuild_swapchain_resources())
            {
                return false;
            }
            break;

        case vkgc::present_status::error:
            VKGC_VERIFYF(false, "unexpected error occurred while presenting to the swapchain");
            break;
        }

        glfwPollEvents();
        return true;
    });

    if (pipeline_cache.is_valid())
    {
        if (auto const cache_data = vk_object_registry.pipeline_cache_data(pipeline_cache); !cache_data.empty())
        {
            if (!vkgc::save_binary_file(pipeline_cache_path, cache_data))
            {
                std::println(
                    stderr,
                    "[Vulkan] : Warning : failed to persist pipeline cache [{}]",
                    pipeline_cache_path.string());
            }
        }
    }

    return true;
}

int main()
{
    vkgc::bootstrap_app();

    {
        auto const [width, height] = std::pair<std::uint32_t, std::uint32_t>{1280, 800};

        vkgc::vulkan_context vk_context{{.enable_validation{true}}, {}};
        VKGC_VERIFY(vk_context.is_valid());

        vkgc::vulkan_object_registry vk_object_registry{vk_context.device()};

        vkgc::window window{"Chapter 03 — glTF", width, height};
        VKGC_VERIFY(window.is_valid());
        VKGC_VERIFY(window.create_surface(vk_context.instance().handle()));

        if (!run_app(vk_context, vk_object_registry, window))
        {
            return -1;
        }
    }

    vkgc::terminate_app();
}
