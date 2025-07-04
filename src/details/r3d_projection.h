#ifndef R3D_PROJECTION_H
#define R3D_PROJECTION_H

#include <raylib.h>

typedef struct {
    Vector2 position;
    bool inViewport;
    bool outNear;
    bool outFar;
} r3d_project_point_result_t;

typedef struct {
    Rectangle screenRect;
    bool isVisible;
    bool coversEntireScreen;
} r3d_project_light_result_t;

r3d_project_point_result_t r3d_project_point(Vector3 point, Matrix viewProj, int screenWidth, int screenHeight);
r3d_project_light_result_t r3d_project_sphere_light(Vector3 center, float radius, Vector3 viewPos, Matrix viewProj, int screenWidth, int screenHeight, float nearPlane);
r3d_project_light_result_t r3d_project_cone_light(Vector3 tip, Vector3 dir, float length, float radius, Vector3 viewPos, Matrix viewProj, int screenWidth, int screenHeight, float nearPlane);

#endif // R3D_PROJECTION_H
