module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include <volk.h>

#include "math/glm.hxx"
#include "math/pack_unpack.hxx"
#include "diagnostic/assert.hxx"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/packing.hpp>

module vkgc.geometry;

import vkgc.vulkan_vertex_layout;

namespace vkgc::geometry
{
    namespace
    {
        [[nodiscard, maybe_unused]] std::uint32_t format_size(VkFormat const format)
        {
            switch (format)
            {
            case VK_FORMAT_R8G8_SNORM:
                return 2u;
            case VK_FORMAT_R16G16_SNORM:
            case VK_FORMAT_R16G16_SFLOAT:
            case VK_FORMAT_R16G16_UNORM:
            case VK_FORMAT_R8G8B8A8_UNORM:
                return 4u;
            case VK_FORMAT_R32G32_SFLOAT:
                return 8u;
            case VK_FORMAT_R32G32B32_SFLOAT:
                return 12u;
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                return 16u;
            default:
                VKGC_CHECKF(false, "unsupported vertex attribute format {}", static_cast<int>(format));
                return 0u;
            }
        }

        void store_bytes(std::byte* const dst, void const* const src, std::size_t const count)
        {
            std::memcpy(dst, src, count);
        }

        // void store_floats(std::byte* const dst, float const* const src, std::size_t const count)
        // {
        //     store_bytes(dst, src, count * sizeof(float));
        // }

        template <glm::length_t Cap = 4, glm::length_t L, typename T, glm::qualifier Q>
        void store_glm_vec(std::byte* const dst, glm::vec<L, T, Q> const& src)
        {
            store_bytes(dst, glm::value_ptr(src), std::min(Cap, L) * sizeof(T));
        }

        // Per-vertex values, computed once then encoded per attribute.
        struct vertex_values
        {
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec2 texcoord;
            glm::vec4 color;
        };

        // Encode one attribute of one vertex. Lean dispatch: a switch on the
        // semantic, then on the supported VkFormats for it, reusing the
        // math/pack_unpack.hxx helpers for the lean integer / half encodings.
        void write_attribute(
            std::byte* const dst,
            vertex_attribute const& layout_attribute,
            vertex_values const& vertex_values)
        {
            switch (layout_attribute.semantic)
            {
            case attribute_semantic::position:
                switch (layout_attribute.format)
                {
                case VK_FORMAT_R32G32B32_SFLOAT:
                    store_glm_vec(dst, vertex_values.position);
                    return;
                default:
                    break;
                }
                break;

            case attribute_semantic::normal:
                switch (layout_attribute.format)
                {
                case VK_FORMAT_R32G32B32_SFLOAT:
                    store_glm_vec(dst, vertex_values.normal);
                    return;
                case VK_FORMAT_R8G8_SNORM:
                {
                    glm::vec<2, std::int8_t> const oct{math::pack_normal_oct<std::int8_t>(vertex_values.normal)};
                    store_bytes(dst, glm::value_ptr(oct), 2);
                    return;
                }
                case VK_FORMAT_R16G16_SNORM:
                {
                    glm::vec<2, std::int16_t> const oct{math::pack_normal_oct<std::int16_t>(vertex_values.normal)};
                    store_bytes(dst, glm::value_ptr(oct), 4);
                    return;
                }
                default:
                    break;
                }
                break;

            case attribute_semantic::texcoord:
                switch (layout_attribute.format)
                {
                case VK_FORMAT_R32G32_SFLOAT:
                    store_glm_vec(dst, vertex_values.texcoord);
                    return;
                case VK_FORMAT_R16G16_SFLOAT:
                {
                    std::uint32_t const packed{math::pack_uv_half(vertex_values.texcoord)};
                    store_bytes(dst, &packed, sizeof packed);
                    return;
                }
                case VK_FORMAT_R16G16_UNORM:
                {
                    std::uint32_t const packed{math::pack_uv_unorm(vertex_values.texcoord)};
                    store_bytes(dst, &packed, sizeof packed);
                    return;
                }
                default:
                    break;
                }
                break;

            case attribute_semantic::color:
                switch (layout_attribute.format)
                {
                case VK_FORMAT_R32G32B32A32_SFLOAT:
                    store_glm_vec(dst, vertex_values.color);
                    return;
                case VK_FORMAT_R32G32B32_SFLOAT:
                    store_glm_vec<3>(dst, vertex_values.color);
                    return;
                case VK_FORMAT_R8G8B8A8_UNORM:
                {
                    std::uint32_t const packed{glm::packUnorm4x8(vertex_values.color)};
                    store_bytes(dst, &packed, sizeof packed);
                    return;
                }
                default:
                    break;
                }
                break;
            }

            VKGC_CHECKF(
                false,
                "unsupported (semantic={}, format={}) pair",
                static_cast<int>(layout_attribute.semantic),
                static_cast<int>(layout_attribute.format));
        }

        // A box face is an `cols_segments` x `rows_segments` grid laid out in a
        // local XY plane (local +Z is the outward normal), then placed by a pure
        // rotation + translation. Because the rotation has determinant +1 and
        // maps local +Z onto the outward normal, a fixed CCW-in-local winding
        // renders CCW-from-outside on every face.
        struct face_desc
        {
            glm::vec3 normal;
            std::uint32_t cols_segments; // subdivisions along local X (dim_a)
            std::uint32_t rows_segments; // subdivisions along local Y (dim_b)
            float dim_a; // full extent mapped to local X
            float dim_b; // full extent mapped to local Y
            glm::mat4 transform; // local plane -> world (translate * rotate)
        };

        [[nodiscard]]
        std::array<face_desc, 6> build_faces(box_params const& p)
        {
            glm::uvec3 const seg = glm::max(p.segments, glm::uvec3{1u});
            glm::vec3 const size = glm::abs(glm::vec3{p.width, p.height, p.depth});
            glm::vec3 const half = size * .5f;

            glm::mat4 constexpr identity{1.f};

            auto const rot = [&identity](float const degrees, glm::vec3 const axis)
            {
                return glm::rotate(identity, glm::radians(degrees), axis);
            };

            auto const move = [&identity](glm::vec3 const translation)
            {
                return glm::translate(identity, translation);
            };

            return std::array<face_desc, 6>{
                // +X : rotate local +Z onto +X
                face_desc{{1.f, 0.f, 0.f}, seg.z, seg.y, size.z, size.y,
                          move({half.x, 0.f, 0.f}) * rot(90.f, {0.f, 1.f, 0.f})},
                // -X
                face_desc{{-1.f, 0.f, 0.f}, seg.z, seg.y, size.z, size.y,
                          move({-half.x, 0.f, 0.f}) * rot(-90.f, {0.f, 1.f, 0.f})},
                // +Y
                face_desc{{0.f, 1.f, 0.f}, seg.x, seg.z, size.x, size.z,
                          move({0.f, half.y, 0.f}) * rot(-90.f, {1.f, 0.f, 0.f})},
                // -Y
                face_desc{{0.f, -1.f, 0.f}, seg.x, seg.z, size.x, size.z,
                          move({0.f, -half.y, 0.f}) * rot(90.f, {1.f, 0.f, 0.f})},
                // +Z : identity rotation
                face_desc{{0.f, 0.f, 1.f}, seg.x, seg.y, size.x, size.y,
                          move({0.f, 0.f, half.z})},
                // -Z : rotate 180 about +Y
                face_desc{{0.f, 0.f, -1.f}, seg.x, seg.y, size.x, size.y,
                          move({0.f, 0.f, -half.z}) * rot(180.f, {0.f, 1.f, 0.f})},
            };
        }

        [[nodiscard]]
        vertex_values make_vertex(
            face_desc const& face,
            glm::vec4 const& color,
            std::uint32_t const i,
            std::uint32_t const j)
        {
            float const u = static_cast<float>(i) / static_cast<float>(face.cols_segments);
            float const v = static_cast<float>(j) / static_cast<float>(face.rows_segments);

            glm::vec3 const local{(u - .5f) * face.dim_a, (v - .5f) * face.dim_b, 0.f};
            glm::vec3 const position{face.transform * glm::vec4{local, 1.f}};

            return vertex_values{position, face.normal, glm::vec2{u, v}, color};
        }

        // maybe_unused: only referenced from VKGC_CHECKF (compiled out in Shipping).
        [[nodiscard, maybe_unused]]
        bool is_strip(VkPrimitiveTopology const topology)
        {
            return topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        }
    }

    std::uint32_t box_vertex_count(box_params const& p)
    {
        std::array<face_desc, 6> const faces = build_faces(p);
        bool const indexed = p.index_type != VK_INDEX_TYPE_NONE_KHR;

        std::uint32_t total{0};
        for (face_desc const& face : faces)
        {
            std::uint32_t const sa = face.cols_segments;
            std::uint32_t const sb = face.rows_segments;
            std::uint32_t const cols = sa + 1u;
            std::uint32_t const rows = sb + 1u;

            if (indexed)
            {
                // Indexed meshes share the per-face grid for every topology.
                total += cols * rows;
                continue;
            }

            switch (p.topology)
            {
            case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
                total += cols * rows;
                break;
            case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
                total += 2u * (sa * rows + sb * cols);
                break;
            case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
                total += 6u * sa * sb;
                break;
            default:
                VKGC_CHECKF(
                    false,
                    "strip topologies require an index buffer; set box_params::index_type");
                break;
            }
        }

        return total;
    }

    std::uint32_t box_index_count(box_params const& p)
    {
        if (p.index_type == VK_INDEX_TYPE_NONE_KHR)
        {
            return 0u;
        }

        std::array<face_desc, 6> const faces = build_faces(p);

        switch (p.topology)
        {
        case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        {
            std::uint32_t total{0};
            for (face_desc const& face : faces)
            {
                total += (face.cols_segments + 1u) * (face.rows_segments + 1u);
            }

            return total;
        }
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        {
            std::uint32_t total{0};
            for (face_desc const& face : faces)
            {
                std::uint32_t const sa = face.cols_segments;
                std::uint32_t const sb = face.rows_segments;
                total += 2u * (sa * (sb + 1u) + sb * (sa + 1u));
            }

            return total;
        }
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        {
            std::uint32_t total{0};
            for (face_desc const& face : faces)
            {
                total += 6u * face.cols_segments * face.rows_segments;
            }

            return total;
        }
        case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        {
            // One polyline per grid row; runs are separated by a restart token.
            std::uint32_t vertices{0};
            std::uint32_t runs{0};
            for (face_desc const& face : faces)
            {
                std::uint32_t const cols = face.cols_segments + 1u;
                std::uint32_t const lines = face.rows_segments + 1u;
                vertices += lines * cols;
                runs += lines;
            }

            return vertices + (runs - 1u);
        }
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        {
            // One strip per quad-row; runs are separated by a restart token.
            std::uint32_t vertices{0};
            std::uint32_t runs{0};
            for (face_desc const& face : faces)
            {
                std::uint32_t const cols = face.cols_segments + 1u;
                std::uint32_t const bands = face.rows_segments;
                vertices += bands * 2u * cols;
                runs += bands;
            }

            return vertices + (runs - 1u);
        }
        default:
            VKGC_CHECKF(false, "unsupported topology {}", static_cast<int>(p.topology));
            return 0u;
        }
    }

    void generate_box(
        box_params const& params,
        vertex_layout const& layout,
        std::span<std::byte> vertices,
        std::span<std::byte> indices)
    {
        VKGC_CHECKF(layout.stride > 0u, "vertex layout stride must be non-zero");

        for ([[maybe_unused]] vertex_attribute const& layout_attribute : layout.attributes)
        {
            VKGC_CHECKF(
                layout_attribute.offset + format_size(layout_attribute.format) <= layout.stride,
                "attribute (offset={}, size={}) overruns vertex stride {}",
                layout_attribute.offset,
                format_size(layout_attribute.format),
                layout.stride);
        }

        auto const indexed_mesh = params.index_type != VK_INDEX_TYPE_NONE_KHR;

        VKGC_CHECKF(
            indexed_mesh || !is_strip(params.topology),
            "strip topologies require an index buffer (set index_type to UINT16/UINT32)");

        [[maybe_unused]] std::uint32_t const vertex_count{box_vertex_count(params)};
        [[maybe_unused]] std::uint32_t const index_count{box_index_count(params)};

        VKGC_CHECKF(
            vertices.size() >= static_cast<std::size_t>(vertex_count) * layout.stride,
            "vertex span too small: have {} bytes, need {}",
            vertices.size(),
            static_cast<std::size_t>(vertex_count) * layout.stride);

        VKGC_CHECK(params.index_type == VK_INDEX_TYPE_UINT16 || params.index_type == VK_INDEX_TYPE_UINT32);
        auto const index_size = params.index_type == VK_INDEX_TYPE_UINT16
            ? sizeof(std::uint16_t)
            : sizeof(std::uint32_t);

        if (indexed_mesh)
        {
            VKGC_CHECKF(
                indices.size() >= static_cast<std::size_t>(index_count) * index_size,
                "index span too small: have {} bytes, need {}",
                indices.size(),
                static_cast<std::size_t>(index_count) * index_size);

            if (params.index_type == VK_INDEX_TYPE_UINT16)
            {
                VKGC_CHECKF(
                    vertex_count < std::numeric_limits<std::uint16_t>::max() - 1,
                    "UINT16 index overflow: {} vertices exceed 65534 (restart token reserved)",
                    vertex_count);
            }
        }

        std::array<face_desc, 6> const faces = build_faces(params);

        std::size_t vertex_cursor{0};
        auto const push_vertex = [&](vertex_values const& vv)
        {
            std::byte* const base = vertices.data() + vertex_cursor * layout.stride;
            for (vertex_attribute const& layout_attribute : layout.attributes)
            {
                write_attribute(base + layout_attribute.offset, layout_attribute, vv);
            }

            ++vertex_cursor;
        };

        std::uint32_t const restart_token = params.index_type == VK_INDEX_TYPE_UINT16
            ? std::numeric_limits<std::uint16_t>::max() - 1
            : std::numeric_limits<std::uint32_t>::max() - 1;

        std::size_t index_cursor{0};
        auto const push_index = [&](std::uint32_t const value)
        {
            std::byte* const dst = indices.data() + index_cursor * index_size;
            if (params.index_type == VK_INDEX_TYPE_UINT16)
            {
                auto const narrow = static_cast<std::uint16_t>(value);
                std::memcpy(dst, &narrow, sizeof narrow);
            }
            else
            {
                std::memcpy(dst, &value, sizeof value);
            }

            ++index_cursor;
        };

        // Strip / line-strip runs are separated by a restart token; emit one
        // before every run except the very first (spanning faces as well).
        bool first_strip_run{true};
        auto const begin_strip_run = [&]
        {
            if (!first_strip_run)
            {
                push_index(restart_token);
            }
            first_strip_run = false;
        };

        auto const no_colors = params.face_colors.empty();

        for (std::uint32_t f{0}; f < 6u; ++f)
        {
            face_desc const& face = faces[f];
            glm::vec4 const color = no_colors ? glm::vec4{1.f} : params.face_colors[f];

            std::uint32_t const sa = face.cols_segments;
            std::uint32_t const sb = face.rows_segments;
            std::uint32_t const cols = sa + 1u;

            if (indexed_mesh)
            {
                auto const base = static_cast<std::uint32_t>(vertex_cursor);

                // Emit the shared grid, row-major.
                for (std::uint32_t j{0}; j <= sb; ++j)
                {
                    for (std::uint32_t i{0}; i <= sa; ++i)
                    {
                        push_vertex(make_vertex(face, color, i, j));
                    }
                }

                auto const grid = [base, cols](std::uint32_t const i, std::uint32_t const j)
                {
                    return base + j * cols + i;
                };

                switch (params.topology)
                {
                case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
                    for (std::uint32_t j{0}; j <= sb; ++j)
                    {
                        for (std::uint32_t i{0}; i <= sa; ++i)
                        {
                            push_index(grid(i, j));
                        }
                    }
                    break;

                case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
                    for (std::uint32_t j{0}; j <= sb; ++j) // horizontal edges
                    {
                        for (std::uint32_t i{0}; i < sa; ++i)
                        {
                            push_index(grid(i, j));
                            push_index(grid(i + 1u, j));
                        }
                    }

                    for (std::uint32_t i{0}; i <= sa; ++i) // vertical edges
                    {
                        for (std::uint32_t j{0}; j < sb; ++j)
                        {
                            push_index(grid(i, j));
                            push_index(grid(i, j + 1u));
                        }
                    }

                    break;

                case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
                    for (std::uint32_t j{0}; j <= sb; ++j)
                    {
                        begin_strip_run();
                        for (std::uint32_t i{0}; i <= sa; ++i)
                        {
                            push_index(grid(i, j));
                        }
                    }

                    break;

                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
                    for (std::uint32_t j{0}; j < sb; ++j)
                    {
                        for (std::uint32_t i{0}; i < sa; ++i)
                        {
                            push_index(grid(i, j));
                            push_index(grid(i + 1u, j));
                            push_index(grid(i + 1u, j + 1u));
                            push_index(grid(i, j));
                            push_index(grid(i + 1u, j + 1u));
                            push_index(grid(i, j + 1u));
                        }
                    }

                    break;

                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
                    for (std::uint32_t j{0}; j < sb; ++j)
                    {
                        begin_strip_run();
                        for (std::uint32_t i{0}; i <= sa; ++i)
                        {
                            push_index(grid(i, j + 1u));
                            push_index(grid(i, j));
                        }
                    }

                    break;

                default:
                    VKGC_CHECKF(false, "unsupported topology {}", static_cast<int>(params.topology));
                    break;
                }
            }
            else
            {
                // Non-indexed: expand primitives straight into the vertex buffer.
                switch (params.topology)
                {
                case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
                    for (std::uint32_t j{0}; j <= sb; ++j)
                    {
                        for (std::uint32_t i{0}; i <= sa; ++i)
                        {
                            push_vertex(make_vertex(face, color, i, j));
                        }
                    }

                    break;

                case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
                    for (std::uint32_t j{0}; j <= sb; ++j) // horizontal edges
                    {
                        for (std::uint32_t i{0}; i < sa; ++i)
                        {
                            push_vertex(make_vertex(face, color, i, j));
                            push_vertex(make_vertex(face, color, i + 1u, j));
                        }
                    }

                    for (std::uint32_t i{0}; i <= sa; ++i) // vertical edges
                    {
                        for (std::uint32_t j{0}; j < sb; ++j)
                        {
                            push_vertex(make_vertex(face, color, i, j));
                            push_vertex(make_vertex(face, color, i, j + 1u));
                        }
                    }

                    break;

                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
                    for (std::uint32_t j{0}; j < sb; ++j)
                    {
                        for (std::uint32_t i{0}; i < sa; ++i)
                        {
                            push_vertex(make_vertex(face, color, i, j));
                            push_vertex(make_vertex(face, color, i + 1u, j));
                            push_vertex(make_vertex(face, color, i + 1u, j + 1u));
                            push_vertex(make_vertex(face, color, i, j));
                            push_vertex(make_vertex(face, color, i + 1u, j + 1u));
                            push_vertex(make_vertex(face, color, i, j + 1u));
                        }
                    }

                    break;

                default:
                    VKGC_CHECKF(false, "unsupported non-indexed topology {}", static_cast<int>(params.topology));
                    break;
                }
            }
        }

        VKGC_CHECKF(
            vertex_cursor == static_cast<std::size_t>(vertex_count),
            "internal: wrote {} vertices, expected {}",
            vertex_cursor,
            vertex_count);

        if (indexed_mesh)
        {
            VKGC_CHECKF(
                index_cursor == static_cast<std::size_t>(index_count),
                "internal: wrote {} indices, expected {}",
                index_cursor,
                index_count);
        }
    }
}
