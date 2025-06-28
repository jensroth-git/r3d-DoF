#include "r3d.h"

#include <raymath.h>

R3D_Sprite R3D_LoadSprite(Texture2D texture, int xFrameCount, int yFrameCount)
{
    R3D_Sprite sprite = { 0 };

    sprite.material = LoadMaterialDefault();
    sprite.material.maps[MATERIAL_MAP_ALBEDO].texture = texture;
    sprite.material.maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;

    sprite.frameSize.x = (float)texture.width / xFrameCount;
    sprite.frameSize.y = (float)texture.height / yFrameCount;

    sprite.xFrameCount = xFrameCount;
    sprite.yFrameCount = yFrameCount;

    return sprite;
}

void R3D_UnloadSprite(R3D_Sprite sprite)
{
    if (IsMaterialValid(sprite.material)) {
        UnloadMaterial(sprite.material);
    }
}

void R3D_UpdateSprite(R3D_Sprite* sprite, float speed)
{
    R3D_UpdateSpriteEx(sprite, 0, sprite->xFrameCount * sprite->yFrameCount, speed);
}

void R3D_UpdateSpriteEx(R3D_Sprite* sprite, int firstFrame, int lastFrame, float speed)
{
    sprite->currentFrame = Wrap(sprite->currentFrame + speed, firstFrame, lastFrame);
}
