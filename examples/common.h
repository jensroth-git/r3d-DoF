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
