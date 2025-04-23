#include "./common.h"
#include "r3d.h"
#include <rlgl.h>

/* === Resources === */

static Model		sponza = { 0 };
static R3D_Skybox	skybox = { 0 };
static Camera3D		camera = { 0 };
static R3D_Light    lights[2] = { 0 };

static bool sky = false;


/* === Examples === */

const char* Init(void)
{
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    R3D_SetSSAO(true);
    R3D_SetSSAORadius(4.0f);
    R3D_SetBloomMode(R3D_BLOOM_MIX);

    sponza = RES_LoadModel("sponza.glb");

    for (int i = 0; i < sponza.materialCount; i++) {
        sponza.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
        sponza.materials[i].maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
        sponza.materials[i].maps[MATERIAL_MAP_ROUGHNESS].value = 1.0f;
        sponza.materials[i].maps[MATERIAL_MAP_METALNESS].value = 1.0f;

        GenTextureMipmaps(&sponza.materials[i].maps[MATERIAL_MAP_ALBEDO].texture);
        SetTextureFilter(sponza.materials[i].maps[MATERIAL_MAP_ALBEDO].texture, TEXTURE_FILTER_TRILINEAR);

        GenTextureMipmaps(&sponza.materials[i].maps[MATERIAL_MAP_NORMAL].texture);
        SetTextureFilter(sponza.materials[i].maps[MATERIAL_MAP_NORMAL].texture, TEXTURE_FILTER_TRILINEAR);

        // REVIEW: Issue with the model textures
        sponza.materials[i].maps[MATERIAL_MAP_ROUGHNESS].texture = (Texture2D){ .id = rlGetTextureIdDefault() };
    }

    // NOTE: Toggle sky with 'T' key
    skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox3.png", CUBEMAP_LAYOUT_AUTO_DETECT);


    BoundingBox sceneBounds = GetModelBoundingBox(sponza);
    R3D_SetSceneBounds(sceneBounds);

    for (int i = 0; i < 2; i++) {
        lights[i] = R3D_CreateLight(R3D_LIGHT_DIR);

        R3D_LightLookAt(lights[i], (Vector3) { i ? -10 : 10, 20, 0 }, Vector3Zero());
        R3D_SetLightActive(lights[i], true);
        R3D_SetLightEnergy(lights[i], 10.0f);

        R3D_SetShadowUpdateMode(lights[i], R3D_SHADOW_UPDATE_MANUAL);
        R3D_EnableShadow(lights[i], 4096);
    }

    camera = (Camera3D){
        .position = (Vector3) { 0, 0, 0 },
        .target = (Vector3) { 0, 0, -1 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    DisableCursor();

    return "[r3d] - sponza example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_FREE);

    if (IsKeyPressed(KEY_T)) {
        if (sky) R3D_DisableSkybox();
        else R3D_EnableSkybox(skybox);
        sky = !sky;
    }

    if (IsKeyPressed(KEY_F)) {
        bool fxaa = R3D_HasState(R3D_FLAG_FXAA);
        if (fxaa) R3D_ClearState(R3D_FLAG_FXAA);
        else R3D_SetState(R3D_FLAG_FXAA);
    }

    if (IsKeyPressed(KEY_O)) {
        R3D_SetSSAO(!R3D_GetSSAO());
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        R3D_Tonemap tonemap = R3D_GetTonemapMode();
        R3D_SetTonemapMode((tonemap + 5 - 1) % 5);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        R3D_Tonemap tonemap = R3D_GetTonemapMode();
        R3D_SetTonemapMode((tonemap + 1) % 5);
    }
}

void Draw(void)
{
    R3D_Begin(camera);
        R3D_DrawModel(sponza, (Vector3) { 0 }, 1.0f);
    R3D_End();

    BeginMode3D(camera);
        DrawSphere(R3D_GetLightPosition(lights[0]), 0.5f, WHITE);
        DrawSphere(R3D_GetLightPosition(lights[1]), 0.5f, WHITE);
    EndMode3D();

    R3D_Tonemap tonemap = R3D_GetTonemapMode();

    switch (tonemap) {
    case R3D_TONEMAP_LINEAR: {
        const char* txt = "< TONEMAP LINEAR >";
        DrawText(txt, GetScreenWidth() - MeasureText(txt, 20) - 10, 10, 20, LIME);
    }
    break;
    case R3D_TONEMAP_REINHARD: {
        const char* txt = "< TONEMAP REINHARD >";
        DrawText(txt, GetScreenWidth() - MeasureText(txt, 20) - 10, 10, 20, LIME);
    }
    break;
    case R3D_TONEMAP_FILMIC: {
        const char* txt = "< TONEMAP FILMIC >";
        DrawText(txt, GetScreenWidth() - MeasureText(txt, 20) - 10, 10, 20, LIME);
    }
    break;
    case R3D_TONEMAP_ACES: {
        const char* txt = "< TONEMAP ACES >";
        DrawText(txt, GetScreenWidth() - MeasureText(txt, 20) - 10, 10, 20, LIME);

    } break;
    case R3D_TONEMAP_AGX: {
        const char* txt = "< TONEMAP AGX >";
        DrawText(txt, GetScreenWidth() - MeasureText(txt, 20) - 10, 10, 20, LIME);

    } break;
    }    
        
    DrawFPS(10, 10);
}

void Close(void)
{
    UnloadModel(sponza);
    R3D_UnloadSkybox(skybox);
    R3D_Close();
}
