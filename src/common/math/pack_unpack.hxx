#pragma once

/*
 * Vertex-attribute packing helpers — convert "fat" interleaved attributes
 * (float normals, float UVs) into the lean integer / half-float layouts the
 * GPU can consume directly as vertex-input formats.
 *
 * Two families live here:
 *
 *   - Octahedral normal encoding. A unit vector is folded onto an octahedron
 *     and projected to a 2-component value in [-1, 1]^2, then quantized to a
 *     signed-integer pair. Pack a normal as VK_FORMAT_R8G8_SNORM (2 bytes) or
 *     R16G16_SNORM (4 bytes) instead of R32G32B32_SFLOAT (12 bytes). GLM has
 *     no built-in for this; see Cigolle et al., "A Survey of Efficient
 *     Representations for Independent Unit Vectors" (JCGT 2014).
 *
 *   - UV packing. Thin wrappers over GLM's <glm/gtc/packing.hpp> so texture
 *     coordinates round-trip through R16G16_SFLOAT (half, tiling-safe) or
 *     R16G16_UNORM ([0, 1] only) with the same call style as the normals.
 *
 * The integer quantization matches the Vulkan *_SNORM / *_UNORM conventions
 * exactly (decode = clamp(v / max, -1)), so a value packed here decodes to the
 * same float the GPU produces when reading the attribute. Do NOT substitute a
 * different remap unless you also change how the shader reads the attribute.
 *
 * Header-only — pure inline/template math, so it drops into a module unit's
 * global module fragment the same way math/glm.hxx does:
 *
 *     module;
 *     #include "math/pack_unpack.hxx"
 *     export module vkgc.foo;
 *     ...
 */

#include "glm/gtx/norm.hpp"
#include "math/glm.hxx"

#include <glm/gtc/packing.hpp>

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>

namespace vkgc::math
{
    // Signed integer widths a normal may be quantized to (R8/R16 SNORM).
    template <typename T>
    concept OctStorage = std::same_as<T, std::int8_t> || std::same_as<T, std::int16_t>;

    namespace detail
    {
        // sign() that maps zero to +1 — the convention the octahedral fold needs.
        [[nodiscard]] inline glm::vec2 sign_not_zero(glm::vec2 const v) noexcept
        {
            return glm::vec2{v.x >= 0.f ? 1.f : -1.f, v.y >= 0.f ? 1.f : -1.f};
        }

        // Reflect a point across the diagonal folds of the octahedron's lower half.
        [[nodiscard]] inline glm::vec2 oct_wrap(glm::vec2 const v) noexcept
        {
            return (1.f - glm::abs(glm::vec2{v.y, v.x})) * sign_not_zero(v);
        }

        // Encode a value in [-1, 1] to a signed integer using the Vulkan SNORM rule.
        template <OctStorage T>
        [[nodiscard]] T snorm_encode(float const x) noexcept
        {
            float constexpr max{static_cast<float>(std::numeric_limits<T>::max())};
            return static_cast<T>(std::lround(glm::clamp(x, -1.f, 1.f) * max));
        }

        // Decode a signed integer back to [-1, 1], matching the Vulkan SNORM rule.
        template <OctStorage T>
        [[nodiscard]] float snorm_decode(T const v) noexcept
        {
            float constexpr max{static_cast<float>(std::numeric_limits<T>::max())};
            return glm::max(static_cast<float>(v) / max, -1.f);
        }
    }

    // --- Octahedral normal mapping --------------------------------------------

    // Fold a unit vector onto octahedral coordinates in [-1, 1]^2.
    [[nodiscard]] inline glm::vec2 encode_oct(glm::vec3 const n) noexcept
    {
        glm::vec3 const a = glm::abs(n);
        glm::vec2 const p = glm::vec2{n.x, n.y} / (a.x + a.y + a.z);
        return n.z <= 0.f ? detail::oct_wrap(p) : p;
    }

    // Reconstruct a unit vector from octahedral coordinates in [-1, 1]^2.
    [[nodiscard]] inline glm::vec3 decode_oct(glm::vec2 const e) noexcept
    {
        glm::vec3 v{e.x, e.y, 1.f - std::abs(e.x) - std::abs(e.y)};

        if (v.z < 0.f)
        {
            glm::vec2 const w = detail::oct_wrap(glm::vec2{v.x, v.y});
            v.x = w.x;
            v.y = w.y;
        }

        return glm::normalize(v);
    }

    // Pack a (not necessarily normalized) normal into a signed-integer pair,
    // ready to upload as VK_FORMAT_R8G8_SNORM (std::int8_t) or R16G16_SNORM
    // (std::int16_t). The input is normalized internally, so any non-zero length
    // is fine; the zero vector is undefined. Fast variant: a single rounded
    // quantization.
    template <OctStorage T>
    [[nodiscard]] glm::vec<2, T> pack_normal_oct(glm::vec3 const normal) noexcept
    {
        glm::vec2 const e = encode_oct(glm::normalize(normal));
        return {detail::snorm_encode<T>(e.x), detail::snorm_encode<T>(e.y)};
    }

    // Unpack a signed-integer pair produced by pack_normal_oct back to a unit vector.
    template <OctStorage T>
    [[nodiscard]] glm::vec3 unpack_normal_oct(glm::vec<2, T> const oct) noexcept
    {
        return decode_oct(glm::vec2{detail::snorm_decode<T>(oct.x), detail::snorm_decode<T>(oct.y)});
    }

    // Precise variant: quantize, then test the four integer neighbors and keep
    // whichever decodes closest (smallest squared distance) to the source normal.
    // Costs three extra decodes per call; halves the worst-case angular error.
    template <OctStorage T>
    [[nodiscard]] glm::vec<2, T> pack_normal_oct_precise(glm::vec3 const normal) noexcept
    {
        int constexpr lo = static_cast<int>(std::numeric_limits<T>::min());
        int constexpr hi = static_cast<int>(std::numeric_limits<T>::max());

        glm::vec3 const n = glm::normalize(normal);
        glm::vec<2, T> const base = pack_normal_oct<T>(n);

        glm::vec<2, T> best = base;
        float best_sq_distance = glm::distance2(unpack_normal_oct<T>(base), n);

        for (auto index : {0, 1, 2, 3})
        {
            int const cx = static_cast<int>(base.x) + index / 2;
            int const cy = static_cast<int>(base.y) + index % 2;

            if (cx < lo || cx > hi || cy < lo || cy > hi)
            {
                continue;
            }

            glm::vec<2, T> const candidate{static_cast<T>(cx), static_cast<T>(cy)};
            float const sq_distance = glm::distance2(unpack_normal_oct<T>(candidate), n);

            if (sq_distance < best_sq_distance)
            {
                best_sq_distance = sq_distance;
                best = candidate;
            }
        }

        return best;
    }

    // --- UV packing -----------------------------------------------------------

    // Pack texture coordinates into a half-float pair (R16G16_SFLOAT, 4 bytes).
    // Half is the safe default: it tolerates UVs outside [0, 1] (tiling/atlas).
    [[nodiscard]] inline std::uint32_t pack_uv_half(glm::vec2 const uv) noexcept
    {
        return glm::packHalf2x16(uv);
    }

    [[nodiscard]] inline glm::vec2 unpack_uv_half(std::uint32_t const packed) noexcept
    {
        return glm::unpackHalf2x16(packed);
    }

    // Pack texture coordinates into an unsigned-normalized pair (R16G16_UNORM,
    // 4 bytes). Use only when UVs are guaranteed within [0, 1]; values outside
    // the unit square are clamped and will not round-trip.
    [[nodiscard]] inline std::uint32_t pack_uv_unorm(glm::vec2 const uv) noexcept
    {
        return glm::packUnorm2x16(uv);
    }

    [[nodiscard]] inline glm::vec2 unpack_uv_unorm(std::uint32_t const packed) noexcept
    {
        return glm::unpackUnorm2x16(packed);
    }
}
