#include "game.h"
#include "render.h"
#include "resource_dir.h"	// utility header for SearchAndSetResourceDir

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
