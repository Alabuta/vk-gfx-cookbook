#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <print>
#include <span>
#include <utility>
#include <vector>
#include <array>

#include <filesystem>

#include <volk.h>
#include <vk_mem_alloc.h>

#include "math/glm.hxx"

#include "GLFW/glfw3.h"
#include "math/pack_unpack.hxx"

#include "vulkan/format.hxx"
#include "vulkan/assert.hxx"

#include <ranges>

// constexpr std::string_view kShaderDir = COOKBOOK_SHADER_DIR_STRING;
constexpr std::string_view kCacheDir = COOKBOOK_CACHE_DIR_STRING;

import vkgc.bootstrap;
import vkgc.file_io;
import vkgc.window;
import vkgc.vulkan_instance;
import vkgc.vulkan_surface;
import vkgc.vulkan_device;
import vkgc.vulkan_handle;
import vkgc.vulkan_object_registry;
import vkgc.vulkan_resource_helpers;
import vkgc.vulkan_presenter;
import vkgc.vulkan_frame_ring;
import vkgc.vulkan_helpers;

namespace vkgc
{
    std::uint32_t constexpr kFramesInFlight{2}; // belongs to renderer settings
}

[[nodiscard]]
static std::unordered_map<VkImage, vkgc::vk_image_view_handle> create_swapchain_image_views(
    vkgc::vulkan_object_registry& vk_object_registry,
    std::span<VkImage const> const images,
    VkFormat const format)
{
    std::unordered_map<VkImage, vkgc::vk_image_view_handle> image_view_handles;
    image_view_handles.reserve(images.size());

    for (std::uint32_t i{0}; auto image_handle : images)
    {
        VkImageViewCreateInfo const create_info{
            .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .image{image_handle},
            .viewType{VK_IMAGE_VIEW_TYPE_2D},
            .format{format},
            .components{
                .r{VK_COMPONENT_SWIZZLE_IDENTITY},
                .g{VK_COMPONENT_SWIZZLE_IDENTITY},
                .b{VK_COMPONENT_SWIZZLE_IDENTITY},
                .a{VK_COMPONENT_SWIZZLE_IDENTITY}
            },
            .subresourceRange{
                .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                .baseMipLevel{0},
                .levelCount{1},
                .baseArrayLayer{0},
                .layerCount{1}
            }
        };

        auto const debug_name = std::format("swapchain image [#{}]", i++);
        if (auto const view = vk_object_registry.create_image_view(create_info, debug_name.c_str()); view.is_valid())
        {
            image_view_handles.emplace(image_handle, view);
            continue;
        }

        for (auto const image_view_handle : image_view_handles | std::views::values)
        {
            vk_object_registry.destroy_immediate(image_view_handle);
        }

        return {};
    }

    return image_view_handles;
}

[[nodiscard]]
static bool create_depth_attachment(
    vkgc::vulkan_object_registry& vk_object_registry,
    VkFormat const image_format,
    VkExtent3D const image_extent,
    vkgc::vk_image_handle& image,
    vkgc::vk_image_view_handle& image_view)
{
    VkImageCreateInfo const image_create_info{
        .sType{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .imageType{VK_IMAGE_TYPE_2D},
        .format{image_format},
        .extent{image_extent},
        .mipLevels{1},
        .arrayLayers{1},
        .samples{VK_SAMPLE_COUNT_1_BIT},
        .tiling{VK_IMAGE_TILING_OPTIMAL},
        .usage{VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
        .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
        .queueFamilyIndexCount{0},
        .pQueueFamilyIndices{nullptr},
        .initialLayout{VK_IMAGE_LAYOUT_UNDEFINED}
    };

    VmaAllocationCreateInfo constexpr allocation_create_info{
        .flags{VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT},
        .usage{VMA_MEMORY_USAGE_UNKNOWN},
        .requiredFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
        .preferredFlags{0},
        .memoryTypeBits{0},
        .pool{VK_NULL_HANDLE},
        .pUserData{nullptr},
        .priority{0}
    };

    image = vk_object_registry.create_memory_bound_image(
        image_create_info,
        allocation_create_info,
        "depth attachment image");
    if (!image.is_valid())
    {
        return false;
    }

    VkImageViewCreateInfo const depth_view_create_info{
        .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .image{vk_object_registry.resolve_handle(image)},
        .viewType{VK_IMAGE_VIEW_TYPE_2D},
        .format{image_format},
        .components{
            .r{VK_COMPONENT_SWIZZLE_IDENTITY},
            .g{VK_COMPONENT_SWIZZLE_IDENTITY},
            .b{VK_COMPONENT_SWIZZLE_IDENTITY},
            .a{VK_COMPONENT_SWIZZLE_IDENTITY}
        },
        .subresourceRange{
            .aspectMask{VK_IMAGE_ASPECT_DEPTH_BIT},
            .baseMipLevel{0},
            .levelCount{1},
            .baseArrayLayer{0},
            .layerCount{1}
        }
    };

    image_view = vk_object_registry.create_image_view(depth_view_create_info, "depth attachment image view");
    if (!image_view.is_valid())
    {
        vk_object_registry.destroy_immediate(image);
        return false;
    }

    return true;
}

[[nodiscard]]
static vkgc::vk_buffer_handle create_vertex_buffer(vkgc::vulkan_object_registry& resources, std::size_t const size)
{
    if (!VKGC_ENSURE(size > 0))
    {
        return {};
    }

    VkBufferCreateInfo const buffer_create_info{
        .sType{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .size{static_cast<VkDeviceSize>(size)},
        .usage{VK_BUFFER_USAGE_VERTEX_BUFFER_BIT},
        .sharingMode{VK_SHARING_MODE_EXCLUSIVE },
        .queueFamilyIndexCount{0},
        .pQueueFamilyIndices{nullptr},
    };

    VmaAllocationCreateInfo constexpr buffer_allocation_info{
        .flags{
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
        },
        .usage{VMA_MEMORY_USAGE_AUTO},
        .requiredFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
        .preferredFlags{0},
        .memoryTypeBits{0},
        .pool{VK_NULL_HANDLE},
        .pUserData{nullptr},
        .priority{0}
    };

    return resources.create_memory_bound_buffer(buffer_create_info, buffer_allocation_info);
}

static bool run_app(std::uint32_t width, std::uint32_t height)
{
    // Vulkan context
    vkgc::vulkan_instance vulkan_instance{{.enable_validation{true}}};
    VKGC_VERIFY(vulkan_instance.is_valid());

    vkgc::window window{"Swapchain example", width, height};
    VKGC_VERIFY(window.is_valid());

    vkgc::vulkan_surface const window_surface = vulkan_instance.create_window_surface(window);
    VKGC_VERIFY(window_surface.is_valid());

    vkgc::vulkan_device vulkan_device = vulkan_instance.create_device({
        .surface{window_surface.handle()},
        .extensions{}
    });
    VKGC_VERIFY(vulkan_device.is_valid());

    vkgc::vulkan_object_registry vk_object_registry{vulkan_device};

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
        vulkan_device,
        vk_object_registry,
        window_surface.handle(),
        vkgc::kFramesInFlight,
        swapchain_params
    };
    VKGC_VERIFY(presenter.is_valid());

    auto connect_on_resize = window.connect_on_resize(
        [&width, &height, &presenter](std::uint32_t const new_width, std::uint32_t const new_height)
        {
            width = new_width;
            height = new_height;
            presenter.request_rebuild();
        });

    auto swapchain_image_view_handles = create_swapchain_image_views(
        vk_object_registry,
        presenter.images(),
        presenter.surface_format().format);
    if (swapchain_image_view_handles.empty())
    {
        std::println(stderr, "[Vulkan] : Error : failed to crate swapchain image views");
        return false;
    }

    vkgc::vulkan_frame_ring frame_ring{vk_object_registry, vkgc::kFramesInFlight};

    auto const main_queue_command_pool = vk_object_registry.create_command_pool({
        .sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
        .pNext{nullptr},
        .flags{VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT},
        .queueFamilyIndex{vulkan_device.main_queue_family_index()}
    });
    if (!main_queue_command_pool.is_valid())
    {
        return false;
    }

    auto const command_buffers = vk_object_registry.allocate_command_buffers(
        main_queue_command_pool,
        vkgc::kFramesInFlight,
        true);
    if (command_buffers.empty())
    {
        return false;
    }

    VkFormat depth_attachment_format{VK_FORMAT_UNDEFINED};
    for (auto preferred_depth_format : {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT})
    {
        VkFormatProperties2 fmt_properties{
            .sType{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2},
            .pNext{nullptr},
            .formatProperties{}
        };

        vkGetPhysicalDeviceFormatProperties2(vulkan_device.physical_device(), preferred_depth_format, &fmt_properties);

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

    if (!create_depth_attachment(
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

        VKGC_VERIFY_VKSUCCESS(vkDeviceWaitIdle(vulkan_device.handle()));

        vk_object_registry.destroy_immediate(depth_attachment_image_view);
        vk_object_registry.destroy_immediate(depth_attachment_image);

        for (auto const image_view_handle : swapchain_image_view_handles | std::views::values)
        {
            vk_object_registry.destroy_immediate(image_view_handle);
        }
        swapchain_image_view_handles.clear();

        swapchain_params.framebuffer_extent = {.width{width}, .height{height}};
        presenter.recreate_swapchain(window_surface.handle(), swapchain_params);
        VKGC_VERIFYF(presenter.is_valid(), "unexpected error occurred while recreating the swapchain");

        swapchain_image_view_handles = create_swapchain_image_views(
            vk_object_registry,
            presenter.images(),
            presenter.surface_format().format);
        if (swapchain_image_view_handles.empty())
        {
            std::println(stderr, "[Vulkan] : Error : failed to crate swapchain image views");
            return false;
        }

        if (!create_depth_attachment(
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

    std::uint32_t frame_index{vkgc::kFramesInFlight}; // renderer state

    // Compiled at build time from shaders/chapter02/hello_triangle/chapter02_triangle.slang by the
    // chapter02_hello_triangle_shaders step (vkgc_compile_shaders in CMakeLists.txt). The .spv mirrors
    // the source tree under .cache/, so the path matches the shader's location under shaders/.
    std::filesystem::path const path{
        std::filesystem::path{kCacheDir} / "chapter02/hello_triangle/chapter02_triangle.spv"};
    auto spirv_words = vkgc::load_binary_file<std::uint32_t>(path);
    if (!VKGC_ENSUREF(!spirv_words.empty(), "failed to load shader [{}]", path.string()))
    {
        return false;
    }

    {
        /*std::array create_info{
            vkgc::shader_create_info{
                .stage{VK_SHADER_STAGE_VERTEX_BIT},
                .next_stage{VK_SHADER_STAGE_FRAGMENT_BIT},
                .entry_point{"main"},
                .debug_name{"triangle.vs"}
            },
            vkgc::shader_create_info{
                .stage{VK_SHADER_STAGE_FRAGMENT_BIT},
                .next_stage{0},
                .entry_point{"main"},
                .debug_name{"triangle.ps"}
            }
        };

        auto shaders = vk_object_registry.create_shaders(create_info, std::as_bytes(std::span{spirv_words}));
        if (!VKGC_ENSUREF(!shaders.empty(), "failed to create shaders [{}]", path.string()))
        {
            return false;
        }*/
    }

    auto const shader_code = std::as_bytes(std::span{spirv_words});
    VKGC_CHECKF(
        !shader_code.empty() && shader_code.size() % sizeof(std::uint32_t) == 0, "invalid byte code buffer size");

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

    std::array shader_stages{
        *shader_vertex_stage.as_pointer(),
        *shader_fragment_stage.as_pointer()
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
        .primitiveRestartEnable{VK_FALSE}
    };

    struct vertex
    {
        glm::vec2 position;
        std::uint32_t uv;
    };

    std::array const vertices{
        vertex{
            glm::vec2{-.5f, -.5f},
            vkgc::math::pack_uv_unorm(glm::vec2{0.f, 0.f})
        },
        vertex{
            glm::vec2{.5f, -.5f},
            vkgc::math::pack_uv_unorm(glm::vec2{1.f, 0.f})
        },
        vertex{
            glm::vec2{.0f, .5f},
            vkgc::math::pack_uv_unorm(glm::vec2{.5f, 1.f})
        },
    };

    std::uint32_t const vertex_count{static_cast<std::uint32_t>(vertices.size())};
    std::size_t const size_in_bytes{sizeof(vertex) * vertex_count};

    auto const vertex_buffer = create_vertex_buffer(vk_object_registry, size_in_bytes);
    if (!vertex_buffer.is_valid())
    {
        return false;
    }

    if (auto const vertex_buffer_memory = vk_object_registry.bound_allocation(vertex_buffer);
        VKGC_ENSURE(vertex_buffer_memory.is_valid()))
    {
        VmaAllocationInfo2 allocation_info;
        vmaGetAllocationInfo2(
            vulkan_device.vma_allocator(),
            vk_object_registry.resolve_handle(vertex_buffer_memory),
            &allocation_info
        );

        void* mapped_ptr;
        if (!VKGC_ENSURE_VKSUCCESS(
            vkMapMemory(
                vulkan_device.handle(),
                allocation_info.allocationInfo.deviceMemory,
                allocation_info.allocationInfo.offset,
                size_in_bytes,
                0,
                &mapped_ptr)))
        {
            return false;
        }

        memcpy(mapped_ptr, vertices.data(), size_in_bytes);

        vkUnmapMemory(vulkan_device.handle(), allocation_info.allocationInfo.deviceMemory);
    }

    std::array const binding_descriptions{
        VkVertexInputBindingDescription{
            .binding{0},
            .stride{sizeof(vertex)},
            .inputRate{VK_VERTEX_INPUT_RATE_VERTEX}
       }
    };

    std::array constexpr attribute_descriptions{
        VkVertexInputAttributeDescription{
            .location{0},
            .binding{0},
            .format{VK_FORMAT_R32G32_SFLOAT},
            .offset{offsetof(vertex, position)}
        },
        VkVertexInputAttributeDescription{
            .location{1},
            .binding{0},
            .format{VK_FORMAT_R16G16_UNORM},
            .offset{offsetof(vertex, uv)}
        }
        // { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal) },
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_state{
        .sType{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO},
        .pNext{nullptr},
        .flags{0},
        .vertexBindingDescriptionCount{static_cast<std::uint32_t>(binding_descriptions.size())},
        .pVertexBindingDescriptions{binding_descriptions.data()},
        .vertexAttributeDescriptionCount{static_cast<std::uint32_t>(attribute_descriptions.size())},
        .pVertexAttributeDescriptions{attribute_descriptions.data()}
    };

    auto const pipeline_layout = vk_object_registry.create_pipeline_layout(
        {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .setLayoutCount{0},
            .pSetLayouts{nullptr},
            .pushConstantRangeCount{0},
            .pPushConstantRanges{nullptr}
        },
        "triangle pipeline layout");
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
        .polygonMode{VK_POLYGON_MODE_FILL },
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

    std::filesystem::path const pipeline_cache_path{std::filesystem::path{kCacheDir} / "chapter02_triangle.pipeline"};

    vkgc::vk_pipeline_cache_handle pipeline_cache;
    if (auto const cache_data = vkgc::load_binary_file<std::byte>(pipeline_cache_path); !cache_data.empty())
    {
        pipeline_cache = vk_object_registry.create_pipeline_cache_from_data(cache_data, "triangle pipeline cache");
    }
    else
    {
        pipeline_cache = vk_object_registry.create_pipeline_cache_empty("triangle pipeline cache");
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
        "triangle pipeline");
    if (!VKGC_ENSURE(pipeline.is_valid()))
    {
        return false;
    }

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

        auto [swapchain_image_acquired_semaphore, present_wait_semaphore, swapchain_image] = acquired.value();

        auto const swapchain_image_acquired_semaphore_raw = vk_object_registry.resolve_handle(
            swapchain_image_acquired_semaphore);
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
            "failed to reset [#{}] frame render command buffer", frame_slot_index))
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
                "failed to begin [#{}] frame render command buffer record", frame_slot_index))
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

        auto const swapchain_image_view_handle = vk_object_registry.resolve_handle(
            swapchain_image_view_handles[swapchain_image]);
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
            .clearValue{.color{.64f, .64f, .64f, 1.f}}
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
            .clearValue {.depthStencil{0.f, 0}}
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

        VkRect2D const scissor{
            .offset{0, 0}, .extent{render_width, render_height}
        };
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        auto const pipeline_handle = vk_object_registry.resolve_handle(pipeline);
        if (!VKGC_ENSURE_VKHANDLE(pipeline_handle))
        {
            return false;
        }

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_handle);

        // Update shader data
        // Record command buffer

        VkDeviceSize buffer_offset{0};
        /*vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,
            1,
            &descriptorSetTex,
            0,
            nullptr);*/

        auto const vertex_buffer_handle = vk_object_registry.resolve_handle(vertex_buffer);
        if (!VKGC_ENSURE_VKHANDLE(vertex_buffer_handle))
        {
            return false;
        }

        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer_handle, &buffer_offset);
        vkCmdDraw(command_buffer, vertex_count, 1, 0, 0);

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
            "failed to end [#{}] frame render command buffer record", frame_slot_index))
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
                vkQueueSubmit2(vulkan_device.main_queue(), 1, &submit_info, VK_NULL_HANDLE),
                "failed to submit [#{}] frame render command buffer",
                frame_slot_index);
        }

        frame_ring.end_frame();

        ++frame_index;

        // End render loop

        switch (presenter.request_presentation(vulkan_device.main_queue()))
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

    auto const [width, height] = std::pair<std::uint32_t, std::uint32_t>{1280, 800};

    if (!run_app(width, height))
    {
        return -1;
    }

    vkgc::terminate_app();
}
