#include <arpa/inet.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "common.h"
#include "net.h"
#include "raylib.h"

#define WIDTH 1280
#define HEIGHT 720

#define CELL_SIZE 64.0

Color icons[] = {
    {0xFF, 0x00, 0x00, 0xFF},
    {0x0, 0xFF, 0x00, 0xFF},
    {0x0, 0x00, 0xFF, 0xFF},
};

const int base_x_offset = (WIDTH - (CELL_SIZE * MAP_WIDTH)) / 2;
const int base_y_offset = (HEIGHT - (CELL_SIZE * MAP_HEIGHT)) / 2;

const int MAP[MAP_HEIGHT][MAP_WIDTH] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0},
    {0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0},
    {0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

typedef struct {
    // Position in grid space
    Vector2 position;
    char name[9];
    Color color;
    float health;
    uint8_t spells[1];
    player_action action;
    uint8_t selected_spell;
} player;

typedef struct {
    player_action action;
    Vector2 position;
} player_move;

player players[MAX_PLAYER_COUNT] = {0};
int player_count = 0;

int current_player = 0;
round_state state = RS_PLAYING;
game_state gs = GS_WAITING;
int winner_id = 0;

uint8_t my_spells[MAX_SPELL_COUNT] = {0, 1, 3};

bool is_cell_in_zone(Vector2 player, Vector2 origin, Vector2 cell, const spell *s);

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

bool is_walkable(int x, int y) {
    return MAP[y][x] != 1;
}

// TODO: Implement BFS or A* to check if moves are valid
bool can_player_move(player *p, Vector2 cell, int range) {
    // Check bounds
    if (cell.x < 0 || cell.x >= MAP_WIDTH || cell.y < 0 || cell.y >= MAP_HEIGHT) {
        return false;
    }

    if (!is_walkable(cell.x, cell.y)) {
        return false;
    }

    // Check if cell is in range
    if (fabs(p->position.x - cell.x) + fabs(p->position.y - cell.y) > range) {
        return false;
    }

    // Check if there is any wall between the cell and the player using A* or BFS. For now only supports 1 range
    return true;
}

const spell *get_selected_spell() {
    player *p = &players[current_player];
    return &all_spells[p->spells[p->selected_spell]];
}

int player_cast_spell(player *p, Vector2 origin) {
    const spell *s = get_selected_spell();
    if (s->type == ST_MOVE || s->type == ST_TARGET) {
        return can_player_move(p, origin, s->range);
    } else {
        printf("not implemented\n");
        exit(1);
    }
    return false;
}

player_move player_exec_action(player *p) {
    const Vector2 g = screen2grid(GetMousePosition());
    if (IsMouseButtonPressed(0)) {
        if (p->action == PA_SPELL) {
            // TODO: Player should be able to cast a spell even if there is nobody on the clicked cell
            // But it should check if the player has the range to cast a spell here
            if (player_cast_spell(p, g)) {
                return (player_move){.action = PA_SPELL, .position = g};
            }
        }
    }
    return (player_move){.action = PA_NONE};
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

bool is_cell_in_zone(Vector2 player, Vector2 origin, Vector2 cell, const spell *s) {
    if (cell.x < 0 || cell.x >= MAP_WIDTH || cell.y < 0 || cell.y >= MAP_HEIGHT || MAP[(int)cell.y][(int)cell.x] == 1)
        return false;

    const int player_d = fabsf(player.x - cell.x) + fabsf(player.y - cell.y);
    const int spell_d = fabsf(origin.x - cell.x) + fabsf(origin.y - cell.y);
    if (spell_d >= s->zone_size || player_d > s->range)
        return false;
    return true;
}

void render_spell_actions(player *p) {
    const spell *s = get_selected_spell();
    if (s->type == ST_MOVE) {
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                const int x_pos = p->position.x + x;
                const int y_pos = p->position.y + y;
                if (can_player_move(p, (Vector2){x_pos, y_pos}, s->range)) {
                    Vector2 s = grid2screen((Vector2){x_pos, y_pos});
                    DrawRectangleLinesEx((Rectangle){s.x, s.y, CELL_SIZE, CELL_SIZE}, 8, p->color);
                }
            }
        }
    } else if (s->type == ST_TARGET) {
        for (int x = -s->range; x <= s->range; x++) {
            for (int y = -s->range; y <= s->range; y++) {
                const int x_pos = p->position.x + x;
                const int y_pos = p->position.y + y;
                Vector2 g = {x_pos, y_pos};
                if (can_player_move(p, g, s->range)) {
                    Vector2 s = grid2screen(g);
                    DrawRectangleLinesEx((Rectangle){s.x, s.y, CELL_SIZE, CELL_SIZE}, 8, RED);
                }
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

void render_tooltip(const spell *s) {
    Vector2 mouse = GetMousePosition();
    const int tooltip_width = 250;
    const int tooltip_height = 150;
    Rectangle tooltip = {mouse.x, mouse.y - tooltip_height, tooltip_width, tooltip_height};
    DrawRectangleRec(tooltip, RAYWHITE);
    DrawRectangleLinesEx(tooltip, 5, BLACK);
    DrawText(s->name, tooltip.x + 8, tooltip.y + 8, 18, BLACK);

    if (s->type == ST_MOVE) {
        DrawText("Moves the player", tooltip.x + 8, tooltip.y + 24, 18, BLACK);
        DrawText(TextFormat("Range = %d", s->range), tooltip.x + 8, tooltip.y + 42, 18, BLACK);
    } else if (s->type == ST_TARGET || s->type == ST_ZONE) {
        DrawText(TextFormat("Damage = %d", s->damage), tooltip.x + 8, tooltip.y + 24, 18, BLACK);
        DrawText(TextFormat("Range = %d", s->range), tooltip.x + 8, tooltip.y + 42, 18, BLACK);
        DrawText(TextFormat("Speed = %d", s->speed), tooltip.x + 8, tooltip.y + 74, 18, BLACK);
    }
}

bool is_over_toolbar_cell(uint8_t cell_id) {
    int toolbar_y = base_y_offset + CELL_SIZE * MAP_HEIGHT + 16;
    Rectangle cell = {base_x_offset + (CELL_SIZE + 8) * cell_id, toolbar_y, CELL_SIZE, CELL_SIZE};
    Vector2 mouse = GetMousePosition();
    return CheckCollisionPointRec(mouse, cell);
}

void render_player_actions(player *p) {
    // Toolbar rendering
    int toolbar_y = base_y_offset + CELL_SIZE * MAP_HEIGHT + 16;

    for (int i = 0; i < MAX_SPELL_COUNT; i++) {
        Color c = icons[all_spells[p->spells[i]].icon];
        draw_cell(base_x_offset + (CELL_SIZE + 8) * i, toolbar_y, c);
    }

    if (p->action == PA_SPELL) {
        draw_cell_lines(base_x_offset + (CELL_SIZE + 8) * p->selected_spell, toolbar_y, 4, BLACK);
    }

    for (int i = 0; i < MAX_SPELL_COUNT; i++) {
        if (is_over_toolbar_cell(i)) {
            render_tooltip(&all_spells[p->spells[i]]);
            if (IsMouseButtonPressed(0)) {
                p->action = PA_SPELL;
                p->selected_spell = i;
            }
        }
    }

    // In-game rendering
    if (p->action == PA_SPELL) {
        render_spell_actions(p);
    }
}

void render_infos() {
    DrawRectangleRec((Rectangle){base_x_offset, 0, MAP_WIDTH * CELL_SIZE, base_y_offset}, GRAY);
    const int player_info_width = (MAP_WIDTH * CELL_SIZE) / player_count;
    for (int i = 0; i < player_count; i++) {
        const int x = base_x_offset + player_info_width * i;
        DrawRectangleLines(x, 0, player_info_width, base_y_offset, RED);
        DrawCircle(x + 8 + 8, 16, 12, players[i].color);
        if (i == current_player) {
            DrawText(TextFormat("%s (you)", players[i].name), x + 32, 8, 24, BLACK);
        } else {
            DrawText(TextFormat("%s", players[i].name), x + 32, 8, 24, BLACK);
        }
        DrawText(TextFormat("Health: %d", (int)players[i].health), x + 8, 32, 24, BLACK);
    }
}

int client_fd = 0;

int connect_to_server(const char *ip, uint16_t port) {
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        return -1;
    }

    int status;
    if ((status = connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
        return -1;
    }

    return 0;
}

void handle_packet(net_packet *p) {
    if (p->type == PKT_PING) {
        printf("PING\n");
    } else if (p->type == PKT_JOIN) {
        net_packet_join *join = (net_packet_join *)p->content;
        printf("Joined: %.*s with ID=%d\n", 8, join->username, join->id);
        memcpy(players[join->id].name, join->username, 8);
        players[join->id].name[8] = '\0';
        player_count++;
    } else if (p->type == PKT_CONNECTED) {
        net_packet_connected *c = (net_packet_connected *)p->content;
        printf("My ID is %d\n", c->id);
        current_player = c->id;

        net_packet_player_build b = pkt_player_build(rand(), my_spells);
        memcpy(players[current_player].spells, my_spells, MAX_SPELL_COUNT);
        send_sock(PKT_PLAYER_BUILD, &b, client_fd);
    } else if (p->type == PKT_GAME_START) {
        printf("Starting Game !!\n");
        gs = GS_STARTED;
    } else if (p->type == PKT_PLAYER_UPDATE) {
        net_packet_player_update *u = (net_packet_player_update *)p->content;
        players[u->id].health = u->health;
        players[u->id].position.x = u->x;
        players[u->id].position.y = u->y;
        printf("Player Update %d %d %d H=%d\n", u->id, u->x, u->y, u->health);
    } else if (p->type == PKT_ROUND_END) {
        printf("Round ended\n");
        state = RS_PLAYING;
    } else if (p->type == PKT_GAME_END) {
        net_packet_game_end *e = (net_packet_game_end*)p->content;
        winner_id = e->winner_id;
        gs = GS_ENDED;
    }

    free(p->content);
}

int main() {
    InitWindow(WIDTH, HEIGHT, "Duel Game");
    SetTargetFPS(60);

    if (connect_to_server("127.0.0.1", 3000) < 0) {
        fprintf(stderr, "Could not connect to server\n");
        exit(1);
    }

    printf("Connected to server !\n");

    players[0].color = YELLOW;
    players[0].action = PA_SPELL;

    players[1].color = GREEN;
    players[1].action = PA_SPELL;

    players[2].color = BLUE;
    players[2].action = PA_SPELL;

    players[3].color = RED;
    players[3].action = PA_SPELL;

    fd_set master_set;
    FD_ZERO(&master_set);
    FD_SET(client_fd, &master_set);
    int fd_count = client_fd;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    srand(time(NULL));
    net_packet_join join = {0};
    memcpy(join.username, TextFormat("User %d", rand()), 8);
    send_sock(PKT_JOIN, &join, client_fd);

    while (!WindowShouldClose()) {
        fd_set read_fds = master_set;
        int activity = select(fd_count + 1, &read_fds, NULL, NULL, &timeout);
        if (activity < 0) {
            exit(1);
        }

        if (FD_ISSET(client_fd, &read_fds)) {
            net_packet p = {0};
            if (packet_read(&p, client_fd) < 0) {
                exit(1);
            }
            handle_packet(&p);
        }

        BeginDrawing();
        {
            ClearBackground(GetColor(0x181818FF));

            if (gs == GS_WAITING) {
                DrawText("Waiting for game to start", 0, 0, 64, WHITE);
            } else if (gs == GS_STARTED) {
                if (state == RS_PLAYING) {
                    if (IsKeyPressed(KEY_Q)) {
                        players[current_player].action = PA_SPELL;
                        players[current_player].selected_spell = 0;
                    } else if (IsKeyPressed(KEY_W)) {
                        players[current_player].action = PA_SPELL;
                        players[current_player].selected_spell = 1;
                    } else if (IsKeyPressed(KEY_E)) {
                        players[current_player].action = PA_SPELL;
                        players[current_player].selected_spell = 2;
                    }
                }

                if (state == RS_PLAYING) {
                    player_move move = player_exec_action(&players[current_player]);
                    if (move.action != PA_NONE) {
                        state = RS_WAITING;
                        // Send action to server
                        net_packet_player_action a = pkt_player_action(current_player, move.action, move.position.x,
                                                                       move.position.y, get_selected_spell()->id);
                        send_sock(PKT_PLAYER_ACTION, &a, client_fd);
                    }
                }

                // When waiting, we should preview our action

                render_map();
                for (int i = 0; i < player_count; i++) {
                    render_player(&players[i]);
                }
                if (state == RS_PLAYING) {
                    render_player_actions(&players[current_player]);
                }
                render_infos();
            } else if (gs == GS_ENDED) {
                if (winner_id == 255) {
                    DrawText("Game draw", 0, 0, 64, WHITE);
                } else {
                    DrawText(TextFormat("%s won the game !", players[winner_id].name), 0, 0, 64, WHITE);
                }
            }
        }
        EndDrawing();
    }
    CloseWindow();
}
