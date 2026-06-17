// Invariant exercise for math/transforms.hxx — specifically rperspective.
//
// Not a unit-test-framework test (the project ships none yet) — a small
// standalone executable that builds reversed-Z projection matrices over a
// spread of frustums and checks the defining invariants: the near plane maps to
// NDC depth 1, the far plane to 0, interior depths match the closed-form
// hyperbolic reversed-Z value at every sample and fall monotonically as a point
// recedes (a linear reversed mapping would pass the endpoints and monotonicity
// but fail the closed-form match), and the frustum corner maps to NDC (1, 1) —
// no Y-flip is baked in, since the project flips Y at the viewport. Like
// pack_unpack_test, its real
// job is twofold: prove the header compiles clean under the project's -Werror
// -Wconversion -Wold-style-cast wall, and guard the matrix against future edits.
// Returns EXIT_FAILURE on any violation.

#include "math/transforms.hxx"

#include "math/glm.hxx"

#include <array>
#include <cmath>
#include <cstdlib>
#include <print>
#include <string_view>

static int failures{0};

// Assert measured <= limit; the comparison is computed here so the verdict and
// the reported number can never drift apart.
static void check_le(float const measured, float const limit, std::string_view const what)
{
    if (!(measured <= limit))
    {
        ++failures;
        std::println(stderr, "FAIL  {}: measured {:.6g}, limit {:.6g}", what, measured, limit);
    }
}

namespace
{
    struct frustum
    {
        float vertical_fov; // radians
        float aspect;       // width / height
        float znear;
        float zfar;
    };

    // NDC depth of an on-axis point at view-space distance d in front of the
    // camera. The camera looks down -Z, so the point's view-space z is -d.
    [[nodiscard]] float ndc_depth(glm::mat4 const& m, float const d)
    {
        glm::vec4 const clip = m * glm::vec4{0.f, 0.f, -d, 1.f};
        return clip.z / clip.w;
    }

    // Closed-form reversed-Z NDC depth for an on-axis point at view distance d —
    // the exact value rperspective's matrix must reproduce. rperspective is
    // perspectiveRH_ZO with znear/zfar swapped, which yields clip.z =
    // znear*(zfar - d) and clip.w = d, hence depth = znear*(zfar - d) /
    // (d*(zfar - znear)). Checking against this pins the hyperbolic depth
    // distribution itself, not merely that depth decreases monotonically.
    [[nodiscard]] float reversed_z_depth(frustum const& f, float const d)
    {
        return f.znear * (f.zfar - d) / (d * (f.zfar - f.znear));
    }

    // NDC xy of a point sitting on the frustum's upper-right edge at view
    // distance d, where the half-extents are (aspect * d * tan(fovy/2),
    // d * tan(fovy/2)). It must land on (1, 1) before the viewport Y-flip.
    [[nodiscard]] glm::vec2 ndc_corner(frustum const& f, glm::mat4 const& m, float const d)
    {
        float const t = std::tan(f.vertical_fov / 2.f);
        glm::vec4 const clip = m * glm::vec4{f.aspect * d * t, d * t, -d, 1.f};
        return glm::vec2{clip.x / clip.w, clip.y / clip.w};
    }
}

int main()
{
    using vkgc::math::rperspective;

    std::array const frustums{
        frustum{glm::radians(60.f), 16.f / 9.f, 0.1f, 1000.f},
        frustum{glm::radians(90.f), 1.f, 0.01f, 100.f},
        frustum{glm::radians(45.f), 4.f / 3.f, 1.f, 10000.f},
    };

    // Loose regression guard: the endpoints are exact in real arithmetic but
    // shed a few ULPs through the matrix multiply and the perspective divide,
    // more so for the wide znear..zfar ranges. This bound sits well above the
    // observed fp32 error and trips only on a structural (not rounding) change.
    float constexpr eps{1e-5f};

    float worst_near{0.f};
    float worst_far{0.f};
    float worst_interior{0.f};
    float worst_corner{0.f};

    for (frustum const& f : frustums)
    {
        glm::mat4 const m = rperspective(f.vertical_fov, f.aspect, f.znear, f.zfar);

        // Reversed-Z endpoints: near -> 1, far -> 0.
        worst_near = glm::max(worst_near, std::abs(ndc_depth(m, f.znear) - 1.f));
        worst_far = glm::max(worst_far, std::abs(ndc_depth(m, f.zfar) - 0.f));

        // Depth must fall strictly as the point recedes (closer == nearer 1).
        // Walk znear -> zfar geometrically so the precision-rich near range is
        // sampled densely instead of swamped by the far range.
        int constexpr steps{32};
        float prev{ndc_depth(m, f.znear)};

        for (int i{1}; i <= steps; ++i)
        {
            float const u = static_cast<float>(i) / static_cast<float>(steps);
            float const d = f.znear * std::pow(f.zfar / f.znear, u);
            float const z = ndc_depth(m, d);

            // Pin the hyperbolic value at this sample, not just the slope sign.
            worst_interior = glm::max(worst_interior, std::abs(z - reversed_z_depth(f, d)));

            if (!(z < prev))
            {
                ++failures;
                std::println(stderr, "FAIL  reversed-Z not monotonic at d={:.6g}: {:.6g} !< {:.6g}", d, z, prev);
            }

            prev = z;
        }

        // Frustum corner maps to NDC (1, 1) at any depth; sample the midpoint.
        glm::vec2 const corner = ndc_corner(f, m, (f.znear + f.zfar) * 0.5f);
        worst_corner = glm::max(worst_corner, glm::max(std::abs(corner.x - 1.f), std::abs(corner.y - 1.f)));
    }

    check_le(worst_near, eps, "near plane -> NDC depth 1");
    check_le(worst_far, eps, "far plane -> NDC depth 0");
    check_le(worst_interior, eps, "interior depth -> closed-form reversed-Z");
    check_le(worst_corner, eps, "frustum corner -> NDC (1, 1)");

    std::println(
        "rperspective: worst near err={:.3g}, far err={:.3g}, interior err={:.3g}, corner err={:.3g}",
        worst_near, worst_far, worst_interior, worst_corner);

    if (failures == 0)
    {
        std::println("transforms: all reversed-Z checks passed");
        return EXIT_SUCCESS;
    }

    std::println(stderr, "transforms: {} check(s) failed", failures);
    return EXIT_FAILURE;
}
