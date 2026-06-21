module;

#include <algorithm>
#include <cstdint>
#include <vector>

#include <volk.h>

module vkgc.vulkan_vertex_layout;

namespace vkgc
{
    std::vector<VkVertexInputAttributeDescription> to_attribute_descriptions(vertex_layout const& layout)
    {
        std::vector<VkVertexInputAttributeDescription> descriptions;
        descriptions.reserve(layout.attributes.size());

        for (std::uint32_t location{0}; auto&& layout_attribute : layout.attributes)
        {
            descriptions.push_back({
                .location{location},
                .binding{layout_attribute.binding},
                .format{layout_attribute.format},
                .offset{layout_attribute.offset},
            });

            ++location;
        }

        descriptions.shrink_to_fit();
        return descriptions;
    }

    std::vector<VkVertexInputBindingDescription> to_binding_descriptions(vertex_layout const& layout)
    {
        std::vector<VkVertexInputBindingDescription> descriptions;
        descriptions.reserve(layout.attributes.size());

        for (auto&& layout_attribute : layout.attributes)
        {
            auto const already_present = std::ranges::any_of(descriptions, [&layout_attribute](auto&& existing)
            {
                return existing.binding == layout_attribute.binding;
            });

            if (!already_present)
            {
                descriptions.push_back({
                    .binding{layout_attribute.binding},
                    .stride{layout.stride},
                    .inputRate{VK_VERTEX_INPUT_RATE_VERTEX},
                });
            }
        }

        descriptions.shrink_to_fit();
        return descriptions;
    }
}
