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
#include "ui.h"

extern const spell all_spells[];
extern const int spell_count;

#define WIDTH 1280
#define HEIGHT 720

// TODO: Cell size should be given by the map
#define CELL_SIZE 64.0

#define MAX_ANIMATION_POOL 16

// Scenes

typedef enum {
    SCENE_MAIN_MENU,
    SCENE_IN_GAME,
} game_scene;

game_scene active_scene = SCENE_MAIN_MENU;

// UI
Font font = {0};
Texture2D simple_border = {0};
Texture2D box = {0};
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

int base_x_offset = 0;
int base_y_offset = 0;

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
    PAS_STUNNED,
    PAS_BURNING,
    PAS_BUMPING,
    PAS_DYING,
} player_animation_state;

typedef struct {
    player_action action;
    Vector2 position;
    int spell;
} player_move;

typedef struct {
    int id;
    bool init;
    // Position in grid space
    Vector2 position;
    char name[9];
    Color color;
    int health;
    uint8_t max_health;
    bool dead;

    uint8_t spells[MAX_SPELL_COUNT];
    uint8_t cooldowns[MAX_SPELL_COUNT];
    player_action action;
    uint8_t selected_spell;
    map_layer action_range;

    player_move round_move;
    bool waiting;

    // TODO: We should be able to stack effects (stun + burn)
    spell_effect effect;
    const spell *spell_effect;
    uint8_t effect_round_left;

    anim_id animation;
    anim_id action_animation;
    player_animation_state animation_state;
    Vector2 moving_target;  // Where the player is trying to move. Used for animation
} player;

player players[MAX_PLAYER_COUNT] = {0};
int player_count = 0;
char current_player_username[8] = {0};
slider health_bars[MAX_PLAYER_COUNT] = {0};

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

uint8_t my_spells[MAX_SPELL_COUNT] = {0, 1, 2, 3};
button toolbar_spells_buttons[MAX_SPELL_COUNT] = {0};

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
    simple_border = LoadTexture("assets/sprites/ui/button.png");
    box = LoadTexture("assets/sprites/ui/box.png");
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
    for (int y = 0; y < game_map.height; y++) {
        for (int x = 0; x < game_map.width; x++) {
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

bool can_player_move(player *p, Vector2 cell) {
    return get_map(&p->action_range, cell.x, cell.y) == 1;
}

const spell *get_selected_spell() {
    player *p = &players[current_player];
    return &all_spells[p->spells[p->selected_spell]];
}

typedef struct {
    int x, y, dist;
} Node;

typedef struct {
    Node *nodes;
    int front, rear;
} Queue;

void compute_spell_range(player *p) {
    Queue q = {.front = 0, .rear = 0};
    q.nodes = calloc(game_map.width * game_map.height, sizeof(Node));
    q.nodes[q.rear++] = (Node){p->position.x, p->position.y, 0};
    clear_map(&p->action_range);
    set_map(&p->action_range, p->position.x, p->position.y, 1);

    int range = get_selected_spell()->range;
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    while (q.front != q.rear) {
        Node current = q.nodes[q.front++];

        if (current.dist == range) {
            continue;
        }

        for (int i = 0; i < 4; i++) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            if (get_map(&game_map, nx, ny) == 0 && get_map(&p->action_range, nx, ny) == 0) {
                set_map(&p->action_range, nx, ny, 1);
                q.nodes[q.rear++] = (Node){nx, ny, current.dist + 1};
            }
        }
    }

    free(q.nodes);
}

int player_cast_spell(player *p, Vector2 origin) {
    const spell *s = get_selected_spell();
    if (s->type == ST_MOVE || s->type == ST_TARGET) {
        return can_player_move(p, origin);
    } else {
        printf("not implemented\n");
        exit(1);
    }
    return false;
}

player_move player_exec_action(player *p) {
    if (p->effect == SE_STUN) {
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

    for (int y = 0; y < game_map.height; y++) {
        for (int x = 0; x < game_map.width; x++) {
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
            p->position = p->moving_target;
        }
    } else if (p->animation_state == PAS_DAMAGE) {
        c = RED;
        if (anim_finished(p->action_animation)) {
            if (p->health <= 0) {
                p->action_animation = new_animation(AT_ONESHOT, 5.f, 1);
                p->animation_state = PAS_DYING;
            }
        }
    } else if (p->animation_state == PAS_STUNNED) {
        c = GRAY;
        player_texture = player_textures[0];
    } else if (p->animation_state == PAS_BURNING) {
        c = ORANGE;
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
            p->moving_target = (Vector2){0};
        }
    } else if (p->animation_state == PAS_DYING) {
        c = GREEN;
        if (anim_finished(p->action_animation)) {
            p->dead = true;
        }
    }

    if (p->animation_state != PAS_NONE && anim_finished(p->action_animation)) {
        p->action_animation = NO_ANIMATION;
        p->animation_state = PAS_NONE;
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

void set_selected_spell(player *p, int spell) {
    p->selected_spell = spell;
    compute_spell_range(p);
}

void render_spell_actions(player *p) {
    const spell *s = get_selected_spell();
    if (s->type == ST_MOVE) {
        for (int x = -s->range; x <= s->range; x++) {
            for (int y = -s->range; y <= s->range; y++) {
                const int x_pos = p->position.x + x;
                const int y_pos = p->position.y + y;
                if (can_player_move(p, (Vector2){x_pos, y_pos})) {
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
                if (can_player_move(p, g)) {
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

void render_spell_tooltip(const spell *s) {
    Rectangle rec = {GetMousePosition().x, GetMousePosition().y, 400, 150};
    const char *description =
        TextFormat("%s\nDamage: %d | Range: %d | Speed: %d", s->description, s->damage, s->range, s->speed);
    render_tooltip(rec, s->name, description);
}

bool is_over_toolbar_cell(uint8_t cell_id) {
    int toolbar_y = base_y_offset + CELL_SIZE * game_map.height + 16;
    Rectangle cell = {base_x_offset + (CELL_SIZE + 8) * cell_id, toolbar_y, CELL_SIZE, CELL_SIZE};
    Vector2 mouse = GetMousePosition();
    return CheckCollisionPointRec(mouse, cell);
}

void render_player_actions(player *p) {
    // Toolbar rendering
    int hoverred_button = -1;
    for (int i = 0; i < MAX_SPELL_COUNT; i++) {
        button *b = &toolbar_spells_buttons[i];
        bool on_cooldown = p->cooldowns[i] > 0;
        Color c = on_cooldown ? GRAY : icons[all_spells[p->spells[i]].icon];
        if (on_cooldown) {
            const char *text = TextFormat("%d", p->cooldowns[i]);
            toolbar_spells_buttons[i].text = text;
        }

        b->color = c;
        button_render(b);

        if (button_hover(b)) {
            hoverred_button = p->spells[i];
        }
        if (button_clicked(b)) {
            p->action = PA_SPELL;
            p->selected_spell = i;
        }
    }

    if (p->action == PA_SPELL) {
        draw_cell_lines(base_x_offset + (CELL_SIZE + 8) * p->selected_spell, toolbar_spells_buttons[0].rec.y);
    }

    if (hoverred_button != -1) {
        render_spell_tooltip(&all_spells[hoverred_button]);
    }

    // In-game rendering
    if (p->cooldowns[p->selected_spell] == 0) {
        if (p->action == PA_SPELL) {
            render_spell_actions(p);
        }
    }
}

void render_preview_move(Vector2 pos) {
    Rectangle source = {0, 0, game_slot.width, game_slot.height};
    Rectangle dest = {pos.x, pos.y, CELL_SIZE, CELL_SIZE};
    DrawTexturePro(game_slot, source, dest, (Vector2){0}, 0, GRAY);
}

// TODO: Preview animation
void render_player_move(player_move *move) {
    if (move->action == PA_SPELL) {
        Vector2 pos = grid2screen(move->position);
        render_preview_move(pos);
    }
}

void render_infos() {
    int over_player = -1;

    const int player_info_width = (game_map.width * CELL_SIZE) / player_count;
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

        slider_render(&health_bars[i]);
        if (slider_hover(&health_bars[i])) {
            over_player = i;
        }

        if (players[i].effect_round_left > 0) {
            // TODO: Display an icon instead
            DrawText(TextFormat("Effect %d : %d rounds left", players[i].effect, players[i].effect_round_left), x,
                     y + 24, 24, BLACK);
        }
    }

    if (over_player != -1) {
        player *p = &players[over_player];
        Rectangle rec = {GetMousePosition().x, GetMousePosition().y, 200, 75};
        const char *description = TextFormat("%d/%d", p->health, p->max_health);
        render_tooltip(rec, "Health", description);
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
    memcpy(join.username, current_player_username, 8);
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

void init_in_game_ui() {
    int toolbar_y = base_y_offset + CELL_SIZE * game_map.height + 16;
    for (int i = 0; i < MAX_SPELL_COUNT; i++) {
        int x = base_x_offset + (CELL_SIZE + 8) * i;
        toolbar_spells_buttons[i].rec = (Rectangle){x, toolbar_y, CELL_SIZE, CELL_SIZE};
        toolbar_spells_buttons[i].color = icons[all_spells[my_spells[i]].icon];
        toolbar_spells_buttons[i].font_size = 56;
        toolbar_spells_buttons[i].text = NULL;
    }

    const int player_info_width = (game_map.width * CELL_SIZE) / player_count;
    for (int i = 0; i < player_count; i++) {
        int y = 35;
        int life_bar_width = 0.35 * player_info_width;
        int height = 32;
        int x = base_x_offset + player_info_width * (i + 1) - life_bar_width * 1.25f;
        health_bars[i].rec = (Rectangle){x, y, life_bar_width, height};
        health_bars[i].color = RED;
        health_bars[i].max = players[i].max_health;
        health_bars[i].value = players[i].max_health;
    }
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
        net_packet_player_build b = pkt_player_build(100, players[current_player].spells);
        send_sock(PKT_PLAYER_BUILD, &b, client_fd);
    } else if (p->type == PKT_MAP) {
        net_packet_map *m = (net_packet_map *)p->content;
        printf("Map is %d/%d\n", m->width, m->height);
        if (m->type == MLT_BACKGROUND) {
            init_map(&game_map, m->width, m->height, m->content);
            init_map(&players[current_player].action_range, game_map.width, game_map.height, NULL);
            compute_map_variants();
            base_x_offset = (WIDTH - (CELL_SIZE * game_map.width)) / 2;
            base_y_offset = (HEIGHT - (CELL_SIZE * game_map.height)) / 2;
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
        set_selected_spell(&players[current_player], 0);
        init_in_game_ui();
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
        printf("Game last round\n");
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

        // TODO: We should do pathfinding instead of sliding across the map
        if (s->type == ST_MOVE) {
            p->action_animation = new_animation(AT_ONESHOT, .3f, 1);
            p->moving_target = a->target;
            p->animation_state = target == NULL ? PAS_MOVING : PAS_BUMPING;
        } else if (s->type == ST_TARGET) {
            slash_animation = new_animation(AT_ONESHOT, 0.3f / 3.f, 3);
            slash_cell = a->target;
            if (target != NULL) {
                target->health -= s->damage;
                health_bars[target->id].value = target->health;
                target->action_animation = new_animation(AT_ONESHOT, 0.3f, 1);
                target->animation_state = PAS_DAMAGE;
                if (s->effect != SE_NONE) {
                    target->effect = s->effect;
                    target->effect_round_left = s->effect_duration;
                    target->spell_effect = s;
                }
            }
        }
    } else if (a->action == PA_CANT_PLAY) {
        if (p->effect_round_left > 0) {
            p->action_animation = new_animation(AT_ONESHOT, 1.f, 1);
            p->animation_state = PAS_STUNNED;
        }
    }
    p->waiting = false;
}

void play_effects() {
    for (int i = 0; i < player_count; i++) {
        players[i].position = updates[i].position;
        if (players[i].effect == SE_BURN) {
            players[i].health -= players[i].spell_effect->damage;
            printf("player took a tick of burn and has %d hp left\n", players[i].health);
            players[i].action_animation = new_animation(AT_ONESHOT, 1.f, 1);
            players[i].animation_state = PAS_BURNING;
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
    compute_spell_range(&players[current_player]);
    if (gs == GS_ENDING) {
        gs = GS_ENDED;
    }
}

fd_set master_set;
int fd_count;
struct timeval timeout;

bool join_game(const char *ip, int port, const char *username) {
    if (connect_to_server(ip, port) < 0) {
        fprintf(stderr, "Could not connect to server\n");
        return false;
    }

    printf("Connected to server !\n");

    strncpy(current_player_username, username, 8);
    reset_game();

    FD_ZERO(&master_set);
    FD_SET(client_fd, &master_set);
    fd_count = client_fd;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    active_scene = SCENE_IN_GAME;
    return true;
}

void render_scene_in_game() {
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

        const int keybinds[] = {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I};
        if (gs == GS_WAITING) {
            DrawText("Waiting for game to start", 0, 0, 64, WHITE);
        } else if (gs == GS_STARTED || gs == GS_ENDING) {
            round_state next_state = state;
            if (state == RS_PLAYING) {
                for (int i = 0; i < MAX_SPELL_COUNT; i++) {
                    if (IsKeyPressed(keybinds[i])) {
                        players[current_player].action = PA_SPELL;
                        set_selected_spell(&players[current_player], i);
                    }
                }
            }

            if (state == RS_PLAYING) {
                player_move move = player_exec_action(&players[current_player]);
                if (move.action != PA_NONE) {
                    next_state = RS_WAITING;
                    // Send action to server
                    net_packet_player_action a =
                        pkt_player_action(current_player, move.action, move.position.x, move.position.y, move.spell);
                    send_sock(PKT_PLAYER_ACTION, &a, client_fd);
                    players[current_player].round_move = move;
                    players[current_player].waiting = true;
                }
            } else if (state == RS_PLAYING_ROUND) {
                play_round();
                next_state = RS_WAITING_ANIMATIONS;
            } else if (state == RS_WAITING_ANIMATIONS) {
                if (wait_for_animations()) {
                    if (action_step < action_count - 1) {
                        action_step++;
                        next_state = RS_PLAYING_ROUND;
                    } else {
                        play_effects();
                        next_state = RS_PLAYING_EFFECTS;
                    }
                }
            } else if (state == RS_PLAYING_EFFECTS) {
                for (int i = 0; i < player_count; i++) {
                    if (players[i].animation_state == PAS_DYING && anim_finished(players[i].action_animation)) {
                        players[i].dead = true;
                    }
                }
                if (wait_for_animations()) {
                    for (int i = 0; i < player_count; i++) {
                        if (players[i].health <= 0 && players[i].dead == false) {
                            players[i].action_animation = new_animation(AT_ONESHOT, 3.f, 1);
                            players[i].animation_state = PAS_DYING;
                        }
                    }
                }

                if (wait_for_animations()) {
                    end_round();
                    next_state = RS_PLAYING;
                }
            }

            // TODO: When waiting, we should preview our action

            render_map();

            // TODO: Always render currently animation player on top
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
                render_player_move(&players[current_player].round_move);
            } else if (state == RS_PLAYING_ROUND || state == RS_WAITING_ANIMATIONS) {
                if (players[current_player].waiting) {
                    render_player_move(&players[current_player].round_move);
                }
            }
            render_infos();
            state = next_state;
            for (int i = 0; i < player_count; i++) {
                player *p = &players[i];
                Vector2 screen_pos = grid2screen(p->position);
                Rectangle player_rec = {screen_pos.x, screen_pos.y, CELL_SIZE, CELL_SIZE};
                Vector2 mp = GetMousePosition();
                if (CheckCollisionPointRec(mp, player_rec)) {
                    Rectangle player_box = render_box(mp.x, mp.y, 350, 125);
                    int box_center = get_width_center(player_box, p->name, 32);
                    DrawText(TextFormat("%s", p->name), box_center, player_box.y, 32, BLACK);
                }
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

const int card_width = (WIDTH - 150) / 2;
card c = {(Rectangle){50, 150, card_width, 550}, UI_NORD};
card c2 = {(Rectangle){665, 150, card_width, 550}, UI_NORD};

input_buf ip_buf = {.prefix = "IP", .max_length = 32};
input_buf port_buf = {.prefix = "Port", .max_length = 32};
input_buf password_buf = {.prefix = "Password", .max_length = 32};
input_buf username_buf = {.prefix = "Username", .max_length = 8};

input_buf *inputs[] = {&ip_buf, &port_buf, &password_buf, &username_buf};
#define MAIN_MENU_INPUT_COUNT (int)(sizeof(inputs) / sizeof(inputs[0]))
size_t selected_input = 0;

const char *error = NULL;

button confirm_button = {(Rectangle){0}, UI_GREEN, "Connect", 32};

button *spell_select_buttons;
bool *spell_selection;

const char *try_join() {
    int total = 0;
    for (int i = 0; i < spell_count; i++) {
        total += spell_selection[i] == true;
    }

    if (total != MAX_SPELL_COUNT) {
        return "You need to select MAX_SPELL_COUNT spells";
    }

    int ptr = 0;
    for (int i = 0; i < spell_count; i++) {
        if (spell_selection[i]) {
            my_spells[ptr] = i;
            ptr++;
        }
    }

    int port;
    if (strtoint(input_to_text(&port_buf), &port) == false) {
        return "Invalid port format";
    }

    if (input_is_blank(&username_buf)) {
        return "No username";
    }

    input_trim(&username_buf);
    const char *username = input_to_text(&username_buf);
    if (join_game(input_to_text(&ip_buf), port, username) == false) {
        return "Could not join server.";
    }
    return NULL;
}

buttoned_slider health_slider = {0};
buttoned_slider physic_power_slider = {0};
buttoned_slider magic_power_slider = {0};
buttoned_slider *stat_sliders[] = {&health_slider, &physic_power_slider, &magic_power_slider};
const int stat_sliders_count = (int)(sizeof(stat_sliders) / sizeof(stat_sliders[0]));

int get_total_stats() {
    int stat_total = 0;
    stat_total += health_slider.slider.value;
    stat_total += physic_power_slider.slider.value;
    stat_total += magic_power_slider.slider.value;
    return stat_total;
}

void render_scene_main_menu() {
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        input_erase(inputs[selected_input]);
    } else if (IsKeyPressed(KEY_ENTER)) {
        error = try_join();
    } else if ((IsKeyPressed(KEY_TAB) || IsKeyPressedRepeat(KEY_TAB)) && !IsKeyDown(KEY_LEFT_SHIFT)) {
        selected_input = (selected_input + 1) % MAIN_MENU_INPUT_COUNT;
    } else if ((IsKeyPressed(KEY_TAB) || IsKeyPressedRepeat(KEY_TAB)) && IsKeyDown(KEY_LEFT_SHIFT)) {
        if (selected_input == 0) {
            selected_input = MAIN_MENU_INPUT_COUNT;
        } else {
            selected_input--;
        }
    }

    int key;
    while ((key = GetCharPressed())) {
        input_write(inputs[selected_input], key);
    }

    for (int i = 0; i < MAIN_MENU_INPUT_COUNT; i++) {
        if (input_clicked(inputs[i])) {
            selected_input = i;
        }
    }

    if (button_clicked(&confirm_button)) {
        error = try_join();
    }

    for (int i = 0; i < spell_count; i++) {
        if (button_clicked(&spell_select_buttons[i])) {
            int total = 0;
            for (int j = 0; j < spell_count; j++) {
                total += spell_selection[j] == true;
            }
            if (spell_selection[i] == false && total == MAX_SPELL_COUNT) {
                error = "Spell selection limit reached";
            } else {
                spell_selection[i] = !spell_selection[i];
                if (spell_selection[i]) {
                    spell_select_buttons[i].text = "O";
                } else {
                    spell_select_buttons[i].text = "X";
                }
            }
        }
    }

    for (int i = 0; i < stat_sliders_count; i++) {
        buttoned_slider *b = stat_sliders[i];
        if (button_clicked(&b->minus)) {
            buttoned_slider_decrement(b);
        }
        if (button_clicked(&b->plus)) {
            if (get_total_stats() < 150) {
                buttoned_slider_increment(b);
            }
        }
    }

    BeginDrawing();
    {
        ClearBackground(UI_BLACK);
        int title_center = get_width_center((Rectangle){0, 0, WIDTH, HEIGHT}, "Duel Game", 64);
        DrawText("Duel Game", title_center, 32, 64, WHITE);
        card_render(&c);
        DrawTextCenter(c.rec, "Server", 46, WHITE);

        card_render(&c2);
        DrawTextCenter(c2.rec, "Build", 46, WHITE);

        for (size_t i = 0; i < MAIN_MENU_INPUT_COUNT; i++) {
            input_render(inputs[i], i == selected_input);
        }
        button_render(&confirm_button);
        if (error != NULL) {
            DrawText(error, 0, 0, 32, UI_RED);
        }

        int button_tooltip = -1;
        int total = 0;
        for (int i = 0; i < spell_count; i++) {
            if (button_hover(&spell_select_buttons[i])) {
                button_tooltip = i;
            }
            button_render(&spell_select_buttons[i]);
            total += spell_selection[i] == true;
        }
        DrawText(TextFormat("Total : %d/%d", total, 4), c2.rec.x + 8, c.rec.y + 100, 32, WHITE);

        DrawText(TextFormat("Health", (int)health_slider.slider.value), c2.rec.x + 8, health_slider.rec.y, 32, WHITE);
        buttoned_slider_render(&health_slider);

        DrawText(TextFormat("AD", (int)physic_power_slider.slider.value), c2.rec.x + 8, physic_power_slider.rec.y, 32,
                 WHITE);
        buttoned_slider_render(&physic_power_slider);

        DrawText(TextFormat("AP", (int)magic_power_slider.slider.value), c2.rec.x + 8, magic_power_slider.rec.y, 32,
                 WHITE);
        buttoned_slider_render(&magic_power_slider);

        DrawText(TextFormat("Total stats: %d / 150", get_total_stats()), c2.rec.x + 8, c2.rec.y + c2.rec.height - 32,
                 32, WHITE);

        if (button_tooltip != -1) {
            render_spell_tooltip(&all_spells[button_tooltip]);
        }
    }
    EndDrawing();
}

int main(int argc, char **argv) {
    srand(time(NULL));

    char *ip = NULL;
    int port = 0;
    const char *username = NULL;

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
        } else if (strcmp(arg, "--username") == 0) {
            username = POPARG(argc, argv);
        } else {
            printf("Unknown arg : '%s'\n", arg);
            exit(1);
        }
    }

    InitWindow(WIDTH, HEIGHT, "Duel Game");
    SetTargetFPS(60);

    load_assets();

    if (username == NULL) {
        username = TextFormat("User %d", rand());
    }

    if (ip != NULL) {
        join_game(ip, port, username);
    } else {
        input_set_text(&ip_buf, "127.0.0.1");
        input_set_text(&port_buf, "3000");
        input_set_text(&username_buf, username);

        for (int i = 0; i < MAIN_MENU_INPUT_COUNT; i++) {
            inputs[i]->rec = (Rectangle){c.rec.x + 8, c.rec.y + 100 + 60 * i, c.rec.width - 25, 50};
        }
        confirm_button.rec = (Rectangle){c.rec.x + 8, c.rec.y + c.rec.height - 100, c.rec.width - 25, 75};

        spell_select_buttons = malloc(sizeof(button) * spell_count);
        spell_selection = malloc(sizeof(bool) * spell_count);
        {
            int max_row_count = 4;
            int x = c2.rec.x + 8;
            int y = c2.rec.y + 150;
            for (int i = 0; i < spell_count; i++) {
                spell_select_buttons[i].rec = (Rectangle){x, y, 50, 50};
                spell_select_buttons[i].color = icons[all_spells[i].icon];
                spell_select_buttons[i].font_size = 32;
                spell_select_buttons[i].text = "X";
                spell_selection[i] = false;
                if (i != 0 && i % (max_row_count - 1) == 0) {
                    x = c2.rec.x + 8;
                    y += 60;
                } else {
                    x += 60;
                }
            }
        }
        {
            int w = c2.rec.width - 150;
            int y = 325;
            buttoned_slider_init(&health_slider, (Rectangle){c2.rec.x + 125, c2.rec.y + y, w, 50}, 100, 10);
            y += 60;
            buttoned_slider_init(&physic_power_slider, (Rectangle){c2.rec.x + 125, c2.rec.y + y, w, 50}, 100, 10);
            y += 60;
            buttoned_slider_init(&magic_power_slider, (Rectangle){c2.rec.x + 125, c2.rec.y + y, w, 50}, 100, 10);
        }
    }

    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        players[i].id = i;
    }

    while (!WindowShouldClose()) {
        if (active_scene == SCENE_MAIN_MENU) {
            render_scene_main_menu();
        } else if (active_scene == SCENE_IN_GAME) {
            render_scene_in_game();
        }
    }

    CloseWindow();
}
