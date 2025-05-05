#include <math.h>
#include <stdint.h>
#include "raylib.h"

#define WIDTH 1280
#define HEIGHT 720

#define MAP_WIDTH 16
#define MAP_HEIGHT 8

#define CELL_SIZE 64.0

const int base_x_offset = (WIDTH - (CELL_SIZE * MAP_WIDTH)) / 2;
const int base_y_offset = (HEIGHT - (CELL_SIZE * MAP_HEIGHT)) / 2;

const int MAP[MAP_HEIGHT][MAP_WIDTH] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0},
    {0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0},
    {0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

typedef enum {
    PA_MOVE,
    PA_ATTACK,
    PA_SPELL,
} player_action;

typedef enum {
    ST_TARGET,
    ST_ZONE,
    ST_LINE,
} spell_type;

typedef struct {
    const char *name;
    spell_type type;
    int damage;
    int range;
    int zone_size;  // TODO: Should not be on all spells
} spell;

typedef struct {
    // Position in grid space
    Vector2 position;
    Color color;
    float health;
    spell attack;
    spell spells[1];
    player_action action;
} player;

player players[3] = {0};
const int player_count = sizeof(players) / sizeof(players[0]);
int current_player = 0;

bool is_target_in_range_target(Vector2 origin, int range, player *target);
bool is_cell_in_zone(Vector2 player, Vector2 origin, Vector2 cell, spell *s);

void draw_cell(int x, int y, Color c) {
    DrawRectangle(x, y, CELL_SIZE, CELL_SIZE, c);
}

void draw_cell_lines(int x, int y, int t, Color c) {
    DrawRectangleLinesEx((Rectangle){x, y, CELL_SIZE, CELL_SIZE}, t, c);
}

bool v2eq(Vector2 a, Vector2 b) {
    return a.x == b.x && a.y == b.y;
}

Vector2 grid2screen(Vector2 g) {
    return (Vector2){base_x_offset + g.x * CELL_SIZE, base_y_offset + g.y * CELL_SIZE};
}

Vector2 screen2grid(Vector2 s) {
    return (Vector2){(int)((s.x - base_x_offset) / CELL_SIZE), (int)((s.y - base_y_offset) / CELL_SIZE)};
}

void player_take_damage(player *p, int damage) {
    p->health = fmax(0, p->health - damage);
}

bool can_player_move(player *p, Vector2 cell) {
    if (cell.x < 0 || cell.x >= MAP_WIDTH || cell.y < 0 || cell.y >= MAP_HEIGHT) {
        return false;
    }
    if (MAP[(int)cell.y][(int)cell.x] == 1) {
        return false;
    }
    for (int i = 0; i < player_count; i++) {
        if (v2eq(players[i].position, cell) && players[i].health > 0) {
            return false;
        }
    }
    return fabs(p->position.x - cell.x) <= 1 && fabs(p->position.y - cell.y) <= 1;
}

int player_cast_spell(player *p, spell *s, Vector2 origin) {
    int hit = false;
    for (int i = 0; i < player_count; i++) {
        if (i == current_player || players[i].health <= 0)
            continue;
        player *target = &players[i];
        if (s->type == ST_TARGET) {
            if (v2eq(target->position, origin) && is_target_in_range_target(p->position, s->range, target)) {
                player_take_damage(&players[i], s->damage);
                return true;
            }
        } else if (s->type == ST_ZONE) {
            for (int x = -s->range; x <= s->range; x++) {
                for (int y = -s->range; y <= s->range; y++) {
                    const Vector2 cell = {origin.x + x, origin.y + y};
                    if (is_cell_in_zone(p->position, origin, cell, s) && v2eq(players[i].position, cell)) {
                        player_take_damage(&players[i], s->damage);
                        hit = true;
                    }
                }
            }
        }
    }
    return hit;
}

bool player_exec_action(player *p) {
    const Vector2 g = screen2grid(GetMousePosition());
    if (IsMouseButtonPressed(0)) {
        if (p->action == PA_MOVE) {
            if (can_player_move(p, g)) {
                p->position = g;
                return true;
            }
        } else if (p->action == PA_ATTACK) {
            return player_cast_spell(p, &p->attack, g);
        } else if (p->action == PA_SPELL) {
            spell *s = &p->spells[0];
            return player_cast_spell(p, s, g);
        }
    }
    return false;
}

bool is_over_cell(int x, int y) {
    const Rectangle cell_rect = {x, y, CELL_SIZE, CELL_SIZE};
    return CheckCollisionPointRec(GetMousePosition(), cell_rect);
}

Color get_cell_color(int cell_type) {
    if (cell_type == 0)
        return PURPLE;
    if (cell_type == 1)
        return GRAY;
    return SKYBLUE;
}

void render_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            const int x_pos = base_x_offset + x * CELL_SIZE;
            const int y_pos = base_y_offset + y * CELL_SIZE;
            Color c = is_over_cell(x_pos, y_pos) ? RED : get_cell_color(MAP[y][x]);
            DrawRectangle(x_pos, y_pos, CELL_SIZE, CELL_SIZE, c);
            DrawRectangleLines(x_pos, y_pos, CELL_SIZE, CELL_SIZE, BLACK);
        }
    }
}

void render_player(player *p) {
    if (p->health <= 0)
        return;
    const int x_pos = base_x_offset + p->position.x * CELL_SIZE;
    const int y_pos = base_y_offset + p->position.y * CELL_SIZE;
    DrawCircle(x_pos + CELL_SIZE / 2, y_pos + CELL_SIZE / 2, 24, p->color);
}

bool is_target_in_range_target(Vector2 origin, int range, player *target) {
    if (target->health <= 0)
        return false;

    for (int x = -range; x <= range; x++) {
        for (int y = -range; y <= range; y++) {
            const int sx = origin.x + x;
            const int sy = origin.y + y;
            if (target->position.x == sx && target->position.y == sy) {
                return true;
            }
        }
    }
    return false;
}

bool is_cell_in_zone(Vector2 player, Vector2 origin, Vector2 cell, spell *s) {
    if (cell.x < 0 || cell.x >= MAP_WIDTH || cell.y < 0 || cell.y >= MAP_HEIGHT || MAP[(int)cell.y][(int)cell.x] == 1)
        return false;

    const int player_d = fabsf(player.x - cell.x) + fabsf(player.y - cell.y);
    const int spell_d = fabsf(origin.x - cell.x) + fabsf(origin.y - cell.y);
    if (spell_d >= s->zone_size || player_d > s->range)
        return false;
    return true;
}

void render_spell_actions(player *p, spell *s) {
    if (s->type == ST_TARGET) {
        for (int i = 0; i < player_count; i++) {
            if (i == current_player)
                continue;
            if (is_target_in_range_target(p->position, s->range, &players[i])) {
                Vector2 s = grid2screen(players[i].position);
                DrawRectangleLinesEx((Rectangle){s.x, s.y, CELL_SIZE, CELL_SIZE}, 8, RED);
            }
        }
    } else if (s->type == ST_ZONE) {
        Vector2 g = screen2grid(GetMousePosition());
        for (int x = -s->range; x <= s->range; x++) {
            for (int y = -s->range; y <= s->range; y++) {
                const Vector2 cell = {g.x + x, g.y + y};
                if (is_cell_in_zone(p->position, g, cell, s)) {
                    Vector2 s = grid2screen(cell);
                    DrawRectangleLinesEx((Rectangle){s.x, s.y, CELL_SIZE, CELL_SIZE}, 8, RED);
                }
            }
        }
    }
}

void render_player_actions(player *p) {
    // Toolbar rendering
    int toolbar_y = base_y_offset + CELL_SIZE * MAP_HEIGHT + 16;
    draw_cell(base_x_offset, toolbar_y, GRAY);
    draw_cell(base_x_offset + CELL_SIZE + 8, toolbar_y, GRAY);
    draw_cell(base_x_offset + (CELL_SIZE + 8) * 2, toolbar_y, GRAY);

    if (p->action == PA_MOVE) {
        draw_cell_lines(base_x_offset, toolbar_y, 4, BLACK);
    } else if (p->action == PA_ATTACK) {
        draw_cell_lines(base_x_offset + CELL_SIZE + 8, toolbar_y, 4, BLACK);
    } else if (p->action == PA_SPELL) {
        draw_cell_lines(base_x_offset + (CELL_SIZE + 8) * 2, toolbar_y, 4, BLACK);
    }

    if (p->action == PA_MOVE) {
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                const int x_pos = p->position.x + x;
                const int y_pos = p->position.y + y;
                if (can_player_move(p, (Vector2){x_pos, y_pos})) {
                    Vector2 s = grid2screen((Vector2){x_pos, y_pos});
                    DrawRectangleLinesEx((Rectangle){s.x, s.y, CELL_SIZE, CELL_SIZE}, 8, p->color);
                }
            }
        }
    } else if (p->action == PA_ATTACK) {
        render_spell_actions(p, &p->attack);
    } else if (p->action == PA_SPELL) {
        render_spell_actions(p, &p->spells[0]);
    }
}

void render_infos() {
    DrawRectangleRec((Rectangle){base_x_offset, 0, MAP_WIDTH * CELL_SIZE, base_y_offset}, GRAY);
    const int player_info_width = (MAP_WIDTH * CELL_SIZE) / player_count;
    for (int i = 0; i < player_count; i++) {
        const int x = base_x_offset + player_info_width * i;
        DrawRectangleLines(x, 0, player_info_width, base_y_offset, RED);
        DrawCircle(x + 8 + 8, 16, 12, players[i].color);
        DrawText(TextFormat("Health: %d", (int)players[i].health), x + 8, 32, 24, BLACK);
    }
}

int main() {
    InitWindow(WIDTH, HEIGHT, "Duel Game");
    SetTargetFPS(60);

    spell auto_attack = {.name = "Attack", .type = ST_TARGET, .damage = 25, .range = 1};

    players[0].position = (Vector2){0, 0};
    players[0].color = YELLOW;
    players[0].health = 100;
    players[0].action = PA_MOVE;
    players[0].attack = auto_attack;
    players[0].spells[0].name = "A";
    players[0].spells[0].type = ST_TARGET;
    players[0].spells[0].damage = 75;
    players[0].spells[0].range = 2;

    players[1].position = (Vector2){MAP_WIDTH - 1, MAP_HEIGHT - 1};
    players[1].color = GREEN;
    players[1].health = 100;
    players[1].action = PA_MOVE;
    players[1].attack = auto_attack;
    players[1].spells[0].name = "B";
    players[1].spells[0].type = ST_ZONE;
    players[1].spells[0].damage = 50;
    players[1].spells[0].range = 5;
    players[1].spells[0].zone_size = 2;

    players[2].position = (Vector2){MAP_WIDTH - 1, 0};
    players[2].color = BLUE;
    players[2].health = 100;
    players[2].action = PA_MOVE;
    players[2].attack = auto_attack;
    players[2].spells[0].name = "C";
    players[2].spells[0].type = ST_LINE;
    players[2].spells[0].damage = 25;
    players[2].spells[0].range = 5;

    while (!WindowShouldClose()) {
        BeginDrawing();
        {
            ClearBackground(GetColor(0x181818FF));
            if (IsKeyPressed(KEY_Q)) {
                players[current_player].action = PA_MOVE;
            } else if (IsKeyPressed(KEY_W)) {
                players[current_player].action = PA_ATTACK;
            } else if (IsKeyPressed(KEY_E)) {
                players[current_player].action = PA_SPELL;
            }

            if (player_exec_action(&players[current_player])) {
                do {
                    current_player = (current_player + 1) % player_count;
                } while (players[current_player].health <= 0);
                players[current_player].action = PA_MOVE;
            }

            render_map();
            for (int i = 0; i < player_count; i++) {
                render_player(&players[i]);
            }
            render_player_actions(&players[current_player]);
            render_infos();
        }
        EndDrawing();
    }
    CloseWindow();
}
