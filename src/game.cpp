#include "game.h"
#include <cstdlib> // для rand()

const std::vector<std::string> level_names = {
    "Empty Level",
    "Test Level",
    "Test HP",
    "Beginning",
    "Way",
};

bool Player::hit(int damage) {
    if (damage>0) hit_count += damage;
    if ((!immortal) || (damage<0)) health -= damage;
    return (!immortal) && (health <= 0);
}

void move_player_wasd(Player& player)
{
	// Время с прошлого кадра
	float dt = GetFrameTime();

	// Управление WASD
	Vector2 move = { 0, 0 };
	if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) move.y -= 1;
	if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) move.y += 1;
	if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) move.x -= 1;
	if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) move.x += 1;
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
    if (hit_count > 0 && player.invincible_until < level_time) {
        dead = player.hit(1);
        player.invincible_until = level_time + 0.5f;
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

void level_way_init(std::vector<AttackEvent>& events, Player& player)
{
    player.health = 3;
    player.immortal = false;

    // 1. Два расходящихся луча из центра верха
    {
        float start_time = 0.0f;
        float interval = 0.12f;
        int count = 12;
        float radius = 10.0f;

        add_beam_events(events, start_time, interval, count,
                        { WORLD_SIZE / 2.0f, 0 }, { -120.0f, 150.0f }, radius);
        add_beam_events(events, start_time, interval, count,
                        { WORLD_SIZE / 2.0f, 0 }, { 120.0f, 150.0f }, radius);
    }

    // 2. Широкая линия сверху
    events.push_back({3.0f, [](float dt, std::vector<Projectile>& p, Player&) {
        spawn_bullet_line(p, {0, 250}, dt, {0, 0}, {WORLD_SIZE, 0}, 110, 10);
    }});

    // 3. Две бомбы, падающие одновременно
    {
        float bomb_drop_time = 6.0f;
        float bomb_vel_y = 140.0f;
        float bomb_radius = 20.0f;
        Vector2 bomb1_start = { WORLD_SIZE / 3.0f, 0 };
        Vector2 bomb2_start = { 2.0f * WORLD_SIZE / 3.0f, 0 };
        float travel_time = (WORLD_SIZE / 2.0f) / bomb_vel_y;
        float explosion_time = bomb_drop_time + travel_time;

        events.push_back({bomb_drop_time, [=](float, std::vector<Projectile>& p, Player&) {
            Projectile bomb1, bomb2;
            bomb1.pos = bomb1_start;
            bomb1.vel = { 0, bomb_vel_y };
            bomb1.r = bomb_radius;
            bomb2.pos = bomb2_start;
            bomb2.vel = { 0, bomb_vel_y };
            bomb2.r = bomb_radius;
            p.push_back(bomb1);
            p.push_back(bomb2);
        }});

        events.push_back({explosion_time, [=, &events](float, std::vector<Projectile>& p, Player&) {
            // Удаляем бомбы
            for (auto it = p.begin(); it != p.end(); ) {
                if (it->r == bomb_radius && (std::abs(it->pos.x - bomb1_start.x) < 10.0f ||
                                             std::abs(it->pos.x - bomb2_start.x) < 10.0f)) {
                    it = p.erase(it);
                } else {
                    ++it;
                }
            }
            // Первая бомба – круговая волна
            spawn_circular_burst(p, { bomb1_start.x, WORLD_SIZE / 2.0f }, 160.0f, 10, 8);
            // Вторая бомба – взрыв лучами
            add_beam_burst(events, explosion_time, { bomb2_start.x, WORLD_SIZE / 2.0f },
                           6, 0.15f, 10, 180.0f, 10);
            // Сортировка событий после добавления новых
            sort_attack_events(events);
        }});
    }

    // 4. Веер из пяти лучей + горизонтальный луч
    {
        float start_time = 12.0f;
        float interval = 0.12f;
        int count = 12;
        float speed = 200.0f;
        float radius = 10.0f;
        Vector2 center_top = { WORLD_SIZE / 2.0f, 0 };
        std::vector<float> angles = { -45.0f, -22.5f, 0.0f, 22.5f, 45.0f };

        // веер
        add_fan_attack(events, start_time, center_top, angles,
                       interval, count, speed, radius);

        // Горизонтальный луч из левой границы
        add_beam_events(events, start_time, interval, count,
                        { 0, 700.0f }, { 200.0f, 0 }, radius);
    }

    // 5. Прицельная пуля + встречная линия снизу
    {
        float start_time = 17.5f;

        events.push_back({start_time, [](float, std::vector<Projectile>& p, Player& pl) {
            float x = 100 + static_cast<float>(rand()) / RAND_MAX * (WORLD_SIZE - 200);
            spawn_targeted_bullet(p, { x, 0 }, pl.pos, 180.0f, 20);
        }});

        events.push_back({start_time, [](float dt, std::vector<Projectile>& p, Player&) {
            spawn_bullet_line(p, {0, -200}, dt, {0, WORLD_SIZE}, {WORLD_SIZE, WORLD_SIZE}, 120, 10);
        }});
    }

    // 6. Два встречных веера (по 3 луча каждый)
    {
        float start_time = 20.0f;
        float interval = 0.15f;
        int count = 8;
        float speed = 180.0f;
        float radius = 10.0f;

        // Веер из левой границы: углы -30°, 0°, +30° относительно направления вправо (90°)
        Vector2 left_center = { 0, WORLD_SIZE / 2.0f };
        std::vector<float> left_angles = { 60.0f, 90.0f, 120.0f };
        add_fan_attack(events, start_time, left_center, left_angles,
                       interval, count, speed, radius);

        // Веер из правой границы: углы 240°, 270°, 300° (налево)
        Vector2 right_center = { WORLD_SIZE, WORLD_SIZE / 2.0f };
        std::vector<float> right_angles = { 240.0f, 270.0f, 300.0f };
        add_fan_attack(events, start_time, right_center, right_angles,
                       interval, count, speed, radius);
    }

    // 7. Каскадная бомба с диагональными пулями
    {
        float bomb_drop_time = 23.0f;
        float bomb_vel_y = 120.0f;
        float bomb_radius = 20.0f;
        Vector2 bomb_start = { WORLD_SIZE / 2.0f, 0 };
        float travel_time = (WORLD_SIZE / 2.0f) / bomb_vel_y;
        float explosion_time = bomb_drop_time + travel_time;

        events.push_back({bomb_drop_time, [=](float, std::vector<Projectile>& p, Player&) {
            Projectile bomb;
            bomb.pos = bomb_start;
            bomb.vel = { 0, bomb_vel_y };
            bomb.r = bomb_radius;
            p.push_back(bomb);
        }});

        // Первый взрыв
        events.push_back({explosion_time, [=, &events](float, std::vector<Projectile>& p, Player&) {
            // Удаляем бомбу
            for (auto it = p.begin(); it != p.end(); ++it) {
                if (it->r == bomb_radius && std::abs(it->pos.x - WORLD_SIZE/2.0f) < 10.0f &&
                    std::abs(it->pos.y - WORLD_SIZE/2.0f) < 10.0f) {
                    p.erase(it);
                    break;
                }
            }
            // Первая круговая волна
            spawn_circular_burst(p, { WORLD_SIZE/2.0f, WORLD_SIZE/2.0f }, 140.0f, 10, 8);

            // Диагональные пули
            const float diagonal_speed = 600.0f;
            spawn_targeted_bullet(p, {0, 0}, {WORLD_SIZE, WORLD_SIZE}, diagonal_speed, 15);
            spawn_targeted_bullet(p, {WORLD_SIZE, 0}, {0, WORLD_SIZE}, diagonal_speed, 15);

            // Второй взрыв через время пересечения в центре
            const float half_world = WORLD_SIZE / 2.0f;
            const float diagonal_half_distance = sqrt(half_world * half_world + half_world * half_world);
            const float crossing_time = diagonal_half_distance / diagonal_speed;
            float second_explosion_time = explosion_time + crossing_time;

            events.push_back({second_explosion_time, [](float, std::vector<Projectile>& pr, Player&) {
                spawn_circular_burst(pr, { WORLD_SIZE/2.0f, WORLD_SIZE/2.0f }, 200.0f, 10, 16);
            }});
            
            // Сортировка событий после добавления новых
            sort_attack_events(events);
        }});
    }

    sort_attack_events(events);
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
        float start_horizontal = 22.0f;
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
    events.push_back({24.0f, [](float dt, std::vector<Projectile>& p, Player& pl) {
        // Случайная X от 100 до 924
        float x = 100 + static_cast<float>(rand()) / RAND_MAX * (WORLD_SIZE - 200);
        spawn_targeted_bullet(p, {x, 0}, pl.pos, 150.0f, 20);
    }});

    // Атака 7: веер из пяти лучей
    {
        float start_fan = 32.0f;
        float interval = 0.2f;
        int bullets_per_beam = 15;
        float speed = 200;
        float radius = 10;
        Vector2 center_top = { WORLD_SIZE/2.0f, 0 };
        std::vector<float> angles = {-40, -20, 0, 20, 40};

        add_fan_attack(events, start_fan, center_top, angles, interval, bullets_per_beam, speed, radius);
    }

    // Атака 8: вторая бомба с взрывом лучами + линия
    {
        float bomb_drop_time = 40.0f;
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

        // широкая линия с щелями
        events.push_back({explosion_time, [](float dt, std::vector<Projectile>& p, Player&) {
            spawn_bullet_line(p, {0, 180}, dt, {0, 0}, {WORLD_SIZE, 0}, 140, 10);
        }});

        // Взрыв: удаляем бомбу и добавляем burst лучей
        events.push_back({explosion_time, [&events, bomb_radius, explosion_time](float dt, std::vector<Projectile>& p, Player&) {
            // Удаляем бомбу
            for (auto it = p.begin(); it != p.end(); ++it) {
                if (it->r == bomb_radius &&
                    std::abs(it->pos.x - WORLD_SIZE/2.0f) < 10.0f &&
                    std::abs(it->pos.y - WORLD_SIZE/2.0f) < 10.0f)
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

    bool is_event_level = (level_id == LevelId::BEGINNING || level_id == LevelId::WAY);

    if (is_event_level)
    {
        if (reset_level) {
            // Очистка событий, инициализация уровня
            events.clear();
            next_event = 0;
            event_level_completed = false;
            // Вызываем инициализацию выбранного уровня
            if (level_id == LevelId::BEGINNING) {
                level_beginning_init(events, player);
            }
            else if (level_id == LevelId::WAY) {
                level_way_init(events, player);
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

void reset_game_state(std::vector<Projectile>& projectiles, float& level_time, Player& player)
{
    projectiles.clear();
    player.hit_count = 0;
    level_time = 0.0f;
    player.pos = { WORLD_SIZE / 2.0f, WORLD_SIZE / 2.0f };
    player.immortal = false;
    player.invincible_until = 0;
}

void update_player_height(Texture& player_texture, float& player_height, const float& player_width)
{
	player_height = (float)player_width * ((float)(player_texture.height) / (float)(player_texture.width));
}
