#include "game.h"

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
	const int default_window_width = 870;
	const int default_window_height = 600;
	const int min_window_width = 640;
	const int min_window_height = 480;

	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
	InitWindow(default_window_width, default_window_height, "Projectile game"); // Create the window and OpenGL context
	SetWindowMinSize(min_window_width, min_window_height);
}

void draw_ui(const Player& player, float level_time, const GameState game_state)
{
    int start_x = 10;
    int start_y = 10;
    int line_height = 25;
    int font_size = 20;

    // Формируем строки для отображения
    std::vector<std::string> lines;
    // lines.push_back("Projectile game");
    lines.push_back("Collisions: " + std::to_string(player.hit_count));
    lines.push_back("FPS: " + std::to_string(GetFPS()));
    lines.push_back("Screen res: (" + std::to_string(GetScreenWidth()) + ", " + std::to_string(GetScreenHeight()) + ")");
    lines.push_back("Player pos: (" + std::format("{:.1f}", player.pos.x) + ", " + std::format("{:.1f}", player.pos.y) + ")");
    lines.push_back("Level time: " + std::format("{:.3f}", level_time) + " s");
    lines.push_back("HP: " + std::format("{}", player.health));
    if (player.immortal) lines.push_back("Immortal");
    if (game_state == GameState::GAME_OVER) lines.push_back("Game over!");
    else if (game_state == GameState::LEVEL_COMPLETED) lines.push_back("Level completed!");

    // Отрисовка каждой строки
    for (size_t i = 0; i < lines.size(); ++i) {
        DrawText(lines[i].c_str(), start_x, start_y + i * line_height, font_size, WHITE);
    }
}

void render_scene(Texture& player_texture, const std::vector<Projectile>& projectiles, const Player& player, float level_time, const GameState game_state)
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
    Vector2 screen_pos = world_to_screen(player.pos, screen_width, screen_height);
    Rectangle src_rect = { 0, 0, (float)player_texture.width, (float)player_texture.height };
    Vector2 origin = {
        world_to_screen(player.width, screen_width, screen_height) / 2.0f,
        world_to_screen(player.height, screen_width, screen_height) / 2.0f
    };
    DrawTexturePro(player_texture, src_rect,
        (Rectangle){
            screen_pos.x, 
            screen_pos.y,
            world_to_screen(player.width, screen_width, screen_height),
            world_to_screen(player.height, screen_width, screen_height)
        },
        origin, 0.0f, WHITE
    );

    // Отрисовка хитбокса игрока
    Vector2 width_height_vector = (Vector2){
        world_to_screen(player.width, screen_width, screen_height),
        world_to_screen(player.height, screen_width, screen_height)
    };
    Vector2 hitbox_topleft = world_to_screen(player.pos, screen_width, screen_height) - width_height_vector / 2.0;
    DrawRectangleLinesEx(
        (Rectangle){
            hitbox_topleft.x, hitbox_topleft.y,
            width_height_vector.x, width_height_vector.y
        },
        2.0f, BLUE
    );

	draw_frame(screen_width, screen_height);

    // Интерфейс (многострочный текст слева)
    draw_ui(player, level_time, game_state);

    EndDrawing();
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
