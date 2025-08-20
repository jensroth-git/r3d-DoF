#include "./common.h"
#include "r3d.h"
#include "rcamera.h"
#include <stdlib.h>
#include <stdio.h>

/* === Resources === */

#define X_INSTANCES 10
#define Y_INSTANCES 10
#define INSTANCE_COUNT (X_INSTANCES * Y_INSTANCES)

static R3D_Mesh meshSphere = { 0 };
static R3D_Material matDefault = { 0 };
static Camera3D camDefault = { 0 };
static Matrix instances[INSTANCE_COUNT];
static Color instanceColors[INSTANCE_COUNT];

/* === Example === */

const char* Init(void)
{
    R3D_Init(GetScreenWidth(), GetScreenHeight(), R3D_FLAG_FXAA);
    SetTargetFPS(0);

    /* --- Enable and configure DOF --- */

    R3D_SetDofMode(R3D_DOF_ENABLED);
    R3D_SetDofFocusPoint(2.5f);
    R3D_SetDofFocusScale(10.0f);
    R3D_SetDofMaxBlurSize(30.0f);
    R3D_SetDofDebugMode(0);

    //R3D_SetBloomMode(R3D_BLOOM_SCREEN);
    
    /* --- Setup scene lighting --- */

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3) { 0, -1, 0 });
    R3D_SetLightActive(light, true);

    /* --- Load sphere mesh and material --- */

    meshSphere = R3D_GenMeshSphere(0.2f, 64, 64, true);
    matDefault = R3D_GetDefaultMaterial();

    /* --- Generate instances --- */

    const float spacing = 0.5f;
    const float offsetX = (X_INSTANCES * spacing) / 2;
    const float offsetZ = (Y_INSTANCES * spacing) / 2;
    int idx = 0;
    for (int x = 0; x < X_INSTANCES; x++) {
        for (int y = 0; y < Y_INSTANCES; y++) {
            instances[idx] = MatrixTranslate(x * spacing - offsetX, 0, y * spacing - offsetZ);
            instanceColors[idx] = (Color) {
                (unsigned char)(rand() % 255),
                (unsigned char)(rand() % 255),
                (unsigned char)(rand() % 255),
                255
            };
            idx++;
        }
    }

    /* --- Configure the camera and ready to go! */

    camDefault = (Camera3D) {
        .position = (Vector3) { 0, 2, 2 },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    return "[r3d] - DoF example";
}

void Update(float delta)
{
    /* --- Rotate camera --- */

    // Matrix rotation = MatrixRotate(GetCameraUp(&camDefault), 0.1f * delta);
    // Vector3 view = Vector3Subtract(camDefault.position, camDefault.target);
    // view = Vector3Transform(view, rotation);
    // camDefault.position = Vector3Add(camDefault.target, view);

    /* --- Adjust DoF based on mouse position --- */

    // Vector2 mousePosition = GetMousePosition();
    // float mouseWheel = GetMouseWheelMove();
    //
    // float focusPoint = 0.5f + (5.0f - (mousePosition.y / GetScreenHeight()) * 5.0f);
    // R3D_SetDofFocusPoint(focusPoint);
    //
    // float focusScale = 0.5f + (5.0f - (mousePosition.x / GetScreenWidth()) * 5.0f);
    // R3D_SetDofFocusScale(focusScale);
    //
    // if (mouseWheel != 0.0f) {
    //     float maxBlurSize = R3D_GetDofMaxBlurSize();
    //     maxBlurSize += mouseWheel * 0.1f;
    //     R3D_SetDofMaxBlurSize(maxBlurSize);
    // }
    //
    // if (IsKeyPressed(KEY_F1)) {
    //     int debugMode = R3D_GetDofDebugMode();
    //     R3D_SetDofDebugMode((debugMode + 1) % 3);
    // }
}

void Draw(void)
{
    /* --- Ensure Clear Background --- */
    ClearBackground(BLACK);
  
    /* --- Render R3D scene --- */

    R3D_Begin(camDefault);
        R3D_SetBackgroundColor((Color){0, 0, 0, 255});
        R3D_DrawMeshInstancedEx(&meshSphere, &matDefault, instances, instanceColors, INSTANCE_COUNT);
    R3D_End();

    /* --- Draw DoF values --- */

    char dofText[128];
    snprintf(dofText, sizeof(dofText), "Focus Point: %.2f\nFocus Scale: %.2f\nMax Blur Size: %.2f\nDebug Mode: %d",
        R3D_GetDofFocusPoint(), R3D_GetDofFocusScale(), R3D_GetDofMaxBlurSize(), R3D_GetDofDebugMode());
    DrawText(dofText, 10, 50, 20, WHITE);

    /* --- Print instructions --- */

    DrawText("F1: Toggle Debug Mode\nScroll: Adjust Max Blur Size\nMouse Left/Right: Shallow/Deep DoF\nMouse Up/Down: Adjust Focus Point Depth", 300, 10, 20, WHITE);

    /* --- Draw FPS & Profiling--- */

    char fpsText[32];
    snprintf(fpsText, sizeof(fpsText), "FPS: %d", GetFPS());
    DrawText(fpsText, 10, 10, 20, WHITE);

    char profText[64];
    snprintf(profText, sizeof(profText), "DoF: %.2f, Bloom: %.2f", R3D_ProfGetGPUZoneMS("DOF Pass", 64), R3D_ProfGetGPUZoneMS("Bloom Pass", 64));
    DrawText(profText, 10, 30, 20, WHITE);
}

void Close(void)
{
    R3D_UnloadMesh(&meshSphere);
    R3D_Close();
}
