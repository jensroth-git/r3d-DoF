#include "./r3d_light.h"

#include <raymath.h>
#include <stddef.h>
#include <rlgl.h>
#include <glad.h>

/* === Internal functions === */

static r3d_shadow_map_t r3d_light_create_shadow_map_dir(int resolution)
{
    r3d_shadow_map_t shadowMap = { 0 };

    glGenFramebuffers(1, &shadowMap.id);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.id);

    glGenTextures(1, &shadowMap.depth);
    glBindTexture(GL_TEXTURE_2D, shadowMap.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap.depth, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "Framebuffer creation error for the directional shadow map");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    shadowMap.texelSize = 1.0f / resolution;
    shadowMap.resolution = resolution;

    return shadowMap;
}

static r3d_shadow_map_t r3d_light_create_shadow_map_spot(int resolution)
{
    r3d_shadow_map_t shadowMap = { 0 };

    shadowMap.resolution = resolution;

    glGenFramebuffers(1, &shadowMap.id);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.id);

    glGenTextures(1, &shadowMap.depth);
    glBindTexture(GL_TEXTURE_2D, shadowMap.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap.depth, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "Framebuffer creation error for the Shadow Map Spot");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    shadowMap.texelSize = 1.0f / resolution;
    shadowMap.resolution = resolution;

    return shadowMap;
}

static r3d_shadow_map_t r3d_light_create_shadow_map_omni(int resolution)
{
    r3d_shadow_map_t shadowMap = { 0 };

    glGenFramebuffers(1, &shadowMap.id);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.id);

    glGenTextures(1, &shadowMap.depth);
    glBindTexture(GL_TEXTURE_CUBE_MAP, shadowMap.depth);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT16,
            resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL
        );
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, shadowMap.depth, 0);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "Framebuffer creation error for the omni shadow map");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    shadowMap.texelSize = 1.0f / resolution;
    shadowMap.resolution = resolution;

    return shadowMap;
}

/* === Public functions === */

void r3d_light_init(r3d_light_t* light)
{
    light->shadow = (r3d_shadow_t){ 0 };
    light->color = (Vector3){ 1, 1, 1 };
    light->position = (Vector3){ 0 };
    light->direction = (Vector3){ 0, 0, -1 };
    light->specular = 0.5f;
    light->energy = 1.0f;
    light->range = 100.0f;
    light->size = 0.001f;
    light->near = 0.05f;
    light->attenuation = 1.0f;
    light->innerCutOff = -1.0f;
    light->outerCutOff = -1.0f;
    light->type = R3D_LIGHT_DIR;
    light->enabled = false;
}

void r3d_light_create_shadow_map(r3d_light_t* light, int resolution)
{
    switch (light->type) {
    case R3D_LIGHT_DIR:
        light->shadow.map = r3d_light_create_shadow_map_dir(resolution);
        break;
    case R3D_LIGHT_SPOT:
        light->shadow.map = r3d_light_create_shadow_map_spot(resolution);
        break;
    case R3D_LIGHT_OMNI:
        light->shadow.map = r3d_light_create_shadow_map_omni(resolution);
        break;
    }
}

void r3d_light_destroy_shadow_map(r3d_light_t* light)
{
    if (light->shadow.map.id != 0) {
        rlUnloadTexture(light->shadow.map.depth);
        rlUnloadFramebuffer(light->shadow.map.id);
    }
}

void r3d_light_process_shadow_update(r3d_light_t* light)
{
    switch (light->shadow.updateConf.mode) {
    case R3D_SHADOW_UPDATE_MANUAL:
        break;
    case R3D_SHADOW_UPDATE_INTERVAL:
        if (!light->shadow.updateConf.shoudlUpdate) {
            light->shadow.updateConf.timerSec += GetFrameTime();
            if (light->shadow.updateConf.timerSec >= light->shadow.updateConf.frequencySec) {
                light->shadow.updateConf.shoudlUpdate = true;
                light->shadow.updateConf.timerSec = 0.0f;
            }
        }
        break;
    case R3D_SHADOW_UPDATE_CONTINUOUS:
        light->shadow.updateConf.shoudlUpdate = true;
        break;
    }
}

void r3d_light_indicate_shadow_update(r3d_light_t* light)
{
    switch (light->shadow.updateConf.mode) {
    case R3D_SHADOW_UPDATE_MANUAL:
        light->shadow.updateConf.shoudlUpdate = false;
        break;
    case R3D_SHADOW_UPDATE_INTERVAL:
        light->shadow.updateConf.shoudlUpdate = false;
        light->shadow.updateConf.timerSec = 0.0f;
        break;
    case R3D_SHADOW_UPDATE_CONTINUOUS:
        break;
    }
}

void r3d_light_get_matrix_vp_dir(r3d_light_t* light, BoundingBox sceneBounds, Matrix* view, Matrix* proj)
{
    // Calculating the center of the scene
    Vector3 sceneCenter = {
        (sceneBounds.min.x + sceneBounds.max.x) * 0.5f,
        (sceneBounds.min.y + sceneBounds.max.y) * 0.5f,
        (sceneBounds.min.z + sceneBounds.max.z) * 0.5f
    };

    // Calculating the half-extents of the scene with a safety margin
    const float SCENE_MARGIN = 1.1f; // 10% margin
    Vector3 sceneExtents = {
        (sceneBounds.max.x - sceneBounds.min.x) * 0.5f * SCENE_MARGIN,
        (sceneBounds.max.y - sceneBounds.min.y) * 0.5f * SCENE_MARGIN,
        (sceneBounds.max.z - sceneBounds.min.z) * 0.5f * SCENE_MARGIN
    };

    // Normalizing the light direction
    Vector3 lightDir = Vector3Normalize(light->direction);

    // Calculating the light position (placed at a distance from the center of the scene)
    float maxSceneExtent = fmaxf(sceneExtents.x, fmaxf(sceneExtents.y, sceneExtents.z));
    float lightDistance = maxSceneExtent * 2.0f;
    Vector3 lightPos = Vector3Add(sceneCenter, Vector3Scale(Vector3Negate(lightDir), lightDistance));

    // Keeps the calculated position of the light
    light->position = lightPos;

    // Calculating the view matrix with a stable up vector
    Vector3 upVector;
    if (fabsf(lightDir.y) > 0.99f) {
        // If the direction is nearly vertical, use Z as the "up" vector
        upVector = (Vector3){ 0.0f, 0.0f, 1.0f };
    }
    else {
        upVector = (Vector3){ 0.0f, 1.0f, 0.0f };
    }
    *view = MatrixLookAt(lightPos, sceneCenter, upVector);

    // Calculating the bounding volume of the scene in light space
    Matrix viewMatrix = *view;
    Vector3 corners[8] = {
        {sceneBounds.min.x, sceneBounds.min.y, sceneBounds.min.z},
        {sceneBounds.max.x, sceneBounds.min.y, sceneBounds.min.z},
        {sceneBounds.min.x, sceneBounds.max.y, sceneBounds.min.z},
        {sceneBounds.max.x, sceneBounds.max.y, sceneBounds.min.z},
        {sceneBounds.min.x, sceneBounds.min.y, sceneBounds.max.z},
        {sceneBounds.max.x, sceneBounds.min.y, sceneBounds.max.z},
        {sceneBounds.min.x, sceneBounds.max.y, sceneBounds.max.z},
        {sceneBounds.max.x, sceneBounds.max.y, sceneBounds.max.z}
    };

    float minX = INFINITY, maxX = -INFINITY;
    float minY = INFINITY, maxY = -INFINITY;
    float minZ = INFINITY, maxZ = -INFINITY;

    for (int i = 0; i < 8; i++) {
        Vector3 transformed = Vector3Transform(corners[i], viewMatrix);
        minX = fminf(minX, transformed.x);
        maxX = fmaxf(maxX, transformed.x);
        minY = fminf(minY, transformed.y);
        maxY = fmaxf(maxY, transformed.y);
        minZ = fminf(minZ, transformed.z);
        maxZ = fmaxf(maxZ, transformed.z);
    }

    // Creating the orthographic projection matrix
    // WARNING: In camera space, objects in front of the camera have negative Z values.
    // Here, maxZ corresponds to the closest plane (less negative) and minZ to the farthest plane.
    // To obtain positive distances for the projection, we reverse the signs:
    // near = -maxZ and far = -minZ (which guarantees near < far).

    light->near = -maxZ;    // Save near plane (can be used in shaders)
    light->far = -minZ;     // Save far plane (can be used in shaders)
    *proj = MatrixOrtho(minX, maxX, minY, maxY, light->near, light->far);
}

Matrix r3d_light_get_matrix_view_spot(r3d_light_t* light)
{
    return MatrixLookAt(light->position,
        Vector3Add(light->position, light->direction), (Vector3) { 0, 1, 0 }
    );
}

Matrix r3d_light_get_matrix_proj_spot(r3d_light_t* light)
{
    light->near = 0.05f;        // Save near plane (can be used in shaders)
    light->far = light->range;  // Save far plane (can be used in shaders)
    return MatrixPerspective(90 * DEG2RAD, 1.0, light->near, light->far);
}

Matrix r3d_light_get_matrix_view_omni(r3d_light_t* light, int face)
{
    static const Vector3 dirs[6] = {
        {  1.0,  0.0,  0.0 }, // +X
        { -1.0,  0.0,  0.0 }, // -X
        {  0.0,  1.0,  0.0 }, // +Y
        {  0.0, -1.0,  0.0 }, // -Y
        {  0.0,  0.0,  1.0 }, // +Z
        {  0.0,  0.0, -1.0 }  // -Z
    };

    static const Vector3 ups[6] = {
        {  0.0, -1.0,  0.0 }, // +X
        {  0.0, -1.0,  0.0 }, // -X
        {  0.0,  0.0,  1.0 }, // +Y
        {  0.0,  0.0, -1.0 }, // -Y
        {  0.0, -1.0,  0.0 }, // +Z
        {  0.0, -1.0,  0.0 }  // -Z
    };

    return MatrixLookAt(
        light->position, Vector3Add(light->position, dirs[face]), ups[face]
    );
}

Matrix r3d_light_get_matrix_proj_omni(r3d_light_t* light)
{
    light->near = 0.05f;        // Save near plane (can be used in shaders)
    light->far = light->range;  // Save far plane (can be used in shaders)
    return MatrixPerspective(90 * DEG2RAD, 1.0, light->near, light->far);
}
