#pragma once

#include "raylib.h"
#include <vector>
#include <string>
#include "game.h" // для типов Projectile, Player, GameState и констант

// Функции отрисовки
void create_main_window();
Vector2 world_to_screen(Vector2 world_pos, int screen_w, int screen_h);
float world_to_screen(float world_dimension, int screen_w, int screen_h);
void draw_frame(int screen_w, int screen_h);
void draw_ui(const Player& player, float level_time, const GameState game_state, const LevelId& current_level);
void render_scene(Texture& player_texture, const std::vector<Projectile>& projectiles, const Player& player, float level_time, const GameState game_state, const LevelId& current_level);
void draw_menu(int selected_level, const std::vector<std::string>& visible_level_names);
