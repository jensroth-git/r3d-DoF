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

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexColor;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;

/* === Fragments === */

out vec4 FragColor;

/* === Helper functions === */

vec3 LinearToSRGB(vec3 color)
{
	// color = clamp(color, vec3(0.0), vec3(1.0));
	// const vec3 a = vec3(0.055f);
	// return mix((vec3(1.0f) + a) * pow(color.rgb, vec3(1.0f / 2.4f)) - a, 12.92f * color.rgb, lessThan(color.rgb, vec3(0.0031308f)));
	// Approximation from http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
	return max(vec3(1.055) * pow(color, vec3(0.416666667)) - vec3(0.055), vec3(0.0));
}

/* === Main program === */

void main()
{
    // Sampling scene color texture
    vec3 result = texture(uTexColor, vTexCoord).rgb;

    // Color adjustment
	result = mix(vec3(0.0), result, uBrightness);
	result = mix(vec3(0.5), result, uContrast);
	result = mix(vec3(dot(vec3(1.0), result) * 0.33333), result, uSaturation);

	// Linear to sRGB conversion
	result = LinearToSRGB(result);

    // Final color output
    FragColor = vec4(result, 1.0);
}
