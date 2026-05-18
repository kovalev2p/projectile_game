#pragma once

#include "raylib.h"
#include <raymath.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include <string>
#include <format>
#include <functional>

#define WORLD_SIZE 1024.0f

const float small_number = 1e-6;

extern bool debug_mode;

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
    WAY = 4,
};

extern const std::vector<std::string> level_names;

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

        bool hit(int damage);
};

using AttackAction = std::function<void(float dt, std::vector<Projectile>&, Player&)>;
// dt (time_offset) — разница между текущим временем и запланированным (чтобы выровнять расположение)

struct AttackEvent {
    float time;
    AttackAction action;
};

// ------------------------------------------------------------
// Функции игровой логики
void move_player_wasd(Player& player);
bool circle_rect_collision(Vector2 circle_center, float radius,
                           Vector2 rect_center, float rect_width, float rect_height);
void update_projectiles(std::vector<Projectile>& projectiles, Player& player, GameState& game_state, const float level_time);
void spawn_bullet_line(std::vector<Projectile>& projectiles, const Vector2 velocity, const float time_offset,
    const Vector2 line_start_pos, const Vector2 line_end_pos, const float step, const float bullet_radius);
void add_beam_events(std::vector<AttackEvent>& events, float start_time, float interval, int count,
                     Vector2 start_pos, Vector2 velocity, float radius);
void spawn_circular_burst(std::vector<Projectile>& projectiles, Vector2 center, float speed, float radius, int count);
void add_beam_burst(std::vector<AttackEvent>& events, float start_time, Vector2 center,
                    int beam_count, float beam_interval, int bullets_per_beam,
                    float beam_speed, float beam_radius);
void spawn_targeted_bullet(std::vector<Projectile>& projectiles, Vector2 start, Vector2 target, float speed, float radius);
void add_fan_attack(std::vector<AttackEvent>& events, float start_time, Vector2 origin_pos, std::vector<float> angles,
    float interval, int bullets_per_beam, float speed, float bullet_radius);
void sort_attack_events(std::vector<AttackEvent>& events);

void level_way_init(std::vector<AttackEvent>& events, Player& player);
void level_beginning_init(std::vector<AttackEvent>& events, Player& player);
bool level_test_hp(std::vector<Projectile>& projectiles, const float level_time, const bool reset_level, Player& player);
bool level_test(std::vector<Projectile>& projectiles, const float level_time, const bool reset_level, Player& player);
bool update_level(std::vector<Projectile>& projectiles, const float level_time,
    LevelId level_id, const bool reset_level, Player& player);

void reset_game_state(std::vector<Projectile>& projectiles, float& level_time, Player& player);
void update_player_height(Texture& player_texture, float& player_height, const float& player_width);
std::vector<std::string> get_visible_level_names();
std::vector<LevelId> get_visible_level_ids();
