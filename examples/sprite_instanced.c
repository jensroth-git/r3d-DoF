#include "./common.h"
#include "r3d.h"
#include "raylib.h"
#include "raymath.h"

/* === Resources === */

static Camera3D camera = { 0 };

static R3D_Mesh plane = { 0 };
static R3D_Material material = { 0 };

static Texture2D texture = { 0 };
static R3D_Sprite sprite = { 0 };

static Matrix transforms[512] = { 0 };

/* === Examples === */

const char* Init(void)
{
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);
    DisableCursor();

    R3D_SetBackgroundColor(SKYBLUE);

    plane = R3D_GenMeshPlane(1000, 1000, 1, 1, true);
    material = R3D_GetDefaultMaterial();
    material.albedo.color = GREEN;

    texture = LoadTexture(RESOURCES_PATH "tree.png");
    sprite = R3D_LoadSprite(texture, 1, 1);

    for (int i = 0; i < sizeof(transforms) / sizeof(*transforms); i++) {
        float scaleFactor = GetRandomValue(50, 100) / 10.0f;
        Matrix scale = MatrixScale(scaleFactor, scaleFactor, 1.0f);
        Matrix translate = MatrixTranslate(GetRandomValue(-500, 500), scaleFactor, GetRandomValue(-500, 500));;
        transforms[i] = MatrixMultiply(scale, translate);
    }

    camera = (Camera3D){
        .position = (Vector3) { 0, 5, 0 },
        .target = (Vector3) { 0, 5, -1 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    {
        R3D_LightLookAt(light, (Vector3) { 0, 10, 10 }, (Vector3) { 0 });
        R3D_SetLightActive(light, true);
    }

    return "[r3d] - Instanced sprites example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_FREE);
}

void Draw(void)
{
    R3D_Begin(camera);

    R3D_DrawMesh(&plane, &material, MatrixTranslate(0, 0, 0));
    R3D_DrawSpriteInstanced(&sprite, transforms, sizeof(transforms) / sizeof(*transforms));

    R3D_End();
}

void Close(void)
{
    R3D_UnloadSprite(&sprite);
    R3D_UnloadMesh(&plane);
    UnloadTexture(texture);
    R3D_Close();
}
