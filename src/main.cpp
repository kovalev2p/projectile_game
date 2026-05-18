#include "raylib.h"
#include "resource_dir.h"	// utility header for SearchAndSetResourceDir
#include <raymath.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include <string>
#include <format>
#include <functional>

#define WORLD_SIZE 1024.0f

const float small_number = 1e-6;

// Состояния игры
enum class GameState {
    MENU,
    PLAYING,
    GAME_OVER,
    LEVEL_COMPLETED
};

// Идентификаторы уровней
enum class LevelId {
    EMPTY = 0,
    TEST = 1,
    TEST_HP = 2,
    BEGINNING = 3,
};

const std::vector<std::string> level_names = {
    "Empty Level",
    "Test Level",
    "Test HP",
    "Beginning"
};

struct Projectile // снаярд
{
    Vector2 pos; // position
    Vector2 vel; // velocity
	float r; // hitbox radius
};

class Player
{
    public:
        Vector2 pos;
        int health;
        int hit_count;
        bool immortal;
        float speed; // пикселей в секунду
        float width;
        float height;
        float invincible_until; // кадры неуязвимости (по времени уровня)

        bool hit(int damage) {
            if (damage>0) hit_count += damage;
            if ((!immortal) || (damage<0)) health -= damage;
            return (!immortal) && (health <= 0);
        }
};

using AttackAction = std::function<void(float dt, std::vector<Projectile>&, Player&)>;
// dt (time_offset) — разница между текущим временем и запланированным (чтобы выровнять расположение)

struct AttackEvent {
    float time;
    AttackAction action;
};

// ------------------------------------------------------------

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
	InitWindow(default_window_width, default_window_height, "Hello Raylib"); // Create the window and OpenGL context
	SetWindowMinSize(min_window_width, min_window_height);
}

void move_player_wasd(Player& player)
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
	player.pos.x += move.x * player.speed * dt;
	player.pos.y += move.y * player.speed * dt;

	// Ограничение границами мира
	if (player.pos.x < 0) player.pos.x = 0;
	if (player.pos.x > WORLD_SIZE) player.pos.x = WORLD_SIZE;
	if (player.pos.y < 0) player.pos.y = 0;
	if (player.pos.y > WORLD_SIZE) player.pos.y = WORLD_SIZE;
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

void update_projectiles(std::vector<Projectile>& projectiles, Player& player, GameState& game_state, const float level_time)
// Обновление пуль (физика): движение, удаление за границами, проверка столкновений с игроком
{
    float dt = GetFrameTime();
    int hit_count = 0;

    for (auto& p : projectiles) {
        // движение
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;

        // столкновение с игроком (круглая пуля, прямоугольный хитбокс игрока)
        if (circle_rect_collision(p.pos, p.r, player.pos, player.width, player.height))
		{
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
    
    bool dead = false;
    if (hit_count > 0) {
        dead = player.hit(1);
        player.invincible_until = level_time + 1.0f;
    }
    if (dead) game_state = GameState::GAME_OVER;
}

void spawn_bullet_line(
    std::vector<Projectile>& projectiles, const Vector2 velocity, const float time_offset,
    const Vector2 line_start_pos, const Vector2 line_end_pos, const float step, const float bullet_radius
){
    float line_length_x = line_end_pos.x - line_start_pos.x;
    float line_length_y = line_end_pos.y - line_start_pos.y;
    float line_length = sqrt(line_length_x*line_length_x + line_length_y*line_length_y);
    for (float position = 0; position <= line_length; position += step) {
        Projectile bullet;
        bullet.vel = velocity;
        bullet.pos = line_start_pos + (position / line_length) * (line_end_pos - line_start_pos) + velocity * time_offset;
        bullet.r = bullet_radius;
        projectiles.push_back(bullet);
    }
}

// Создаёт луч (вереницу пуль) из одной точки
// interval — интервал между пулями (в секундах)
void add_beam_events(std::vector<AttackEvent>& events, float start_time, float interval, int count,
                     Vector2 start_pos, Vector2 velocity, float radius)
{
    for (int i = 0; i < count; ++i) {
        float event_time = start_time + i * interval;
        events.push_back({event_time, [=](float dt, std::vector<Projectile>& p, Player&) {
            Projectile bullet;
            bullet.pos = start_pos + velocity * dt;
            bullet.vel = velocity;
            bullet.r = radius;
            p.push_back(bullet);
        }});
    }
}

// Создаёт круговой взрыв (N пуль из центра)
void spawn_circular_burst(std::vector<Projectile>& projectiles, Vector2 center, float speed, float radius, int count)
{
    float angle_step = 2 * PI / count;
    for (int i = 0; i < count; ++i) {
        float angle = (-PI/2) + i * angle_step;
        Vector2 vel = { speed * cosf(angle), speed * sinf(angle) };
        Projectile p;
        p.pos = center;
        p.vel = vel;
        p.r = radius;
        projectiles.push_back(p);
    }
}

// Взрыв лучами: из центра вылетает N лучей (каждый луч – вереница пуль)
void add_beam_burst(
    std::vector<AttackEvent>& events, float start_time, Vector2 center, 
    int beam_count, float beam_interval, int bullets_per_beam,
    float beam_speed, float beam_radius
)
{
    float angle_step = 2 * PI / beam_count;
    for (int i = 0; i < beam_count; ++i) {
        float angle = (-PI/2) + i * angle_step;
        Vector2 velocity = { beam_speed * cosf(angle), beam_speed * sinf(angle) };
        // Каждый луч – это серия событий
        add_beam_events(
            events, start_time, beam_interval, bullets_per_beam,
            center, velocity, beam_radius
        );
    }
}

void spawn_targeted_bullet(std::vector<Projectile>& projectiles, Vector2 start, Vector2 target, float speed, float radius)
{
    Vector2 dir = Vector2Subtract(target, start);
    dir = Vector2Normalize(dir);
    Vector2 vel = { dir.x * speed, dir.y * speed };
    Projectile p;
    p.pos = start;
    p.vel = vel;
    p.r = radius;
    projectiles.push_back(p);
}

void add_fan_attack(
    std::vector<AttackEvent>& events, float start_time, Vector2 origin_pos, std::vector<float> angles,
    float interval, int bullets_per_beam, float speed, float bullet_radius
)
{
    for (float angle_deg : angles) {
        float rad = angle_deg * PI / 180.0f;
        Vector2 vel = { speed * sinf(rad), speed * cosf(rad) };
        add_beam_events(events, start_time, interval, bullets_per_beam,
                        origin_pos, vel, bullet_radius);
    }
}

void sort_attack_events(std::vector<AttackEvent>& events) {
    // sorts by time in ascending order
    std::sort(events.begin(), events.end(), [](AttackEvent event1, AttackEvent event2){
        return event1.time < event2.time;
    });
}

void level_beginning_init(std::vector<AttackEvent>& events, Player& player)
{
    player.health = 3;
    player.immortal = false;

    // Атака 1: две плотные линии
    events.push_back({0.0f, [](float dt, std::vector<Projectile>& p, Player&) {
        spawn_bullet_line(p, {0, 180}, dt, {WORLD_SIZE/2.0f, 0}, {WORLD_SIZE, 0}, 10*2+5, 10);
    }});
    events.push_back({2.5f, [](float dt, std::vector<Projectile>& p, Player&) {
        spawn_bullet_line(p, {0, 180}, dt, {0, 0}, {WORLD_SIZE/2.0f, 0}, 10*2+5, 10);
    }});

    // TODO: увеличить время между атаками (не только этими)

    // Атака 2: широкая линия с щелями
    events.push_back({4.7f, [](float dt, std::vector<Projectile>& p, Player&) {
        spawn_bullet_line(p, {0, 180}, dt, {0, 0}, {WORLD_SIZE, 0}, 140, 10);
    }});

    // Атака 3: большая бомба с взрывом в центре
    {
        float bomb_drop_time = 10.0f;
        float bomb_vel_y = 150.0f;
        float bomb_radius = 20.0f;
        Vector2 bomb_start = { WORLD_SIZE/2.0f, 0 };
        float distance_to_center = WORLD_SIZE/2.0f; // 512
        float travel_time = distance_to_center / bomb_vel_y; // ≈ 3.413
        float explosion_time = bomb_drop_time + travel_time;

        // Создаём бомбу
        events.push_back({bomb_drop_time, [=](float dt, std::vector<Projectile>& p, Player&) {
            Projectile bomb;
            bomb.pos = bomb_start;
            bomb.vel = { 0, bomb_vel_y };
            bomb.r = bomb_radius;
            p.push_back(bomb);
        }});

        // Запланируем взрыв: ищем бомбу в центре и заменяем её круговой волной
        events.push_back({explosion_time, [=](float dt, std::vector<Projectile>& p, Player&) {
            // Ищем бомбу с большим радиусом
            for (auto it = p.begin(); it != p.end(); ++it) {
                if (it->r == bomb_radius) {
                    // проверка на центр (не обязательно):
                    // && std::abs(it->pos.x - WORLD_SIZE/2.0f) < 10.0f && std::abs(it->pos.y - WORLD_SIZE/2.0f) < 10.0f
                    p.erase(it);
                    break;
                }
            }
            // Круговая волна из 12 пуль
            spawn_circular_burst(p, { WORLD_SIZE/2.0f, WORLD_SIZE/2.0f }, 180.0f, 10, 12);
        }});
    }

    // Атака 4: три луча сверху (веер после взрыва)
    {
        float start_beam_trio = 16.0f;
        float beam_interval = 0.2f;
        int bullets_per_beam = 20;
        float radius = 10;
        
        add_fan_attack(events, start_beam_trio, {WORLD_SIZE/2, 0}, {-45, 0, 45}, beam_interval, bullets_per_beam, 200, radius);
    }

    // Атака 5: горизонтальные встречные лучи
    {
        float start_horizontal = 20.0f;
        float interval = 0.2f;
        int count = 40;
        float radius = 10;
        // Луч слева направо (верхняя треть)
        add_beam_events(events, start_horizontal, interval, count,
                        {0, WORLD_SIZE/3}, {200, 0}, radius);
        // Луч справа налево (нижняя треть)
        add_beam_events(events, start_horizontal, interval, count,
                        {WORLD_SIZE, WORLD_SIZE*2/3}, {-200, 0}, radius);
    }

    // Атака 6: большая прицельная пуля
    events.push_back({22.0f, [](float dt, std::vector<Projectile>& p, Player& pl) {
        // Случайная X от 100 до 924
        float x = 100 + static_cast<float>(rand()) / RAND_MAX * (WORLD_SIZE - 200);
        spawn_targeted_bullet(p, {x, 0}, pl.pos, 150.0f, 20);
    }});

    // Атака 7: веер из пяти лучей
    {
        float start_fan = 30.0f;
        float interval = 0.2f;
        int bullets_per_beam = 15;
        float speed = 200;
        float radius = 10;
        Vector2 center_top = { WORLD_SIZE/2.0f, 0 };
        std::vector<float> angles = {-40, -20, 0, 20, 40};

        add_fan_attack(events, start_fan, center_top, angles, interval, bullets_per_beam, speed, radius);
    }

    // Атака 8: вторая бомба с взрывом лучами
    {
        float bomb_drop_time = 37.0f;
        float bomb_vel_y = 150.0f;
        float bomb_radius = 20.0f;
        Vector2 bomb_start = { WORLD_SIZE/2.0f, 0 };
        float travel_time = (WORLD_SIZE/2.0f) / bomb_vel_y;
        float explosion_time = bomb_drop_time + travel_time;

        // Создаём бомбу
        events.push_back({bomb_drop_time, [=](float dt, std::vector<Projectile>& p, Player&) {
            Projectile bomb;
            bomb.pos = bomb_start;
            bomb.vel = { 0, bomb_vel_y };
            bomb.r = bomb_radius;
            p.push_back(bomb);
        }});

        // Взрыв: удаляем бомбу и добавляем burst лучей
        events.push_back({explosion_time, [&events, bomb_radius, explosion_time](float dt, std::vector<Projectile>& p, Player&) {
            // Удаляем бомбу
            for (auto it = p.begin(); it != p.end(); ++it) {
                if (it->r == bomb_radius &&
                    std::abs(it->pos.x - WORLD_SIZE/2.0f) < 10.0f &&
                    std::abs(it->pos.y - WORLD_SIZE/2.0f) < 10.0f
                )
                {
                    p.erase(it);
                    break;
                }
            }
            // Взрыв лучами
            int bullets_per_beam = 20;
            add_beam_burst(events, explosion_time, { WORLD_SIZE/2.0f, WORLD_SIZE/2.0f },
                        8, 0.2f, bullets_per_beam, 180.0f, 10);
            
            // поскольку создаются новые события, нужно снова отсортировать
            sort_attack_events(events);
        }});
    }

    sort_attack_events(events);
}

bool level_test_hp(std::vector<Projectile>& projectiles, const float level_time, const bool reset_level, Player& player)
{
    // Переменные уровня
    static float last_spawn_time;
    if (reset_level) {
        last_spawn_time = -100.0f;
        player.health = 3;
        player.immortal = false;
    }

    const float spawn_interval = 1.0f;

    if (level_time - last_spawn_time >= spawn_interval) {
        last_spawn_time = level_time;

        Projectile bullet;
        bullet.pos = { 700.0f, 700.0f };
        bullet.vel = { 0.0f, -200.0f };
		bullet.r = 10;
        projectiles.push_back(bullet);
    }

    return false;
}

bool level_test(std::vector<Projectile>& projectiles, const float level_time, const bool reset_level, Player& player)
// Логика тестового уровня
{
    // Переменные уровня
    static float last_spawn_time;
    if (reset_level) {
        last_spawn_time = -100.0f;
        player.health = 3;
        player.immortal = true;
    }

    const float spawn_interval = 1.5f;
	// const float small_number = 1e-6f;

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

    return false;
}

// Диспетчеризация уровней
// returns true if level completed
bool update_level(
    std::vector<Projectile>& projectiles, const float level_time,
    LevelId level_id, const bool reset_level, Player& player
)
{
    // Статические данные для event-based уровней
    static std::vector<AttackEvent> events;
    static size_t next_event = 0;
    static bool event_level_completed = false;

    bool is_event_level = (level_id == LevelId::BEGINNING);

    if (is_event_level)
    {
        if (reset_level) {
            // Очистка событий, инициализация уровня
            events.clear();
            next_event = 0;
            event_level_completed = false;
            // Вызываем инициализацию конкретного уровня
            if (level_id == LevelId::BEGINNING) {
                level_beginning_init(events, player);
            }
        }

        // Выполняем все события, время которых наступило
        while (next_event < events.size() && level_time >= events[next_event].time) {
            float dt = level_time - events[next_event].time; // разница между текущим временем и запланированным (чтобы выровнять расположение)
            events[next_event].action(dt, projectiles, player);
            next_event++;
        }

        // Проверка завершения уровня
        if (!event_level_completed && next_event == events.size() && projectiles.empty()) {
            event_level_completed = true;
            return true;
        }
        return false;
    }
    else
    {
        if (level_id == LevelId::TEST) {
            return level_test(projectiles, level_time, reset_level, player);
        }
        // LevelId::EMPTY — пустой уровень, снаярды не нужно создавать
        else if (level_id == LevelId::TEST_HP) {
            return level_test_hp(projectiles, level_time, reset_level, player);
        }
        return false;
    }
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

void reset_game_state(std::vector<Projectile>& projectiles, float& level_time, Player& player)
{
    projectiles.clear();
    player.hit_count = 0;
    level_time = 0.0f;
    player.pos = { WORLD_SIZE / 2.0f, WORLD_SIZE / 2.0f };
    player.immortal = false;
    player.invincible_until = 0;
}

int main ()
{
	create_main_window();
	
	SearchAndSetResourceDir("resources");
	Texture player_texture = LoadTexture("wabbit_alpha.png");
	
    Player player;
    player.pos = { WORLD_SIZE / 2.0f, WORLD_SIZE / 2.0f }; // Позиция игрока в игровых координатах (центр мира)
	player.speed = 300.0f; // пикселей в секунду
	player.width = 60.0f;
	player.height = player.width; // updated proportionally to sprite
	update_player_height(player_texture, player.height, player.width);
	player.health = 3; // init
	player.immortal = false;
    player.invincible_until = 0;
	player.hit_count = 0;

	// Система снарядов
	std::vector<Projectile> projectiles;
	float level_time = 0.0f;

	// Меню и состояния
	GameState state = GameState::MENU;
	int selected_level = 0;
	LevelId current_level = LevelId::EMPTY; // временное значение, будет перезаписано при старте
    bool reset_level = true;

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
				reset_game_state(projectiles, level_time, player);
				state = GameState::PLAYING;
                reset_level = true;
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
            if (dt > 0.5) dt = 0.5; // allows to calc time more properly while game paused (for debug and to prevent cheating)
			level_time += dt;
            
			move_player_wasd(player);
            
            // Генерация новых пуль по уровню
            // и установка параметров уровня (стартовое HP)
			bool completed = update_level(projectiles, level_time, current_level, reset_level, player);
            reset_level = false;

            // Обновление пуль (hit_count обновляется внутри)
			update_projectiles(projectiles, player, state, level_time);
            
            if (completed)
            {
                state = GameState::LEVEL_COMPLETED;
            } 
			else if (IsKeyPressed(KEY_ESCAPE))
            {
				state = GameState::MENU;
				// projectiles и hit_count сбросятся при следующем запуске уровня
			}

			render_scene(player_texture, projectiles, player, level_time, state);
		}
        else if (state == GameState::GAME_OVER || state == GameState::LEVEL_COMPLETED)
        {
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::MENU;
            render_scene(player_texture, projectiles, player, level_time, state);
        }
	}

	// cleanup
	UnloadTexture(player_texture);
	CloseWindow();
	return 0;
}
