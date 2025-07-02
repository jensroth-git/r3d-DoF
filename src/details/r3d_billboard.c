#include "./r3d_billboard.h"

#include <raymath.h>

void r3d_transform_to_billboard_front(Matrix* model, const Matrix* invView)
{
    // Extract original scales
    float scaleX = Vector3Length((Vector3) { model->m0, model->m1, model->m2 });
    float scaleY = Vector3Length((Vector3) { model->m4, model->m5, model->m6 });
    float scaleZ = Vector3Length((Vector3) { model->m8, model->m9, model->m10 });

    // Copy the view basis vectors, applying original scales
    model->m0 = invView->m0 * scaleX;
    model->m1 = invView->m1 * scaleX;
    model->m2 = invView->m2 * scaleX;

    model->m4 = invView->m4 * scaleY;
    model->m5 = invView->m5 * scaleY;
    model->m6 = invView->m6 * scaleY;

    model->m8 = invView->m8 * scaleZ;
    model->m9 = invView->m9 * scaleZ;
    model->m10 = invView->m10 * scaleZ;
}

void r3d_transform_to_billboard_y(Matrix* model, const Matrix* invView)
{
    // Extract position
    Vector3 position = { model->m12, model->m13, model->m14 };

    // Extract original scales
    float scaleX = Vector3Length((Vector3) { model->m0, model->m1, model->m2 });
    float scaleY = Vector3Length((Vector3) { model->m4, model->m5, model->m6 });
    float scaleZ = Vector3Length((Vector3) { model->m8, model->m9, model->m10 });

    // Preserve the original Y-axis
    Vector3 upVector = Vector3Normalize((Vector3) { model->m4, model->m5, model->m6 });

    // Compute look direction
    Vector3 lookDirection = Vector3Normalize(Vector3Subtract((Vector3) { invView->m12, invView->m13, invView->m14 }, position));

    // Compute right vector
    Vector3 rightVector = Vector3Normalize(Vector3CrossProduct(upVector, lookDirection));

    // Compute front vector
    Vector3 frontVector = Vector3Normalize(Vector3CrossProduct(rightVector, upVector));

    // Construct new model matrix with preserved scales
    model->m0 = rightVector.x * scaleX;
    model->m1 = rightVector.y * scaleX;
    model->m2 = rightVector.z * scaleX;

    model->m4 = upVector.x * scaleY;
    model->m5 = upVector.y * scaleY;
    model->m6 = upVector.z * scaleY;

    model->m8 = frontVector.x * scaleZ;
    model->m9 = frontVector.y * scaleZ;
    model->m10 = frontVector.z * scaleZ;
}
