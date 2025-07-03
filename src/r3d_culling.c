#include "r3d.h"

#include "./r3d_state.h"

bool R3D_IsPointInFrustum(Vector3 position)
{
	return r3d_frustum_is_point_in(&R3D.state.frustum.shape, &position);
}

bool R3D_IsPointInFrustumXYZ(float x, float y, float z)
{
	return r3d_frustum_is_point_in_xyz(&R3D.state.frustum.shape, x, y, z);
}

bool R3D_IsSphereInFrustum(Vector3 position, float radius)
{
	return r3d_frustum_is_sphere_in(&R3D.state.frustum.shape, &position, radius);
}

bool R3D_IsBoundingBoxInFrustum(BoundingBox aabb)
{
	return r3d_frustum_is_bounding_box_in(&R3D.state.frustum.shape, &aabb);
}
