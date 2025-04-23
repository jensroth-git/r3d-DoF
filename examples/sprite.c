#include "./common.h"

/* === Resources === */

static Model		plane = { 0 };
static Camera3D		camera = { 0 };
static Texture2D    texture = { 0 };
static R3D_Sprite   sprite = { 0 };

/* === Bird Data === */

float birdDirX = 1.0f;
Vector3 birdPos = { 0.0f, 0.5f, 0.0f };

/* === Examples === */

const char* Init(void)
{
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    plane = LoadModelFromMesh(GenMeshPlane(1000, 1000, 1, 1));
    plane.materials[0].maps[MATERIAL_MAP_OCCLUSION].value = 1;
    plane.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value = 1;
    plane.materials[0].maps[MATERIAL_MAP_METALNESS].value = 0;

    texture = RES_LoadTexture("spritesheet.png");
    sprite = R3D_LoadSprite(texture, 4, 1);

    camera = (Camera3D){
        .position = (Vector3) { 0, 2, 5 },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    {
        R3D_LightLookAt(light, (Vector3) { 0, 10, 10 }, (Vector3) { 0 });
        R3D_SetLightActive(light, true);
    }

    return "[r3d] - sprite example";
}

void Update(float delta)
{
    R3D_UpdateSprite(&sprite, 10 * delta);

    Vector3 birdPosPrev = birdPos;

    birdPos.x = 2.0f * sinf(GetTime());
    birdPos.y = 1.0f + cosf(GetTime() * 4.0f) * 0.5f;
    birdDirX = (birdPos.x - birdPosPrev.x >= 0) ? 1 : -1;
}

void Draw(void)
{
    R3D_Begin(camera);

    R3D_ApplyBillboardMode(R3D_BILLBOARD_DISABLED);
    R3D_DrawModel(plane, (Vector3) { 0, -0.5f, 0 }, 1.0f);

    R3D_ApplyBillboardMode(R3D_BILLBOARD_Y_AXIS);
    R3D_DrawSpriteEx(sprite, birdPos, (Vector2) { birdDirX, 1.0f }, 0.0f);

    R3D_End();
}

void Close(void)
{
    R3D_UnloadSprite(sprite);
    UnloadTexture(texture);
    UnloadModel(plane);
    R3D_Close();
}
