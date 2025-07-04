#include "./common.h"
#include "r3d.h"
#include "raylib.h"
#include "raymath.h"

/* === Resources === */

static R3D_Model model = { 0 };
static R3D_Mesh ground = { 0 };
static R3D_Material groundMat = { 0 };
static R3D_Skybox skybox = { 0 };
static Camera3D camera = { 0 };
static bool showSkybox = true;

/* === Example === */

const char* Init(void)
{
    R3D_Flags falgs = R3D_FLAG_TRANSPARENT_SORTING | R3D_FLAG_FXAA;

	R3D_Init(GetScreenWidth(), GetScreenHeight(), falgs);
	SetTargetFPS(60);
    DisableCursor();

    R3D_SetBackgroundColor(BLACK);
    R3D_SetAmbientColor(DARKGRAY);

	R3D_SetSSAO(true);
	R3D_SetSSAORadius(2.0f);
    R3D_SetBloomIntensity(0.1f);
    R3D_SetBloomMode(R3D_BLOOM_MIX);
    R3D_SetTonemapMode(R3D_TONEMAP_ACES);

	model = R3D_LoadModel(RESOURCES_PATH "pbr/car.glb");
    ground = R3D_GenMeshPlane(100.0f, 100.0f, 1, 1, true);

    groundMat = R3D_GetDefaultMaterial();
    groundMat.albedo.color = (Color) { 0, 31, 7, 255 };

	skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox3.png", CUBEMAP_LAYOUT_AUTO_DETECT);
	R3D_EnableSkybox(skybox);

	camera = (Camera3D){
		.position = (Vector3) { 0, 0, 5 },
		.target = (Vector3) { 0, 0, 0 },
		.up = (Vector3) { 0, 1, 0 },
		.fovy = 60,
	};

    R3D_SetSceneBounds((BoundingBox) { { -10, -10, -10 }, { 10, 10, 10 } });

	R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    {
        R3D_SetLightDirection(light, (Vector3) { -1, -1, -1 });
        R3D_EnableShadow(light, 4096);
	    R3D_SetLightActive(light, true);
    }

	return "[r3d] - PBR example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_FREE);

    if (IsKeyPressed(KEY_O)) {
        R3D_SetSSAO(!R3D_GetSSAO());
    }

    if (IsKeyPressed(KEY_T)) {
        showSkybox = !showSkybox;
        if (showSkybox) R3D_EnableSkybox(skybox);
        else R3D_DisableSkybox();
    }
}

void Draw(void)
{
	R3D_Begin(camera);
        R3D_DrawMesh(&ground, &groundMat, MatrixTranslate(0.0f, -0.4f, 0.0f));
		R3D_DrawModelPro(&model, MatrixRotateX(-90.0f * DEG2RAD));
	R3D_End();

	DrawFPS(10, 10);
}

void Close(void)
{
	R3D_UnloadModel(&model, true);
	R3D_UnloadSkybox(skybox);
	R3D_Close();
}
