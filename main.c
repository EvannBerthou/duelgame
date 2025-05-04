#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
    ST_TARGET,
    ST_ZONE,
    ST_LINE,
} spell_type;

typedef struct {
    const char* name;
    spell_type type;
    int damage;
    int range;
} spell;

typedef struct {
    // Position in grid space
    Vector2 position;
    Color color;
    float health;
    spell spells[1];
} player;

player players[3] = {0};
const int player_count = sizeof(players) / sizeof(players[0]);
int current_player = 0;

bool is_target_in_range_target(Vector2 origin, int range, player* target);
bool is_cell_in_zone(Vector2 player, Vector2 origin, Vector2 cell, int range);

Vector2 grid2screen(Vector2 g) {
    return (Vector2){base_x_offset + g.x * CELL_SIZE, base_y_offset + g.y * CELL_SIZE};
}

Vector2 screen2grid(Vector2 s) {
    return (Vector2){(int)((s.x - base_x_offset) / CELL_SIZE), (int)((s.y - base_y_offset) / CELL_SIZE)};
}

bool can_player_move(player* p, int x, int y) {
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
        return false;
    }
    if (MAP[y][x] == 1) {
        return false;
    }
    for (int i = 0; i < player_count; i++) {
        if (players[i].position.x == x && players[i].position.y == y) {
            return false;
        }
    }
    return fabs(p->position.x - x) <= 1 && fabs(p->position.y - y) <= 1;
}

bool player_action(player* p) {
    const Vector2 g = screen2grid(GetMousePosition());
    if (IsMouseButtonPressed(0)) {
        if (can_player_move(p, g.x, g.y)) {
            p->position = g;
            return true;
        }
        spell* s = &p->spells[0];
        bool target_hit = false;
        for (int i = 0; i < player_count; i++) {
            if (i == current_player)
                continue;
            player* target = &players[i];
            if (s->type == ST_TARGET) {
                if (target->position.x == g.x && target->position.y == g.y &&
                    is_target_in_range_target(p->position, s->range, target)) {
                    players[i].health -= s->damage;
                    target_hit = true;
                }
            } else if (s->type == ST_ZONE) {
                Vector2 g = screen2grid(GetMousePosition());
                for (int x = -s->range; x <= s->range; x++) {
                    for (int y = -s->range; y <= s->range; y++) {
                        const Vector2 cell = {g.x + x, g.y + y};
                        if (is_cell_in_zone(p->position, g, cell, s->range)) {
                            if (players[i].position.x == cell.x && players[i].position.y == cell.y) {
                                players[i].health -= s->damage;
                                target_hit = true;
                            }
                        }
                    }
                }
            }
        }
        return target_hit;
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

void render_player(player* p) {
    const int x_pos = base_x_offset + p->position.x * CELL_SIZE;
    const int y_pos = base_y_offset + p->position.y * CELL_SIZE;
    DrawCircle(x_pos + CELL_SIZE / 2, y_pos + CELL_SIZE / 2, 24, p->color);
}

bool is_target_in_range_target(Vector2 origin, int range, player* target) {
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

bool is_cell_in_zone(Vector2 player, Vector2 origin, Vector2 cell, int range) {
    if (cell.x < 0 || cell.x >= MAP_WIDTH || cell.y < 0 || cell.y >= MAP_HEIGHT || MAP[(int)cell.y][(int)cell.x] == 1)
        return false;

    const int player_d = fabsf(player.x - cell.x) + fabsf(player.y - cell.y);
    const int spell_d = fabsf(origin.x - cell.x) + fabsf(origin.y - cell.y);
    if (spell_d >= range || player_d >= range * 2)
        return false;
    return true;
}

void render_spell_actions(player* p, spell* s) {
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
                if (is_cell_in_zone(p->position, g, cell, s->range)) {
                    Vector2 s = grid2screen(cell);
                    DrawRectangleLinesEx((Rectangle){s.x, s.y, CELL_SIZE, CELL_SIZE}, 8, RED);
                }
            }
        }
    }
}

void render_player_actions(player* p) {
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            const int x_pos = p->position.x + x;
            const int y_pos = p->position.y + y;
            if (can_player_move(p, x_pos, y_pos)) {
                Vector2 s = grid2screen((Vector2){x_pos, y_pos});
                DrawRectangleLinesEx((Rectangle){s.x, s.y, CELL_SIZE, CELL_SIZE}, 8, p->color);
            }
        }
    }
    render_spell_actions(p, &p->spells[0]);

    DrawText(p->spells[0].name, 0, 0, 24, WHITE);
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

    players[0].position = (Vector2){0, 0};
    players[0].color = YELLOW;
    players[0].health = 100;
    players[0].spells[0].name = "A";
    players[0].spells[0].type = ST_TARGET;
    players[0].spells[0].damage = 75;
    players[0].spells[0].range = 2;

    players[1].position = (Vector2){MAP_WIDTH - 1, MAP_HEIGHT - 1};
    players[1].color = GREEN;
    players[1].health = 100;
    players[1].spells[0].name = "B";
    players[1].spells[0].type = ST_ZONE;
    players[1].spells[0].damage = 50;
    players[1].spells[0].range = 2;

    players[2].position = (Vector2){MAP_WIDTH - 1, 0};
    players[2].color = BLUE;
    players[2].health = 100;
    players[2].spells[0].name = "C";
    players[2].spells[0].type = ST_LINE;
    players[2].spells[0].damage = 25;
    players[2].spells[0].range = 5;

    while (!WindowShouldClose()) {
        BeginDrawing();
        {
            ClearBackground(GetColor(0x181818FF));
            if (player_action(&players[current_player])) {
                do {
                    current_player = (current_player + 1) % player_count;
                } while (players[current_player].health <= 0);
            }

            render_map();
            for (int i = 0; i < player_count; i++) {
                if (players[i].health <= 0)
                    continue;
                render_player(&players[i]);
            }
            render_player_actions(&players[current_player]);
            render_infos();
        }
        EndDrawing();
    }
    CloseWindow();
}
