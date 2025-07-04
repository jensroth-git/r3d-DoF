#include "./common.h"
#include "r3d.h"
#include "raymath.h"

/* === Resources === */

static R3D_Model model = { 0 };
static Matrix modelMatrix = { 0 };
static R3D_Skybox skybox = { 0 };
static Camera3D camera = { 0 };

static float modelScale = 1.0f;

/* === Example === */

const char* Init(void)
{
	R3D_Init(GetScreenWidth(), GetScreenHeight(), R3D_FLAG_FXAA);
	SetTargetFPS(60);

	R3D_SetSSAO(true);
	R3D_SetSSAORadius(4.0f);

	R3D_SetTonemapMode(R3D_TONEMAP_ACES);
	R3D_SetTonemapExposure(0.75f);
	R3D_SetTonemapWhite(1.25f);

	model = R3D_LoadModel(RESOURCES_PATH "pbr/musket.glb");
	{
		Matrix transform = MatrixRotateY(PI / 2);

		for (int i = 0; i < model.materialCount; i++) {
			SetTextureFilter(model.materials[i].albedo.texture, TEXTURE_FILTER_BILINEAR);
			SetTextureFilter(model.materials[i].orm.texture, TEXTURE_FILTER_BILINEAR);
		}
	}

	modelMatrix = MatrixIdentity();

	skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox2.png", CUBEMAP_LAYOUT_AUTO_DETECT);
	R3D_EnableSkybox(skybox);

	camera = (Camera3D){
		.position = (Vector3) { 0, 0, 0.5f },
		.target = (Vector3) { 0, 0, 0 },
		.up = (Vector3) { 0, 1, 0 },
		.fovy = 60,
	};

	R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
	{
		R3D_SetLightDirection(light, (Vector3) { 0, -1, -1 });
		R3D_SetLightActive(light, true);
	}

	return "[r3d] - PBR example";
}

void Update(float delta)
{
	modelScale = Clamp(modelScale + GetMouseWheelMove() * 0.1, 0.25f, 2.5f);

	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
	{
		float pitch = (GetMouseDelta().y * 0.005f) / modelScale;
		float yaw = (GetMouseDelta().x * 0.005f) / modelScale;

		Matrix rotate = MatrixRotateXYZ((Vector3) { pitch, yaw, 0.0f });
		modelMatrix = MatrixMultiply(modelMatrix, rotate);
	}
}

void Draw(void)
{
	R3D_Begin(camera);
		Matrix scale = MatrixScale(modelScale, modelScale, modelScale);
		Matrix transform = MatrixMultiply(modelMatrix, scale);
		R3D_DrawModelPro(&model, transform);
	R3D_End();

	DrawFPS(10, 10);

	DrawText("Model made by TommyLingL", 10, GetScreenHeight() - 30, 20, LIME);
}

void Close(void)
{
	R3D_UnloadModel(&model, true);
	R3D_UnloadSkybox(skybox);
	R3D_Close();
}
