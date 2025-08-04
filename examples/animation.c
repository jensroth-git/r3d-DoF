#include "./common.h"
#include "r3d.h"
#include "raylib.h"
#include "raymath.h"

/* === Resources === */

static R3D_Mesh plane = { 0 };
static R3D_Model dancer = { 0 };
static R3D_Material material = { 0 };
static Matrix instances[2*2] = { 0 };

static Camera3D camera = { 0 };

static int animCount = 0;
static R3D_ModelAnimation* anims = NULL;

static R3D_Light lights[2] = { 0 };

/* === Example === */

const char* Init(void)
{
    R3D_Init(GetScreenWidth(), GetScreenHeight(), R3D_FLAG_FXAA | R3D_FLAG_NO_FRUSTUM_CULLING);
    SetTargetFPS(60);

    R3D_SetSSAO(true);
    R3D_SetBloomIntensity(0.03f);
    R3D_SetBloomMode(R3D_BLOOM_ADDITIVE);
    R3D_SetTonemapMode(R3D_TONEMAP_ACES);

    R3D_SetBackgroundColor(BLACK);
    R3D_SetAmbientColor((Color) { 7, 7, 7, 255 });

    plane = R3D_GenMeshPlane(32, 32, 1, 1, true);
    dancer = R3D_LoadModel(RESOURCES_PATH "dancer.glb");
    material = R3D_GetDefaultMaterial();

    for (int z = 0; z < 2; z++) {
        for (int x = 0; x < 2; x++) {
            instances[z * 2 + x] = MatrixTranslate((float)x - 0.5f, 0, (float)z - 0.5f);
        }
    }

    Image checked = GenImageChecked(256, 256, 4, 4, (Color) { 20, 20, 20, 255 }, WHITE);
    material.albedo.texture = LoadTextureFromImage(checked);
    UnloadImage(checked);

    material.orm.roughness = 0.5f;
    material.orm.metalness = 0.5f;

    anims = R3D_LoadModelAnimations(RESOURCES_PATH "dancer.glb", &animCount, 60);

    camera = (Camera3D) {
        .position = (Vector3) { 0, 2.0f, 3.5f },
        .target = (Vector3) { 0, 1.0f, 1.5f },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    lights[0] = R3D_CreateLight(R3D_LIGHT_OMNI);
    R3D_SetLightPosition(lights[0], (Vector3) { -10.0f, 25.0f, 0.0f });
    R3D_EnableShadow(lights[0], 4096);
    R3D_SetLightActive(lights[0], true);

    lights[1] = R3D_CreateLight(R3D_LIGHT_OMNI);
    R3D_SetLightPosition(lights[1], (Vector3) { +10.0f, 25.0f, 0.0f });
    R3D_EnableShadow(lights[1], 4096);
    R3D_SetLightActive(lights[1], true);

    DisableCursor();

    return "[r3d] - Animation example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_FREE);
    dancer.anim = &anims[0];
    dancer.animFrame++;

    R3D_SetLightColor(lights[0], ColorFromHSV(90.0f * (float)GetTime() + 90.0f, 1.0f, 1.0f));
    R3D_SetLightColor(lights[1], ColorFromHSV(90.0f * (float)GetTime() - 90.0f, 1.0f, 1.0f));
}

void Draw(void)
{
    static int frame = 0;

    R3D_Begin(camera);
        R3D_DrawMesh(&plane, &material, MatrixIdentity());
        R3D_DrawModel(&dancer, (Vector3) { 0, 0, 1.5f }, 1.0f);
        R3D_DrawModelInstanced(&dancer, instances, 2*2);
    R3D_End();

	DrawCredits("Model made by zhuoyi0904");
}

void Close(void)
{
    R3D_UnloadMesh(&plane);
    R3D_UnloadModel(&dancer, true);
    R3D_UnloadMaterial(&material);
    R3D_Close();
}
