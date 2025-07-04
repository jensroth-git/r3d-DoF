#include "./r3d_projection.h"
#include "./r3d_collision.h"
#include <raymath.h>
#include <float.h>

r3d_project_point_result_t r3d_project_point(Vector3 point, Matrix viewProj, int screenWidth, int screenHeight)
{
    r3d_project_point_result_t result = { 0 };

    // Transform the 3D point into homogeneous clip space coordinates
    Vector4 clipSpace;
    clipSpace.x = point.x * viewProj.m0 + point.y * viewProj.m4 + point.z * viewProj.m8 + viewProj.m12;
    clipSpace.y = point.x * viewProj.m1 + point.y * viewProj.m5 + point.z * viewProj.m9 + viewProj.m13;
    clipSpace.z = point.x * viewProj.m2 + point.y * viewProj.m6 + point.z * viewProj.m10 + viewProj.m14;
    clipSpace.w = point.x * viewProj.m3 + point.y * viewProj.m7 + point.z * viewProj.m11 + viewProj.m15;

    // Checks if the point is behind the near plane  
    result.outNear = (clipSpace.w <= 0.0);

    // Checks if the point is beyond the far plane  
    result.outFar = (clipSpace.z > clipSpace.w);

    // Compute the screen coordinates even if the point is not visible
    // Divide by w to convert to Normalized Device Coordinates (NDC)
    float invW = 1.0f / clipSpace.w;
    float ndcX = clipSpace.x * invW;
    float ndcY = clipSpace.y * invW;

    // Determine if the point is within the viewport bounds
    result.inViewport = (ndcX >= -1.0f && ndcX <= 1.0f && ndcY >= -1.0f && ndcY <= 1.0f);

    // Convert NDC to screen space coordinates
    result.position.x = (ndcX + 1.0f) * 0.5f * screenWidth;
    result.position.y = (1.0f - (ndcY + 1.0f) * 0.5f) * screenHeight;

    return result;
}

// Utility function to check if a point is in front of the near plane
static inline bool r3d_is_point_in_front_of_near_plane(Vector3 point, Matrix viewProj, float nearPlane) {
    // Transform the point into view space
    Vector4 viewPoint = {
        viewProj.m0 * point.x + viewProj.m4 * point.y + viewProj.m8 * point.z + viewProj.m12,
        viewProj.m1 * point.x + viewProj.m5 * point.y + viewProj.m9 * point.z + viewProj.m13,
        viewProj.m2 * point.x + viewProj.m6 * point.y + viewProj.m10 * point.z + viewProj.m14,
        viewProj.m3 * point.x + viewProj.m7 * point.y + viewProj.m11 * point.z + viewProj.m15
    };
    return viewPoint.z > nearPlane;
}

r3d_project_light_result_t r3d_project_sphere_light(Vector3 center, float radius, Vector3 viewPos, Matrix viewProj, int screenWidth, int screenHeight, float nearPlane)
{
    r3d_project_light_result_t result = { 0 };
    
    // If the camera is inside the sphere, all the light is visible
    if (r3d_collision_check_point_in_sphere(viewPos, center, radius)) {
        result.isVisible = true;
        result.coversEntireScreen = true;
        result.screenRect.x = 0;
        result.screenRect.y = 0;
        result.screenRect.width = (float)screenWidth;
        result.screenRect.height = (float)screenHeight;
        return result;
    }

    // Create points to sample the sphere
    Vector3 points[26]; // 6 axial points + 20 points on circles
    int pointCount = 0;

    // Axial points (6 main directions)
    points[pointCount++] = (Vector3){center.x + radius, center.y, center.z};
    points[pointCount++] = (Vector3){center.x - radius, center.y, center.z};
    points[pointCount++] = (Vector3){center.x, center.y + radius, center.z};
    points[pointCount++] = (Vector3){center.x, center.y - radius, center.z};
    points[pointCount++] = (Vector3){center.x, center.y, center.z + radius};
    points[pointCount++] = (Vector3){center.x, center.y, center.z - radius};

    // Points on circles for better coverage
    for (int ring = 0; ring < 2; ring++) {
        float z = center.z + radius * (ring == 0 ? 0.5f : -0.5f);
        float ringRadius = radius * sqrtf(1.0f - 0.25f); // radius at this height
        
        for (int i = 0; i < 10; i++) {
            float angle = i * (2.0f * PI / 10.0f);
            points[pointCount++] = (Vector3){
                center.x + ringRadius * cosf(angle),
                center.y + ringRadius * sinf(angle),
                z
            };
        }
    }

    // Initialize min/max values
    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;
    
    bool hasValidPoints = false;

    // Project each point
    for (int i = 0; i < pointCount; i++) {
        r3d_project_point_result_t projResult = r3d_project_point(
            points[i], viewProj, screenWidth, screenHeight
        );

        // Do not completely ignore points behind the near plane
        // Instead, clamp them to the edges of the screen
        if (projResult.outNear) {
            // Point behind the camera - potentially contributes to edges
            hasValidPoints = true;
            
            // Conservative expansion for points behind
            minX = fminf(minX, 0.0f);
            minY = fminf(minY, 0.0f);
            maxX = fmaxf(maxX, (float)screenWidth);
            maxY = fmaxf(maxY, (float)screenHeight);
        } else {
            // Point in front of the camera - use normal projection
            hasValidPoints = true;
            
            if (projResult.position.x < minX) minX = projResult.position.x;
            if (projResult.position.x > maxX) maxX = projResult.position.x;
            if (projResult.position.y < minY) minY = projResult.position.y;
            if (projResult.position.y > maxY) maxY = projResult.position.y;
        }
    }

    // Check if any part of the sphere crosses the near plane
    float distanceToCenter = sqrtf(
        (center.x - viewPos.x) * (center.x - viewPos.x) +
        (center.y - viewPos.y) * (center.y - viewPos.y) +
        (center.z - viewPos.z) * (center.z - viewPos.z)
    );
    
    bool intersectsNearPlane = (distanceToCenter < radius + nearPlane);

    if (!hasValidPoints && !intersectsNearPlane) {
        result.isVisible = false;
        return result;
    }

    // If the sphere crosses the near plane, be more conservative
    if (intersectsNearPlane) {
        minX = fminf(minX, 0.0f);
        minY = fminf(minY, 0.0f);
        maxX = fmaxf(maxX, (float)screenWidth);
        maxY = fmaxf(maxY, (float)screenHeight);
    }

    // Clamp at the screen limits
    minX = fmaxf(0.0f, minX);
    minY = fmaxf(0.0f, minY);
    maxX = fminf((float)screenWidth, maxX);
    maxY = fminf((float)screenHeight, maxY);

    // Build the final rectangle
    result.isVisible = (maxX > minX) && (maxY > minY);
    result.coversEntireScreen = (minX <= 0.0f && minY <= 0.0f && 
                                maxX >= screenWidth && maxY >= screenHeight);
    
    if (result.isVisible) {
        result.screenRect.x = minX;
        result.screenRect.y = minY;
        result.screenRect.width = maxX - minX;
        result.screenRect.height = maxY - minY;
    }

    return result;
}

r3d_project_light_result_t r3d_project_cone_light(Vector3 tip, Vector3 dir, float length, float radius, Vector3 viewPos, Matrix viewProj, int screenWidth, int screenHeight, float nearPlane)
{
    r3d_project_light_result_t result = { 0 };

    // If the camera is inside the cone, all the light is visible
    if (r3d_collision_check_point_in_cone(viewPos, tip, dir, length, radius)) {
        result.isVisible = true;
        result.coversEntireScreen = true;
        result.screenRect.x = 0;
        result.screenRect.y = 0;
        result.screenRect.width = (float)screenWidth;
        result.screenRect.height = (float)screenHeight;
        return result;
    }

    // Normalize the direction vector
    float dirLen = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    Vector3 normalizedDir = { dir.x / dirLen, dir.y / dirLen, dir.z / dirLen };

    // Calculate the position of the base of the cone
    Vector3 base = {
        tip.x + normalizedDir.x * length,
        tip.y + normalizedDir.y * length,
        tip.z + normalizedDir.z * length
    };

    // Find two vectors perpendicular to the direction vector
    Vector3 right = { 0 };
    Vector3 up = { 0 };

    if (fabsf(normalizedDir.x) < fabsf(normalizedDir.y) && fabsf(normalizedDir.x) < fabsf(normalizedDir.z)) {
        right = (Vector3){ 0, -normalizedDir.z, normalizedDir.y };
    }
    else if (fabsf(normalizedDir.y) < fabsf(normalizedDir.z)) {
        right = (Vector3){ -normalizedDir.z, 0, normalizedDir.x };
    }
    else {
        right = (Vector3){ -normalizedDir.y, normalizedDir.x, 0 };
    }

    float rightLen = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
    right.x /= rightLen;
    right.y /= rightLen;
    right.z /= rightLen;

    up.x = normalizedDir.y * right.z - normalizedDir.z * right.y;
    up.y = normalizedDir.z * right.x - normalizedDir.x * right.z;
    up.z = normalizedDir.x * right.y - normalizedDir.y * right.x;

    // Define the points of the cone to be projected
    Vector3 points[17];  // 1 vertex + 16 points on the base
    points[0] = tip;  // Top of the cone

    // Generate 16 points evenly distributed around the base circle
    for (int i = 0; i < 16; i++) {
        float angle = i * (2.0f * PI / 16.0f);
        float cosA = cosf(angle);
        float sinA = sinf(angle);

        points[i + 1] = (Vector3){
            base.x + radius * (cosA * right.x + sinA * up.x),
            base.y + radius * (cosA * right.y + sinA * up.y),
            base.z + radius * (cosA * right.z + sinA * up.z)
        };
    }

    // Initialize min/max values
    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;
    
    bool hasValidPoints = false;

    // Project each point
    for (int i = 0; i < 17; i++) {
        r3d_project_point_result_t projResult = r3d_project_point(
            points[i], viewProj, screenWidth, screenHeight
        );

        if (projResult.outNear) {
            // Point behind the camera - potentially contributes to edges
            hasValidPoints = true;
            
            // Conservative expansion for points behind
            minX = fminf(minX, 0.0f);
            minY = fminf(minY, 0.0f);
            maxX = fmaxf(maxX, (float)screenWidth);
            maxY = fmaxf(maxY, (float)screenHeight);
        } else {
            // Point in front of the camera - use normal projection
            hasValidPoints = true;
            
            if (projResult.position.x < minX) minX = projResult.position.x;
            if (projResult.position.x > maxX) maxX = projResult.position.x;
            if (projResult.position.y < minY) minY = projResult.position.y;
            if (projResult.position.y > maxY) maxY = projResult.position.y;
        }
    }

    // Check if the cone crosses the near plane
    float tipDistance = sqrtf(
        (tip.x - viewPos.x) * (tip.x - viewPos.x) +
        (tip.y - viewPos.y) * (tip.y - viewPos.y) +
        (tip.z - viewPos.z) * (tip.z - viewPos.z)
    );
    
    bool intersectsNearPlane = (tipDistance < nearPlane) || 
                               (tipDistance < length + radius);

    if (!hasValidPoints && !intersectsNearPlane) {
        result.isVisible = false;
        return result;
    }

    // If the cone crosses the near plane, be more conservative
    if (intersectsNearPlane) {
        minX = fminf(minX, 0.0f);
        minY = fminf(minY, 0.0f);
        maxX = fmaxf(maxX, (float)screenWidth);
        maxY = fmaxf(maxY, (float)screenHeight);
    }

    // Clamp at the screen limits
    minX = fmaxf(0.0f, minX);
    minY = fmaxf(0.0f, minY);
    maxX = fminf((float)screenWidth, maxX);
    maxY = fminf((float)screenHeight, maxY);

    // Build the final rectangle
    result.isVisible = (maxX > minX) && (maxY > minY);
    result.coversEntireScreen = (minX <= 0.0f && minY <= 0.0f && 
                                maxX >= screenWidth && maxY >= screenHeight);
    
    if (result.isVisible) {
        result.screenRect.x = minX;
        result.screenRect.y = minY;
        result.screenRect.width = maxX - minX;
        result.screenRect.height = maxY - minY;
    }

    return result;
}
