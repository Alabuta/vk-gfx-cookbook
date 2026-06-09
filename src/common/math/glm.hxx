#pragma once

/*
 * Project-wide GLM entry point. Include this instead of <glm/glm.hpp>
 * directly so there is one canonical include site for linear-algebra types.
 *
 * The GLM configuration macros (GLM_FORCE_DEPTH_ZERO_TO_ONE, etc.) are NOT
 * set here — they live on the vkgc::dependencies::math CMake target as
 * INTERFACE compile definitions, so they apply uniformly to every TU in a
 * binary that links it (see cmake/Interfaces.cmake). Any target whose TUs
 * touch GLM must link vkgc::dependencies::math; this header alone does not
 * configure GLM.
 *
 * Header-only — GLM is pure template code, so this wraps cleanly into a
 * module unit's global module fragment the same way vulkan/format.hxx and
 * vulkan/assert.hxx do:
 *
 *     module;
 *     #include "math/glm.hxx"
 *     module vkgc.foo;
 *     ...
 */

#include <glm/glm.hpp>
