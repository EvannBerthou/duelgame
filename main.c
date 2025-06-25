// TODO: Unify int/uint8_t

#include <arpa/inet.h>
#include <limits.h>
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

extern const spell all_spells[];

#define WIDTH 1280
#define HEIGHT 720

// TODO: Cell size should be given by the map
#define CELL_SIZE 64.0

#define MAX_ANIMATION_POOL 16

// UI
Font font = {0};
Texture2D box_side = {0};
Texture2D box_mid = {0};
Texture2D spell_box = {0};
Texture2D spell_box_select = {0};
Texture2D game_slot = {0};
Texture2D life_bar_bg = {0};

// TODO: Use icons instead of colors
Color icons[] = {
    {0xFF, 0x00, 0x00, 0xFF},
    {0x0, 0xFF, 0x00, 0xFF},
    {0x0, 0x00, 0xFF, 0xFF},
};

const int base_x_offset = (WIDTH - (CELL_SIZE * MAP_WIDTH)) / 2;
const int base_y_offset = (HEIGHT - (CELL_SIZE * MAP_HEIGHT)) / 2;

map_layer game_map = {0};
map_layer variants = {0};
map_layer props = {0};
map_layer props_animations = {0};

#define WALL_ORIENTATION_COUNT 16

// Each name states where the wall are.
const char *wall_frames[WALL_ORIENTATION_COUNT] = {
    "top_right_bottom_left", "top_right_left", "top_right_bottom", "top_right",
    "top_bottom_left",       "top_left",       "top_bottom",       "top",
    "right_bottom_left",     "right_left",     "right_bottom",     "right",
    "bottom_left",           "left",           "bottom",           "mid"};

typedef enum {
    AT_LOOP,
    AT_ONESHOT,
} animation_type;

typedef int anim_id;

typedef struct {
    bool active;

    animation_type type;

    double current_time;
    double animation_time;

    int current_frame;
    int max_frame;

    int finished;  // Only for oneshot. Is true on last frame of animation.
} animation;

#define NO_ANIMATION 255
animation anim_pool[MAX_ANIMATION_POOL] = {0};

anim_id new_animation(animation_type type, double animation_time, int max_frame) {
    for (int i = 0; i < MAX_ANIMATION_POOL; i++) {
        if (anim_pool[i].active == false) {
            anim_pool[i].active = true;
            anim_pool[i].type = type;
            anim_pool[i].current_time = 0;
            anim_pool[i].animation_time = animation_time;
            anim_pool[i].current_frame = 0;
            anim_pool[i].max_frame = max_frame;
            anim_pool[i].finished = false;
            return i;
        }
    }
    printf("Animation pool is full\n");
    exit(1);
}

void step_animations() {
    double ft = GetFrameTime();
    for (int i = 0; i < MAX_ANIMATION_POOL; i++) {
        animation *a = &anim_pool[i];
        if (a->active == false) {
            a->finished = false;
            continue;
        }
        a->current_time += ft;
        if (a->current_time >= a->animation_time) {
            a->current_time = 0;
            if (a->type == AT_LOOP) {
                a->current_frame = (a->current_frame + 1) % a->max_frame;
            } else if (a->type == AT_ONESHOT) {
                a->current_frame++;
                if (a->current_frame == a->max_frame) {
                    a->active = false;
                    a->finished = true;
                }
            }
        }
    }
}

int get_frame(anim_id id) {
    if (id >= MAX_ANIMATION_POOL) {
        printf("Invalid ID (%d > %d)\n", id, MAX_ANIMATION_POOL);
        return 0;
    }
    return anim_pool[id].current_frame;
}

double get_process(anim_id id) {
    if (id >= MAX_ANIMATION_POOL) {
        printf("Invalid ID (%d > %d)\n", id, MAX_ANIMATION_POOL);
        return 0;
    }

    animation *a = &anim_pool[id];
    if (a->finished == true) {
        return 1;
    }

    if (a->active == false) {
        return 0;
    }
    double progress = a->current_time / a->animation_time;
    return progress > 1 ? 1 : progress;
}

bool anim_finished(anim_id id) {
    if (id >= MAX_ANIMATION_POOL) {
        printf("Invalid ID (%d > %d)\n", id, MAX_ANIMATION_POOL);
        return false;
    }

    return anim_pool[id].finished;
}

// Waits for all AT_ONESHOT animations to be finshed. Returns true when all animations have ended
bool wait_for_animations() {
    for (int i = 0; i < MAX_ANIMATION_POOL; i++) {
        if (anim_pool[i].active && anim_pool[i].type == AT_ONESHOT && anim_pool[i].finished == false) {
            return false;
        }
    }
    return true;
}

typedef enum {
    PAS_NONE,
    PAS_MOVING,
    PAS_DAMAGE,
    PAS_BUMPING,
    PAS_DYING,
} player_animation_state;

typedef struct {
    bool init;
    // Position in grid space
    Vector2 position;
    char name[9];
    Color color;
    uint8_t health;
    uint8_t max_health;
    uint8_t spells[MAX_SPELL_COUNT];
    uint8_t cooldowns[MAX_SPELL_COUNT];
    player_action action;
    uint8_t selected_spell;

    spell_effect effect;
    uint8_t effect_round_left;

    anim_id animation;
    anim_id action_animation;
    player_animation_state animation_state;
    Vector2 moving_target;  // Where the player is trying to move. Used for animation
} player;

typedef struct {
    player_action action;
    Vector2 position;
    int spell;
} player_move;

player players[MAX_PLAYER_COUNT] = {0};
int player_count = 0;

typedef struct {
    int player;
    player_action action;
    Vector2 target;
    int spell;
} player_round_action;

// Buffer of actions to display
#define MAX_PLAYER_ROUND_ACTION_COUNT 4
player_round_action actions[MAX_PLAYER_ROUND_ACTION_COUNT] = {0};
int action_count = 0;
int action_step = 0;

typedef struct {
    int player;
    Vector2 position;
    int health;
    int max_health;
    spell_effect effect;
    int effect_round_left;
} player_round_update;

player_round_update updates[MAX_PLAYER_COUNT] = {0};

int current_player = 0;
round_state state = RS_PLAYING;
game_state gs = GS_WAITING;
int winner_id = 0;

uint8_t my_spells[MAX_SPELL_COUNT] = {0, 1, 3, 4, 0, 0, 0, 0};

bool is_cell_in_zone(Vector2 player, Vector2 origin, Vector2 cell, const spell *s);

#define FLOOR_TEXTURE_COUNT 8
Texture2D floor_textures[FLOOR_TEXTURE_COUNT] = {0};

Texture2D wall_textures[WALL_ORIENTATION_COUNT] = {0};

#define PLAYER_ANIMATION_COUNT 4
Texture2D player_textures[PLAYER_ANIMATION_COUNT] = {0};

#define WALL_TORCH_ANIMATION_COUNT 8
Texture2D wall_torch[WALL_TORCH_ANIMATION_COUNT] = {0};
anim_id torch_anim = 0;

#define SLASH_ANIMATION_COUNT 3
Texture2D slash_attack[SLASH_ANIMATION_COUNT] = {0};
anim_id slash_animation = NO_ANIMATION;
Vector2 slash_cell = {0};

void load_assets() {
    font = LoadFont("assets/fonts/default.ttf");
    box_side = LoadTexture("assets/sprites/ui/box_side.png");
    box_mid = LoadTexture("assets/sprites/ui/box_mid.png");
    spell_box = LoadTexture("assets/sprites/ui/spell_box.png");
    spell_box_select = LoadTexture("assets/sprites/ui/spell_box_select.png");
    game_slot = LoadTexture("assets/sprites/ui/game_slot.png");
    life_bar_bg = LoadTexture("assets/sprites/ui/life_bar_bg.png");

    for (int i = 0; i < FLOOR_TEXTURE_COUNT; i++) {
        floor_textures[i] = LoadTexture(TextFormat("assets/sprites/floor_%d.png", i + 1));
    }
    for (int i = 0; i < WALL_ORIENTATION_COUNT; i++) {
        wall_textures[i] = LoadTexture(TextFormat("assets/sprites/walls/wall_%s.png", wall_frames[i]));
    }
    for (int i = 0; i < PLAYER_ANIMATION_COUNT; i++) {
        player_textures[i] = LoadTexture(TextFormat("assets/sprites/wizzard_f_idle_anim_f%d.png", i));
    }
    for (int i = 0; i < WALL_TORCH_ANIMATION_COUNT; i++) {
        wall_torch[i] = LoadTexture(TextFormat("assets/sprites/wall_torch_anim_f%d.png", i));
    }
    for (int i = 0; i < SLASH_ANIMATION_COUNT; i++) {
        slash_attack[i] = LoadTexture(TextFormat("assets/sprites/attacks/slash_%d.png", i));
    }
}

void compute_map_variants() {
    init_map(&variants, game_map.width, game_map.height, NULL);
    for (int y = 0; y < game_map.height; y++) {
        for (int x = 0; x < game_map.width; x++) {
            if (get_map(&game_map, x, y) == 0) {
                if (rand() % 100 > 90) {  // 10% of chance to have a random floor cell texture
                    set_map(&variants, x, y, (rand() % FLOOR_TEXTURE_COUNT - 1) + 1);
                } else {
                    set_map(&variants, x, y, 0);
                }
            }
            if (get_map(&game_map, x, y) == 1) {
                int value = 0;
                value |= (get_map(&game_map, x, y + 1) == 1) << 0;
                value |= (get_map(&game_map, x - 1, y) == 1) << 1;
                value |= (get_map(&game_map, x + 1, y) == 1) << 2;
                value |= (get_map(&game_map, x, y - 1) == 1) << 3;
                set_map(&variants, x, y, value);
            }
        }
    }
}

void set_props_animations() {
    init_map(&props_animations, game_map.width, game_map.height, NULL);
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (get_map(&props, x, y)) {
                anim_id id = new_animation(AT_LOOP, 0.2f, WALL_TORCH_ANIMATION_COUNT);
                set_map(&props_animations, x, y, id);
            }
        }
    }
}

float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

void draw_cell(int x, int y, Color c) {
    Rectangle source = {0, 0, spell_box.width, spell_box.height};
    Rectangle dest = {x, y, CELL_SIZE, CELL_SIZE};
    DrawTexturePro(spell_box, source, dest, (Vector2){0}, 0, c);
}

void draw_cell_lines(int x, int y) {
    Rectangle source = {0, 0, spell_box_select.width, spell_box_select.height};
    Rectangle dest = {x - 8, y - 8, CELL_SIZE + 16, CELL_SIZE + 16};
    DrawTexturePro(spell_box_select, source, dest, (Vector2){0}, 0, WHITE);
}

bool v2eq(Vector2 a, Vector2 b) {
    return a.x == b.x && a.y == b.y;
}

Vector2 grid2screen(Vector2 g) {
    return (Vector2){base_x_offset + g.x * CELL_SIZE, base_y_offset + g.y * CELL_SIZE};
}

Vector2 screen2grid(Vector2 s) {
    if (s.x - base_x_offset < 0 || s.y - base_y_offset < 0) {
        return (Vector2){-1, -1};
    }
    return (Vector2){(int)((s.x - base_x_offset) / CELL_SIZE), (int)((s.y - base_y_offset) / CELL_SIZE)};
}

bool is_walkable(int x, int y) {
    return get_map(&game_map, x, y) != 1;
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
    if (p->effect != SE_NONE) {
        return (player_move){.action = PA_CANT_PLAY, .position = (Vector2){0, 0}};
    }

    const Vector2 g = screen2grid(GetMousePosition());
    if (get_map(&game_map, g.x, g.y) != -1 && IsMouseButtonPressed(0)) {
        if (p->action == PA_SPELL) {
            if (p->cooldowns[p->selected_spell] == 0 && player_cast_spell(p, g)) {
                int selected_spell = p->spells[p->selected_spell];
                p->cooldowns[p->selected_spell] = get_selected_spell()->cooldown;
                if (p->cooldowns[p->selected_spell] > 0) {
                    p->selected_spell = 0;
                }
                return (player_move){.action = PA_SPELL, .position = g, .spell = selected_spell};
            }
        }
    }
    return (player_move){.action = PA_NONE};
}

bool is_over_cell(int x, int y) {
    const Rectangle cell_rect = {x, y, CELL_SIZE, CELL_SIZE};
    return CheckCollisionPointRec(GetMousePosition(), cell_rect);
}

Texture2D get_cell_texture(int x, int y) {
    int cell_type = get_map(&game_map, x, y);
    if (cell_type == 0)
        return floor_textures[get_map(&variants, x, y)];
    if (cell_type == 1)
        return wall_textures[get_map(&variants, x, y)];
    return (Texture2D){0};
}

Color get_cell_color(int cell_type) {
    if (cell_type == 0)
        return PURPLE;
    if (cell_type == 1)
        return GRAY;
    return SKYBLUE;
}

Texture2D get_prop_texture(int prop_type, int anim_id) {
    if (prop_type == 1)
        return wall_torch[get_frame(anim_id)];
    return (Texture2D){0};
}

void render_map() {
    for (int y = 0; y < game_map.height; y++) {
        for (int x = 0; x < game_map.width; x++) {
            const int x_pos = base_x_offset + x * CELL_SIZE;
            const int y_pos = base_y_offset + y * CELL_SIZE;
            Texture2D t = get_cell_texture(x, y);
            if (t.id != 0) {
                Color tint = is_over_cell(x_pos, y_pos) ? RED : WHITE;
                DrawTextureEx(t, (Vector2){x_pos, y_pos}, 0, 4, tint);
            } else {
                Color c = PURPLE;
                DrawRectangle(x_pos, y_pos, CELL_SIZE, CELL_SIZE, c);
                DrawRectangleLines(x_pos, y_pos, CELL_SIZE, CELL_SIZE, BLACK);
            }
        }
    }

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            const int x_pos = base_x_offset + x * CELL_SIZE + 8;
            const int y_pos = base_y_offset + y * CELL_SIZE - 8;
            Texture2D texture = get_prop_texture(get_map(&props, x, y), get_map(&props_animations, x, y));
            DrawTextureEx(texture, (Vector2){x_pos, y_pos}, 0, 3, WHITE);
        }
    }
}

void render_player(player *p) {
    if (p->health <= 0 && p->animation_state == PAS_NONE)
        return;

    int x_pos = base_x_offset + p->position.x * CELL_SIZE + 8;
    int y_pos = base_y_offset + p->position.y * CELL_SIZE - 24;
    Vector2 player_position = {x_pos, y_pos};
    Texture2D player_texture = player_textures[get_frame(p->animation)];
    Color c = WHITE;

    if (p->effect == SE_STUN) {
        c = GRAY;
        player_texture = player_textures[0];
    }

    // Player is doing the slide animation
    if (p->animation_state == PAS_MOVING) {
        double progress = get_process(p->action_animation);

        int x_pos = base_x_offset + p->position.x * CELL_SIZE + 8;
        int y_pos = base_y_offset + p->position.y * CELL_SIZE - 24;

        const int x_target = base_x_offset + p->moving_target.x * CELL_SIZE + 8;
        const int y_target = base_y_offset + p->moving_target.y * CELL_SIZE - 24;
        player_position.x = lerp(x_pos, x_target, progress);
        player_position.y = lerp(y_pos, y_target, progress);

        if (anim_finished(p->action_animation)) {
            p->action_animation = NO_ANIMATION;
            p->animation_state = PAS_NONE;
            p->position = p->moving_target;
        }
    } else if (p->animation_state == PAS_DAMAGE) {
        c = RED;
        if (anim_finished(p->action_animation)) {
            p->action_animation = NO_ANIMATION;
            p->animation_state = PAS_NONE;
            if (p->health <= 0) {
                p->action_animation = new_animation(AT_ONESHOT, 5.f, 1);
                p->animation_state = PAS_DYING;
            }
        }
    } else if (p->animation_state == PAS_BUMPING) {
        double progress = get_process(p->action_animation);
        if (progress < 0.5f) {  // Moving forward
            const int x_target = base_x_offset + p->moving_target.x * CELL_SIZE + 8;
            const int y_target = base_y_offset + p->moving_target.y * CELL_SIZE - 24;
            player_position.x = lerp(x_pos, x_target, progress * 2);
            player_position.y = lerp(y_pos, y_target, progress * 2);
        } else {  // Moving backwards
            const int x_target = base_x_offset + p->moving_target.x * CELL_SIZE + 8;
            const int y_target = base_y_offset + p->moving_target.y * CELL_SIZE - 24;
            player_position.x = lerp(x_target, x_pos, (progress - 0.5) * 2);
            player_position.y = lerp(y_target, y_pos, (progress - 0.5) * 2);
        }

        if (anim_finished(p->action_animation)) {
            p->action_animation = NO_ANIMATION;
            p->animation_state = PAS_NONE;
            p->moving_target = (Vector2){0};
        }
    } else if (p->animation_state == PAS_DYING) {
        c = GREEN;
    }

    DrawTextureEx(player_texture, player_position, 0, 3, c);
}

bool is_cell_in_zone(Vector2 player, Vector2 origin, Vector2 cell, const spell *s) {
    if (is_walkable(cell.x, cell.y))
        return false;

    const int player_d = fabsf(player.x - cell.x) + fabsf(player.y - cell.y);
    const int spell_d = fabsf(origin.x - cell.x) + fabsf(origin.y - cell.y);
    if (spell_d >= s->zone_size || player_d > s->range)
        return false;
    return true;
}

void render_game_slot(Vector2 pos) {
    Rectangle source = {0, 0, game_slot.width, game_slot.height};
    Rectangle dest = {pos.x, pos.y, CELL_SIZE, CELL_SIZE};
    DrawTexturePro(game_slot, source, dest, (Vector2){0}, 0, WHITE);
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
                    render_game_slot(s);
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
                    render_game_slot(s);
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
                    render_game_slot(s);
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
        DrawText(TextFormat("Speed = %d", s->speed), tooltip.x + 8, tooltip.y + 58, 18, BLACK);
    } else if (s->type == ST_TARGET || s->type == ST_ZONE) {
        DrawText(TextFormat("Damage = %d", s->damage), tooltip.x + 8, tooltip.y + 24, 18, BLACK);
        DrawText(TextFormat("Range = %d", s->range), tooltip.x + 8, tooltip.y + 42, 18, BLACK);
        DrawText(TextFormat("Speed = %d", s->speed), tooltip.x + 8, tooltip.y + 58, 18, BLACK);
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
        const int x = base_x_offset + (CELL_SIZE + 8) * i;
        const int y = toolbar_y;

        bool on_cooldown = p->cooldowns[i] > 0;
        Color c = on_cooldown ? GRAY : icons[all_spells[p->spells[i]].icon];
        draw_cell(x, y, c);

        if (on_cooldown) {
            const char *text = TextFormat("%d", p->cooldowns[i]);
            Vector2 size = MeasureTextEx(GetFontDefault(), text, 56, 0);
            DrawText(text, x + (CELL_SIZE - size.x) / 2, y + (CELL_SIZE - size.y) / 2, 56, WHITE);
        }
    }

    if (p->action == PA_SPELL) {
        draw_cell_lines(base_x_offset + (CELL_SIZE + 8) * p->selected_spell, toolbar_y);
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
    if (p->cooldowns[p->selected_spell] == 0) {
        if (p->action == PA_SPELL) {
            render_spell_actions(p);
        }
    }
}

Rectangle render_box(int x, int y, int w, int h) {
    // Left side
    Rectangle source_left = {0, 0, box_side.width, box_side.height};
    float scale_factor_h = (float)h / (float)box_side.height;
    Rectangle dest_left = {x, y, box_side.width * scale_factor_h, h};
    DrawTexturePro(box_side, source_left, dest_left, (Vector2){0}, 0, WHITE);

    // Right side
    Rectangle source_right = {0, 0, -box_side.width, box_side.height};
    Rectangle dest_right = {x + w - box_side.width * scale_factor_h, y, box_side.width * scale_factor_h, h};
    DrawTexturePro(box_side, source_right, dest_right, (Vector2){0}, 0, WHITE);

    // Mid
    Rectangle source_mid = {0, 0, box_mid.width, box_mid.height};
    Rectangle dest_mid = {x + dest_left.width, y, dest_right.x - dest_left.x - dest_left.width, h};
    DrawTexturePro(box_mid, source_mid, dest_mid, (Vector2){0}, 0, WHITE);

    Rectangle res = {0};
    res.x = dest_mid.x;
    res.y = dest_mid.y + 7 * scale_factor_h;
    res.width = dest_right.x - dest_mid.x;
    res.height = (dest_mid.height - res.y) - 5 * scale_factor_h;
    return res;
}

Rectangle render_life_bar(int x, int y, int w, int health, int max_health) {
    Rectangle source = {0, 0, life_bar_bg.width, life_bar_bg.height};
    float ratio = (float)w / life_bar_bg.width;
    Rectangle dest = {x, y, 0.35 * w, life_bar_bg.height * ratio * 0.35};
    DrawTexturePro(life_bar_bg, source, dest, (Vector2){0}, 0, WHITE);

    int percent = ((float)health / (float)max_health) * 100;

    // TODO: Better way than fixed offsets
    for (int i = 0; i < 10; i++) {
        Rectangle life_cell = {x + 22 + 14 * i, y + 14, 8, 12};
        Color c = i * 10 <= percent ? RED : BLUE;
        DrawRectangleRec(life_cell, c);
    }
    return dest;
}

void render_infos() {
    int over_player = -1;

    const int player_info_width = (MAP_WIDTH * CELL_SIZE) / player_count;
    for (int i = 0; i < player_count; i++) {
        int start_x = base_x_offset + player_info_width * i;
        Rectangle inner = render_box(start_x, 0, player_info_width, base_y_offset);
        const int x = inner.x;
        const int y = inner.y;

        if (i == current_player) {
            DrawTextEx(font, TextFormat("%s (you)", players[i].name), (Vector2){x, y}, 24, 1, BLACK);
        } else {
            DrawTextEx(font, TextFormat("%s", players[i].name), (Vector2){x, y}, 24, 1, BLACK);
        }
        int life_bar_width = 0.35 * player_info_width;
        Rectangle life_bar_box = render_life_bar(x + inner.width - life_bar_width, y + 2, player_info_width,
                                                 players[i].health, players[i].max_health);

        if (players[i].effect_round_left > 0) {
            // TODO: Display an icon instead
            DrawText(TextFormat("Effect %d : %d rounds left", players[i].effect, players[i].effect_round_left), x,
                     y + 24, 24, BLACK);
        }

        if (CheckCollisionPointRec(GetMousePosition(), life_bar_box)) {
            over_player = i;
        }
    }
    if (over_player != -1) {
        player *p = &players[over_player];
        Vector2 mp = GetMousePosition();
        mp.y -= 36;
        DrawTextEx(GetFontDefault(), TextFormat("%d/%d", p->health, p->max_health), mp, 38, 1, BLACK);
    }
}

int client_fd = 0;

int connect_to_server(const char *ip, uint16_t port) {
    printf("Connecting to %s:%d\n", ip, port);

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

void player_join() {
    net_packet_join join = {0};
    memcpy(join.username, TextFormat("User %d", rand()), 8);
    send_sock(PKT_JOIN, &join, client_fd);
}

void reset_game() {
    gs = GS_WAITING;

    free_map(&game_map);
    free_map(&variants);
    free_map(&props);
    free_map(&props_animations);

    for (int i = 0; i < MAX_ANIMATION_POOL; i++) {
        anim_pool[i] = (animation){0};
    }

    player_count = 0;

    players[0].color = YELLOW;
    players[1].color = GREEN;
    players[2].color = BLUE;
    players[3].color = RED;

    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        players[i].animation = new_animation(AT_LOOP, 0.5f, PLAYER_ANIMATION_COUNT);
        players[i].action_animation = NO_ANIMATION;
        players[i].animation_state = PAS_NONE;
        players[i].action = PA_SPELL;
        players[i].selected_spell = 0;
    }

    action_count = 0;
    action_step = 0;

    player_join();
}

// We do not use pkt_player_update to set health and other things. I think a better way is to only use
//  PKT_PLAYER_ACTION to simulate the round and maybe resend a pkt_player_update at the end of the round to resync.
//  PKT_PLAYER_UPDATE could also be used just to force player informations when an admin updates it.
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

        memcpy(players[current_player].spells, my_spells, MAX_SPELL_COUNT);
        net_packet_player_build b = pkt_player_build(rand(), players[current_player].spells);
        send_sock(PKT_PLAYER_BUILD, &b, client_fd);
    } else if (p->type == PKT_MAP) {
        net_packet_map *m = (net_packet_map *)p->content;
        printf("Map is %d/%d\n", m->width, m->height);
        if (m->type == MLT_BACKGROUND) {
            init_map(&game_map, m->width, m->height, m->content);
            compute_map_variants();
        } else if (m->type == MLT_PROPS) {
            init_map(&props, m->width, m->height, m->content);
            set_props_animations();
        } else {
            printf("Unknown map layer\n");
            exit(1);
        }
    } else if (p->type == PKT_GAME_START) {
        printf("Starting Game !!\n");
        gs = GS_STARTED;
    } else if (p->type == PKT_PLAYER_UPDATE) {
        net_packet_player_update *u = (net_packet_player_update *)p->content;
        printf("Player Update %d %d %d H=%d\n", u->id, u->x, u->y, u->health);
        player *player = &players[u->id];

        if (gs == GS_WAITING) {
            player->health = u->health;
            player->max_health = u->max_health;
            player->position.x = u->x;
            player->position.y = u->y;
            player->effect = u->effect;
            player->effect_round_left = u->effect_round_left;
        } else {
            updates[u->id].position = (Vector2){u->x, u->y};
            updates[u->id].health = u->health;
            updates[u->id].max_health = u->max_health;
            updates[u->id].effect = u->effect;
            updates[u->id].effect_round_left = u->effect_round_left;
        }
    } else if (p->type == PKT_PLAYER_ACTION) {
        net_packet_player_action *a = (net_packet_player_action *)p->content;
        printf("New action: %d %d\n", a->id, a->spell);
        player_round_action *action = &actions[action_count];
        action->player = a->id;
        action->action = a->action;
        action->target = (Vector2){a->x, a->y};
        action->spell = a->spell;
        action_count++;
    } else if (p->type == PKT_ROUND_END) {
        printf("Round ended\n");
        state = RS_PLAYING_ROUND;
    } else if (p->type == PKT_GAME_END) {
        net_packet_game_end *e = (net_packet_game_end *)p->content;
        winner_id = e->winner_id;
        state = RS_PLAYING_ROUND;
        gs = GS_ENDING;
    } else if (p->type == PKT_GAME_RESET) {
        printf("Client: game reset");
        reset_game();
    }

    free(p->content);
}

void play_round() {
    player_round_action *a = &actions[action_step];
    printf("Playing round %d/%d for %d\n", action_step, action_count, a->player);
    player *p = &players[a->player];

    if (a->action == PA_SPELL) {
        const spell *s = &all_spells[a->spell];
        printf("Casting %s\n", s->name);

        player *target = NULL;
        for (int i = 0; i < player_count; i++) {
            if (v2eq(a->target, players[i].position)) {
                target = &players[i];
                break;
            }
        }

        if (s->type == ST_MOVE) {
            p->action_animation = new_animation(AT_ONESHOT, 0.3f, 1);
            p->moving_target = a->target;
            p->animation_state = target == NULL ? PAS_MOVING : PAS_BUMPING;
        } else if (s->type == ST_TARGET) {
            if (s->effect == SE_STUN) {
                if (target != NULL) {
                    target->effect = SE_STUN;
                    target->effect_round_left = s->effect_duration;
                }
            } else {
                slash_animation = new_animation(AT_ONESHOT, 0.3f / 3.f, 3);
                slash_cell = a->target;
                if (target != NULL) {
                    target->action_animation = new_animation(AT_ONESHOT, 0.3f, 1);
                    target->animation_state = PAS_DAMAGE;
                    target->health -= s->damage;
                }
            }
        }
    } else if (a->action == PA_CANT_PLAY) {
        if (p->effect_round_left > 0) {
            p->action_animation = new_animation(AT_ONESHOT, 1.f, 1);
            p->animation_state = PAS_DAMAGE;  // TODO: Do another animation maybe?
        }
    }
}

void end_round() {
    for (int i = 0; i < player_count; i++) {
        players[i].health = updates[i].health;
        players[i].position = updates[i].position;
        players[i].effect = updates[i].effect;
        players[i].effect_round_left = updates[i].effect_round_left;
        for (int j = 0; j < MAX_SPELL_COUNT; j++) {
            if (players[i].cooldowns[j] > 0) {
                players[i].cooldowns[j]--;
            }
        }
    }
    action_step = 0;
    action_count = 0;
}

int main(int argc, char **argv) {
    char *ip = "127.0.0.1";
    int port = 3000;

    POPARG(argc, argv);  // program name
    while (argc > 0) {
        const char *arg = POPARG(argc, argv);
        if (strcmp(arg, "--ip") == 0) {
            ip = POPARG(argc, argv);
        } else if (strcmp(arg, "--port") == 0) {
            const char *value = POPARG(argc, argv);
            if (!strtoint(value, &port)) {
                printf("Error parsing port to int '%s'\n", value);
                exit(1);
            }
        } else {
            printf("Unknown arg : '%s'\n", arg);
            exit(1);
        }
    }

    srand(time(NULL));
    InitWindow(WIDTH, HEIGHT, "Duel Game");
    SetTargetFPS(60);

    load_assets();

    if (connect_to_server(ip, port) < 0) {
        fprintf(stderr, "Could not connect to server\n");
        exit(1);
    }

    printf("Connected to server !\n");

    reset_game();

    fd_set master_set;
    FD_ZERO(&master_set);
    FD_SET(client_fd, &master_set);
    int fd_count = client_fd;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

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

        step_animations();

        BeginDrawing();
        {
            ClearBackground(GetColor(0x181818FF));

            if (gs == GS_WAITING) {
                DrawText("Waiting for game to start", 0, 0, 64, WHITE);
            } else if (gs == GS_STARTED || gs == GS_ENDING) {
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
                                                                       move.position.y, move.spell);
                        send_sock(PKT_PLAYER_ACTION, &a, client_fd);
                    }
                }

                // TODO: When waiting, we should preview our action

                render_map();
                for (int i = 0; i < player_count; i++) {
                    render_player(&players[i]);
                }

                if (slash_animation != NO_ANIMATION) {
                    Texture2D slash_texture = slash_attack[get_frame(slash_animation)];
                    Vector2 position = grid2screen(slash_cell);
                    DrawTextureEx(slash_texture, position, 0, 1, WHITE);
                    if (anim_finished(slash_animation)) {
                        slash_animation = NO_ANIMATION;
                    }
                }

                if (state == RS_PLAYING) {
                    render_player_actions(&players[current_player]);
                } else if (state == RS_WAITING) {
                } else if (state == RS_PLAYING_ROUND) {
                    play_round();
                    state = RS_WAITING_ANIMATIONS;
                } else if (state == RS_WAITING_ANIMATIONS) {
                    if (wait_for_animations()) {
                        if (action_step < action_count - 1) {
                            action_step++;
                            state = RS_PLAYING_ROUND;
                        } else {
                            end_round();
                            state = RS_PLAYING;
                        }
                    }
                }
                render_infos();

                // Wait for all animations to finish before really ending the game
                // TODO: Broken
                if (gs == GS_ENDING && wait_for_animations()) {
                    gs = GS_ENDED;
                }
            } else if (gs == GS_ENDED) {
                if (IsKeyPressed(KEY_R)) {
                    // TODO: Should be checked server side
                    if (current_player == 0) {
                        send_sock(PKT_GAME_RESET, NULL, client_fd);
                    }
                }

                if (winner_id == 255) {
                    DrawText("Game draw", 0, 0, 64, WHITE);
                } else {
                    DrawText(TextFormat("%s won the game !", players[winner_id].name), 0, 0, 64, WHITE);
                }

                if (current_player == 0) {
                    DrawText("Press R to reset the game!", 0, 72, 64, WHITE);
                }
            }
        }
        EndDrawing();
    }
    CloseWindow();
}
