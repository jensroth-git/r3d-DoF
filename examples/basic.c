#include "./common.h"
#include "r3d.h"

/* === Resources === */

static R3D_Mesh plane = { 0 };
static R3D_Mesh sphere = { 0 };
static R3D_Material material = { 0 };
static Camera3D camera = { 0 };

/* === Example === */

const char* Init(void)
{
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    plane = R3D_GenMeshPlane(1000, 1000, 1, 1, true);
    sphere = R3D_GenMeshSphere(0.5f, 64, 64, true);
    material = R3D_GetDefaultMaterial();

    camera = (Camera3D) {
        .position = (Vector3) { 0, 2, 2 },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    {
        R3D_LightLookAt(light, (Vector3) { 0, 10, 5 }, (Vector3) { 0 });
        R3D_SetLightOuterCutOff(light, 45.0f);
        R3D_SetLightInnerCutOff(light, 22.5f);
        R3D_EnableShadow(light, 4096);
        R3D_SetLightActive(light, true);
    }

    return "[r3d] - Basic example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_ORBITAL);
}

void Draw(void)
{
    R3D_Begin(camera);
        R3D_DrawMesh(&plane, &material, MatrixTranslate(0, -0.5f, 0));
        R3D_DrawMesh(&sphere, &material, MatrixIdentity());
    R3D_End();
}

void Close(void)
{
    R3D_UnloadMesh(&plane);
    R3D_UnloadMesh(&sphere);
    R3D_Close();
}
