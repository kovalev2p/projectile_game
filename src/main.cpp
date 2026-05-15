#include "raylib.h"
#include "resource_dir.h"	// utility header for SearchAndSetResourceDir
#include <raymath.h>
#include <algorithm>
#include <vector>

#define WORLD_SIZE 1024

struct Projectile // снаярд
{
    Vector2 pos; // position
    Vector2 vel; // velocity
};

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

void create_main_window()
{
	const int default_window_width = 800;
	const int default_window_height = 600;
	const int min_window_width = 640;
	const int min_window_height = 480;

	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
	InitWindow(default_window_width, default_window_height, "Hello Raylib"); // Create the window and OpenGL context
	SetWindowMinSize(min_window_width, min_window_height);
}

void move_player_wasd(Vector2& player_pos, float& player_speed)
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
}

int update_projectiles(std::vector<Projectile>& projectiles, const Vector2& player_pos, const float player_radius)
// Обновление пуль (физика): движение, удаление за границами, проверка столкновений с игроком
// returns: hit_count (per this frame)
{
    float dt = GetFrameTime();
    int hit_count = 0;

    for (auto& p : projectiles) {
        // движение
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;

        // столкновение с игроком (точечная пуля, круговой хитбокс игрока)
        float dx = p.pos.x - player_pos.x;
        float dy = p.pos.y - player_pos.y;
        if (dx*dx + dy*dy < player_radius * player_radius) {
            hit_count++;
            p.pos.x = -2*WORLD_SIZE; // помечаем как "мёртвую", будет удалена при очистке
        }
    }

    // удаляем пули, вышедшие за пределы мира
    auto iter = std::remove_if(projectiles.begin(), projectiles.end(),
        [](const Projectile& p) {
            return p.pos.x < 0 || p.pos.x > WORLD_SIZE ||
                   p.pos.y < 0 || p.pos.y > WORLD_SIZE;
        });
    projectiles.erase(iter, projectiles.end());

    return hit_count;
}


void update_level(std::vector<Projectile>& projectiles, const float level_time)
// Уровень: генерирует пули в соответсвии с паттерном (по расписанию)
{
    static float last_spawn_time = -100.0f;
    const float spawn_interval = 3.0f;

    // каждые 3 секунды создаём пулю в (700, 512), летящую вверх (скорость -200 по Y)
    if (level_time - last_spawn_time >= spawn_interval) {
        last_spawn_time = level_time;
        Projectile bullet;
        bullet.pos = { 700.0f, 512.0f };
        bullet.vel = { 0.0f, -200.0f }; // вверх
        projectiles.push_back(bullet);
    }
}

int main ()
{
	create_main_window();
	
	SearchAndSetResourceDir("resources");
	Texture wabbit = LoadTexture("wabbit_alpha.png");
	
	// Позиция игрока в игровых координатах (центр мира)
	Vector2 player_pos = { WORLD_SIZE / 2.0f, WORLD_SIZE / 2.0f };
	float player_speed = 300.0f; // пикселей в секунду

	// Система снарядов
	std::vector<Projectile> projectiles;
	int hit_count = 0;
	float level_time = 0.0f;
	
	// Радиус игрока для коллизий (половина ширины спрайта)
	float player_radius = (float)wabbit.width / 2.0f;

	// game loop
	while (!WindowShouldClose()) // run the loop until the user presses ESCAPE or presses the Close button on the window
	{
		float dt = GetFrameTime();
		level_time += dt;
		
		move_player_wasd(player_pos, player_speed);
		
		// Обновление пуль и получение количества попаданий
		int hits = update_projectiles(projectiles, player_pos, player_radius);
		hit_count += hits;
		
		// Генерация новых пуль по уровню
		update_level(projectiles, level_time);

		BeginDrawing();
		ClearBackground(BLACK);
		
		DrawText(TextFormat("Collisions: %d", hit_count), 200, 200, 20, WHITE); // счётчик столкновений
		
		// Отрисовка снарядов (красные круги)
		for (const auto& p : projectiles) {
			Vector2 screen_pos = world_to_screen(p.pos, GetScreenWidth(), GetScreenHeight());
			DrawCircleV(screen_pos, 5.0f, RED);
		}
		
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
		DrawText(TextFormat("Time: %.3f", level_time), 10, 55, 20, WHITE);

		EndDrawing(); // ready for next frame
	}

	// cleanup
	UnloadTexture(wabbit);
	CloseWindow();
	return 0;
}
