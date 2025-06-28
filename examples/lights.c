#include "./common.h"
#include "raylib.h"

/* === Resources === */

static Model		plane = { 0 };
static Mesh		    sphere = { 0 };
static Material     material = { 0 };
static Camera3D		camera = { 0 };

static Matrix* transforms = NULL;

static R3D_Light lights[100] = { 0 };


/* === Examples === */

const char* Init(void)
{
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    plane = LoadModelFromMesh(GenMeshPlane(1000, 1000, 1, 1));
    plane.materials[0].maps[MATERIAL_MAP_OCCLUSION].value = 1;
    plane.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value = 1;
    plane.materials[0].maps[MATERIAL_MAP_METALNESS].value = 0;

    sphere = GenMeshSphere(0.35f, 16, 16);

    material = LoadMaterialDefault();
    material.maps[MATERIAL_MAP_OCCLUSION].value = 1;
    material.maps[MATERIAL_MAP_ROUGHNESS].value = 0.25;
    material.maps[MATERIAL_MAP_METALNESS].value = 0.75;

    camera = (Camera3D) {
        .position = (Vector3) { 0, 2, 2 },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    transforms = RL_MALLOC(100 * 100 * sizeof(Matrix));

    for (int x = -50; x < 50; x++) {
        for (int z = -50; z < 50; z++) {
            int index = (z + 50) * 100 + (x + 50);
            transforms[index] = MatrixTranslate(x, 0, z);
        }
    }

    for (int x = -5; x < 5; x++) {
        for (int z = -5; z < 5; z++) {
            int index = (z + 5) * 10 + (x + 5);
            lights[index] = R3D_CreateLight(R3D_LIGHT_OMNI);
            R3D_SetLightPosition(lights[index], (Vector3) { x * 10, 10, z * 10 });
            R3D_SetLightColor(lights[index], ColorFromHSV((float)index / 100 * 360, 1.0f, 1.0f));
            R3D_SetLightRange(lights[index], 20.0f);
            R3D_SetLightActive(lights[index], true);
        }
    }

    return "[r3d] - lights example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_ORBITAL);
}

void Draw(void)
{
    R3D_Begin(camera);
        R3D_DrawModel(plane, (Vector3) { 0, -0.5f, 0 }, 1.0f);
        R3D_DrawMeshInstanced(sphere, material, transforms, 100 * 100);
    R3D_End();

    if (IsKeyDown(KEY_SPACE)) {
        BeginMode3D(camera);
        for (int i = 0; i < 100; i++) {
            R3D_DrawLightShape(lights[i]);
        }
        EndMode3D();
    }

    DrawFPS(10, 10);
    DrawText("Press SPACE to show the lights", 10, GetScreenHeight() - 34, 24, BLACK);
}

void Close(void)
{
    UnloadModel(plane);
    UnloadMesh(sphere);
    UnloadMaterial(material);
    R3D_Close();
}
