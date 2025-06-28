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

#define NUM_LIGHTS 8

#define BILLBOARD_FRONT 1
#define BILLBOARD_Y_AXIS 2

/* === Attributes === */

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aTangent;

/* === Instance attributes === */

layout(location = 10) in mat4 iMatModel;
layout(location = 14) in vec4 iColor;

/* === Uniforms === */

uniform mat4 uMatLightVP[NUM_LIGHTS];

uniform mat4 uMatInvView;       ///< Only for billboard modes
uniform mat4 uMatModel;
uniform mat4 uMatVP;

uniform lowp int uBillboardMode;

uniform vec4 uColAlbedo;

uniform vec2 uTexCoordOffset;
uniform vec2 uTexCoordScale;

/* === Varyings === */

out vec3 vPosition;
out vec2 vTexCoord;
out vec4 vColor;
out mat3 vTBN;

out vec4 vPosLightSpace[NUM_LIGHTS];

/* === Helper functions === */

void BillboardFront(inout mat4 model, inout mat3 normal)
{
    // Extract the original scales of the model
    float scaleX = length(vec3(model[0]));
    float scaleY = length(vec3(model[1]));
    float scaleZ = length(vec3(model[2]));

    // Copy the inverted view vectors for the X, Y, and Z axes
    // while applying the original scales
    model[0] = vec4(normalize(uMatInvView[0].xyz) * scaleX, 0.0);
    model[1] = vec4(normalize(uMatInvView[1].xyz) * scaleY, 0.0);
    model[2] = vec4(normalize(uMatInvView[2].xyz) * scaleZ, 0.0);

    // Update the normal matrix
    // For normals, use the inverse transpose of the scales
    float invScaleX = 1.0 / scaleX;
    float invScaleY = 1.0 / scaleY;
    float invScaleZ = 1.0 / scaleZ;

    normal[0] = normalize(uMatInvView[0].xyz) * invScaleX;
    normal[1] = normalize(uMatInvView[1].xyz) * invScaleY;
    normal[2] = normalize(uMatInvView[2].xyz) * invScaleZ;
}

void BillboardY(inout mat4 model, inout mat3 normal)
{
    // Extract the model position
    vec3 position = vec3(model[3]);
    
    // Extract the original scales of the model
    float scaleX = length(vec3(model[0]));
    float scaleY = length(vec3(model[1]));
    float scaleZ = length(vec3(model[2]));
    
    // Preserve the original Y-axis of the model (vertical direction)
    vec3 upVector = normalize(vec3(model[1]));
    
    // Direction from the camera to the object
    vec3 lookDirection = normalize(position - vec3(uMatInvView[3]));
    
    // Compute the right vector using the cross product
    vec3 rightVector = normalize(cross(upVector, lookDirection));
    
    // Recalculate the front vector to ensure orthogonality
    vec3 frontVector = normalize(cross(rightVector, upVector));
    
    // Construct the new model matrix while preserving the scales
    model[0] = vec4(rightVector * scaleX, 0.0);
    model[1] = vec4(upVector * scaleY, 0.0);
    model[2] = vec4(frontVector * scaleZ, 0.0);
    
    // Update the normal matrix
    // For normals, use the inverse transpose of the scales
    float invScaleX = 1.0 / scaleX;
    float invScaleY = 1.0 / scaleY;
    float invScaleZ = 1.0 / scaleZ;
    
    normal[0] = rightVector * invScaleX;
    normal[1] = upVector * invScaleY;
    normal[2] = frontVector * invScaleZ;
}

/* === Main program === */

void main()
{
    vTexCoord = uTexCoordOffset + aTexCoord * uTexCoordScale;
    vColor = aColor * iColor * uColAlbedo;

    mat4 matModel = uMatModel * transpose(iMatModel);
    mat3 matNormal = mat3(0.0);

    if (uBillboardMode == BILLBOARD_FRONT) BillboardFront(matModel, matNormal);
    else if (uBillboardMode == BILLBOARD_Y_AXIS) BillboardY(matModel, matNormal);
    else matNormal = transpose(inverse(mat3(matModel)));

    vPosition = vec3(matModel * vec4(aPosition, 1.0));

    // The TBN matrix is used to transform vectors from tangent space to world space
    // It is currently used to transform normals from a normal map to world space normals
    vec3 T = normalize(vec3(matModel * vec4(aTangent.xyz, 0.0)));
    vec3 N = normalize(matNormal * aNormal);
    vec3 B = normalize(cross(N, T)) * aTangent.w;
    vTBN = mat3(T, B, N);

    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        vPosLightSpace[i] = uMatLightVP[i] * vec4(vPosition, 1.0);
    }

    gl_Position = uMatVP * (matModel * vec4(aPosition, 1.0));
}
