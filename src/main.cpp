#include "raylib.h"
#include "resource_dir.h"	// utility header for SearchAndSetResourceDir
#include <raymath.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include <string>
#include <format>

#define WORLD_SIZE 1024

// Состояния игры
enum class GameState {
    MENU,
    PLAYING
};

// Идентификаторы уровней
enum class LevelId {
    EMPTY = 0,
    TEST = 1
};

const std::vector<std::string> level_names = {
    "Empty Level",
    "Test Level"
};

struct Projectile // снаярд
{
    Vector2 pos; // position
    Vector2 vel; // velocity
	float r; // hitbox radius
};

Vector2 world_to_screen(Vector2 world_pos, int screen_w, int screen_h)
// Преобразование игровых координат в экранные с сохранением пропорций
{
    float scale_x = (float)screen_w / WORLD_SIZE;
    float scale_y = (float)screen_h / WORLD_SIZE;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    float view_w = WORLD_SIZE * scale;
    float view_h = WORLD_SIZE * scale;
    float offset_x = (screen_w - view_w); // allign to end
    float offset_y = (screen_h - view_h);

    return (Vector2){ offset_x + world_pos.x * scale, offset_y + world_pos.y * scale };
}

float world_to_screen(float world_dimension, int screen_w, int screen_h)
{
	float scale_x = (float)screen_w / WORLD_SIZE;
    float scale_y = (float)screen_h / WORLD_SIZE;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    float view_w = WORLD_SIZE * scale;
    float view_h = WORLD_SIZE * scale;

	return (float)(world_dimension * scale);
}

void draw_frame(int screen_w, int screen_h)
// Отрисовка рамки вокруг игровой области
{
    // Вычисляем размеры игровой области (как в world_to_screen)
    float scale_x = (float)screen_w / WORLD_SIZE;
    float scale_y = (float)screen_h / WORLD_SIZE;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    float view_w = WORLD_SIZE * scale;
    float view_h = WORLD_SIZE * scale;
    float offset_x = (screen_w - view_w); // allign to end
    float offset_y = (screen_h - view_h);
    DrawRectangleLines(offset_x, offset_y, view_w, view_h, WHITE);
}

void create_main_window()
{
	const int default_window_width = 850;
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

bool circle_rect_collision(Vector2 circle_center, float radius,
                           Vector2 rect_center, float rect_width, float rect_height)
{
	// adapted from: https://stackoverflow.com/a/402010

    // Находим вектор от центра прямоугольника до центра круга
    float dx = std::abs(circle_center.x - rect_center.x);
    float dy = std::abs(circle_center.y - rect_center.y);

    // Половина ширины и высоты прямоугольника
    float half_w = rect_width * 0.5f;
    float half_h = rect_height * 0.5f;

    // Если круг слишком далеко по горизонтали или вертикали, пересечения нет
    if (dx > half_w + radius) return false;
    if (dy > half_h + radius) return false;

    // Если центр круга достаточно близко к любой из осей, пересечение гарантировано
    if (dx <= half_w) return true;
    if (dy <= half_h) return true;

    // Проверка на попадание в угол
    float dx_corner = dx - half_w;
    float dy_corner = dy - half_h;
    float corner_dist_sq = dx_corner * dx_corner + dy_corner * dy_corner;
    return corner_dist_sq <= (radius * radius);
}

int update_projectiles(std::vector<Projectile>& projectiles, const Vector2& player_pos, const float& player_width, const float& player_height)
// Обновление пуль (физика): движение, удаление за границами, проверка столкновений с игроком
// returns: hit_count (per this frame)
{
    float dt = GetFrameTime();
    int hit_count = 0;

    for (auto& p : projectiles) {
        // движение
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;

        // столкновение с игроком (круглая пуля, прямоугольный хитбокс игрока)
        if (
			circle_rect_collision(p.pos, p.r, player_pos, player_width, player_height)
		){
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

void level_test(std::vector<Projectile>& projectiles, const float level_time)
// Логика тестового уровня
{
    static float last_spawn_time = -100.0f;
    const float spawn_interval = 1.5f;
	const float small_number = 1e-6f;

    // каждые spawn_interval секунды создаём пули
    if (level_time - last_spawn_time >= spawn_interval) {
        last_spawn_time = level_time;

        Projectile bullet;
        bullet.pos = { 700.0f, 700.0f };
        bullet.vel = { 0.0f, -200.0f }; // вверх
		bullet.r = 5;
        projectiles.push_back(bullet);

		Projectile bullet_2;
        bullet_2.pos = { 300.0f, 700.0f };
        bullet_2.vel = { 0.0f, -200.0f }; // вверх
		bullet_2.r = 10;
        projectiles.push_back(bullet_2);

		// статическая пуля для проверки хитбоксов
		bool has_static = false;
		for (auto& p : projectiles) {
			if (fabs(p.vel.x) < small_number && fabs(p.vel.y) < small_number) {
				has_static = true;
				break;
			}
		}
		if (!has_static) {
			Projectile bullet_static;
			bullet_static.pos = { 600.0f, 700.0f };
			bullet_static.vel = { 0.0f, 0.0f };
			bullet_static.r = 20;
			projectiles.push_back(bullet_static);
		}
    }
}

void update_level(std::vector<Projectile>& projectiles, const float level_time, LevelId level_id)
// Диспетчеризация уровней
{
    if (level_id == LevelId::TEST) {
        level_test(projectiles, level_time);
    }
    // LevelId::EMPTY — пустой уровень, снаярды не нужно создавать
}

void draw_ui(int hit_count, int fps, Vector2 player_pos, float level_time)
{
    int start_x = 10;
    int start_y = 10;
    int line_height = 25;
    int font_size = 20;

    // Формируем строки для отображения
    std::vector<std::string> lines;
    // lines.push_back("Projectile game");
    lines.push_back("Collisions: " + std::to_string(hit_count));
    lines.push_back("FPS: " + std::to_string(fps));
    lines.push_back("Screen res: (" + std::to_string(GetScreenWidth()) + ", " + std::to_string(GetScreenHeight()) + ")");
    lines.push_back("Player pos: (" + std::to_string((int)player_pos.x) + ", " + std::to_string((int)player_pos.y) + ")");
    lines.push_back("Level time: " + std::format("{:.3f}", level_time) + " s");

    // Отрисовка каждой строки
    for (size_t i = 0; i < lines.size(); ++i) {
        DrawText(lines[i].c_str(), start_x, start_y + i * line_height, font_size, WHITE);
    }
}

void render_scene(
    Texture& player_texture, const int& hit_count, const std::vector<Projectile>& projectiles,
    const Vector2& player_pos, const float& level_time, const float& player_width, const float& player_height
)
{
    BeginDrawing();
    ClearBackground(BLACK);

    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    // Отрисовка снарядов
    for (const auto& p : projectiles) {
        Vector2 screen_pos = world_to_screen(p.pos, screen_width, screen_height);
        float screen_radius = world_to_screen(p.r, screen_width, screen_height);
        DrawCircleV(screen_pos, screen_radius, RED);
    }

    // Отрисовка спрайта игрока
    Vector2 screen_pos = world_to_screen(player_pos, screen_width, screen_height);
    Rectangle src_rect = { 0, 0, (float)player_texture.width, (float)player_texture.height };
    Vector2 origin = {
        world_to_screen(player_width, screen_width, screen_height) / 2.0f,
        world_to_screen(player_height, screen_width, screen_height) / 2.0f
    };
    DrawTexturePro(player_texture, src_rect,
        (Rectangle){
            screen_pos.x, 
            screen_pos.y,
            world_to_screen(player_width, screen_width, screen_height),
            world_to_screen(player_height, screen_width, screen_height)
        },
        origin, 0.0f, WHITE
    );

    // Отрисовка хитбокса игрока
    Vector2 width_height_vector = (Vector2){
        world_to_screen(player_width, screen_width, screen_height),
        world_to_screen(player_height, screen_width, screen_height)
    };
    Vector2 hitbox_topleft = world_to_screen(player_pos, screen_width, screen_height) - width_height_vector / 2.0;
    DrawRectangleLinesEx(
        (Rectangle){
            hitbox_topleft.x, hitbox_topleft.y,
            width_height_vector.x, width_height_vector.y
        },
        2.0f, BLUE
    );

	draw_frame(screen_width, screen_height);

    // Интерфейс (многострочный текст слева)
    draw_ui(hit_count, GetFPS(), player_pos, level_time);

    EndDrawing();
}

void update_player_height(Texture& player_texture, float& player_height, const float& player_width)
{
	player_height = (float)player_width * ((float)(player_texture.height) / (float)(player_texture.width));
}

void draw_menu(int selected_level)
{
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();
    int start_x = screen_width - 400;
    int start_y = screen_height / 2 - (level_names.size() * 30) / 2;
    int line_height = 35;
    int font_size = 25;

    DrawText("SELECT LEVEL", start_x, start_y - 40, font_size, YELLOW);
    for (size_t i = 0; i < level_names.size(); ++i) {
        std::string prefix = (i == (size_t)selected_level) ? "> " : "  ";
        std::string line = prefix + level_names[i];
        Color color = (i == (size_t)selected_level) ? GREEN : WHITE;
        DrawText(line.c_str(), start_x, start_y + i * line_height, font_size, color);
    }
}

void reset_game_state(std::vector<Projectile>& projectiles, int& hit_count, float& level_time, Vector2& player_pos)
{
    projectiles.clear();
    hit_count = 0;
    level_time = 0.0f;
    player_pos = { WORLD_SIZE / 2.0f, WORLD_SIZE / 2.0f };
}

int main ()
{
	create_main_window();
	
	SearchAndSetResourceDir("resources");
	Texture player_texture = LoadTexture("wabbit_alpha.png");
	
	// Позиция игрока в игровых координатах (центр мира)
	Vector2 player_pos = { WORLD_SIZE / 2.0f, WORLD_SIZE / 2.0f };
	float player_speed = 300.0f; // пикселей в секунду
	float player_width = 60.0f;
	float player_height = player_width; // updated proportionally to sprite
	update_player_height(player_texture, player_height, player_width);

	// Система снарядов
	std::vector<Projectile> projectiles;
	int hit_count = 0;
	float level_time = 0.0f;

	// Меню и состояния
	GameState state = GameState::MENU;
	int selected_level = 0;
	LevelId current_level = LevelId::EMPTY; // временное значение, будет перезаписано при старте

    SetExitKey(KEY_NULL); // don't close by ESC
	// game loop
	while (!WindowShouldClose())
	{
		if (state == GameState::MENU)
		{
			// Управление в меню
			if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
				selected_level = (selected_level - 1 + level_names.size()) % level_names.size();
			if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
				selected_level = (selected_level + 1) % level_names.size();
			if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE))
			{
				current_level = static_cast<LevelId>(selected_level);
				reset_game_state(projectiles, hit_count, level_time, player_pos);
				state = GameState::PLAYING;
			}
			if (IsKeyPressed(KEY_ESCAPE))
			{
				break; // close window
			}

			// Отрисовка меню
			BeginDrawing();
			ClearBackground(BLACK);
			draw_frame(GetScreenWidth(), GetScreenHeight());
			// рисуем только FPS в левом верхнем углу
			DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 20, WHITE);
			draw_menu(selected_level);
			EndDrawing();
		}
		else if (state == GameState::PLAYING)
		{
			float dt = GetFrameTime();
			level_time += dt;
		
			move_player_wasd(player_pos, player_speed);
            
            // Обновление пуль и получение количества попаданий
			int hits = update_projectiles(projectiles, player_pos, player_width, player_height);
			hit_count += hits;
            
            // Генерация новых пуль по уровню
			update_level(projectiles, level_time, current_level);

			if (IsKeyPressed(KEY_ESCAPE))
			{
				state = GameState::MENU;
				// projectiles и hit_count сбросятся при следующем запуске уровня
			}

			render_scene(player_texture, hit_count, projectiles, player_pos, level_time, player_width, player_height);
		}
	}

	// cleanup
	UnloadTexture(player_texture);
	CloseWindow();
	return 0;
}
