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

#define BLOOM_MIX           1
#define BLOOM_ADDITIVE      2
#define BLOOM_SCREEN        3

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexColor;
uniform sampler2D uTexBloomBlur;

uniform lowp int uBloomMode;
uniform float uBloomIntensity;

/* === Fragments === */

out vec3 FragColor;

/* === Main program === */

void main()
{
    // Sampling scene color texture
    vec3 color = texture(uTexColor, vTexCoord).rgb;

    // Apply bloom
    vec3 bloom = texture(uTexBloomBlur, vTexCoord).rgb;
    bloom *= uBloomIntensity;

    if (uBloomMode == BLOOM_MIX) {
        color = mix(color, bloom, uBloomIntensity);
    }
    else if (uBloomMode == BLOOM_ADDITIVE) {
        color += bloom;
    }
    else if (uBloomMode == BLOOM_SCREEN) {
        bloom = clamp(bloom, vec3(0.0), vec3(1.0));
        color = max((color + bloom) - (color * bloom), vec3(0.0));
    }

    // Final color output
    FragColor = vec3(color);
}
