#include "./common.h"
#include "r3d.h"
#include "raylib.h"

/* === Resources === */

static R3D_Mesh sphere = { 0 };
static R3D_Material material = { 0 };
static R3D_Skybox skybox = { 0 };
static Camera3D camera = { 0 };

static R3D_InterpolationCurve curve = { 0 };
static R3D_ParticleSystem particles = { 0 };

/* === Example === */

const char* Init(void)
{
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    R3D_SetBloomMode(R3D_BLOOM_ADDITIVE);
    R3D_SetBackgroundColor((Color) { 4, 4, 4 });
    R3D_SetAmbientColor(BLACK);

    sphere = R3D_GenMeshSphere(0.1f, 16, 32, true);

    material = R3D_GetDefaultMaterial();
    material.emission.color = (Color) { 255, 0, 0, 255 };
    material.emission.multiplier = 1.0f;

    curve = R3D_LoadInterpolationCurve(3);
    R3D_AddKeyframe(&curve, 0.0f, 0.0f);
    R3D_AddKeyframe(&curve, 0.5f, 1.0f);
    R3D_AddKeyframe(&curve, 1.0f, 0.0f);

    particles = R3D_LoadParticleSystem(2048);
    particles.initialVelocity = (Vector3){ 0, 10.0f, 0 };
    particles.scaleOverLifetime = &curve;
    particles.spreadAngle = 45.0f;
    particles.emissionRate = 2048;
    particles.lifetime = 2.0f;

    R3D_CalculateParticleSystemBoundingBox(&particles);

    camera = (Camera3D) {
        .position = (Vector3) { -7, 7, -7 },
        .target = (Vector3) { 0, 1, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    return "[r3d] - particles example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_ORBITAL);
    R3D_UpdateParticleSystem(&particles, GetFrameTime());
}

void Draw(void)
{
    R3D_Begin(camera);
        R3D_DrawParticleSystem(&particles, &sphere, &material);
    R3D_End();

    BeginMode3D(camera);
        DrawBoundingBox(particles.aabb, GREEN);
    EndMode3D();

    DrawFPS(10, 10);
}

void Close(void)
{
    R3D_UnloadInterpolationCurve(curve);
    R3D_UnloadParticleSystem(&particles);

    R3D_UnloadMesh(&sphere);
    R3D_UnloadMaterial(&material);

    R3D_Close();
}
