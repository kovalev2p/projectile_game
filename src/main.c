#include "raylib.h"
#include "resource_dir.h"	// utility header for SearchAndSetResourceDir
#include <raymath.h>

#define WORLD_SIZE 1024

const int default_window_width = 800;
const int default_window_height = 600;
const int min_window_width = 640;
const int min_window_height = 480;

Vector2 world_to_screen(Vector2 world_pos, int screen_w, int screen_h)
// Преобразование игровых координат в экранные с сохранением пропорций
{
    float scale_x = (float)screen_w / WORLD_SIZE;
    float scale_y = (float)screen_h / WORLD_SIZE;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    float view_w = WORLD_SIZE * scale;
    float view_h = WORLD_SIZE * scale;
    float offset_x = (screen_w - view_w) * 0.5f;
    float offset_y = (screen_h - view_h) * 0.5f;
    return (Vector2){ offset_x + world_pos.x * scale, offset_y + world_pos.y * scale };
}

int main ()
{
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
	InitWindow(default_window_width, default_window_height, "Hello Raylib"); // Create the window and OpenGL context
	SetWindowMinSize(min_window_width, min_window_height);
	SearchAndSetResourceDir("resources");

	Texture wabbit = LoadTexture("wabbit_alpha.png");
	
	// Позиция игрока в игровых координатах (центр мира)
	Vector2 player_pos = { WORLD_SIZE / 2.0f, WORLD_SIZE / 2.0f };
	float player_speed = 300.0f; // пикселей в секунду

	// game loop
	while (!WindowShouldClose()) // run the loop until the user presses ESCAPE or presses the Close button on the window
	{
		// Время с прошлого кадра
		float dt = GetFrameTime();

		// Управление WASD
		Vector2 move = { 0, 0 };
		if (IsKeyDown(KEY_W)) move.y -= 1;
		if (IsKeyDown(KEY_S)) move.y += 1;
		if (IsKeyDown(KEY_A)) move.x -= 1;
		if (IsKeyDown(KEY_D)) move.x += 1;
		if (move.x != 0 || move.y != 0) move = Vector2Normalize(move); // нормализация скорости

		// Обновление позиции
		player_pos.x += move.x * player_speed * dt;
		player_pos.y += move.y * player_speed * dt;

		// Ограничение границами мира
		if (player_pos.x < 0) player_pos.x = 0;
		if (player_pos.x > WORLD_SIZE) player_pos.x = WORLD_SIZE;
		if (player_pos.y < 0) player_pos.y = 0;
		if (player_pos.y > WORLD_SIZE) player_pos.y = WORLD_SIZE;

		BeginDrawing();
		ClearBackground(BLACK);

		DrawText("Hello Raylib", 200,200,20,WHITE);
		
		// Отрисовка спрайта игрока
		Vector2 screen_pos = world_to_screen(player_pos, GetScreenWidth(), GetScreenHeight());
		Rectangle src_rect = { 0, 0, (float)wabbit.width, (float)wabbit.height };
		Vector2 origin = { (float)wabbit.width / 2, (float)wabbit.height / 2 };
		DrawTexturePro(wabbit, src_rect, 
			(Rectangle){ screen_pos.x, screen_pos.y, (float)wabbit.width, (float)wabbit.height },
			origin, 0.0f, WHITE);

		// Отладочная информация (FPS и позиция)
		DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 20, WHITE);
		DrawText(TextFormat("Pos: (%.1f, %.1f)", player_pos.x, player_pos.y), 10, 35, 20, WHITE);

		EndDrawing(); // ready for next frame
	}

	// cleanup
	UnloadTexture(wabbit);
	CloseWindow();
	return 0;
}
