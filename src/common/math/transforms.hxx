#pragma once

/*
 * Camera / clip-space transform helpers built on GLM — the canonical site for
 * the projection matrices the renderer feeds to shaders, mirroring how
 * math/glm.hxx is the canonical GLM include site.
 *
 * Vulkan conventions:
 *
 *   - Clip-space depth is [0, 1], not OpenGL's [-1, 1]. The helpers below pin
 *     this explicitly through GLM's *_ZO entry points, so they stay correct
 *     even in a TU that has not set GLM_FORCE_DEPTH_ZERO_TO_ONE.
 *
 *   - The Vulkan Y-axis flip (clip-space Y points down) is NOT applied here. The
 *     project flips Y with a negative-height VkViewport, so the projection is
 *     left Y-positive to suit; do not negate it again.
 *
 * Header-only — pure inline GLM math, so it drops into a module unit's global
 * module fragment the same way math/glm.hxx and math/pack_unpack.hxx do:
 *
 *     module;
 *     #include "math/transforms.hxx"
 *     export module vkgc.foo;
 *     ...
 */

#include "math/glm.hxx"

#include <glm/ext/matrix_clip_space.hpp>

namespace vkgc::math
{
    // Right-handed reversed-Z perspective projection with Vulkan [0, 1] clip
    // depth: the near plane maps to depth 1 and the far plane to depth 0 — the
    // reverse of the conventional near->0 mapping. Paired with a floating-point
    // depth buffer cleared to 0 and a VK_COMPARE_OP_GREATER depth test, the
    // inverted range spreads the float mantissa evenly across the frustum and
    // all but eliminates the far-plane z-fighting the standard mapping suffers.
    //
    // vfov is the vertical field of view in radians; aspect is width / height.
    // Preconditions:
    // znear > 0 and zfar > znear, both finite.
    //
    // The reversal is just glm::perspectiveRH_ZO with znear and zfar passed in
    // swapped — under [0, 1] clip depth that argument swap is provably identical
    // to the hand-built reversed-Z matrix, so we reuse GLM's tested frustum math
    // rather than transcribe entries by hand.
    [[nodiscard]]
    inline glm::mat4 rperspective(float const vfov, float const aspect, float const znear, float const zfar) noexcept
    {
        return glm::perspectiveRH_ZO(vfov, aspect, zfar, znear);
    }
}
