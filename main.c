#include <stdint.h>
#include "raylib.h"

#define WIDTH 1280
#define HEIGHT 720

#define MAP_WIDTH 16
#define MAP_HEIGHT 8

#define CELL_SIZE 64.0

const int base_x_offset = (WIDTH - (CELL_SIZE * MAP_WIDTH)) / 2;
const int base_y_offset = (HEIGHT - (CELL_SIZE * MAP_HEIGHT)) / 2;

typedef struct {
    // Position in grid space
    Vector2 position;
} player;

Vector2 grid2screen(Vector2 g) {
    return (Vector2){base_x_offset + g.x * CELL_SIZE, base_y_offset + g.y * CELL_SIZE};
}

Vector2 screen2grid(Vector2 s) {
    return (Vector2){(int)((s.x - base_x_offset) / CELL_SIZE), (int)((s.y - base_y_offset) / CELL_SIZE)};
}

void player_action(player* p) {
    const Vector2 g = screen2grid(GetMousePosition());
    if (IsMouseButtonPressed(0)) {
        p->position = g;
    }
}

bool is_over_cell(int x, int y) {
    const Rectangle cell_rect = {x, y, CELL_SIZE, CELL_SIZE};
    return CheckCollisionPointRec(GetMousePosition(), cell_rect);
}

void render_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            const int x_pos = base_x_offset + x * CELL_SIZE;
            const int y_pos = base_y_offset + y * CELL_SIZE;
            Color c = is_over_cell(x_pos, y_pos) ? RED : PURPLE;
            DrawRectangle(x_pos, y_pos, CELL_SIZE, CELL_SIZE, c);
            DrawRectangleLines(x_pos, y_pos, CELL_SIZE, CELL_SIZE, BLACK);
        }
    }
}

void render_player(player* p) {
    const int x_pos = base_x_offset + p->position.x * CELL_SIZE;
    const int y_pos = base_y_offset + p->position.y * CELL_SIZE;
    DrawCircle(x_pos + CELL_SIZE / 2, y_pos + CELL_SIZE / 2, 24, YELLOW);
}

int main() {
    InitWindow(WIDTH, HEIGHT, "Duel Game");

    player p1 = {0};

    while (!WindowShouldClose()) {
        BeginDrawing();
        {
            ClearBackground(GetColor(0x181818FF));

            render_map();
            render_player(&p1);
            player_action(&p1);
        }
        EndDrawing();
    }
    CloseWindow();
}
