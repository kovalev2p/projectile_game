#include "raylib.h"
#include "resource_dir.h"	// utility header for SearchAndSetResourceDir

int main ()
{
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
	InitWindow(800, 600, "Hello Raylib"); // Create the window and OpenGL context
	SearchAndSetResourceDir("resources");

	Texture wabbit = LoadTexture("wabbit_alpha.png");
	
	// game loop
	while (!WindowShouldClose()) // run the loop until the user presses ESCAPE or presses the Close button on the window
	{
		BeginDrawing();
		ClearBackground(BLACK);

		DrawText("Hello Raylib", 200,200,20,WHITE);
		DrawTexture(wabbit, 400, 200, WHITE);

		EndDrawing(); // ready for next frame
	}

	// cleanup
	UnloadTexture(wabbit);
	CloseWindow();
	return 0;
}
