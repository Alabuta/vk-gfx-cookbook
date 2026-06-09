// Round-trip exercise for math/pack_unpack.hxx.
//
// Not a unit-test-framework test (the project ships none yet) — a small
// standalone executable that packs/unpacks a spread of normals and UVs, checks
// each survives within the precision its storage type allows, and returns
// EXIT_FAILURE on any violation. Its real job is twofold: prove the header
// compiles clean under the project's -Werror -Wconversion -Wold-style-cast
// wall, and guard the round-trip invariants against future edits.
//
// The oct bounds below are deliberately loose regression guards, not precision
// asserts: the suite prints the measured worst-case errors, so tighten the
// limits toward the printed numbers if you want a sharper precision guard.

#include "math/pack_unpack.hxx"

#include "math/glm.hxx"

#include <glm/gtc/constants.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <print>
#include <string_view>

static int failures{0};

// Assert measured < limit; the comparison is computed here so the verdict and
// the reported numbers can never drift apart.
static void check_lt(float const measured, float const limit, std::string_view const what)
{
    if (!(measured < limit))
    {
        ++failures;
        std::println(stderr, "FAIL  {}: measured {:.6g}, limit {:.6g}", what, measured, limit);
    }
}

// Assert measured <= limit.
static void check_le(float const measured, float const limit, std::string_view const what)
{
    if (!(measured <= limit))
    {
        ++failures;
        std::println(stderr, "FAIL  {}: measured {:.6g}, limit {:.6g}", what, measured, limit);
    }
}

static float deg(float const rad)
{
    return rad * 180.f / glm::pi<float>();
}

// Worst-case angular error (radians) of an oct round-trip, sampled over the
// encoding's own [-1, 1]^2 domain rather than a (theta, phi) sphere grid. A
// uniform oct grid is uniform in the space the quantization error varies over
// and lands on the fold seams by construction, where the error peaks; an angle
// lattice oversamples the poles and can miss the seams entirely. The pack
// function carries its own storage type T and unpack_normal_oct deduces it, so
// the two cannot be paired up wrong.
template <class T>
static float max_oct_error(T pack)
{
    int constexpr steps{256};

    float worst{0.f};

    for (int i{0}; i <= steps; ++i)
    {
        float const ox = 2.f * static_cast<float>(i) / static_cast<float>(steps) - 1.f; // [-1, 1]

        for (int j{0}; j <= steps; ++j)
        {
            float const oy = 2.f * static_cast<float>(j) / static_cast<float>(steps) - 1.f; // [-1, 1]

            glm::vec3 const n = vkgc::math::decode_oct(glm::vec2{ox, oy});
            glm::vec3 const r = vkgc::math::unpack_normal_oct(pack(n));

            float const cos_angle = glm::clamp(glm::dot(n, r), -1.f, 1.f);
            worst = glm::max(worst, std::acos(cos_angle));
        }
    }

    return worst;
}

int main()
{
    using namespace vkgc::math;

    // --- Octahedral normals: error must stay within the storage budget, and
    //     the precise variant must never do worse than the fast one. ---
    {
        // Loose regression bounds (see file header). int8 oct sits a few degrees
        // off worst-case; int16 a small fraction of a degree.
        float constexpr int8_limit_deg{8.f};
        float constexpr int16_limit_deg{0.1f};
        float constexpr precise_slack_deg{1e-4f}; // absorbs float noise in the angle

        float const i8_fast = deg(max_oct_error(pack_normal_oct<std::int8_t>));
        float const i8_precise = deg(max_oct_error(pack_normal_oct_precise<std::int8_t>));
        float const i16_fast = deg(max_oct_error(pack_normal_oct<std::int16_t>));
        float const i16_precise = deg(max_oct_error(pack_normal_oct_precise<std::int16_t>));

        check_lt(i8_fast, int8_limit_deg, "int8 oct fast (deg)");
        check_lt(i8_precise, int8_limit_deg, "int8 oct precise (deg)");
        check_le(i8_precise, i8_fast + precise_slack_deg, "int8 precise <= fast (deg)");

        check_lt(i16_fast, int16_limit_deg, "int16 oct fast (deg)");
        check_lt(i16_precise, int16_limit_deg, "int16 oct precise (deg)");
        check_le(i16_precise, i16_fast + precise_slack_deg, "int16 precise <= fast (deg)");

        std::println(
            "oct error (deg): int8 fast={:.4f} precise={:.4f} | int16 fast={:.5f} precise={:.5f}",
            i8_fast, i8_precise, i16_fast, i16_precise);
    }

    // --- UV half (R16G16_SFLOAT): tiling-safe. Inputs are deliberately
    //     non-dyadic (plus a couple of high-magnitude tiling coords) so the fp16
    //     rounding path is actually exercised — dyadic values round-trip exact
    //     and would make this check vacuous. ---
    {
        // fp16 round-to-nearest relative error is bounded by 2^-11 (half a ULP of
        // the 10-bit significand) ~= 4.9e-4; sit just above it.
        float constexpr half_max_rel_error{7e-4f};

        std::array constexpr cases{
            glm::vec2{0.f, 0.f},          // exact anchor
            glm::vec2{0.1f, 0.3f},        // non-dyadic, in [0, 1]
            glm::vec2{1.f / 3.f, 0.789f}, // non-dyadic
            glm::vec2{12.3f, -2.5f},      // tiling magnitude (and an exact anchor)
            glm::vec2{200.7f, 0.123f}     // large magnitude: fp16 ULP is coarse here
        };

        float worst_rel{0.f};

        for (glm::vec2 const uv : cases)
        {
            glm::vec2 const r = unpack_uv_half(pack_uv_half(uv));
            glm::vec2 const denom{glm::max(std::abs(uv.x), 1.f), glm::max(std::abs(uv.y), 1.f)};
            glm::vec2 const rel = glm::abs(r - uv) / denom;
            worst_rel = glm::max(worst_rel, glm::max(rel.x, rel.y));
        }

        check_lt(worst_rel, half_max_rel_error, "uv half relative error");
        std::println("uv half worst relative error: {:.6g}", worst_rel);
    }

    // --- UV unorm (R16G16_UNORM): [0, 1] only. Non-dyadic inputs exercise the
    //     quantization; 0.5 lands on 32767.5 and forces a round. ---
    {
        // unorm16 round-to-nearest absolute error is bounded by 0.5 ULP =
        // 0.5/65535; cap at 0.75 ULP so a switch to truncation (~1 ULP) trips it.
        float constexpr unorm_max_abs_error{0.75f / 65535.f};

        std::array constexpr cases{
            glm::vec2{0.f, 0.f},
            glm::vec2{0.25f, 0.5f},
            glm::vec2{0.123f, 0.789f},
            glm::vec2{1.f, 1.f}
        };

        float worst_abs{0.f};

        for (glm::vec2 const uv : cases)
        {
            glm::vec2 const r = unpack_uv_unorm(pack_uv_unorm(uv));
            glm::vec2 const e = glm::abs(r - uv);
            worst_abs = glm::max(worst_abs, glm::max(e.x, e.y));
        }

        check_lt(worst_abs, unorm_max_abs_error, "uv unorm absolute error");
        std::println("uv unorm worst absolute error: {:.6g}", worst_abs);
    }

    if (failures == 0)
    {
        std::println("pack_unpack: all round-trip checks passed");
        return EXIT_SUCCESS;
    }

    std::println(stderr, "pack_unpack: {} check(s) failed", failures);
    return EXIT_FAILURE;
}
