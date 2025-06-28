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

#include "./r3d_drawcall.h"
#include "../r3d_state.h"

#include <stdlib.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>


/* === Internal functions === */

// This function supports instanced rendering when necessary
static void r3d_draw_vertex_arrays(const r3d_drawcall_t* call);
static void r3d_draw_vertex_arrays_inst(const r3d_drawcall_t* call, int locInstanceModel, int locInstanceColor);

// Comparison functions for sorting draw calls in the arrays
static int r3d_drawcall_compare_front_to_back(const void* a, const void* b);
static int r3d_drawcall_compare_back_to_front(const void* a, const void* b);


/* === Function definitions === */

void r3d_drawcall_sort_front_to_back(r3d_drawcall_t* calls, size_t count)
{
    qsort(calls, count, sizeof(r3d_drawcall_t), r3d_drawcall_compare_front_to_back);
}

void r3d_drawcall_sort_back_to_front(r3d_drawcall_t* calls, size_t count)
{
    qsort(calls, count, sizeof(r3d_drawcall_t), r3d_drawcall_compare_back_to_front);
}

void r3d_drawcall_raster_depth(const r3d_drawcall_t* call)
{
    if (call->geometryType != R3D_DRAWCALL_GEOMETRY_MESH) {
        return;
    }

    Matrix matMVP = MatrixMultiply(call->transform, rlGetMatrixTransform());
    matMVP = MatrixMultiply(matMVP, rlGetMatrixModelview());
    matMVP = MatrixMultiply(matMVP, rlGetMatrixProjection());

    // Send model view projection matrix
    r3d_shader_set_mat4(raster.depth, uMatMVP, matMVP);

    // Send alpha and bind albedo
    r3d_shader_set_float(raster.depth, uAlpha, ((float)call->material.maps[MATERIAL_MAP_ALBEDO].color.a / 255));
    r3d_shader_bind_sampler2D_opt(raster.depth, uTexAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].texture.id, white);

    // Bind vertex buffers
    if (!rlEnableVertexArray(call->geometry.mesh.vaoId)) {
        // Bind positions
        rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);
        // Bind texture coordinates
        rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);
        // Bind vertex colors
        if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0) {
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
        }
        else {
            // Set default value for defined vertex attribute in shader but not provided by mesh
            // WARNING: It could result in GPU undefined behaviour
            float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, value, SHADER_ATTRIB_VEC4, 4);
            rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
        }
        // Bind indices
        if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] > 0) {
            rlEnableVertexBufferElement(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
        }
    }

    // Draw vertex buffers
    r3d_draw_vertex_arrays(call);

    // Unbind vertex buffers
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Unbind samplers
    r3d_shader_unbind_sampler2D(raster.depth, uTexAlbedo);
}

void r3d_drawcall_raster_depth_inst(const r3d_drawcall_t* call)
{
    if (call->geometryType != R3D_DRAWCALL_GEOMETRY_MESH) {
        return;
    }

    Matrix matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    Matrix matVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());

    // Send matrices
    r3d_shader_set_mat4(raster.depthInst, uMatModel, matModel);
    r3d_shader_set_mat4(raster.depthInst, uMatVP, matVP);

    // Send billboard related data
    r3d_shader_set_int(raster.depthInst, uBillboardMode, call->instanced.billboardMode);
    if (call->instanced.billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.depthInst, uMatInvView, R3D.state.transform.invView);
    }

    // Send alpha and bind albedo
    r3d_shader_set_float(raster.depthInst, uAlpha, ((float)call->material.maps[MATERIAL_MAP_ALBEDO].color.a / 255));
    r3d_shader_bind_sampler2D_opt(raster.depthInst, uTexAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].texture.id, white);

    // Bind vertex buffers
    if (!rlEnableVertexArray(call->geometry.mesh.vaoId)) {
        // Bind positions
        rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);
        // Bind texture coordinates
        rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);
        // Bind vertex colors
        if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0) {
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
        }
        else {
            // Set default value for defined vertex attribute in shader but not provided by mesh
            // WARNING: It could result in GPU undefined behaviour
            float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, value, SHADER_ATTRIB_VEC4, 4);
            rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
        }
        // Bind indices
        if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] > 0) {
            rlEnableVertexBufferElement(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
        }
    }

    // Draw vertex buffers
    r3d_draw_vertex_arrays_inst(call, 10, -1);

    // Unbind vertex buffers
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Unbind samplers
    r3d_shader_unbind_sampler2D(raster.depthInst, uTexAlbedo);
}

void r3d_drawcall_raster_depth_cube(const r3d_drawcall_t* call)
{
    if (call->geometryType != R3D_DRAWCALL_GEOMETRY_MESH) {
        return;
    }

    Matrix matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    Matrix matMVP = MatrixMultiply(matModel, rlGetMatrixModelview());
    matMVP = MatrixMultiply(matMVP, rlGetMatrixProjection());

    // Send matrices
    r3d_shader_set_mat4(raster.depthCube, uMatModel, matModel);
    r3d_shader_set_mat4(raster.depthCube, uMatMVP, matMVP);

    // Send alpha and bind albedo
    r3d_shader_set_float(raster.depthCube, uAlpha, ((float)call->material.maps[MATERIAL_MAP_ALBEDO].color.a / 255));
    r3d_shader_bind_sampler2D_opt(raster.depthCube, uTexAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].texture.id, white);

    // Bind vertex buffers
    if (!rlEnableVertexArray(call->geometry.mesh.vaoId)) {
        // Bind positions
        rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);
        // Bind texture coordinates
        rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);
        // Bind vertex colors
        if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0) {
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
        }
        else {
            // Set default value for defined vertex attribute in shader but not provided by mesh
            // WARNING: It could result in GPU undefined behaviour
            float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, value, SHADER_ATTRIB_VEC4, 4);
            rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
        }
        // Bind indices
        if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] > 0) {
            rlEnableVertexBufferElement(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
        }
    }

    // Draw vertex buffers
    r3d_draw_vertex_arrays(call);

    // Unbind vertex buffers
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Unbind samplers
    r3d_shader_unbind_sampler2D(raster.depthCube, uTexAlbedo);
}

void r3d_drawcall_raster_depth_cube_inst(const r3d_drawcall_t* call)
{
    if (call->geometryType != R3D_DRAWCALL_GEOMETRY_MESH) {
        return;
    }

    Matrix matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    Matrix matVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());

    // Send matrices
    r3d_shader_set_mat4(raster.depthCubeInst, uMatModel, matModel);
    r3d_shader_set_mat4(raster.depthCubeInst, uMatVP, matVP);

    // Send billboard related data
    r3d_shader_set_int(raster.depthCubeInst, uBillboardMode, call->instanced.billboardMode);
    if (call->instanced.billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.depthCubeInst, uMatInvView, R3D.state.transform.invView);
    }

    // Send alpha and bind albedo
    r3d_shader_set_float(raster.depthCubeInst, uAlpha, ((float)call->material.maps[MATERIAL_MAP_ALBEDO].color.a / 255));
    r3d_shader_bind_sampler2D_opt(raster.depthCubeInst, uTexAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].texture.id, white);

    // Bind vertex buffers
    if (!rlEnableVertexArray(call->geometry.mesh.vaoId)) {
        // Bind positions
        rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);
        // Bind texture coordinates
        rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);
        // Bind vertex colors
        if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0) {
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
        }
        else {
            // Set default value for defined vertex attribute in shader but not provided by mesh
            // WARNING: It could result in GPU undefined behaviour
            float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, value, SHADER_ATTRIB_VEC4, 4);
            rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
        }
        // Bind indices
        if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] > 0) {
            rlEnableVertexBufferElement(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
        }
    }

    // Draw vertex buffers
    r3d_draw_vertex_arrays_inst(call, 10, -1);

    // Unbind vertex buffers
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Unbind samplers
    r3d_shader_unbind_sampler2D(raster.depthCubeInst, uTexAlbedo);
}

void r3d_drawcall_raster_geometry(const r3d_drawcall_t* call)
{
    Matrix matModel = MatrixIdentity();
    Matrix matView = rlGetMatrixModelview();
    Matrix matModelView = MatrixIdentity();
    Matrix matProjection = rlGetMatrixProjection();

    // Compute model and model/view matrices
    matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    matModelView = MatrixMultiply(matModel, matView);

    // Set additional matrix uniforms
    r3d_shader_set_mat4(raster.geometry, uMatNormal, MatrixTranspose(MatrixInvert(matModel)));
    r3d_shader_set_mat4(raster.geometry, uMatModel, matModel);

    // Set factor material maps
    r3d_shader_set_float(raster.geometry, uValEmission, call->material.maps[MATERIAL_MAP_EMISSION].value);
    r3d_shader_set_float(raster.geometry, uValOcclusion, call->material.maps[MATERIAL_MAP_OCCLUSION].value);
    r3d_shader_set_float(raster.geometry, uValRoughness, call->material.maps[MATERIAL_MAP_ROUGHNESS].value);
    r3d_shader_set_float(raster.geometry, uValMetalness, call->material.maps[MATERIAL_MAP_METALNESS].value);

    // Set color material maps
    r3d_shader_set_col3(raster.geometry, uColAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].color);
    r3d_shader_set_col3(raster.geometry, uColEmission, call->material.maps[MATERIAL_MAP_EMISSION].color);

    // Bind active texture maps
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexNormal, call->material.maps[MATERIAL_MAP_NORMAL].texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexEmission, call->material.maps[MATERIAL_MAP_EMISSION].texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexOcclusion, call->material.maps[MATERIAL_MAP_OCCLUSION].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexRoughness, call->material.maps[MATERIAL_MAP_ROUGHNESS].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexMetalness, call->material.maps[MATERIAL_MAP_METALNESS].texture.id, black);

    // Setup sprite related uniforms
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_shader_set_vec2(raster.geometry, uTexCoordOffset, call->geometry.sprite.uvOffset);
        r3d_shader_set_vec2(raster.geometry, uTexCoordScale, call->geometry.sprite.uvScale);
    }
    else {
        r3d_shader_set_vec2(raster.geometry, uTexCoordOffset, ((Vector2) { 0, 0 }));
        r3d_shader_set_vec2(raster.geometry, uTexCoordScale, ((Vector2) { 1, 1 }));
    }

    // Try binding vertex array objects (VAO) or use VBOs if not possible
    // WARNING: UploadMesh() enables all vertex attributes available in mesh and sets default attribute values
    // for shader expected vertex attributes that are not provided by the mesh (i.e. colors)
    // This could be a dangerous approach because different meshes with different shaders can enable/disable some attributes
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_MESH) {
        if (!rlEnableVertexArray(call->geometry.mesh.vaoId)) {
            // Bind mesh VBO - Positions
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);

            // Bind mesh VBO - TexCoords
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);

            // Bind mesh VBO - Normals
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL, 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL);

            // Bind mesh VBO - Tagents
            if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT] != 0) {
                rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
                rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, 4, RL_FLOAT, 0, 0, 0);
                rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
            }
            else {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                float value[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
                rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
            }

            // Bind mesh VBO - Colors
            if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0) {
                rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
                rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
            }
            else {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
            }

            if (call->geometry.mesh.indices != NULL) {
                rlEnableVertexBufferElement(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
            }
        }
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++) {
        // Calculate model-view-projection matrix (MVP)
        Matrix matModelViewProjection = MatrixIdentity();
        if (eyeCount == 1) {
            matModelViewProjection = MatrixMultiply(matModelView, matProjection);
        }
        else {
            // Setup current eye viewport (half screen width)
            rlViewport(eye * rlGetFramebufferWidth() / 2, 0, rlGetFramebufferWidth() / 2, rlGetFramebufferHeight());
            matModelViewProjection = MatrixMultiply(MatrixMultiply(matModelView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        r3d_shader_set_mat4(raster.geometry, uMatMVP, matModelViewProjection);

        // Rasterisation du mesh en tenant compte du rendu instanci� si besoin
        r3d_draw_vertex_arrays(call);
    }

    // Unbind all bound texture maps
    r3d_shader_unbind_sampler2D(raster.geometry, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexOcclusion);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexRoughness);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexMetalness);

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Restore rlgl internal modelview and projection matrices
    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
}

void r3d_drawcall_raster_geometry_inst(const r3d_drawcall_t* call)
{
    if (call->instanced.count == 0 || call->instanced.transforms == NULL) {
        return;
    }

    // Get current view / projection matrices
    Matrix matView = rlGetMatrixModelview();
    Matrix matProjection = rlGetMatrixProjection();

    // Compute model matrix
    Matrix matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());

    // Set additional matrix uniforms
    r3d_shader_set_mat4(raster.geometryInst, uMatModel, matModel);

    // Set factor material maps
    r3d_shader_set_float(raster.geometryInst, uValEmission, call->material.maps[MATERIAL_MAP_EMISSION].value);
    r3d_shader_set_float(raster.geometryInst, uValOcclusion, call->material.maps[MATERIAL_MAP_OCCLUSION].value);
    r3d_shader_set_float(raster.geometryInst, uValRoughness, call->material.maps[MATERIAL_MAP_ROUGHNESS].value);
    r3d_shader_set_float(raster.geometryInst, uValMetalness, call->material.maps[MATERIAL_MAP_METALNESS].value);

    // Set color material maps
    r3d_shader_set_col3(raster.geometryInst, uColAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].color);
    r3d_shader_set_col3(raster.geometryInst, uColEmission, call->material.maps[MATERIAL_MAP_EMISSION].color);

    // Setup billboard mode
    r3d_shader_set_int(raster.geometryInst, uBillboardMode, call->instanced.billboardMode);

    if (call->instanced.billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.geometryInst, uMatInvView, R3D.state.transform.invView);
    }

    // Bind active texture maps
    r3d_shader_bind_sampler2D_opt(raster.geometryInst, uTexAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometryInst, uTexNormal, call->material.maps[MATERIAL_MAP_NORMAL].texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.geometryInst, uTexEmission, call->material.maps[MATERIAL_MAP_EMISSION].texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.geometryInst, uTexOcclusion, call->material.maps[MATERIAL_MAP_OCCLUSION].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometryInst, uTexRoughness, call->material.maps[MATERIAL_MAP_ROUGHNESS].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometryInst, uTexMetalness, call->material.maps[MATERIAL_MAP_METALNESS].texture.id, black);

    // Setup sprite related uniforms
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_shader_set_vec2(raster.geometryInst, uTexCoordOffset, call->geometry.sprite.uvOffset);
        r3d_shader_set_vec2(raster.geometryInst, uTexCoordScale, call->geometry.sprite.uvScale);
    }
    else {
        r3d_shader_set_vec2(raster.geometryInst, uTexCoordOffset, ((Vector2) { 0, 0 }));
        r3d_shader_set_vec2(raster.geometryInst, uTexCoordScale, ((Vector2) { 1, 1 }));
    }

    // Try binding vertex array objects (VAO) or use VBOs if not possible
    // WARNING: UploadMesh() enables all vertex attributes available in mesh and sets default attribute values
    // for shader expected vertex attributes that are not provided by the mesh (i.e. colors)
    // This could be a dangerous approach because different meshes with different shaders can enable/disable some attributes
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_MESH) {
        if (!rlEnableVertexArray(call->geometry.mesh.vaoId)) {
            // Bind mesh VBO - Positions
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);

            // Bind mesh VBO - TexCoords
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);

            // Bind mesh VBO - Normals
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL, 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL);

            // Bind mesh VBO - Tagents
            if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT] != 0) {
                rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
                rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, 4, RL_FLOAT, 0, 0, 0);
                rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
            }
            else {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                float value[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
                rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
            }

            // Bind mesh VBO - Colors
            if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0) {
                rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
                rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
            }
            else {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
            }

            if (call->geometry.mesh.indices != NULL) {
                rlEnableVertexBufferElement(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
            }
        }
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++) {
        // Calculate model-view-projection matrix (MVP)
        Matrix matVP = MatrixIdentity();
        if (eyeCount == 1) {
            matVP = MatrixMultiply(matView, matProjection);
        }
        else {
            // Setup current eye viewport (half screen width)
            rlViewport(eye * rlGetFramebufferWidth() / 2, 0, rlGetFramebufferWidth() / 2, rlGetFramebufferHeight());
            matVP = MatrixMultiply(MatrixMultiply(matView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        r3d_shader_set_mat4(raster.geometryInst, uMatVP, matVP);

        // Rasterisation du mesh en tenant compte du rendu instanci� si besoin
        r3d_draw_vertex_arrays_inst(call, 10, 14);
    }

    // Unbind all bound texture maps
    r3d_shader_unbind_sampler2D(raster.geometryInst, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.geometryInst, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.geometryInst, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.geometryInst, uTexOcclusion);
    r3d_shader_unbind_sampler2D(raster.geometryInst, uTexRoughness);
    r3d_shader_unbind_sampler2D(raster.geometryInst, uTexMetalness);

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Restore rlgl internal modelview and projection matrices
    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
}

void r3d_drawcall_raster_forward(const r3d_drawcall_t* call)
{
    Matrix matModel = MatrixIdentity();
    Matrix matView = rlGetMatrixModelview();
    Matrix matModelView = MatrixIdentity();
    Matrix matProjection = rlGetMatrixProjection();

    // Compute model and model/view matrices
    matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    matModelView = MatrixMultiply(matModel, matView);

    // Set additional matrix uniforms
    r3d_shader_set_mat4(raster.forward, uMatNormal, MatrixTranspose(MatrixInvert(matModel)));
    r3d_shader_set_mat4(raster.forward, uMatModel, matModel);

    // Set factor material maps
    r3d_shader_set_float(raster.forward, uValEmission, call->material.maps[MATERIAL_MAP_EMISSION].value);
    r3d_shader_set_float(raster.forward, uValOcclusion, call->material.maps[MATERIAL_MAP_OCCLUSION].value);
    r3d_shader_set_float(raster.forward, uValRoughness, call->material.maps[MATERIAL_MAP_ROUGHNESS].value);
    r3d_shader_set_float(raster.forward, uValMetalness, call->material.maps[MATERIAL_MAP_METALNESS].value);

    // Set misc material values
    r3d_shader_set_float(raster.forward, uAlphaScissorThreshold, call->forward.alphaScissorThreshold);

    // Set color material maps
    r3d_shader_set_col4(raster.forward, uColAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].color);
    r3d_shader_set_col3(raster.forward, uColEmission, call->material.maps[MATERIAL_MAP_EMISSION].color);

    // Bind active texture maps
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexNormal, call->material.maps[MATERIAL_MAP_NORMAL].texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexEmission, call->material.maps[MATERIAL_MAP_EMISSION].texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexOcclusion, call->material.maps[MATERIAL_MAP_OCCLUSION].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexRoughness, call->material.maps[MATERIAL_MAP_ROUGHNESS].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexMetalness, call->material.maps[MATERIAL_MAP_METALNESS].texture.id, black);

    // Setup sprite related uniforms
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_shader_set_vec2(raster.forward, uTexCoordOffset, call->geometry.sprite.uvOffset);
        r3d_shader_set_vec2(raster.forward, uTexCoordScale, call->geometry.sprite.uvScale);
    }
    else {
        r3d_shader_set_vec2(raster.forward, uTexCoordOffset, ((Vector2) { 0, 0 }));
        r3d_shader_set_vec2(raster.forward, uTexCoordScale, ((Vector2) { 1, 1 }));
    }

    // Try binding vertex array objects (VAO) or use VBOs if not possible
    // WARNING: UploadMesh() enables all vertex attributes available in mesh and sets default attribute values
    // for shader expected vertex attributes that are not provided by the mesh (i.e. colors)
    // This could be a dangerous approach because different meshes with different shaders can enable/disable some attributes
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_MESH) {
        if (!rlEnableVertexArray(call->geometry.mesh.vaoId)) {
            // Bind mesh VBO - Positions
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);

            // Bind mesh VBO - TexCoords
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);

            // Bind mesh VBO - Normals
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL, 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL);

            // Bind mesh VBO - Tagents
            if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT] != 0) {
                rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
                rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, 4, RL_FLOAT, 0, 0, 0);
                rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
            }
            else {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                float value[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
                rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
            }

            // Bind mesh VBO - Colors
            if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0) {
                rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
                rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
            }
            else {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
            }

            if (call->geometry.mesh.indices != NULL) {
                rlEnableVertexBufferElement(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
            }
        }
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++) {
        // Calculate model-view-projection matrix (MVP)
        Matrix matModelViewProjection = MatrixIdentity();
        if (eyeCount == 1) {
            matModelViewProjection = MatrixMultiply(matModelView, matProjection);
        }
        else {
            // Setup current eye viewport (half screen width)
            rlViewport(eye * rlGetFramebufferWidth() / 2, 0, rlGetFramebufferWidth() / 2, rlGetFramebufferHeight());
            matModelViewProjection = MatrixMultiply(MatrixMultiply(matModelView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        r3d_shader_set_mat4(raster.forward, uMatMVP, matModelViewProjection);

        // Rasterisation du mesh en tenant compte du rendu instanci� si besoin
        r3d_draw_vertex_arrays(call);
    }

    // Unbind all bound texture maps
    r3d_shader_unbind_sampler2D(raster.forward, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.forward, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.forward, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.forward, uTexOcclusion);
    r3d_shader_unbind_sampler2D(raster.forward, uTexRoughness);
    r3d_shader_unbind_sampler2D(raster.forward, uTexMetalness);

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Restore rlgl internal modelview and projection matrices
    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
}

void r3d_drawcall_raster_forward_inst(const r3d_drawcall_t* call)
{
    if (call->instanced.count == 0 || call->instanced.transforms == NULL) {
        return;
    }

    // Get current view / projection matrices
    Matrix matView = rlGetMatrixModelview();
    Matrix matProjection = rlGetMatrixProjection();

    // Compute model matrix
    Matrix matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());

    // Set additional matrix uniforms
    r3d_shader_set_mat4(raster.forwardInst, uMatModel, matModel);

    // Set factor material maps
    r3d_shader_set_float(raster.forwardInst, uValEmission, call->material.maps[MATERIAL_MAP_EMISSION].value);
    r3d_shader_set_float(raster.forwardInst, uValOcclusion, call->material.maps[MATERIAL_MAP_OCCLUSION].value);
    r3d_shader_set_float(raster.forwardInst, uValRoughness, call->material.maps[MATERIAL_MAP_ROUGHNESS].value);
    r3d_shader_set_float(raster.forwardInst, uValMetalness, call->material.maps[MATERIAL_MAP_METALNESS].value);

    // Set misc material values
    r3d_shader_set_float(raster.forwardInst, uAlphaScissorThreshold, call->forward.alphaScissorThreshold);

    // Set color material maps
    r3d_shader_set_col4(raster.forwardInst, uColAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].color);
    r3d_shader_set_col3(raster.forwardInst, uColEmission, call->material.maps[MATERIAL_MAP_EMISSION].color);

    // Setup billboard mode
    r3d_shader_set_int(raster.forwardInst, uBillboardMode, call->instanced.billboardMode);

    if (call->instanced.billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.forwardInst, uMatInvView, R3D.state.transform.invView);
    }

    // Bind active texture maps
    r3d_shader_bind_sampler2D_opt(raster.forwardInst, uTexAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.forwardInst, uTexNormal, call->material.maps[MATERIAL_MAP_NORMAL].texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.forwardInst, uTexEmission, call->material.maps[MATERIAL_MAP_EMISSION].texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.forwardInst, uTexOcclusion, call->material.maps[MATERIAL_MAP_OCCLUSION].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.forwardInst, uTexRoughness, call->material.maps[MATERIAL_MAP_ROUGHNESS].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.forwardInst, uTexMetalness, call->material.maps[MATERIAL_MAP_METALNESS].texture.id, black);

    // Setup sprite related uniforms
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_shader_set_vec2(raster.forwardInst, uTexCoordOffset, call->geometry.sprite.uvOffset);
        r3d_shader_set_vec2(raster.forwardInst, uTexCoordScale, call->geometry.sprite.uvScale);
    }
    else {
        r3d_shader_set_vec2(raster.forwardInst, uTexCoordOffset, ((Vector2) { 0, 0 }));
        r3d_shader_set_vec2(raster.forwardInst, uTexCoordScale, ((Vector2) { 1, 1 }));
    }

    // Try binding vertex array objects (VAO) or use VBOs if not possible
    // WARNING: UploadMesh() enables all vertex attributes available in mesh and sets default attribute values
    // for shader expected vertex attributes that are not provided by the mesh (i.e. colors)
    // This could be a dangerous approach because different meshes with different shaders can enable/disable some attributes
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_MESH) {
        if (!rlEnableVertexArray(call->geometry.mesh.vaoId)) {
            // Bind mesh VBO - Positions
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);

            // Bind mesh VBO - TexCoords
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);

            // Bind mesh VBO - Normals
            rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
            rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL, 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL);

            // Bind mesh VBO - Tagents
            if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT] != 0) {
                rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
                rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, 4, RL_FLOAT, 0, 0, 0);
                rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
            }
            else {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                float value[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
                rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
            }

            // Bind mesh VBO - Colors
            if (call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0) {
                rlEnableVertexBuffer(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
                rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
            }
            else {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
            }

            if (call->geometry.mesh.indices != NULL) {
                rlEnableVertexBufferElement(call->geometry.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
            }
        }
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++) {
        // Calculate model-view-projection matrix (MVP)
        Matrix matVP = MatrixIdentity();
        if (eyeCount == 1) {
            matVP = MatrixMultiply(matView, matProjection);
        }
        else {
            // Setup current eye viewport (half screen width)
            rlViewport(eye * rlGetFramebufferWidth() / 2, 0, rlGetFramebufferWidth() / 2, rlGetFramebufferHeight());
            matVP = MatrixMultiply(MatrixMultiply(matView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        r3d_shader_set_mat4(raster.forwardInst, uMatVP, matVP);

        // Raterization of the instantiated mesh
        r3d_draw_vertex_arrays_inst(call, 10, 14);
    }

    // Unbind all bound texture maps
    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexOcclusion);
    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexRoughness);
    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexMetalness);

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Restore rlgl internal modelview and projection matrices
    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
}


/* === Internal functions === */

void r3d_draw_vertex_arrays(const r3d_drawcall_t* call)
{
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_MESH) {
        if (call->geometry.mesh.indices == NULL) rlDrawVertexArray(0, call->geometry.mesh.vertexCount);
        else rlDrawVertexArrayElements(0, call->geometry.mesh.triangleCount * 3, 0);
    }
    else if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_primitive_draw_quad();
    }
}

void r3d_draw_vertex_arrays_inst(const r3d_drawcall_t* call, int locInstanceModel, int locInstanceColor)
{
    // WARNING: Always use the same attribute locations in shaders for instance matrices and colors.
    // If attribute locations differ between shaders (e.g., between the depth shader and the geometry shader),
    // it will break the rendering. This is because the vertex attributes are assigned based on specific 
    // attribute locations, and if those locations are not consistent across shaders, the attributes 
    // for instance transforms and colors will not be correctly bound. 
    // This results in undefined or incorrect behavior, such as missing or incorrectly transformed meshes.

    unsigned int vboTransforms = 0;
    unsigned int vboColors = 0;

    // Enable the attribute for the transformation matrix (decomposed into 4 vec4 vectors)
    if (locInstanceModel >= 0 && call->instanced.transforms) {
        size_t stride = (call->instanced.transStride == 0) ? sizeof(Matrix) : call->instanced.transStride;
        vboTransforms = rlLoadVertexBuffer(call->instanced.transforms, (int)(call->instanced.count * stride), true);
        rlEnableVertexBuffer(vboTransforms);
        for (int i = 0; i < 4; i++) {
            rlSetVertexAttribute(locInstanceModel + i, 4, RL_FLOAT, false, (int)stride, i * sizeof(Vector4));
            rlSetVertexAttributeDivisor(locInstanceModel + i, 1);
            rlEnableVertexAttribute(locInstanceModel + i);
        }
    }
    else if (locInstanceModel >= 0) {
        const float defaultTransform[4 * 4] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        for (int i = 0; i < 4; i++) {
            glVertexAttrib4fv(locInstanceModel + i, defaultTransform + i * 4);
            rlDisableVertexAttribute(locInstanceModel + i);
        }
    }

    // Handle per-instance colors if available
    if (locInstanceColor >= 0 && call->instanced.colors) {
        size_t stride = (call->instanced.colStride == 0) ? sizeof(Color) : call->instanced.colStride;
        vboColors = rlLoadVertexBuffer(call->instanced.colors, (int)(call->instanced.count * stride), true);
        rlEnableVertexBuffer(vboColors);
        rlSetVertexAttribute(locInstanceColor, 4, RL_UNSIGNED_BYTE, true, (int)call->instanced.colStride, 0);
        rlSetVertexAttributeDivisor(locInstanceColor, 1);
        rlEnableVertexAttribute(locInstanceColor);
    }
    else if (locInstanceColor >= 0) {
        const float defaultColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glVertexAttrib4fv(locInstanceColor, defaultColor);
        rlDisableVertexAttribute(locInstanceColor);
    }

    // Draw instances or a single object depending on the case
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_MESH) {
        if (call->geometry.mesh.indices != NULL) {
            rlDrawVertexArrayElementsInstanced(
                0, call->geometry.mesh.triangleCount * 3, 0, (int)call->instanced.count
            );
        }
        else {
            rlDrawVertexArrayInstanced(
                0, call->geometry.mesh.vertexCount, (int)call->instanced.count
            );
        }
    }
    else if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_primitive_draw_quad();
    }

    // Clean up resources
    if (vboTransforms > 0) {
        for (int i = 0; i < 4; i++) {
            rlDisableVertexAttribute(locInstanceModel + i);
            rlSetVertexAttributeDivisor(locInstanceModel + i, 0);
        }
        rlUnloadVertexBuffer(vboTransforms);
    }
    if (vboColors > 0) {
        rlDisableVertexAttribute(locInstanceColor);
        rlSetVertexAttributeDivisor(locInstanceColor, 0);
        rlUnloadVertexBuffer(vboColors);
    }
}

int r3d_drawcall_compare_front_to_back(const void* a, const void* b)
{
    const r3d_drawcall_t* drawCallA = a;
    const r3d_drawcall_t* drawCallB = b;

    Vector3 posA = { 0 };
    Vector3 posB = { 0 };

    posA.x = drawCallA->transform.m12;
    posA.y = drawCallA->transform.m13;
    posA.z = drawCallA->transform.m14;

    posB.x = drawCallB->transform.m12;
    posB.y = drawCallB->transform.m13;
    posB.z = drawCallB->transform.m14;

    float distA = Vector3DistanceSqr(R3D.state.transform.position, posA);
    float distB = Vector3DistanceSqr(R3D.state.transform.position, posB);

    return (distA > distB) - (distA < distB);
}

int r3d_drawcall_compare_back_to_front(const void* a, const void* b)
{
    const r3d_drawcall_t* drawCallA = a;
    const r3d_drawcall_t* drawCallB = b;

    Vector3 posA = { 0 };
    Vector3 posB = { 0 };

    posA.x = drawCallA->transform.m12;
    posA.y = drawCallA->transform.m13;
    posA.z = drawCallA->transform.m14;

    posB.x = drawCallB->transform.m12;
    posB.y = drawCallB->transform.m13;
    posB.z = drawCallB->transform.m14;

    float distA = Vector3DistanceSqr(R3D.state.transform.position, posA);
    float distB = Vector3DistanceSqr(R3D.state.transform.position, posB);

    return (distA < distB) - (distA > distB);
}
