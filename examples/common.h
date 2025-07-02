#ifndef COMMON_H
#define COMMON_H

#include <r3d.h>
#include <raylib.h>
#include <raymath.h>

#include <stdlib.h>
#include <stddef.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "/"
#endif


/* === Helper functions === */

static inline Texture2D RES_LoadTexture(const char* fileName)
{
	Texture2D texture = LoadTexture(TextFormat("%s%s", RESOURCES_PATH, fileName));

	GenTextureMipmaps(&texture);
	SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_4X);

	return texture;
}

/* === Example functions === */

const char* Init(void);
void Update(float delta);
void Draw(void);
void Close();


/* === Main program === */

int main(void)
{
	InitWindow(800, 600, "");

	const char* title = Init();
	SetWindowTitle(title);

	while (!WindowShouldClose()) {
		Update(GetFrameTime());
		BeginDrawing();
		Draw();
		EndDrawing();
	}

	Close();
	CloseWindow();

	return 0;
}


#endif // COMMON_H
