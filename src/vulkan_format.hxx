#pragma once

#include <format>

#include <volk.h>

template <>
struct std::formatter<VkDebugReportObjectTypeEXT>
{
    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <class FmtContext>
    FmtContext::iterator format(VkDebugReportObjectTypeEXT const code, FmtContext& ctx) const
    {
        char const* name = [code] {
            switch (code)
            {
                case VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT: return "UNKNOWN";
                case VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT: return "INSTANCE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT: return "PHYSICAL_DEVICE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT: return "DEVICE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT: return "QUEUE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT: return "SEMAPHORE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT: return "COMMAND_BUFFER";
                case VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT: return "FENCE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT: return "DEVICE_MEMORY";
                case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT: return "BUFFER";
                case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT: return "IMAGE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT: return "EVENT";
                case VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT: return "QUERY_POOL";
                case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT: return "BUFFER_VIEW";
                case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT: return "IMAGE_VIEW";
                case VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT: return "SHADER_MODULE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT: return "PIPELINE_CACHE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT: return "PIPELINE_LAYOUT";
                case VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT: return "RENDER_PASS";
                case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT: return "PIPELINE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT: return "DESCRIPTOR_SET_LAYOUT";
                case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT: return "SAMPLER";
                case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT: return "DESCRIPTOR_POOL";
                case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT: return "DESCRIPTOR_SET";
                case VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT: return "FRAMEBUFFER";
                case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT: return "COMMAND_POOL";
                case VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT: return "SURFACE_KHR";
                case VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT: return "SWAPCHAIN_KHR";
                case VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT_EXT: return "DEBUG_REPORT_CALLBACK_EXT";
                case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT: return "DISPLAY_KHR";
                case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT: return "DISPLAY_MODE_KHR";
                case VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT: return "VALIDATION_CACHE_EXT";
                case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION_EXT: return "SAMPLER_YCBCR_CONVERSION";
                case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_EXT: return "DESCRIPTOR_UPDATE_TEMPLATE";
                case VK_DEBUG_REPORT_OBJECT_TYPE_CU_MODULE_NVX_EXT: return "CU_MODULE_NVX";
                case VK_DEBUG_REPORT_OBJECT_TYPE_CU_FUNCTION_NVX_EXT: return "CU_FUNCTION_NVX";
                case VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR_EXT: return "ACCELERATION_STRUCTURE_KHR";
                case VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV_EXT: return "ACCELERATION_STRUCTURE_NV";
                case VK_DEBUG_REPORT_OBJECT_TYPE_CUDA_MODULE_NV_EXT: return "CUDA_MODULE_NV";
                case VK_DEBUG_REPORT_OBJECT_TYPE_CUDA_FUNCTION_NV_EXT: return "CUDA_FUNCTION_NV";
                case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA_EXT: return "BUFFER_COLLECTION_FUCHSIA";
                default: return "UNKNOWN";
            }
        }();

        return std::format_to(ctx.out(), "{}", name);
    }
};

template <>
struct std::formatter<VkResult>
{
    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <class FmtContext>
    FmtContext::iterator format(VkResult const code, FmtContext& ctx) const
    {
        char const* name = [code] {
            switch (code)
            {
                case VK_SUCCESS: return "VK_SUCCESS";
                case VK_NOT_READY: return "VK_NOT_READY";
                case VK_TIMEOUT: return "VK_TIMEOUT";
                case VK_EVENT_SET: return "VK_EVENT_SET";
                case VK_EVENT_RESET: return "VK_EVENT_RESET";
                case VK_INCOMPLETE: return "VK_INCOMPLETE";
                case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
                case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
                case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
                case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
                case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
                case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
                case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
                case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
                case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
                case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
                case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
                case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
                case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
                case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
                case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
                case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
                case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
                case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
                case VK_ERROR_VALIDATION_FAILED: return "VK_ERROR_VALIDATION_FAILED";
                case VK_ERROR_NOT_PERMITTED: return "VK_ERROR_NOT_PERMITTED";
                case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
                case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
                case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
                case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
                case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
                case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
                case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
                case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
                case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
                case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
                case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
                case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
                case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
                case VK_INCOMPATIBLE_SHADER_BINARY_EXT: return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
                case VK_PIPELINE_BINARY_MISSING_KHR: return "VK_PIPELINE_BINARY_MISSING_KHR";
                case VK_ERROR_NOT_ENOUGH_SPACE_KHR: return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";
                case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
                default: return "UNKNOWN";
            }
        }();

        return std::format_to(ctx.out(), "{}", name);
    }
};
