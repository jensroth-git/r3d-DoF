#include "r3d.h"

#include <raymath.h>

R3D_Sprite R3D_LoadSprite(Texture2D texture, int xFrameCount, int yFrameCount)
{
    R3D_Sprite sprite = { 0 };

    sprite.material = R3D_GetDefaultMaterial();

    sprite.material.albedo.texture = texture;
    sprite.material.blendMode = R3D_BLEND_ALPHA;
    sprite.material.billboardMode = R3D_BILLBOARD_Y_AXIS;

    sprite.frameSize.x = (float)texture.width / xFrameCount;
    sprite.frameSize.y = (float)texture.height / yFrameCount;

    sprite.xFrameCount = xFrameCount;
    sprite.yFrameCount = yFrameCount;

    return sprite;
}

void R3D_UnloadSprite(const R3D_Sprite* sprite)
{
    R3D_UnloadMaterial(&sprite->material);
}

void R3D_UpdateSprite(R3D_Sprite* sprite, float speed)
{
    R3D_UpdateSpriteEx(sprite, 0, sprite->xFrameCount * sprite->yFrameCount, speed);
}

void R3D_UpdateSpriteEx(R3D_Sprite* sprite, int firstFrame, int lastFrame, float speed)
{
    sprite->currentFrame = Wrap(sprite->currentFrame + speed, (float)firstFrame, (float)lastFrame);
}
