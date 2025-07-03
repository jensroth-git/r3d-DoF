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

#include "./r3d_primitives.h"
#include "../r3d_state.h"
#include "r3d.h"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>

#include <stdlib.h>
#include <assert.h>

/* === Internal functions === */

// Functions applying OpenGL states defined by the material but unrelated to shaders
static void r3d_drawcall_apply_cull_mode(R3D_CullMode mode);
static void r3d_drawcall_apply_blend_mode(R3D_BlendMode mode);
static void r3d_drawcall_apply_shadow_cast_mode(R3D_ShadowCastMode mode);

// This function supports instanced rendering when necessary
static void r3d_drawcall(const r3d_drawcall_t* call);
static void r3d_drawcall_instanced(const r3d_drawcall_t* call, int locInstanceModel, int locInstanceColor);

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

void r3d_drawcall_raster_depth(const r3d_drawcall_t* call, bool shadow)
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
    r3d_shader_set_float(raster.depth, uAlpha, ((float)call->material->albedo.color.a / 255));
    r3d_shader_bind_sampler2D_opt(raster.depth, uTexAlbedo, call->material->albedo.texture.id, white);

    // Applying material parameters that are independent of shaders
    if (shadow) {
        r3d_drawcall_apply_shadow_cast_mode(call->material->shadowCastMode);
    }
    else {
        r3d_drawcall_apply_cull_mode(call->material->cullMode);
    }

    // Rendering the object corresponding to the draw call
    r3d_drawcall(call);

    // Unbind vertex buffers
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Unbind samplers
    r3d_shader_unbind_sampler2D(raster.depth, uTexAlbedo);
}

void r3d_drawcall_raster_depth_inst(const r3d_drawcall_t* call, bool shadow)
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
    r3d_shader_set_int(raster.depthInst, uBillboardMode, call->material->billboardMode);
    if (call->material->billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.depthInst, uMatInvView, R3D.state.transform.invView);
    }

    // Send alpha and bind albedo
    r3d_shader_set_float(raster.depthInst, uAlpha, ((float)call->material->albedo.color.a / 255));
    r3d_shader_bind_sampler2D_opt(raster.depthInst, uTexAlbedo, call->material->albedo.texture.id, white);

    // Applying material parameters that are independent of shaders
    if (shadow) {
        r3d_drawcall_apply_shadow_cast_mode(call->material->shadowCastMode);
    }
    else {
        r3d_drawcall_apply_cull_mode(call->material->cullMode);
    }

    // Rendering the objects corresponding to the draw call
    r3d_drawcall_instanced(call, 10, -1);

    // Unbind vertex buffers
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Unbind samplers
    r3d_shader_unbind_sampler2D(raster.depthInst, uTexAlbedo);
}

void r3d_drawcall_raster_depth_cube(const r3d_drawcall_t* call, bool shadow)
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
    r3d_shader_set_float(raster.depthCube, uAlpha, ((float)call->material->albedo.color.a / 255));
    r3d_shader_bind_sampler2D_opt(raster.depthCube, uTexAlbedo, call->material->albedo.texture.id, white);

    // Applying material parameters that are independent of shaders
    if (shadow) {
        r3d_drawcall_apply_shadow_cast_mode(call->material->shadowCastMode);
    }
    else {
        r3d_drawcall_apply_cull_mode(call->material->cullMode);
    }

    // Rendering the object corresponding to the draw call
    r3d_drawcall(call);

    // Unbind vertex buffers
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Unbind samplers
    r3d_shader_unbind_sampler2D(raster.depthCube, uTexAlbedo);
}

void r3d_drawcall_raster_depth_cube_inst(const r3d_drawcall_t* call, bool shadow)
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
    r3d_shader_set_int(raster.depthCubeInst, uBillboardMode, call->material->billboardMode);
    if (call->material->billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.depthCubeInst, uMatInvView, R3D.state.transform.invView);
    }

    // Send alpha and bind albedo
    r3d_shader_set_float(raster.depthCubeInst, uAlpha, ((float)call->material->albedo.color.a / 255));
    r3d_shader_bind_sampler2D_opt(raster.depthCubeInst, uTexAlbedo, call->material->albedo.texture.id, white);

    // Applying material parameters that are independent of shaders
    if (shadow) {
        r3d_drawcall_apply_shadow_cast_mode(call->material->shadowCastMode);
    }
    else {
        r3d_drawcall_apply_cull_mode(call->material->cullMode);
    }

    // Rendering the objects corresponding to the draw call
    r3d_drawcall_instanced(call, 10, -1);

    // Unbind vertex buffers
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Unbind samplers
    r3d_shader_unbind_sampler2D(raster.depthCubeInst, uTexAlbedo);
}

void r3d_drawcall_raster_geometry(const r3d_drawcall_t* call)
{
    // Compute model/view/projection matrices
    Matrix matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    Matrix matModelView = MatrixMultiply(matModel, rlGetMatrixModelview());
    Matrix matMVP = MatrixMultiply(matModelView, rlGetMatrixProjection());

    // Set additional matrix uniforms
    r3d_shader_set_mat4(raster.geometry, uMatNormal, MatrixTranspose(MatrixInvert(matModel)));
    r3d_shader_set_mat4(raster.geometry, uMatModel, matModel);
    r3d_shader_set_mat4(raster.geometry, uMatMVP, matMVP);

    // Set factor material maps
    r3d_shader_set_float(raster.geometry, uValEmission, call->material->emission.multiplier);
    r3d_shader_set_float(raster.geometry, uValOcclusion, call->material->orm.occlusion);
    r3d_shader_set_float(raster.geometry, uValRoughness, call->material->orm.roughness);
    r3d_shader_set_float(raster.geometry, uValMetalness, call->material->orm.metalness);

    // Set color material maps
    r3d_shader_set_col3(raster.geometry, uColAlbedo, call->material->albedo.color);
    r3d_shader_set_col3(raster.geometry, uColEmission, call->material->emission.color);

    // Bind active texture maps
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexAlbedo, call->material->albedo.texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexNormal, call->material->normal.texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexEmission, call->material->emission.texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexORM, call->material->orm.texture.id, white);

    // Setup sprite related uniforms
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_shader_set_vec2(raster.geometry, uTexCoordOffset, call->geometry.sprite.uvOffset);
        r3d_shader_set_vec2(raster.geometry, uTexCoordScale, call->geometry.sprite.uvScale);
    }
    else {
        r3d_shader_set_vec2(raster.geometry, uTexCoordOffset, ((Vector2) { 0, 0 }));
        r3d_shader_set_vec2(raster.geometry, uTexCoordScale, ((Vector2) { 1, 1 }));
    }

    // Applying material parameters that are independent of shaders
    r3d_drawcall_apply_cull_mode(call->material->cullMode);

    // Rendering the object corresponding to the draw call
    r3d_drawcall(call);

    // Unbind all bound texture maps
    r3d_shader_unbind_sampler2D(raster.geometry, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexORM);
}

void r3d_drawcall_raster_geometry_inst(const r3d_drawcall_t* call)
{
    if (call->instanced.count == 0 || call->instanced.transforms == NULL) {
        return;
    }

    // Compute model/view/projection matrix
    Matrix matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    Matrix matVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());

    // Set additional matrix uniforms
    r3d_shader_set_mat4(raster.geometryInst, uMatModel, matModel);
    r3d_shader_set_mat4(raster.geometryInst, uMatVP, matVP);

    // Set factor material maps
    r3d_shader_set_float(raster.geometryInst, uValEmission, call->material->emission.multiplier);
    r3d_shader_set_float(raster.geometryInst, uValOcclusion, call->material->orm.occlusion);
    r3d_shader_set_float(raster.geometryInst, uValRoughness, call->material->orm.roughness);
    r3d_shader_set_float(raster.geometryInst, uValMetalness, call->material->orm.metalness);

    // Set color material maps
    r3d_shader_set_col3(raster.geometryInst, uColAlbedo, call->material->albedo.color);
    r3d_shader_set_col3(raster.geometryInst, uColEmission, call->material->emission.color);

    // Setup billboard mode
    r3d_shader_set_int(raster.geometryInst, uBillboardMode, call->material->billboardMode);
    if (call->material->billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.geometryInst, uMatInvView, R3D.state.transform.invView);
    }

    // Bind active texture maps
    r3d_shader_bind_sampler2D_opt(raster.geometryInst, uTexAlbedo, call->material->albedo.texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometryInst, uTexNormal, call->material->normal.texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.geometryInst, uTexEmission, call->material->emission.texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.geometryInst, uTexORM, call->material->orm.texture.id, white);

    // Setup sprite related uniforms
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_shader_set_vec2(raster.geometryInst, uTexCoordOffset, call->geometry.sprite.uvOffset);
        r3d_shader_set_vec2(raster.geometryInst, uTexCoordScale, call->geometry.sprite.uvScale);
    }
    else {
        r3d_shader_set_vec2(raster.geometryInst, uTexCoordOffset, ((Vector2) { 0, 0 }));
        r3d_shader_set_vec2(raster.geometryInst, uTexCoordScale, ((Vector2) { 1, 1 }));
    }

    // Applying material parameters that are independent of shaders
    r3d_drawcall_apply_cull_mode(call->material->cullMode);

    // Rendering the objects corresponding to the draw call
    r3d_drawcall_instanced(call, 10, 14);

    // Unbind all bound texture maps
    r3d_shader_unbind_sampler2D(raster.geometryInst, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.geometryInst, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.geometryInst, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.geometryInst, uTexORM);
}

void r3d_drawcall_raster_forward(const r3d_drawcall_t* call)
{
    // Compute model/view/projection matrices
    Matrix matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    Matrix matModelView = MatrixMultiply(matModel, rlGetMatrixModelview());
    Matrix matMVP = MatrixMultiply(matModelView, rlGetMatrixProjection());

    // Set additional matrix uniforms
    r3d_shader_set_mat4(raster.forward, uMatNormal, MatrixTranspose(MatrixInvert(matModel)));
    r3d_shader_set_mat4(raster.forward, uMatModel, matModel);
    r3d_shader_set_mat4(raster.forward, uMatMVP, matMVP);

    // Set factor material maps
    r3d_shader_set_float(raster.forward, uValEmission, call->material->emission.multiplier);
    r3d_shader_set_float(raster.forward, uValOcclusion, call->material->orm.occlusion);
    r3d_shader_set_float(raster.forward, uValRoughness, call->material->orm.roughness);
    r3d_shader_set_float(raster.forward, uValMetalness, call->material->orm.metalness);

    // Set misc material values
    r3d_shader_set_float(raster.forward, uAlphaScissorThreshold, call->material->alphaScissorThreshold);

    // Set color material maps
    r3d_shader_set_col4(raster.forward, uColAlbedo, call->material->albedo.color);
    r3d_shader_set_col3(raster.forward, uColEmission, call->material->emission.color);

    // Bind active texture maps
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexAlbedo, call->material->albedo.texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexNormal, call->material->normal.texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexEmission, call->material->emission.texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexORM, call->material->orm.texture.id, white);

    // Setup sprite related uniforms
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_shader_set_vec2(raster.forward, uTexCoordOffset, call->geometry.sprite.uvOffset);
        r3d_shader_set_vec2(raster.forward, uTexCoordScale, call->geometry.sprite.uvScale);
    }
    else {
        r3d_shader_set_vec2(raster.forward, uTexCoordOffset, ((Vector2) { 0, 0 }));
        r3d_shader_set_vec2(raster.forward, uTexCoordScale, ((Vector2) { 1, 1 }));
    }

    // Applying material parameters that are independent of shaders
    r3d_drawcall_apply_cull_mode(call->material->cullMode);
    r3d_drawcall_apply_blend_mode(call->material->blendMode);

    // Rendering the object corresponding to the draw call
    r3d_drawcall(call);

    // Unbind all bound texture maps
    r3d_shader_unbind_sampler2D(raster.forward, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.forward, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.forward, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.forward, uTexORM);
}

void r3d_drawcall_raster_forward_inst(const r3d_drawcall_t* call)
{
    if (call->instanced.count == 0 || call->instanced.transforms == NULL) {
        return;
    }

    // Compute model/view/projection matrix
    Matrix matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    Matrix matVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());

    // Set additional matrix uniforms
    r3d_shader_set_mat4(raster.forwardInst, uMatModel, matModel);
    r3d_shader_set_mat4(raster.forwardInst, uMatVP, matVP);

    // Set factor material maps
    r3d_shader_set_float(raster.forwardInst, uValEmission, call->material->emission.multiplier);
    r3d_shader_set_float(raster.forwardInst, uValOcclusion, call->material->orm.occlusion);
    r3d_shader_set_float(raster.forwardInst, uValRoughness, call->material->orm.roughness);
    r3d_shader_set_float(raster.forwardInst, uValMetalness, call->material->orm.metalness);

    // Set misc material values
    r3d_shader_set_float(raster.forwardInst, uAlphaScissorThreshold, call->material->alphaScissorThreshold);

    // Set color material maps
    r3d_shader_set_col4(raster.forwardInst, uColAlbedo, call->material->albedo.color);
    r3d_shader_set_col3(raster.forwardInst, uColEmission, call->material->emission.color);

    // Setup billboard mode
    r3d_shader_set_int(raster.forwardInst, uBillboardMode, call->material->billboardMode);
    if (call->material->billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.forwardInst, uMatInvView, R3D.state.transform.invView);
    }

    // Bind active texture maps
    r3d_shader_bind_sampler2D_opt(raster.forwardInst, uTexAlbedo, call->material->albedo.texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.forwardInst, uTexNormal, call->material->normal.texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.forwardInst, uTexEmission, call->material->emission.texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.forwardInst, uTexORM, call->material->orm.texture.id, white);

    // Setup sprite related uniforms
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_shader_set_vec2(raster.forwardInst, uTexCoordOffset, call->geometry.sprite.uvOffset);
        r3d_shader_set_vec2(raster.forwardInst, uTexCoordScale, call->geometry.sprite.uvScale);
    }
    else {
        r3d_shader_set_vec2(raster.forwardInst, uTexCoordOffset, ((Vector2) { 0, 0 }));
        r3d_shader_set_vec2(raster.forwardInst, uTexCoordScale, ((Vector2) { 1, 1 }));
    }

    // Applying material parameters that are independent of shaders
    r3d_drawcall_apply_cull_mode(call->material->cullMode);
    r3d_drawcall_apply_blend_mode(call->material->blendMode);

    // Rendering the objects corresponding to the draw call
    r3d_drawcall_instanced(call, 10, 14);

    // Unbind all bound texture maps
    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexORM);
}

/* === Internal functions === */

void r3d_drawcall_apply_cull_mode(R3D_CullMode mode)
{
    switch (mode)
    {
    case R3D_CULL_NONE:
        glDisable(GL_CULL_FACE);
        break;
    case R3D_CULL_BACK:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case R3D_CULL_FRONT:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    }
}

void r3d_drawcall_apply_blend_mode(R3D_BlendMode mode)
{
    switch (mode)
    {
    case R3D_BLEND_OPAQUE:
        glDisable(GL_BLEND);
        break;
    case R3D_BLEND_ALPHA:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case R3D_BLEND_ADDITIVE:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    case R3D_BLEND_MULTIPLY:
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;
    default:
        break;
    }
}

static void r3d_drawcall_apply_shadow_cast_mode(R3D_ShadowCastMode mode)
{
    switch (mode)
    {
    case R3D_SHADOW_CAST_ALL_FACES:
        glDisable(GL_CULL_FACE);
        break;
    case R3D_SHADOW_CAST_FRONT_FACES:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case R3D_SHADOW_CAST_BACK_FACES:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;

    case R3D_SHADOW_CAST_DISABLED:
    default:
        assert("This shouldn't happen" && false);
        break;
    }
}

static void r3d_drawcall_bind_geometry_mesh(const R3D_Mesh* mesh)
{
    if (!rlEnableVertexArray(mesh->vao)) {
        return;
    }

    // Enable the vertex buffer (fallback if vao is not available)
    rlEnableVertexBuffer(mesh->vbo);

    // Bind positions
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, sizeof(R3D_Vertex), offsetof(R3D_Vertex, position));
    rlEnableVertexAttribute(0);

    // Bind texcoords
    rlSetVertexAttribute(1, 2, RL_FLOAT, false, sizeof(R3D_Vertex), offsetof(R3D_Vertex, texcoord));
    rlEnableVertexAttribute(1);

    // Bind normals
    rlSetVertexAttribute(2, 3, RL_FLOAT, false, sizeof(R3D_Vertex), offsetof(R3D_Vertex, normal));
    rlEnableVertexAttribute(2);

    // Bind colors
    rlSetVertexAttribute(3, 4, RL_FLOAT, false, sizeof(R3D_Vertex), offsetof(R3D_Vertex, color));
    rlEnableVertexAttribute(3);

    // Bind tangents
    rlSetVertexAttribute(4, 4, RL_FLOAT, false, sizeof(R3D_Vertex), offsetof(R3D_Vertex, tangent));
    rlEnableVertexAttribute(4);

    // Bind index buffer
    if (mesh->ebo > 0) {
        rlEnableVertexBufferElement(mesh->ebo);
    }
}

static void r3d_drawcall_unbind_geometry_mesh(void)
{
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();
}

void r3d_drawcall(const r3d_drawcall_t* call)
{
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_MESH) {
        r3d_drawcall_bind_geometry_mesh(call->geometry.mesh);
        if (call->geometry.mesh->indices == NULL) glDrawArrays(GL_TRIANGLES, 0, call->geometry.mesh->vertexCount);
        else glDrawElements(GL_TRIANGLES, call->geometry.mesh->indexCount, GL_UNSIGNED_INT, NULL);
        r3d_drawcall_unbind_geometry_mesh();
    }

    // Sprite mode only requires to render a generic quad
    else if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_primitive_bind_and_draw_quad();
    }
}

void r3d_drawcall_instanced(const r3d_drawcall_t* call, int locInstanceModel, int locInstanceColor)
{
    // Bind the geometry
    switch (call->geometryType) {
    case R3D_DRAWCALL_GEOMETRY_MESH:
        r3d_drawcall_bind_geometry_mesh(call->geometry.mesh);
        break;
    case R3D_DRAWCALL_GEOMETRY_SPRITE:
        r3d_primitive_bind(&R3D.primitive.quad);
        break;
    }

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

    // Draw the geometry
    switch (call->geometryType) {
    case R3D_DRAWCALL_GEOMETRY_MESH:
        if (call->geometry.mesh->indices == NULL) {
            glDrawArraysInstanced(GL_TRIANGLES, 0, call->geometry.mesh->vertexCount, call->instanced.count);
        }
        else {
            glDrawElementsInstanced(GL_TRIANGLES, call->geometry.mesh->indexCount, GL_UNSIGNED_INT, NULL, call->instanced.count);
        }
        break;
    case R3D_DRAWCALL_GEOMETRY_SPRITE:
        r3d_primitive_draw_instanced(&R3D.primitive.quad, call->instanced.count);
        break;
    }

    // Clean up instanced data
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

    // Unbind the geometry
    switch (call->geometryType) {
    case R3D_DRAWCALL_GEOMETRY_MESH:
        r3d_drawcall_unbind_geometry_mesh();
        break;
    case R3D_DRAWCALL_GEOMETRY_SPRITE:
        r3d_primitive_unbind();
        break;
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
