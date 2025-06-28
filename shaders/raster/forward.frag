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

/* === Defines === */

#define PI 3.1415926535897932384626433832795028

#define NUM_LIGHTS  8

#define DIRLIGHT    0
#define SPOTLIGHT   1
#define OMNILIGHT   2

/* === Structs === */

struct Light
{
    sampler2D shadowMap;
    samplerCube shadowCubemap;
    vec3 color;
    vec3 position;
    vec3 direction;
    float specular;
    float energy;
    float range;
    float size;
    float near;
    float far;
    float attenuation;
    float innerCutOff;
    float outerCutOff;
    float shadowMapTxlSz;
    float shadowBias;
    lowp int type;
    bool enabled;
    bool shadow;
};

/* === Varyings === */

in vec3 vPosition;
in vec2 vTexCoord;
in vec4 vColor;
in mat3 vTBN;

in vec4 vPosLightSpace[NUM_LIGHTS];

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexEmission;
uniform sampler2D uTexNormal;
uniform sampler2D uTexOcclusion;
uniform sampler2D uTexRoughness;
uniform sampler2D uTexMetalness;

uniform sampler2D uTexNoise;   //< Noise texture (used for poisson disk)

uniform float uValEmission;
uniform float uValOcclusion;
uniform float uValRoughness;
uniform float uValMetalness;

uniform vec3 uColAmbient;
uniform vec3 uColEmission;

uniform samplerCube uCubeIrradiance;
uniform samplerCube uCubePrefilter;
uniform sampler2D uTexBrdfLut;
uniform vec4 uQuatSkybox;
uniform bool uHasSkybox;

uniform Light uLights[NUM_LIGHTS];

uniform float uAlphaScissorThreshold;
uniform float uBloomHdrThreshold;
uniform vec3 uViewPosition;
uniform float uFar;

/* === Fragments === */

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec3 FragBrightness;

/* === Constants === */

const vec2 POISSON_DISK[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790)
);

/* === PBR functions === */

float DistributionGGX(float cosTheta, float alpha)
{
    // Standard GGX/Trowbridge-Reitz distribution - optimized form
    float a = cosTheta * alpha;
    float k = alpha / (1.0 - cosTheta * cosTheta + a * a);
    return k * k * (1.0 / PI);
}

float GeometryGGX(float NdotL, float NdotV, float roughness)
{
    // Hammon's optimized approximation for GGX Smith geometry term
    // This version is an efficient approximation that:
    // 1. Avoids expensive square root calculations
    // 2. Combines both G1 terms into a single expression
    // 3. Provides very close results to the exact version at a much lower cost
    // SEE: https://www.gdcvault.com/play/1024478/PBR-Diffuse-Lighting-for-GGX
    return 0.5 / mix(2.0 * NdotL * NdotV, NdotL + NdotV, roughness);
}

float SchlickFresnel(float u)
{
    float m = 1.0 - u;
    float m2 = m * m;
    return m2 * m2 * m; // pow(m,5)
}

vec3 ComputeF0(float metallic, float specular, vec3 albedo)
{
    float dielectric = 0.16 * specular * specular;
    // use (albedo * metallic) as colored specular reflectance at 0 angle for metallic materials
    // SEE: https://google.github.io/filament/Filament.md.html
    return mix(vec3(dielectric), albedo, vec3(metallic));
}

/* === Shadow functions === */

float ShadowOmni(int i, float cNdotL)
{
    // Calculate vector from light to fragment
    vec3 lightToFrag = vPosition - uLights[i].position;
    
    // Calculate current depth (distance from light to fragment)
    float currentDepth = length(lightToFrag);
    
    // Normalize direction for cubemap lookup
    vec3 direction = normalize(lightToFrag);
    
    // Calculate bias based on surface orientation
    // Steeper surfaces need larger bias to avoid shadow acne
    float bias = max(uLights[i].shadowBias * (1.0 - cNdotL), 0.05);
    currentDepth = currentDepth - bias;
    
    // Constants for PCSS algorithm
    const int BLOCKER_SEARCH_NUM_SAMPLES = 16;  // Number of samples for blocker search
    const int PCF_NUM_SAMPLES = 16;             // Number of samples for PCF filtering
    const float MIN_PENUMBRA_SIZE = 0.002;      // Minimum penumbra size to ensure some softness
    const float MAX_PENUMBRA_SIZE = 0.02;       // Maximum penumbra size to limit blur
    
    // Use noise texture to randomize rotation for each fragment
    // This helps reduce banding artifacts
    vec4 noiseTexel = texture(uTexNoise, fract(gl_FragCoord.xy / vec2(16.0)));
    float rotationAngle1 = noiseTexel.r * 2.0 * PI;     // For blocker search
    float rotationAngle2 = noiseTexel.g * 2.0 * PI;     // For PCF sampling
    
    // Compute tangent and bitangent vectors to create a local coordinate system
    // These are used to generate offset directions around the main direction
    vec3 tangent, bitangent;
    if (abs(direction.y) < 0.99) tangent = normalize(cross(vec3(0.0, 1.0, 0.0), direction));
    else tangent = normalize(cross(vec3(1.0, 0.0, 0.0), direction));
    bitangent = normalize(cross(direction, tangent));
    
    // Create rotation matrix for blocker search
    mat2 rotMat1 = mat2(
        cos(rotationAngle1), -sin(rotationAngle1),
        sin(rotationAngle1), cos(rotationAngle1)
    );
    
    // 1. BLOCKER SEARCH PHASE
    // Find average depth of occluders (blockers)
    float blockerSum = 0.0;
    float numBlockers = 0.0;
    float searchWidth = uLights[i].size / currentDepth;  // Search width depends on light size and depth
    
    for (int j = 0; j < BLOCKER_SEARCH_NUM_SAMPLES; j++)
    {
        // Rotate Poisson disk sample using the rotation matrix
        vec2 rotatedOffset = rotMat1 * POISSON_DISK[j] * searchWidth;
        
        // Convert 2D offset to 3D direction in cubemap space
        // Use tangent and bitangent as basis vectors for the local plane
        vec3 sampleDir = direction + (tangent * rotatedOffset.x + bitangent * rotatedOffset.y);
        sampleDir = normalize(sampleDir);
        
        // Sample depth from cubemap (multiply by far plane to get actual depth)
        float shadowMapDepth = texture(uLights[i].shadowCubemap, sampleDir).r * uLights[i].far;
        
        // Count only samples that are blockers (closer to light than current fragment)
        if (shadowMapDepth < currentDepth) {
            blockerSum += shadowMapDepth;
            numBlockers++;
        }
    }
    
    // If no blockers found, the point is fully lit
    if (numBlockers < 1.0) {
        return 1.0;
    }
    
    // 2. PENUMBRA ESTIMATION
    // Calculate average blocker depth and penumbra size
    float avgBlockerDepth = blockerSum / numBlockers;
    
    // Penumbra ratio based on similar triangles principle in PCSS
    float penumbraRatio = (currentDepth - avgBlockerDepth) / avgBlockerDepth;
    
    // Calculate filter radius based on penumbra size
    // This approximates the effect of larger shadows for objects further from the occluder
    float filterRadius = penumbraRatio * uLights[i].size * uLights[i].near / currentDepth;
    filterRadius = clamp(filterRadius, MIN_PENUMBRA_SIZE, MAX_PENUMBRA_SIZE);
    
    // Create different rotation matrix for PCF phase
    // Using different rotations helps further reduce pattern artifacts
    mat2 rotMat2 = mat2(
        cos(rotationAngle2), -sin(rotationAngle2),
        sin(rotationAngle2), cos(rotationAngle2)
    );
    
    // 3. PERCENTAGE CLOSER FILTERING (PCF) PHASE
    // Apply PCF with the estimated filter radius
    float shadow = 0.0;
    
    for (int k = 0; k < PCF_NUM_SAMPLES; k++)
    {
        // Apply rotation to the sample and scale by filter radius
        vec2 rotatedOffset = rotMat2 * POISSON_DISK[k] * filterRadius;
        
        // Convert 2D offset to 3D direction in cubemap space
        vec3 sampleDir = direction + (tangent * rotatedOffset.x + bitangent * rotatedOffset.y);
        sampleDir = normalize(sampleDir);
        
        // Sample depth from cubemap and convert to world units
        float closestDepth = texture(uLights[i].shadowCubemap, sampleDir).r * uLights[i].far;
        
        // step(a,b) returns 1.0 if b >= a, otherwise 0.0
        // If closestDepth >= currentDepth, the fragment is lit for this sample
        shadow += step(currentDepth, closestDepth);
    }
    
    // Average the results of all samples
    return shadow / float(PCF_NUM_SAMPLES);
}

float Shadow(int i, float cNdotL)
{
    // Transform position from light space
    vec4 p = vPosLightSpace[i];

    // Convert to NDC space [-1,1], then map to texture coordinates [0,1]
    vec3 projCoords = p.xyz / p.w;
    projCoords = projCoords * 0.5 + 0.5;

    // Early out if outside the shadow map bounds
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || 
        projCoords.y < 0.0 || projCoords.y > 1.0 || 
        projCoords.z < 0.0 || projCoords.z > 1.0) 
        return 1.0;
    
    // Calculate depth bias based on surface orientation
    // Steeper surfaces need a larger bias to avoid shadow acne
    float bias = max(uLights[i].shadowBias * (1.0 - cNdotL), 0.00002);
    float currentDepth = projCoords.z - bias;
    
    // Constants for PCSS algorithm
    const int BLOCKER_SEARCH_NUM_SAMPLES = 16;  // Number of samples for blocker search
    const int PCF_NUM_SAMPLES = 16;             // Number of samples for PCF filtering
    const float MIN_PENUMBRA_SIZE = 0.001;      // Minimum penumbra size to ensure some softness
    const float MAX_PENUMBRA_SIZE = 0.01;       // Maximum penumbra size to limit blur
    
    // Use noise texture to randomize rotation for each fragment
    // This helps reduce banding artifacts
    vec4 noiseTexel = texture(uTexNoise, fract(gl_FragCoord.xy / vec2(16.0)));
    float rotationAngle1 = noiseTexel.r * 2.0 * PI;     // For blocker search
    float rotationAngle2 = noiseTexel.g * 2.0 * PI;     // For PCF sampling
    
    // Precalculate rotation matrix for blocker search
    float cosRot = cos(rotationAngle1);
    float sinRot = sin(rotationAngle1);
    
    // 1. BLOCKER SEARCH PHASE
    // Find average depth of occluders (blockers)
    float blockerSum = 0.0;
    float numBlockers = 0.0;
    float searchWidth = uLights[i].size / projCoords.z;  // Search width depends on light size and depth
    
    for (int j = 0; j < BLOCKER_SEARCH_NUM_SAMPLES; j++)
    {
        // Rotate Poisson disk sample to reduce banding
        vec2 poissonRotated = vec2(
            POISSON_DISK[j].x * cosRot - POISSON_DISK[j].y * sinRot,
            POISSON_DISK[j].x * sinRot + POISSON_DISK[j].y * cosRot
        );
        
        // Apply offset to current position
        vec2 offset = poissonRotated * searchWidth;
        float shadowMapDepth = texture(uLights[i].shadowMap, projCoords.xy + offset).r;
        
        // Count only samples that are blockers (closer to light than current fragment)
        if (shadowMapDepth < currentDepth) {
            blockerSum += shadowMapDepth;
            numBlockers++;
        }
    }
    
    // If no blockers found, the point is fully lit
    if (numBlockers < 1.0) {
        return 1.0;
    }
    
    // 2. PENUMBRA ESTIMATION
    // Calculate average blocker depth and penumbra size
    float avgBlockerDepth = blockerSum / numBlockers;
    
    // Penumbra ratio based on similar triangles principle in PCSS
    float penumbraRatio = (currentDepth - avgBlockerDepth) / avgBlockerDepth;
    
    // Calculate filter radius based on penumbra size
    // This approximates the effect of larger shadows for objects further from the occluder
    float filterRadius = penumbraRatio * uLights[i].size * uLights[i].near / currentDepth;
    filterRadius = clamp(filterRadius, MIN_PENUMBRA_SIZE, MAX_PENUMBRA_SIZE);
    
    // 3. PERCENTAGE CLOSER FILTERING (PCF) PHASE
    // Apply PCF with the estimated filter radius
    float shadow = 0.0;
    float cosRotPCF = cos(rotationAngle2);
    float sinRotPCF = sin(rotationAngle2);
    
    for (int k = 0; k < PCF_NUM_SAMPLES; k++)
    {
        // Use different rotation for PCF to further reduce pattern artifacts
        vec2 poissonRotated = vec2(
            POISSON_DISK[k].x * cosRotPCF - POISSON_DISK[k].y * sinRotPCF,
            POISSON_DISK[k].x * sinRotPCF + POISSON_DISK[k].y * cosRotPCF
        );
        
        vec2 offset = poissonRotated * filterRadius;
        float closestDepth = texture(uLights[i].shadowMap, projCoords.xy + offset).r;
        
        // step(a,b) returns 1.0 if b >= a, otherwise 0.0
        // If closestDepth >= currentDepth, the fragment is lit for this sample
        shadow += step(currentDepth, closestDepth);
    }
    
    // Average the results of all samples
    return shadow / float(PCF_NUM_SAMPLES);
}

/* === Misc functions === */

vec3 RotateWithQuat(vec3 v, vec4 q)
{
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

float GetBrightness(vec3 color)
{
    return length(color);
}

/* === Main === */

void main()
{
    /* Sample albedo texture */

    vec4 albedo = vColor * texture(uTexAlbedo, vTexCoord);

    // TODO: Alpha scissor is unnecessary after a depth pre-pass
    if (albedo.a < uAlphaScissorThreshold) discard;

    /* Sample emission texture */

    vec3 emission = uValEmission * (uColEmission * texture(uTexEmission, vTexCoord).rgb);

    /* Sample ORM texture and extract values */

    float occlusion = uValOcclusion * texture(uTexOcclusion, vTexCoord).r;
    float roughness = uValRoughness * texture(uTexRoughness, vTexCoord).g;
    float metalness = uValMetalness * texture(uTexMetalness, vTexCoord).b;

    /* Compute F0 (reflectance at normal incidence) based on the metallic factor */

    vec3 F0 = ComputeF0(metalness, 0.5, albedo.rgb);

    /* Sample normal and compute view direction vector */

    vec3 N = normalize(vTBN * (texture(uTexNormal, vTexCoord).rgb * 2.0 - 1.0));
    vec3 V = normalize(uViewPosition - vPosition);

    /* Compute the dot product of the normal and view direction */

    float NdotV = dot(N, V);
    float cNdotV = max(NdotV, 1e-4);  // Clamped to avoid division by zero

    /* Loop through all light sources accumulating diffuse and specular light */

    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        if (uLights[i].enabled)
        {
            /* Compute light direction */

            vec3 L = vec3(0.0);
            if (uLights[i].type == DIRLIGHT) L = -uLights[i].direction;
            else L = normalize(uLights[i].position - vPosition);

            /* Compute the dot product of the normal and light direction */

            float NdotL = max(dot(N, L), 0.0);
            float cNdotL = min(NdotL, 1.0); // clamped NdotL

            /* Compute the halfway vector between the view and light directions */

            vec3 H = normalize(V + L);

            float LdotH = max(dot(L, H), 0.0);
            float cLdotH = min(dot(L, H), 1.0);

            float NdotH = max(dot(N, H), 0.0);
            float cNdotH = min(NdotH, 1.0);

            /* Compute light color energy */

            vec3 lightColE = uLights[i].color * uLights[i].energy;

            /* Compute diffuse lighting */

            vec3 diffLight = vec3(0.0);

            if (metalness < 1.0)
            {
                float FD90_minus_1 = 2.0 * cLdotH * cLdotH * roughness - 0.5;
                float FdV = 1.0 + FD90_minus_1 * SchlickFresnel(cNdotV);
                float FdL = 1.0 + FD90_minus_1 * SchlickFresnel(cNdotL);

                float diffBRDF = (1.0 / PI) * (FdV * FdL * cNdotL);
                diffLight = diffBRDF * lightColE;
            }

            /* Compute specular lighting */

            vec3 specLight = vec3(0.0);

            // NOTE: When roughness is 0, specular light should not be entirely disabled.

            if (roughness > 0.0)
            {
                float alphaGGX = roughness * roughness;
                float D = DistributionGGX(cNdotH, alphaGGX);
                float G = GeometryGGX(cNdotL, cNdotV, alphaGGX);

                float cLdotH5 = SchlickFresnel(cLdotH);
                float F90 = clamp(50.0 * F0.g, 0.0, 1.0);
                vec3 F = F0 + (F90 - F0) * cLdotH5;

                vec3 specBRDF = cNdotL * D * F * G;
                specLight = specBRDF * lightColE * uLights[i].specular;
            }

            /* Apply shadow factor if the light casts shadows */

            float shadow = 1.0;
            if (uLights[i].shadow)
            {
                if (uLights[i].type != OMNILIGHT) shadow = Shadow(i, cNdotL);
                else shadow = ShadowOmni(i, cNdotL);
            }

            /* Apply attenuation based on the distance from the light */

            if (uLights[i].type != DIRLIGHT)
            {
                float dist = length(uLights[i].position - vPosition);
                float atten = 1.0 - clamp(dist / uLights[i].range, 0.0, 1.0);
                shadow *= atten * uLights[i].attenuation;
            }

            /* Apply spotlight effect if the light is a spotlight */

            if (uLights[i].type == SPOTLIGHT)
            {
                float theta = dot(L, -uLights[i].direction);
                float epsilon = (uLights[i].innerCutOff - uLights[i].outerCutOff);
                shadow *= smoothstep(0.0, 1.0, (theta - uLights[i].outerCutOff) / epsilon);
            }

            /* Accumulate the diffuse and specular lighting contributions */

            diffuse += diffLight * shadow;
            specular += specLight * shadow;
        }
    }

    /* Compute ambient - (IBL diffuse) */

    vec3 ambient = uColAmbient;

    if (uHasSkybox)
    {
        vec3 kS = F0 + (1.0 - F0) * SchlickFresnel(cNdotV);
        vec3 kD = (1.0 - kS) * (1.0 - metalness);

        vec3 Nr = RotateWithQuat(N, uQuatSkybox);

        ambient = kD * texture(uCubeIrradiance, Nr).rgb;
    }

    /* Compute ambient occlusion map */

    ambient *= occlusion;

    // Light affect should be material-specific
    //float lightAffect = mix(1.0, ao, uValAOLightAffect);
    //specular *= lightAffect;
    //diffuse *= lightAffect;

    /* Skybox reflection - (IBL specular) */

    if (uHasSkybox)
    {
        vec3 R = RotateWithQuat(reflect(-V, N), uQuatSkybox);

        const float MAX_REFLECTION_LOD = 7.0;
        vec3 prefilteredColor = textureLod(uCubePrefilter, R, roughness * MAX_REFLECTION_LOD).rgb;

        float fresnelTerm = SchlickFresnel(cNdotV);
        vec3 F = F0 + (max(vec3(1.0 - roughness), F0) - F0) * fresnelTerm;

        vec2 brdf = texture(uTexBrdfLut, vec2(cNdotV, roughness)).rg;
        vec3 specularReflection = prefilteredColor * (F * brdf.x + brdf.y);

        specular += specularReflection;
    }

    /* Compute the final diffuse color, including ambient and diffuse lighting contributions */

    diffuse = albedo.rgb * (ambient + diffuse);

    /* Compute the final fragment color by combining diffuse, specular, and emission contributions */

    FragColor = vec4(diffuse + specular + emission, albedo.a);

    /* Handle bright colors for bloom */

    float brightness = GetBrightness(FragColor.rgb);
    FragBrightness = (brightness > uBloomHdrThreshold)
        ? vec3(FragColor.rgb) : vec3(0.0);
}
