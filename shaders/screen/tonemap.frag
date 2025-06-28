/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
 */

#version 330 core

/* === Definitions === */

#define TONEMAP_LINEAR 0
#define TONEMAP_REINHARD 1
#define TONEMAP_FILMIC 2
#define TONEMAP_ACES 3
#define TONEMAP_AGX 4

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexColor;
uniform lowp int uTonemapMode;
uniform float uTonemapExposure;
uniform float uTonemapWhite;

/* === Fragments === */

out vec4 FragColor;

// === Helper functions === //

// Based on Reinhard's extended formula, see equation 4 in https://doi.org/cjbgrt
vec3 TonemapReinhard(vec3 color, float pWhite)
{
    float whiteSquared = pWhite * pWhite;
    vec3 whiteSquaredColor = whiteSquared * color;
    // Equivalent to color * (1 + color / whiteSquared) / (1 + color)
    return (whiteSquaredColor + color * color) / (whiteSquaredColor + whiteSquared);
}

vec3 TonemapFilmic(vec3 color, float pWhite)
{
    // exposure bias: input scale (color *= bias, white *= bias) to make the brightness consistent with other tonemappers
    // also useful to scale the input to the range that the uTonemapMode is designed for (some require very high input values)
    // has no effect on the curve's general shape or visual properties
    const float exposureBias = 2.0f;
    const float A = 0.22f * exposureBias * exposureBias; // bias baked into constants for performance
    const float B = 0.30f * exposureBias;
    const float C = 0.10f;
    const float D = 0.20f;
    const float E = 0.01f;
    const float F = 0.30f;

    vec3 colorTonemapped = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    float pWhiteTonemapped = ((pWhite * (A * pWhite + C * B) + D * E) / (pWhite * (A * pWhite + B) + D * F)) - E / F;

    return colorTonemapped / pWhiteTonemapped;
}

// Adapted from https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
// (MIT License).
vec3 TonemapACES(vec3 color, float pWhite)
{
    const float exposureBias = 1.8f;
    const float A = 0.0245786f;
    const float B = 0.000090537f;
    const float C = 0.983729f;
    const float D = 0.432951f;
    const float E = 0.238081f;

    // Exposure bias baked into transform to save shader instructions. Equivalent to `color *= exposureBias`
    const mat3 rgb_to_rrt = mat3(
        vec3(0.59719f * exposureBias, 0.35458f * exposureBias, 0.04823f * exposureBias),
        vec3(0.07600f * exposureBias, 0.90834f * exposureBias, 0.01566f * exposureBias),
        vec3(0.02840f * exposureBias, 0.13383f * exposureBias, 0.83777f * exposureBias)
    );

    const mat3 odt_to_rgb = mat3(
        vec3(1.60475f, -0.53108f, -0.07367f),
        vec3(-0.10208f, 1.10813f, -0.00605f),
        vec3(-0.00327f, -0.07276f, 1.07602f)
    );

    color *= rgb_to_rrt;
    vec3 colorTonemapped = (color * (color + A) - B) / (color * (C * color + D) + E);
    colorTonemapped *= odt_to_rgb;

    pWhite *= exposureBias;
    float pWhiteTonemapped = (pWhite * (pWhite + A) - B) / (pWhite * (C * pWhite + D) + E);

    return colorTonemapped / pWhiteTonemapped;
}

// Polynomial approximation of EaryChow's AgX sigmoid curve.
// x must be within the range [0.0, 1.0]
vec3 AgXContrastApprox(vec3 x)
{
	// Generated with Excel trendline
	// Input data: Generated using python sigmoid with EaryChow's configuration and 57 steps
	// Additional padding values were added to give correct intersections at 0.0 and 1.0
	// 6th order, intercept of 0.0 to remove an operation and ensure intersection at 0.0
	vec3 x2 = x * x;
	vec3 x4 = x2 * x2;
	return 0.021 * x + 4.0111 * x2 - 25.682 * x2 * x + 70.359 * x4 - 74.778 * x4 * x + 27.069 * x4 * x2;
}

// This is an approximation and simplification of EaryChow's AgX implementation that is used by Blender.
// This code is based off of the script that generates the AgX_Base_sRGB.cube LUT that Blender uses.
// Source: https://github.com/EaryChow/AgX_LUT_Gen/blob/main/AgXBasesRGB.py
vec3 TonemapAgX(vec3 color)
{
	// Combined linear sRGB to linear Rec 2020 and Blender AgX inset matrices:
	const mat3 srgbToRec2020AgxInsetMat = mat3(
        0.54490813676363087053, 0.14044005884001287035, 0.088827411851915368603,
        0.37377945959812267119, 0.75410959864013760045, 0.17887712465043811023,
        0.081384976686407536266, 0.10543358536857773485, 0.73224999956948382528
    );

	// Combined inverse AgX outset matrix and linear Rec 2020 to linear sRGB matrices.
	const mat3 agxOutsetRec2020ToSrgbMatrix = mat3(
        1.9645509602733325934, -0.29932243390911083839, -0.16436833806080403409,
        -0.85585845117807513559, 1.3264510741502356555, -0.23822464068860595117,
        -0.10886710826831608324, -0.027084020983874825605, 1.402665347143271889
    );

	// LOG2_MIN      = -10.0
	// LOG2_MAX      =  +6.5
	// MIDDLE_GRAY   =  0.18
	const float minEV = -12.4739311883324;  // log2(pow(2, LOG2_MIN) * MIDDLE_GRAY)
	const float maxEV = 4.02606881166759;   // log2(pow(2, LOG2_MAX) * MIDDLE_GRAY)

	// Large negative values in one channel and large positive values in other
	// channels can result in a colour that appears darker and more saturated than
	// desired after passing it through the inset matrix. For this reason, it is
	// best to prevent negative input values.
	// This is done before the Rec. 2020 transform to allow the Rec. 2020
	// transform to be combined with the AgX inset matrix. This results in a loss
	// of color information that could be correctly interpreted within the
	// Rec. 2020 color space as positive RGB values, but it is less common for Godot
	// to provide this function with negative sRGB values and therefore not worth
	// the performance cost of an additional matrix multiplication.
	// A value of 2e-10 intentionally introduces insignificant error to prevent
	// log2(0.0) after the inset matrix is applied; color will be >= 1e-10 after
	// the matrix transform.
	color = max(color, 2e-10);

	// Do AGX in rec2020 to match Blender and then apply inset matrix.
	color = srgbToRec2020AgxInsetMat * color;

	// Log2 space encoding.
	// Must be clamped because agx_contrast_approx may not work
	// well with values outside of the range [0.0, 1.0]
	color = clamp(log2(color), minEV, maxEV);
	color = (color - minEV) / (maxEV - minEV);

	// Apply sigmoid function approximation.
	color = AgXContrastApprox(color);

	// Convert back to linear before applying outset matrix.
	color = pow(color, vec3(2.4));

	// Apply outset to make the result more chroma-laden and then go back to linear sRGB.
	color = agxOutsetRec2020ToSrgbMatrix * color;

	// Blender's lusRGB.compensate_low_side is too complex for this shader, so
	// simply return the color, even if it has negative components. These negative
	// components may be useful for subsequent color adjustments.
	return color;
}

vec3 Tonemapping(vec3 color, float pWhite) // inputs are LINEAR
{
    // Ensure color values passed to tonemappers are positive.
    // They can be negative in the case of negative lights, which leads to undesired behavior.
    if (uTonemapMode == TONEMAP_REINHARD) return TonemapReinhard(max(vec3(0.0f), color), pWhite);
    if (uTonemapMode == TONEMAP_FILMIC) return TonemapFilmic(max(vec3(0.0f), color), pWhite);
    if (uTonemapMode == TONEMAP_ACES) return TonemapACES(max(vec3(0.0f), color), pWhite);
    if (uTonemapMode == TONEMAP_AGX) return TonemapAgX(color);
    return color; // TONEMAP_LINEAR
}


// === Main program === //

void main()
{
    // Sampling scene color texture
    vec3 result = texture(uTexColor, vTexCoord).rgb;

    // Appply tonemapping
    result *= uTonemapExposure;
    result = Tonemapping(result, uTonemapWhite);

    // Final color output
    FragColor = vec4(result, 1.0);
}
