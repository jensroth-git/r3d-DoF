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

flat in vec3 vEmission;
in vec2 vTexCoord;
in vec3 vColor;
in mat3 vTBN;


/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;
uniform sampler2D uTexEmission;
uniform sampler2D uTexOcclusion;
uniform sampler2D uTexRoughness;
uniform sampler2D uTexMetalness;

uniform float uValOcclusion;
uniform float uValRoughness;
uniform float uValMetalness;


/* === Fragments === */

layout(location = 0) out vec3 FragAlbedo;
layout(location = 1) out vec3 FragEmission;
layout(location = 2) out vec2 FragNormal;
layout(location = 3) out vec3 FragORM;


/* === Helper functions === */

vec2 EncodeOctahedral(vec3 normal)
{
    // Normalize to avoid numerical errors
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    
    // Store X and Y in the plane
    vec2 encoded = normal.xy;
    
    // Fold the negative hemisphere (avoiding the use of boolean vectors)
    if (normal.z < 0.0) {
        vec2 signValue = vec2(normal.x >= 0.0 ? 1.0 : -1.0, normal.y >= 0.0 ? 1.0 : -1.0);
        encoded = (1.0 - abs(encoded.yx)) * signValue;
    }
    
    // Remap from [-1,1] to [0,1] for texture storage
    return encoded * 0.5 + 0.5;
}


/* === Main function === */

void main()
{
    FragAlbedo = vColor * texture(uTexAlbedo, vTexCoord).rgb;
    FragEmission = vEmission * texture(uTexEmission, vTexCoord).rgb;
    FragNormal = EncodeOctahedral(normalize(vTBN * (texture(uTexNormal, vTexCoord).rgb * 2.0 - 1.0)));
    
    FragORM.r = uValOcclusion * texture(uTexOcclusion, vTexCoord).r;
    FragORM.g = uValRoughness * texture(uTexRoughness, vTexCoord).g;
    FragORM.b = uValMetalness * texture(uTexMetalness, vTexCoord).b;
}
