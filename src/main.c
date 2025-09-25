#ifdef WINDOWS_BUILD
// If defined, the following flags inhibit definition of the indicated items.
#define NOGDICAPMASKS      // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES  // VK_*
#define NOWINMESSAGES      // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES        // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS       // SM_*
#define NOMENUS            // MF_*
#define NOICONS            // IDI_*
#define NOKEYSTATES        // MK_*
#define NOSYSCOMMANDS      // SC_*
#define NORASTEROPS        // Binary and Tertiary raster ops
#define NOSHOWWINDOW       // SW_*
#define OEMRESOURCE        // OEM Resource values
#define NOATOM             // Atom Manager routines
#define NOCLIPBOARD        // Clipboard routines
#define NOCOLOR            // Screen colors
#define NOCTLMGR           // Control and Dialog routines
#define NODRAWTEXT         // DrawText() and DT_*
#define NOGDI              // All GDI defines and routines
#define NOKERNEL           // All KERNEL defines and routines
#define NOUSER             // All USER defines and routines
#define NONLS              // All NLS defines and routines
#define NOMB               // MB_* and MessageBox()
#define NOMEMMGR           // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE         // typedef METAFILEPICT
#define NOMINMAX           // Macros min(a,b) and max(a,b)
#define NOMSG              // typedef MSG and associated routines
#define NOOPENFILE         // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL           // SB_* and scrolling routines
#define NOSERVICE          // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND            // Sound driver routines
#define NOTEXTMETRIC       // typedef TEXTMETRIC and associated routines
#define NOWH               // SetWindowsHook and WH_*
#define NOWINOFFSETS       // GWL_*, GCL_*, associated routines
#define NOCOMM             // COMM driver routines
#define NOKANJI            // Kanji support stuff.
#define NOHELP             // Help engine interface.
#define NOPROFILER         // Profiler interface.
#define NODEFERWINDOWPOS   // DeferWindowPos routines
#define NOMCX              // Modem Configuration Extensions
#define MMNOSOUND
typedef struct tagMSG *LPMSG;
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "assets.h"
#include "command.h"
#include "common.h"
#include "net.h"
#include "net_protocol.h"
#include "raylib.h"
#include "ui.h"
#include "version.h"

#define WIDTH 1280
#define HEIGHT 720
#define LOG_LINE_COUNT 20
#define CONSOLE_INPUT_MAX_LENGTH 32
#define CELL_SIZE 64
#define MAX_ANIMATION_POOL 128
#define NO_ANIMATION 255
#define MAX_PLAYER_ROUND_ACTION_COUNT MAX_PLAYER_COUNT
#define RAINDROP_COUNT 4
#define RAINDROP_RAND_SPAWNRATE rand() % 20 + 10
#define OUTER_WALL_COUNT 8
#define MAIN_MENU_INPUT_COUNT (int)(sizeof(inputs) / sizeof(inputs[0]))
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define MAX_PACKET_QUEUE_SIZE 128

#define FOREACH_PLAYER(IT, P)                                                    \
    for (int IT = 0; IT < MAX_PLAYER_COUNT; IT++)                                \
        for (player *P = &players[IT]; P != NULL && P->info.connected; P = NULL) \
            if (1)

#define V(x, y)  \
    (Vector2) {  \
        (x), (y) \
    }

extern const spell all_spells[];
extern const int spell_count;
extern const char *wall_frames[];

// Network
int server_fd = 0;
bool connected = false;
bool accepted = false;
uint64_t last_ping = 0;
fd_set master_set;
int fd_count;
struct timeval timeout;
pthread_t t_network;

queue pkt_queue;

// Console
bool console_open = false;
int log_base = 0;
char console_input[CONSOLE_INPUT_MAX_LENGTH] = {0};
int console_input_cursor = 0;

// Scenes
typedef enum {
    SCENE_MAIN_MENU,
    SCENE_LOBBY,
    SCENE_IN_GAME,
    SCENE_GAME_ENDED,
    SCENE_EDITOR,
} game_scene;

game_scene active_scene = SCENE_MAIN_MENU;

// Assets
Font font = {0};
Texture2D simple_border = {0};
Texture2D box = {0};
Texture2D spell_box = {0};
Texture2D spell_box_select = {0};
Texture2D game_slot = {0};
Texture2D life_bar_bg = {0};
Texture2D icons_sheet = {0};
Rectangle icons[SI_COUNT] = {0};
Texture2D effects_sheet = {0};
Rectangle effects[SE_COUNT] = {0};
Texture2D floor_textures = {0};
Texture2D wall_textures = {0};
Texture2D test_wall_textures = {0};
Texture2D player_textures = {0};
Texture2D wall_torch = {0};
Texture2D vine = {0};
Texture2D slash_attack = {0};
Texture2D heal_attack = {0};

Texture2D animation_sprite = {0};

// Sounds
Sound ui_button_clicked = {0};
Sound ui_tab_switch = {0};
Sound move_sound = {0};
Sound attack_sound = {0};
Sound stun_sound = {0};
Sound burn_sound = {0};
Sound heal_sound = {0};
Sound death_sound = {0};
Sound win_round_sound = {0};
Sound lose_round_sound = {0};
Sound error_sound = {0};
Sound raindrop_sound = {0};

// UI
//   Main menu
buttoned_slider health_slider = {0};
buttoned_slider strength_slider = {0};
buttoned_slider speed_slider = {0};
buttoned_slider *stat_sliders[] = {&health_slider, &strength_slider, &speed_slider};
const int stat_sliders_count = (int)(sizeof(stat_sliders) / sizeof(stat_sliders[0]));

const int card_width = (WIDTH - 150) / 2;
card server_info_card = CARD(50, 150, card_width, 550, UI_NORD, "Server");
card player_info_card = CARD(665, 150, card_width, 550, UI_NORD, "Spells", "Stats");

input_buf ip_buf = {.prefix = "IP", .max_length = 32};
input_buf port_buf = {.prefix = "Port", .max_length = 32};
input_buf password_buf = {.prefix = "Password", .max_length = 8};
input_buf username_buf = {.prefix = "Username", .max_length = 8};
button confirm_button = BUTTON_COLOR(0, 0, 0, 0, UI_GREEN, "Connect", 32);

// Build loading/saving
input_buf build_filepath = {.max_length = 10};
button build_load_button = BUTTON_COLOR(0, 0, 0, 0, UI_GREEN, "Load", 32);
button build_save_button = BUTTON_COLOR(0, 0, 0, 0, UI_GREEN, "Save", 32);

input_buf *inputs[] = {&ip_buf, &port_buf, &password_buf, &username_buf, &build_filepath};

//   Lobby
card player_list_card = CARD(50, 150, card_width, 550, UI_NORD, "Player list");
card server_config_card = CARD(665, 150, card_width, 550, UI_NORD, "Configuration");
button start_game_button = BUTTON_COLOR(75, 600, card_width - 50, 75, UI_GREEN, "Start Game", 36);

button player_build_buttons[MAX_PLAYER_COUNT] = {0};
card player_build_card = CARD(50, 150, WIDTH - 100, 550, UI_NORD, "Player build");
button close_build_card_button = BUTTON_COLOR(WIDTH - 125, 175, 50, 50, UI_RED, "X", 46);
int selected_player_build = -1;

picker map_picker = {0};
buttoned_slider round_count_slider = {0};

// Configuration sync
const char *selected_map = NULL;
int round_count = 0;

//    In game
slider health_bars[MAX_PLAYER_COUNT] = {0};
button toolbar_spells_buttons[MAX_SPELL_COUNT] = {0};

// Rendering
RenderTexture2D lightmap = {0};
RenderTexture2D ui = {0};

// Animation

typedef enum {
    AT_LOOP,
    AT_ONESHOT,
} animation_type;

typedef struct {
    bool active;

    animation_type type;

    double current_time;
    double animation_time;

    int current_frame;
    int max_frame;

    int finished;  // Only for oneshot. Is true on last frame of animation.
} animation;

typedef int anim_id;
animation anim_pool[MAX_ANIMATION_POOL] = {0};

// Player
typedef struct {
    player_action action;
    Vector2 position;
    int spell;
} player_move;

typedef enum {
    PAS_NONE,
    PAS_MOVING,
    PAS_DAMAGE,
    PAS_BURNING,
    PAS_BUMPING,
    PAS_DYING,
} player_animation_state;

typedef struct {
    player_info info;
    Color color;
    bool dead;
    uint8_t selected_spell;
    map_layer action_range;
    player_move round_move;
    anim_id animation;
    anim_id action_animation;
    player_animation_state animation_state;
    Vector2 moving_target;  // Where the player is trying to move. Used for animation
} player;

player players[MAX_PLAYER_COUNT] = {0};
int current_player = -1;
int master_player = 0;
uint8_t my_spells[MAX_SPELL_COUNT] = {0, 1, 2, 3};

typedef struct {
    double animation_time;
    int frame_count;
    Texture2D animation_sprite;
    Vector2 target_cell;
    Sound sound;

    player *caster;
    const spell *spell;
    player *target;

    bool active;
} animation_request;

queue spell_animation_queue = {0};
animation_request current_animation = {0};
anim_id current_spell_animation = NO_ANIMATION;

// Main menu
const char *error = NULL;
size_t selected_input = 0;
button *spell_select_buttons;
bool *spell_selection;
float error_time_remaining = 0.f;

// Turn
typedef struct {
    int player;
    player_action action;
    Vector2 target;
    int spell;
} player_turn_action;

player_turn_action actions[MAX_PLAYER_ROUND_ACTION_COUNT] = {0};
int action_count = 0;
int action_step = 0;

int effect_player_turn = 0;

typedef struct {
    int player;
    Vector2 position;
    net_player_stat stats[STAT_COUNT];
    spell_effect effect[SE_COUNT];
    int effect_round_left[SE_COUNT];
    int banned_spells[MAX_SPELL_COUNT];
} player_turn_update;

player_turn_update updates[MAX_PLAYER_COUNT] = {0};

// Round
time_t round_timer = 0;
int round_scores[MAX_PLAYER_COUNT] = {0};
int winner_id = 0;
int futures_scores[MAX_PLAYER_COUNT] = {0};
round_state state = RS_PLAYING;
round_state next_state = RS_WAITING;

// Game
game_state gs = GS_WAITING;

// Map
int base_x_offset = 0;
int base_y_offset = 0;

typedef enum {
    MCT_FLOOR,
    MCT_WALL,
    MCT_TORCH,
    MCT_VINE,
    MCT_COUNT,
} map_cell_type;

typedef enum {
    MPT_NONE,
    MPT_TORCH,
    MPT_VINE,
    MPT_COUNT,
} map_prop_type;

typedef struct {
    Texture2D *texture;
    int sprite_count;
    int scaling;
    Vector2 offset;
} cell_metadata;

cell_metadata cell_metadatas[MCT_COUNT] = {
    [MCT_FLOOR] = {.texture = &floor_textures, .sprite_count = FLOOR_TEXTURE_COUNT},
    [MCT_WALL] = {.texture = &wall_textures, .sprite_count = WALL_ORIENTATION_COUNT},
};

cell_metadata prop_metadatas[MPT_COUNT] = {
    [MPT_NONE] = {0},
    [MPT_TORCH] = {.texture = &wall_torch,
                   .sprite_count = WALL_TORCH_ANIMATION_COUNT,
                   .scaling = 3,
                   .offset = V(8, -8)},
    [MPT_VINE] = {.texture = &vine, .sprite_count = VINE_ANIMATION_COUNT, .scaling = 4},
};

map_layer game_map = {0};
map_layer variants = {0};
map_layer props = {0};
map_layer props_animations = {0};
anim_id torch_anim = 0;

float raindrop_timers[RAINDROP_COUNT] = {0};
Vector2 raindrop_target[RAINDROP_COUNT] = {0};
Vector2 raindrop_position[RAINDROP_COUNT] = {0};
Sound raindrop_sounds[RAINDROP_COUNT] = {0};

// Editor
map_data editor_map = {0};
const char *editor_map_filepath = NULL;

// Utils

Vector2 get_mouse() {
    float scale = fminf((float)GetScreenWidth() / WIDTH, (float)GetScreenHeight() / HEIGHT);
    int offset_x = (GetScreenWidth() - (WIDTH * scale)) / 2;
    int offset_y = (GetScreenHeight() - (HEIGHT * scale)) / 2;
    Vector2 mouse = GetMousePosition();
    return (Vector2){(mouse.x - offset_x) / scale, (mouse.y - offset_y) / scale};
}

int player_count() {
    int player_count = 0;
    FOREACH_PLAYER(i, player) {
        player_count++;
    }
    return player_count;
}

float lerp(float a, float b, float f) {
    return a + f * (b - a);
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
    return &all_spells[p->info.spells[p->selected_spell]];
}

bool set_error(const char *e) {
    if (e == NULL) {
        error_time_remaining = 0.0;
        free((void *)error);
        error = NULL;
        return false;
    }

    if (error != NULL) {
        free((void *)error);
    }
    error = strdup(e);
    if (error != NULL) {
        error_time_remaining = 3.f;
        PlaySound(error_sound);
        return true;
    }
    return false;
}

void set_scene(game_scene scene) {
    set_error(NULL);
    LOG("Switched from scene %d to %d", active_scene, scene);
    active_scene = scene;
}

// Console
bool is_console_open() {
    return console_open;
}

bool is_console_closed() {
    return !console_open;
}

// TODO: Handle more cursor movement
void update_console() {
    if (IsKeyPressed(KEY_F2)) {
        console_open = !console_open;
    }
    if (console_open && (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP))) {
        if (log_base + LOG_LINE_COUNT < get_log_count()) {
            log_base++;
        }
    }
    if (console_open && (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN))) {
        log_base = fmax(log_base - 1, 0);
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        console_input_cursor = fmax(console_input_cursor - 1, 0);
    }

    if (IsKeyPressed(KEY_ENTER)) {
        if (console_input_cursor > 0) {
            char *input = (char *)TextFormat("%.*s", console_input_cursor, console_input);
            LOG(input);
            command_result result = handle_command(input);
            if (result.valid == false) {
                if (result.content == NULL) {
                    LOGL(LL_ERROR, "Unknown command `%.*s`", console_input_cursor, console_input);
                } else if (result.content != NULL) {
                    LOGL(LL_ERROR, "%s", result.content);
                }
            } else {
                if (result.has_packet) {
                    if (result.content == NULL) {
                        LOGL(LL_ERROR, "Error creating packet");
                    } else {
                        send_sock(result.type, result.content, server_fd);
                    }
                    free(result.content);
                }
            }
            console_input_cursor = 0;
        }
    }

    if (is_console_open()) {
        char c;
        while ((c = GetCharPressed())) {
            if (console_input_cursor < CONSOLE_INPUT_MAX_LENGTH) {
                console_input[console_input_cursor] = c;
                console_input_cursor++;
            }
        }
    }
}

// TODO: Logs should not move when there is a new message and we are not on the latests logs
void render_console() {
    if (is_console_closed()) {
        return;
    }
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), GetColor(0x232323AA));
    for (int i = 0; i <= fmin(LOG_LINE_COUNT, get_log_count()); i++) {
        int idx = get_log_count() - fmin(LOG_LINE_COUNT, get_log_count()) + i - log_base;
        if (get_log(idx) == NULL) {
            continue;
        }
        Color c = WHITE;
        switch (get_level(idx)) {
            case LL_INFO:
                c = WHITE;
                break;
            case LL_DEBUG:
                c = GREEN;
                break;
            case LL_WARNING:
                c = ORANGE;
                break;
            case LL_ERROR:
                c = RED;
                break;
            default:
                c = WHITE;
                break;
        }
        DrawText(get_log(idx), 0, 32 * i, 32, c);
    }

    // Input
    const char *console_input_text = TextFormat("%.*s", console_input_cursor, console_input);
    DrawText(console_input_text, 0, GetScreenHeight() - 32, 32, WHITE);
    DrawRectangle(MeasureText(console_input_text, 32) + 3, GetScreenHeight() - 32, 4, 32, WHITE);

    // Counter
    int start_idx = fmax(get_log_count() - log_base - LOG_LINE_COUNT + 1, 0);
    const char *log_count = TextFormat("%d / %d", start_idx, get_log_count() + 1);
    int log_count_length = MeasureText(log_count, 32);
    DrawText(log_count, GetScreenWidth() - log_count_length, 0, 32, WHITE);
}

// Animations

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
    LOG("Animation pool is full");
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

void reset_animation(anim_id id) {
    if (anim_pool[id].active == false) {
        return;
    }
    anim_pool[id].current_time = 0;
    anim_pool[id].current_frame = 0;
    anim_pool[id].finished = false;
}

void clear_animations() {
    for (int i = 0; i < MAX_ANIMATION_POOL; i++) {
        anim_pool[i].active = false;
    }
}

int get_frame(anim_id id) {
    if (id >= MAX_ANIMATION_POOL) {
        LOG("Invalid ID (%d > %d)", id, MAX_ANIMATION_POOL);
        return 0;
    }
    return anim_pool[id].current_frame;
}

Rectangle get_sprite_anim(Texture2D sheet, anim_id id) {
    int frame = get_frame(id);
    int sprite_width = sheet.width / anim_pool[id].max_frame;
    return (Rectangle){frame * sprite_width, 0, sprite_width, sheet.height};
}

Rectangle get_sprite(Texture2D sheet, int max_frame, int frame) {
    int sprite_width = sheet.width / max_frame;
    return (Rectangle){frame * sprite_width, 0, sprite_width, sheet.height};
}

void DrawSpriteRecFromSheetTint(Texture2D sheet, Rectangle src, Vector2 pos, int scale, Color tint) {
    if (sheet.id == 0) {
        return;
    }
    Rectangle dst = (Rectangle){pos.x, pos.y, src.width * scale, src.height * scale};
    DrawTexturePro(sheet, src, dst, (Vector2){0}, 0, tint);
}

void DrawSpriteFromSheetTint(Texture2D sheet, anim_id id, Vector2 pos, int scale, Color tint) {
    Rectangle src = get_sprite_anim(sheet, id);
    DrawSpriteRecFromSheetTint(sheet, src, pos, scale, tint);
}

void DrawSpriteFromSheet(Texture2D sheet, anim_id id, Vector2 pos, int scale) {
    DrawSpriteFromSheetTint(sheet, id, pos, scale, WHITE);
}

double get_progress(anim_id id) {
    if (id >= MAX_ANIMATION_POOL) {
        LOG("Invalid ID (%d > %d)", id, MAX_ANIMATION_POOL);
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
        LOG("Invalid ID (%d > %d)", id, MAX_ANIMATION_POOL);
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

void play_animation_request() {
    queue_pop(&spell_animation_queue, &current_animation);
    current_spell_animation =
        new_animation(AT_ONESHOT, current_animation.animation_time, current_animation.frame_count);
    PlaySound(current_animation.sound);
    current_animation.active = true;
}

// Assets

void load_assets() {
    font = load_font(DEFAULT_FONT);
    simple_border = load_texture(SIMPLE_BORDER);
    box = load_texture(BOX);
    spell_box = load_texture(SPELL_BOX);
    spell_box_select = load_texture(SPELL_BOX_SELECT);
    game_slot = load_texture(GAME_SLOT);
    life_bar_bg = load_texture(LIFE_BAR_BG);
    floor_textures = load_texture(FLOOR_TEXTURE);
    wall_textures = load_texture(WALL_TEXTURE);
    test_wall_textures = load_texture(TEST_WALL_TEXTURE);
    player_textures = load_texture(PLAYER_TEXTURE);
    wall_torch = load_texture(WALL_TORCH);
    vine = load_texture(VINE);
    slash_attack = load_texture(SLASH_ATTACK);
    heal_attack = load_texture(HEAL_ATTACK);
    icons_sheet = load_texture(ICONS);
    int icon_sprite_width = icons_sheet.height;
    for (int i = 0; i < SI_COUNT; i++) {
        icons[i] = (Rectangle){i * icon_sprite_width, 0, icon_sprite_width, icon_sprite_width};
    }

    effects_sheet = load_texture(EFFECTS);
    int effect_sprite_width = effects_sheet.height;
    for (int i = 0; i < SI_COUNT; i++) {
        effects[i] = (Rectangle){i * effect_sprite_width, 0, effect_sprite_width, effect_sprite_width};
    }

    ui_button_clicked = load_sound(UI_BUTTON_CLICKED);
    ui_tab_switch = load_sound(UI_TAB_SWITCH);
    // TODO: Should loop over the 3 sounds
    move_sound = load_sound(MOVE_SOUND);
    attack_sound = load_sound(ATTACK_SOUND);
    stun_sound = load_sound(STUN_SOUND);
    burn_sound = load_sound(BURN_SOUND);
    heal_sound = load_sound(HEAL_SOUND);
    death_sound = load_sound(DEATH_SOUND);
    win_round_sound = load_sound(WIN_ROUND_SOUND);
    lose_round_sound = load_sound(LOSE_ROUND_SOUND);
    error_sound = load_sound(ERROR_SOUND);
    raindrop_sound = load_sound(RAINDROP_SOUND);
}

// Map

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
            int prop_type = get_map(&props, x, y);
            if (prop_type == 1) {
                anim_id id = new_animation(AT_LOOP, 0.2f, WALL_TORCH_ANIMATION_COUNT);
                set_map(&props_animations, x, y, id);
            } else if (prop_type == 2) {
                anim_id id = new_animation(AT_LOOP, (rand() % 5) + 2.f, VINE_ANIMATION_COUNT);
                anim_pool[id].current_frame = rand() % VINE_ANIMATION_COUNT;
                set_map(&props_animations, x, y, id);
            }
        }
    }
}

void render_outter_map() {
    for (int i = 0; i < game_map.height; i++) {
        Rectangle src = get_sprite(test_wall_textures, OUTER_WALL_COUNT, 0);
        DrawSpriteRecFromSheetTint(test_wall_textures, src, grid2screen(V(-1, i)), 4, WHITE);
        src = get_sprite(test_wall_textures, OUTER_WALL_COUNT, 3);
        DrawSpriteRecFromSheetTint(test_wall_textures, src, grid2screen(V(game_map.width, i)), 4, WHITE);
    }
    for (int i = 0; i < game_map.width; i++) {
        Rectangle src = get_sprite(wall_textures, WALL_ORIENTATION_COUNT, WALL_ORIENTATION_COUNT - 1);
        DrawSpriteRecFromSheetTint(wall_textures, src, grid2screen(V(i, game_map.height)), 4, WHITE);
        src = get_sprite(test_wall_textures, OUTER_WALL_COUNT, 5);
        DrawSpriteRecFromSheetTint(test_wall_textures, src, grid2screen(V(i, -1)), 4, WHITE);

        src = get_sprite(test_wall_textures, OUTER_WALL_COUNT, 2);
        DrawSpriteRecFromSheetTint(test_wall_textures, src, grid2screen(V(i, game_map.height)), 4, WHITE);
    }
    {
        Rectangle src = get_sprite(test_wall_textures, OUTER_WALL_COUNT, 1);
        DrawSpriteRecFromSheetTint(test_wall_textures, src, grid2screen(V(-1, game_map.height)), 4, WHITE);
        src = get_sprite(test_wall_textures, OUTER_WALL_COUNT, 4);
        DrawSpriteRecFromSheetTint(test_wall_textures, src, grid2screen(V(game_map.width, game_map.height)), 4, WHITE);
        src = get_sprite(test_wall_textures, OUTER_WALL_COUNT, 6);
        DrawSpriteRecFromSheetTint(test_wall_textures, src, grid2screen(V(-1, -1)), 4, WHITE);
        src = get_sprite(test_wall_textures, OUTER_WALL_COUNT, 7);
        DrawSpriteRecFromSheetTint(test_wall_textures, src, grid2screen(V(game_map.width, -1)), 4, WHITE);
    }
}

void render_map() {
    for (int y = 0; y < game_map.height; y++) {
        for (int x = 0; x < game_map.width; x++) {
            const int x_pos = base_x_offset + x * CELL_SIZE;
            const int y_pos = base_y_offset + y * CELL_SIZE;
            Vector2 pos = {x_pos, y_pos};
            int cell_type = get_map(&game_map, x, y);
            if (cell_type >= MCT_COUNT) {
                Color c = PURPLE;
                DrawRectangle(x_pos, y_pos, CELL_SIZE, CELL_SIZE, c);
                DrawRectangleLines(x_pos, y_pos, CELL_SIZE, CELL_SIZE, BLACK);
            } else {
                cell_metadata metadata = cell_metadatas[cell_type];
                Texture2D t = *metadata.texture;
                Rectangle src = get_sprite(t, metadata.sprite_count, get_map(&variants, x, y));
                DrawSpriteRecFromSheetTint(t, src, pos, 4, WHITE);
            }
        }
    }

    for (int y = 0; y < game_map.height; y++) {
        for (int x = 0; x < game_map.width; x++) {
            Vector2 pos = {base_x_offset + x * CELL_SIZE, base_y_offset + y * CELL_SIZE};
            int prop_type = get_map(&props, x, y);
            if (prop_type == 0) {
                continue;
            }
            if (prop_type >= MPT_COUNT) {
                LOGL(LL_ERROR, "Unknown prop type with id=%d", prop_type);
                continue;
            }
            cell_metadata metadata = prop_metadatas[prop_type];
            pos.x += metadata.offset.x;
            pos.y += metadata.offset.y;
            Texture2D spritesheet = metadata.texture != NULL ? (*metadata.texture) : (Texture2D){0};
            anim_id a = get_map(&props_animations, x, y);
            DrawSpriteFromSheet(spritesheet, a, pos, metadata.scaling);
        }
    }
    Vector2 grid = screen2grid(get_mouse());
    if (get_map(&game_map, grid.x, grid.y) != -1) {
        Vector2 cursor = grid2screen(screen2grid(get_mouse()));
        DrawRectangleV(cursor, (Vector2){CELL_SIZE, CELL_SIZE}, ColorAlpha(RED, 0.3f));
    }
}

// Spells
typedef struct {
    int x, y, dist;
} map_node;

queue map_queue;

void compute_spell_range(player *p) {
    reset_queue(&map_queue);
    map_node entry = {p->info.x, p->info.y, 0};
    queue_push(&map_queue, &entry);
    clear_map(&p->action_range);
    set_map(&p->action_range, p->info.x, p->info.y, 1);

    int range = get_selected_spell()->range;
    if (range == 0) {
        return;
    }

    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    while (!queue_empty(&map_queue)) {
        map_node current = {0};
        queue_pop(&map_queue, &current);

        if (current.dist == range) {
            continue;
        }

        for (int i = 0; i < 4; i++) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            if (get_map(&game_map, nx, ny) == 0 && get_map(&p->action_range, nx, ny) == 0) {
                set_map(&p->action_range, nx, ny, 1);
                map_node new_node = {nx, ny, current.dist + 1};
                queue_push(&map_queue, &new_node);
            }
        }
    }
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
    if (s->type == ST_MOVE || s->type == ST_TARGET) {
        for (int x = -s->range; x <= s->range; x++) {
            for (int y = -s->range; y <= s->range; y++) {
                const int x_pos = p->info.x + x;
                const int y_pos = p->info.y + y;
                if (can_player_move(p, (Vector2){x_pos, y_pos})) {
                    Vector2 s = grid2screen((Vector2){x_pos, y_pos});
                    render_game_slot(s);
                }
            }
        }
    } else if (s->type == ST_ZONE) {
        Vector2 g = screen2grid(get_mouse());
        for (int x = -s->range; x <= s->range; x++) {
            for (int y = -s->range; y <= s->range; y++) {
                const Vector2 cell = {g.x + x, g.y + y};
                if (is_cell_in_zone((Vector2){p->info.x, p->info.y}, g, cell, s)) {
                    Vector2 s = grid2screen(cell);
                    render_game_slot(s);
                }
            }
        }
    }
}

void render_spell_tooltip(const spell *s) {
    char stats_str[1024] = {0};
    if (s->damage_value != 0) {
        strcat(stats_str, "Damage: ");
        switch (s->damage_type) {
            case FLAT:
                strcat(stats_str, TextFormat("%dHP", s->damage_value));
                break;
            case FLAT_PER_TURN:
                strcat(stats_str, TextFormat("%dHP per turn", s->damage_value));
                break;
            case PERCENTAGE_CURRENT_HP_PER_TURN:
                strcat(stats_str, TextFormat("%d%% current HP per turn", s->damage_value));
                break;
        }
        strcat(stats_str, " | ");
    }

    if (s->effect_duration != 0) {
        strcat(stats_str, TextFormat("Duration: %d turn%s | ", s->effect_duration, s->effect_duration > 1 ? "s" : ""));
    }

    int len = strlen(stats_str);
    if (len > 20) {
        stats_str[len - 2] = '\n';
        stats_str[len - 1] = '\0';
    }

    if (s->cooldown != 0) {
        if (s->effect == SE_BANISH) {
            strcat(stats_str, "Single use per round | ");
        } else {
            strcat(stats_str, TextFormat("Cooldown: %d turn%s | ", s->cooldown, s->cooldown > 1 ? "s" : ""));
        }
    }

    if (s->range == 0) {
        strcat(stats_str, "Self | ");
    } else {
        strcat(stats_str, TextFormat("Range: %d | ", s->range));
    }

    const char *description = TextFormat("%s\n%sSpeed: %d", s->description, stats_str, s->speed);
    set_tooltip(get_mouse(), s->name, description);
}

bool is_over_toolbar_cell(uint8_t cell_id) {
    int toolbar_y = base_y_offset + CELL_SIZE * game_map.height + 16;
    Rectangle cell = {base_x_offset + (CELL_SIZE + 8) * cell_id, toolbar_y, CELL_SIZE, CELL_SIZE};
    Vector2 mouse = get_mouse();
    return CheckCollisionPointRec(mouse, cell);
}

bool is_over_cell(int x, int y) {
    if (is_console_open()) {
        return false;
    }
    const Rectangle cell_rect = {x, y, CELL_SIZE, CELL_SIZE};
    return CheckCollisionPointRec(get_mouse(), cell_rect);
}

// Build
// Janky but works
bool save_build(const char *filepath) {
    uint8_t buf[sizeof(uint8_t) * 2 + MAX_SPELL_COUNT + STAT_COUNT + 9 + 1] = {0};
    buf[0] = MAX_SPELL_COUNT;
    buf[1] = STAT_COUNT;
    int current_spell = 0;
    for (int i = 0; i < spell_count; i++) {
        if (spell_selection[i]) {
            buf[2 + current_spell] = i;
            current_spell++;
        }
    }
    assert(current_spell == MAX_SPELL_COUNT);
    for (int i = 0; i < STAT_COUNT; i++) {
        buf[3 + MAX_SPELL_COUNT + i] = (uint8_t)(stat_sliders[i]->slider.value & 0xFF);
    }
    memcpy(buf + 4 + MAX_SPELL_COUNT + STAT_COUNT, username_buf.buf, fmin(username_buf.ptr, 8));

    return SaveFileData(filepath, buf, sizeof(buf));
}

bool load_build(const char *filepath) {
    int size = 0;
    unsigned char *buf = LoadFileData(filepath, &size);
    if (buf == NULL) {
        return false;
    }
    if (buf[0] != MAX_SPELL_COUNT) {
        return false;
    }
    if (buf[1] != STAT_COUNT) {
        return false;
    }
    for (int i = 0; i < spell_count; i++) {
        spell_selection[i] = false;
        spell_select_buttons[i].color = WHITE;
    }
    for (int i = 0; i < MAX_SPELL_COUNT; i++) {
        spell_selection[buf[2 + i]] = true;
        spell_select_buttons[buf[2 + i]].color = DARKGRAY;
    }
    for (int i = 0; i < STAT_COUNT; i++) {
        stat_sliders[i]->slider.value = (int8_t)(buf[3 + MAX_SPELL_COUNT + i]);
    }
    memcpy(username_buf.buf, buf + 4 + MAX_SPELL_COUNT + STAT_COUNT, 8);
    username_buf.ptr = strlen(username_buf.buf);

    UnloadFileData(buf);
    return true;
}

// Player
int player_cast_spell(player *p, Vector2 origin) {
    const spell *s = get_selected_spell();
    if (s->type == ST_MOVE || s->type == ST_TARGET) {
        return can_player_move(p, origin);
    } else {
        LOGL(LL_ERROR, "not implemented type", s->type);
    }
    return false;
}

player_move player_exec_action(player *p) {
    if (p->info.effect[SE_STUN]) {
        return (player_move){.action = PA_STUNNED, .position = (Vector2){0, 0}};
    }

    player_info *i = &p->info;

    if (p->info.banned[p->selected_spell]) {
        for (int i = 0; i < MAX_SPELL_COUNT; i++) {
            if (p->info.banned[i] == false) {
                set_selected_spell(p, i);
                return (player_move){.action = PA_NONE};
            }
        }
        // In case all spells are banned the player can do nothing
        return (player_move){.action = PA_STUNNED, .position = (Vector2){0, 0}};
    }

    const Vector2 g = screen2grid(get_mouse());
    if (get_map(&game_map, g.x, g.y) != -1 && IsMouseButtonPressed(0)) {
        if (i->action == PA_SPELL) {
            if (i->cooldowns[p->selected_spell] == 0 && player_cast_spell(p, g)) {
                int selected_spell = i->spells[p->selected_spell];
                i->cooldowns[p->selected_spell] = get_selected_spell()->cooldown;
                if (i->cooldowns[p->selected_spell] > 0) {
                    p->selected_spell = 0;
                }
                return (player_move){.action = PA_SPELL, .position = g, .spell = selected_spell};
            }
        }
    }
    return (player_move){.action = PA_NONE};
}

void render_player(player *p) {
    if (p->dead) {
        return;
    }

    player_info *i = &p->info;
    int x_pos = base_x_offset + i->x * CELL_SIZE + 8;
    int y_pos = base_y_offset + i->y * CELL_SIZE - 24;
    Vector2 player_position = {x_pos, y_pos};
    Rectangle player_sprite = get_sprite_anim(player_textures, p->animation);
    Color c = WHITE;

    // Player is doing the slide animation
    if (p->animation_state == PAS_MOVING) {
        double progress = get_progress(p->action_animation);

        int x_pos = base_x_offset + i->x * CELL_SIZE + 8;
        int y_pos = base_y_offset + i->y * CELL_SIZE - 24;

        const int x_target = base_x_offset + p->moving_target.x * CELL_SIZE + 8;
        const int y_target = base_y_offset + p->moving_target.y * CELL_SIZE - 24;
        player_position.x = lerp(x_pos, x_target, progress);
        player_position.y = lerp(y_pos, y_target, progress);

        if (anim_finished(p->action_animation)) {
            i->x = p->moving_target.x;
            i->y = p->moving_target.y;
        }
    } else if (p->animation_state == PAS_DAMAGE) {
        c = RED;
    } else if (p->info.effect[SE_STUN]) {
        c = GRAY;
        player_sprite = get_sprite(player_textures, PLAYER_ANIMATION_COUNT, 0);
    } else if (p->animation_state == PAS_BURNING) {
        c = ORANGE;
    } else if (p->info.turn_effect == SE_FOCUS) {
        c = BLUE;
    } else if (p->animation_state == PAS_BUMPING) {
        double progress = get_progress(p->action_animation);
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
    }

    if (p->animation_state != PAS_NONE && anim_finished(p->action_animation)) {
        p->action_animation = NO_ANIMATION;
        p->animation_state = PAS_NONE;
    }

    // TODO: Make current player more visible. Maybe outline ?
    if (p->info.id == current_player) {
        c = ColorTint(c, YELLOW);
    }
    DrawSpriteRecFromSheetTint(player_textures, player_sprite, player_position, 3, c);
}

void render_player_actions(player *p) {
    if (p->info.effect[SE_STUN]) {
        return;
    }

    // Toolbar rendering
    int hoverred_button = -1;
    player_info *info = &p->info;
    for (int i = 0; i < MAX_SPELL_COUNT; i++) {
        button *b = &toolbar_spells_buttons[i];
        bool on_cooldown = info->cooldowns[i] > 0;
        Rectangle icon = icons[all_spells[info->spells[i]].icon];
        Color tint = WHITE;
        if (on_cooldown && info->banned[i] == false) {
            tint = GRAY;
            const char *text = TextFormat("%d", info->cooldowns[i]);
            toolbar_spells_buttons[i].text = text;
        } else {
            toolbar_spells_buttons[i].text = NULL;
        }

        b->texture = icons_sheet;
        b->texture_sprite = icon;
        b->color = tint;
        b->type = BT_TEXTURE;
        b->disabled = on_cooldown || info->banned[i];
        button_render(b);

        if (is_console_closed()) {
            if (button_hover(b)) {
                hoverred_button = info->spells[i];
            }
            // TODO: Should not be here ?
            if (button_clicked(b) && players[current_player].info.cooldowns[i] == 0) {
                info->action = PA_SPELL;
                set_selected_spell(p, i);
            }
        }
    }

    if (info->action == PA_SPELL) {
        int x = base_x_offset + (CELL_SIZE + 8) * p->selected_spell;
        int y = toolbar_spells_buttons[0].rec.y;
        Rectangle source = {0, 0, spell_box_select.width, spell_box_select.height};
        Rectangle dest = {x - 8, y - 8, CELL_SIZE + 16, CELL_SIZE + 16};
        DrawTexturePro(spell_box_select, source, dest, (Vector2){0}, 0, WHITE);
    }

    // In-game rendering
    if (info->cooldowns[p->selected_spell] == 0) {
        if (info->action == PA_SPELL) {
            render_spell_actions(p);
        }
    }

    if (hoverred_button != -1) {
        render_spell_tooltip(&all_spells[hoverred_button]);
    }
}

void render_preview_move(Vector2 pos) {
    Rectangle source = {0, 0, game_slot.width, game_slot.height};
    Rectangle dest = {pos.x, pos.y, CELL_SIZE, CELL_SIZE};
    DrawTexturePro(game_slot, source, dest, (Vector2){0}, 0, GRAY);
}

void render_player_move(player_move *move) {
    if (move->action == PA_SPELL) {
        Vector2 pos = grid2screen(move->position);
        render_preview_move(pos);
    }
}

player *get_player_at(Vector2 pos) {
    FOREACH_PLAYER(i, player) {
        if (v2eq(V(player->info.x, player->info.y), pos)) {
            return player;
        }
    }
    return NULL;
}

player *get_player(int ith) {
    FOREACH_PLAYER(i, player) {
        if (i == ith)
            return player;
    }
    return NULL;
}

// UI

void render_infos() {
    int over_player = -1;

    const int player_info_width = (game_map.width * CELL_SIZE) / player_count();
    FOREACH_PLAYER(i, player) {
        player_info *info = &players[i].info;
        int start_x = base_x_offset + player_info_width * i;
        Rectangle inner = render_box(start_x, 0, player_info_width, base_y_offset);
        const int x = inner.x + 4;
        const int y = 16;

        if (i == current_player) {
            DrawText(TextFormat("%s", info->name), x, y, 24, BLACK);
        } else {
            DrawText(TextFormat("%s", info->name), x, y, 24, BLACK);
        }

        health_bars[i].value = player->info.stats[STAT_HEALTH].value;
        health_bars[i].max = player->info.stats[STAT_HEALTH].max;
        slider_render(&health_bars[i]);
        if (slider_hover(&health_bars[i])) {
            over_player = i;
        }

        int effect_count = 0;
        const int effect_icon_size = 36;
        for (int j = 0; j < SE_COUNT; j++) {
            if (info->effect[j]) {
                int effect_x = x + (effect_icon_size + 8) * effect_count;
                Rectangle effect_rec = {effect_x, 52, effect_icon_size, effect_icon_size};
                DrawTexturePro(effects_sheet, effects[j], effect_rec, (Vector2){0}, 0, WHITE);
                effect_count++;
            }
        }
        DrawText(TextFormat("%d", round_scores[i]), x + inner.width - 48, y + (inner.height - 48) / 2, 48, BLACK);
    }

    if (over_player != -1) {
        player_info *i = &players[over_player].info;
        const char *description = TextFormat("%d/%d", i->stats[STAT_HEALTH].value, i->stats[STAT_HEALTH].max);
        set_tooltip(get_mouse(), "Health", description);
    }
}

void init_in_game_ui() {
    int toolbar_y = base_y_offset + CELL_SIZE * game_map.height + 16;
    for (int i = 0; i < MAX_SPELL_COUNT; i++) {
        int x = base_x_offset + (CELL_SIZE + 8) * i;
        toolbar_spells_buttons[i].rec = (Rectangle){x, toolbar_y, CELL_SIZE, CELL_SIZE};
        toolbar_spells_buttons[i].texture = icons_sheet;
        toolbar_spells_buttons[i].texture_sprite = icons[all_spells[my_spells[i]].icon];
        toolbar_spells_buttons[i].color = WHITE;
        toolbar_spells_buttons[i].type = BT_TEXTURE;
        toolbar_spells_buttons[i].font_size = 56;
        toolbar_spells_buttons[i].text = NULL;
        toolbar_spells_buttons[i].muted = true;
    }

    const int player_info_width = (game_map.width * CELL_SIZE) / player_count();
    FOREACH_PLAYER(i, player) {
        int y = 16;
        int life_bar_width = 0.5 * player_info_width;
        int height = 32;
        int x = base_x_offset + player_info_width * i + 16 + 150;
        health_bars[i].rec = (Rectangle){x, y, life_bar_width, height};
        health_bars[i].color = RED;
    }
}

// Game
void reset_round() {
    gs = GS_WAITING;
    state = RS_PLAYING;

    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        updates[i] = (player_turn_update){0};
    }

    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].animation == NO_ANIMATION) {
            players[i].animation = new_animation(AT_LOOP, 0.5f, PLAYER_ANIMATION_COUNT);
        } else {
            reset_animation(players[i].animation);
        }
        players[i].action_animation = NO_ANIMATION;
        players[i].animation_state = PAS_NONE;
        players[i].info.action = PA_SPELL;
        players[i].selected_spell = 0;
        players[i].dead = false;
        for (int j = 0; j < MAX_SPELL_COUNT; j++) {
            players[i].info.cooldowns[j] = 0;
            players[i].info.banned[i] = false;
        }
        players[i].info.last_spell = NO_SPELL;
        compute_spell_range(&players[i]);
    }

    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        round_scores[i] = futures_scores[i];
    }

    action_count = 0;
    action_step = 0;
}

void reset_game() {
    reset_round();

    free_map(&game_map);
    free_map(&variants);
    free_map(&props);
    free_map(&props_animations);

    players[0].color = YELLOW;
    players[1].color = GREEN;
    players[2].color = BLUE;
    players[3].color = RED;

    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        round_scores[i] = 0;
        futures_scores[i] = 0;
    }
}

// TODO: Add more animations
void queue_spell_animation(spell_animation anim, Vector2 target, player *caster, const spell *s) {
    player *player_on_cell = NULL;
    FOREACH_PLAYER(i, player) {
        if (v2eq(target, V(player->info.x, player->info.y))) {
            player_on_cell = player;
            break;
        }
    }
    animation_request request = {0};
    request.caster = caster;
    request.target = player_on_cell;
    request.spell = s;
    switch (anim) {
        case SA_NONE:
        case SA_DODGE_READY:
            return;
        case SA_SLASH:
            request.animation_time = 0.3f / 3.f;
            request.frame_count = SLASH_ANIMATION_COUNT;
            request.animation_sprite = slash_attack;
            request.target_cell = target;
            request.sound = attack_sound;
            break;
        case SA_STUN:
            request.animation_time = 0.3f / 3.f;
            request.frame_count = SLASH_ANIMATION_COUNT;
            request.animation_sprite = slash_attack;
            request.target_cell = target;
            request.sound = stun_sound;
            break;
        case SA_FIREBALL:
            request.animation_time = 0.3f / 3.f;
            request.frame_count = SLASH_ANIMATION_COUNT;
            request.animation_sprite = slash_attack;
            request.target_cell = target;
            request.sound = burn_sound;
            break;
        case SA_BURN:
            // TODO: How should this case be handled
            //  player_on_cell->action_animation = new_animation(AT_ONESHOT, 1.f, 1);
            //  player_on_cell->animation_state = PAS_BURNING;
            //  PlaySound(burn_sound);
            request.animation_time = 0.3f / 3.f;
            request.frame_count = SLASH_ANIMATION_COUNT;
            request.animation_sprite = slash_attack;
            request.target_cell = target;
            request.sound = burn_sound;
            break;
        case SA_HEAL:
            request.animation_time = 0.4f / 4.f;
            request.frame_count = HEAL_ANIMATION_COUNT;
            request.animation_sprite = heal_attack;
            request.target_cell = target;
            request.sound = heal_sound;
            break;
        case SA_FOCUS:
        case SA_POISON_CAST:
        case SA_POISON_TICK:
        case SA_ICE_CAST:
        case SA_ICE_TICK:
        case SA_SACRIFY:
        case SA_BLOCK:
        case SA_CLEANSE:
        case SA_BANISH:
        case SA_REVERT:
        case SA_KOCKBACK:
        case SA_FORTIFY:
        case SA_SLOWDOWN:
        case SA_SPEEDUP:
            request.animation_time = 0.3f / 3.f;
            request.frame_count = SLASH_ANIMATION_COUNT;
            request.animation_sprite = slash_attack;
            request.target_cell = target;
            request.sound = attack_sound;
            break;
    }

    queue_push(&spell_animation_queue, &request);
}

bool execute_spell(player *p, const spell *s, Vector2 cell, player *target) {
    // TODO: We should do pathfinding instead of sliding across the map
    // and scale animation time with distance
    if (s->type == ST_MOVE) {
        if (s->effect == SE_DODGE) {
            p->info.turn_effect = SE_DODGE;
            p->info.turn_effect_duration_left = 1;
            p->moving_target = cell;
        } else {
            p->action_animation = new_animation(AT_ONESHOT, 0.3f, 1);
            p->moving_target = cell;
            p->animation_state = target == NULL ? PAS_MOVING : PAS_BUMPING;
            if (v2eq(cell, V(p->info.x, p->info.y)) == false) {
                PlaySound(move_sound);
            }
        }
    } else if (s->type == ST_TARGET) {
        if (target != NULL) {
            if (target->info.turn_effect == SE_DODGE) {
                target->action_animation = new_animation(AT_ONESHOT, 0.3f, 1);
                player *new_target = get_player_at(target->moving_target);
                target->animation_state = new_target == NULL ? PAS_MOVING : PAS_BUMPING;
                PlaySound(move_sound);
                return false;
            } else if (target->info.turn_effect == SE_BLOCK) {
                p->info.effect[SE_STUN] = true;
                p->info.effect_round_left[SE_STUN] = 2;
                p->info.spell_effect[SE_STUN] = &all_spells[6];  // TODO: Should not be hardcoded
                return false;
            } else {
                int damage = get_spell_damage(&p->info, s);
                if (damage != 0) {
                    target->info.stats[STAT_HEALTH].value = fmax(target->info.stats[STAT_HEALTH].value - damage, 0);
                    LOG("Player %s now has %d HP", target->info.name, target->info.stats[STAT_HEALTH].value);
                    target->action_animation = new_animation(AT_ONESHOT, 0.3f, 1);
                    target->animation_state = PAS_DAMAGE;
                }
                if (s->stat_value != 0) {
                    if (s->stat_max) {
                        target->info.stats[s->stat].max = fmin(target->info.stats[s->stat].max + s->stat_value, 200);
                        target->info.stats[s->stat].value =
                            fmin(target->info.stats[s->stat].value + s->stat_value, 200);
                    } else {
                        target->info.stats[s->stat].value =
                            fmin(target->info.stats[s->stat].value + s->damage_value, target->info.stats[s->stat].max);
                    }
                }
            }
        }
    } else {
        LOGL(LL_ERROR, "Unknown spell type %d from %s", s->type, s->name);
    }
    return true;
}

// TODO: Should only queue animations but MOVE are a special case for now
void play_turn() {
    player_turn_action *a = &actions[action_step];
    LOG("Playing turn %d/%d for %d", action_step, action_count, a->player);
    player *p = &players[a->player];
    if (p->dead) {
        return;
    }

    if (a->action == PA_SPELL) {
        const spell *s = &all_spells[a->spell];
        if (s->type != ST_TARGET) {
            player *target = NULL;
            FOREACH_PLAYER(i, player) {
                if (v2eq(a->target, (Vector2){player->info.x, player->info.y})) {
                    target = player;
                    break;
                }
            }
            execute_spell(p, s, a->target, target);
        } else {
            queue_spell_animation(s->cast_animation, a->target, p, s);
        }
        p->info.last_spell = a->spell;
    } else if (a->action == PA_STUNNED) {
        if (p->info.effect_round_left[SE_STUN] > 0) {
            p->action_animation = new_animation(AT_ONESHOT, 1.f, 1);
        }
    }
    p->info.state = RS_PLAYING;
}

void play_effects(player *player) {
    player_info *info = &player->info;
    if (player->dead == false) {
        for (int i = 0; i < SE_COUNT; i++) {
            if (info->effect[i]) {
                const spell *s = info->spell_effect[i];
                if (s->cast_type == CT_EFFECT || s->cast_type == CT_CAST_EFFECT) {
                    queue_spell_animation(s->effect_animation, V(info->x, info->y), player, s);
                }
            }
        }
    }
}

void end_turn() {
    next_state = RS_PLAYING;
    FOREACH_PLAYER(i, player) {
        for (int j = 0; j < STAT_COUNT; j++) {
            player->info.stats[j] = updates[i].stats[j];
        }
        player->info.x = updates[i].position.x;
        player->info.y = updates[i].position.y;
        for (int j = 0; j < SE_COUNT; j++) {
            player->info.effect[j] = updates[i].effect[j];
            player->info.effect_round_left[j] = updates[i].effect_round_left[j];
        }
        for (int j = 0; j < MAX_SPELL_COUNT; j++) {
            if (player->info.cooldowns[j] > 0) {
                player->info.cooldowns[j]--;
            }
            player->info.banned[j] = updates[i].banned_spells[j];
        }
        if (player->info.turn_effect_duration_left == 0) {
            player->info.turn_effect = SE_NONE;
        } else {
            player->info.turn_effect_duration_left--;
        }
    }
    action_step = 0;
    action_count = 0;
    current_animation.active = false;
    compute_spell_range(&players[current_player]);
    if (gs == GS_ROUND_ENDING || gs == GS_GAME_ENDING) {
        if (winner_id == current_player) {
            PlaySound(win_round_sound);
        } else {
            PlaySound(lose_round_sound);
        }
        if (gs == GS_GAME_ENDING) {
            set_scene(SCENE_GAME_ENDED);
        } else {
            gs = GS_ROUND_ENDED;
            reset_round();
            send_sock(PKT_PLAYER_READY, NULL, server_fd);
        }
    }
}

// Network
int connect_to_server(const char *ip, uint16_t port) {
    LOG("Connecting to %s:%d", ip, port);

#ifdef WINDOWS_BUILD
    WSADATA wsaData = {0};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        LOG("WSAStartup failed");
        return -1;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        server_fd = 0;
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        closesocket(server_fd);
        server_fd = 0;
        return -1;
    }
    return 0;
#else
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        server_fd = 0;
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        close(server_fd);
        server_fd = 0;
        return -1;
    }
#endif

    if (connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(server_fd);
        server_fd = 0;
        return -1;
    }

    return 0;
}

void player_join(const char *username) {
    net_packet_join join = pkt_join(255, username, input_to_text(&password_buf));
    send_sock(PKT_JOIN, &join, server_fd);
}

void handle_packet(net_packet *p) {
    if (p->type == PKT_PING) {
        net_packet_ping *ping = (net_packet_ping *)p->content;
        last_ping = ping->recieve_time - ping->send_time;
    } else if (p->type == PKT_JOIN) {
        net_packet_join *join = (net_packet_join *)p->content;
        LOG("Joined: %.*s with ID=%d", 8, join->username, join->id);
        memcpy(players[join->id].info.name, join->username, 8);
        players[join->id].info.name[8] = '\0';
        players[join->id].info.connected = true;
    } else if (p->type == PKT_CONNECTED) {
        net_packet_connected *c = (net_packet_connected *)p->content;
        set_scene(SCENE_LOBBY);
        LOG("My ID is %d", c->id);
        current_player = c->id;
        master_player = c->master;

        memcpy(players[current_player].info.spells, my_spells, MAX_SPELL_COUNT);
        int base_health = 50 + health_slider.slider.value;
        int strength = strength_slider.slider.value;
        int speed = 100 + speed_slider.slider.value;
        LOG("Joining with %d strength", strength);
        net_packet_player_build b =
            pkt_player_build(current_player, base_health, players[current_player].info.spells, strength, speed);
        send_sock(PKT_PLAYER_BUILD, &b, server_fd);
        connected = true;
    } else if (p->type == PKT_UPDATE_SERVER_CONFIGURATION) {
        net_packet_update_server_configuration *u = (net_packet_update_server_configuration *)p->content;
        selected_map = map_picker.options[u->map_index];
        round_count = u->round_count;
    } else if (p->type == PKT_DISCONNECT) {
        net_packet_disconnect *d = (net_packet_disconnect *)p->content;
        LOG("Player %d disconnected", d->id);
        players[d->id].info.connected = false;
        master_player = d->new_master;
        set_scene(SCENE_LOBBY);
        gs = GS_WAITING;
        connected = false;
    } else if (p->type == PKT_SERVER_MAP_LIST) {
        net_packet_server_map_list *list = (net_packet_server_map_list *)p->content;
        clear_picker(&map_picker);
        for (int i = 0; i < list->map_count; i++) {
            char str[33] = {0};
            memcpy(str, list->map_names + 32 * i, 32);
            picker_add_option(&map_picker, str);
        }
    } else if (p->type == PKT_PLAYER_BUILD) {
        net_packet_player_build *b = (net_packet_player_build *)p->content;
        player *player = &players[b->id];
        LOG("Setting build for %d", b->id);
        LOG("Build for %d is %d HP %d STR", b->id, b->health, b->strength);
        player->info.stats[STAT_HEALTH].base = b->health;
        player->info.stats[STAT_HEALTH].max = b->health;
        player->info.stats[STAT_HEALTH].value = b->health;
        player->info.stats[STAT_STRENGTH].base = b->strength;
        player->info.stats[STAT_STRENGTH].max = b->strength;
        player->info.stats[STAT_STRENGTH].value = b->strength;
        player->info.stats[STAT_SPEED].base = b->speed;
        player->info.stats[STAT_SPEED].max = b->speed;
        player->info.stats[STAT_SPEED].value = b->speed;
        for (int i = 0; i < MAX_SPELL_COUNT; i++) {
            player->info.spells[i] = b->spells[i];
        }
    } else if (p->type == PKT_MAP) {
        net_packet_map *m = (net_packet_map *)p->content;
        LOG("Map is %d/%d", m->width, m->height);
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
            LOG("Unknown map layer");
            exit(1);
        }
        LOG("Map loaded");
    } else if (p->type == PKT_GAME_START) {
        LOG("Starting Game !!");
        set_scene(SCENE_IN_GAME);
        gs = GS_STARTED;
        set_selected_spell(&players[current_player], 0);
        init_in_game_ui();
    } else if (p->type == PKT_PLAYER_UPDATE) {
        net_packet_player_update *u = (net_packet_player_update *)p->content;
        LOG("Player Update %d %d %d H=%d STR=%d (%d immediate)", u->id, u->x, u->y, u->stats[STAT_HEALTH].value,
            u->stats[STAT_STRENGTH].value, u->immediate);
        player *player = &players[u->id];

        if (gs == GS_WAITING || u->immediate) {
            for (int i = 0; i < STAT_COUNT; i++) {
                player->info.stats[i] = u->stats[i];
            }
            player->info.x = u->x;
            player->info.y = u->y;
            for (int i = 0; i < SE_COUNT; i++) {
                player->info.effect[i] = u->effect[i];
                player->info.effect_round_left[i] = u->effect_round_left[i];
                player->info.banned[i] = u->banned_spells[i];
            }
        } else {
            updates[u->id].position = (Vector2){u->x, u->y};
            for (int i = 0; i < STAT_COUNT; i++) {
                updates[u->id].stats[i] = u->stats[i];
            }
            for (int j = 0; j < SE_COUNT; j++) {
                updates[u->id].effect[j] = u->effect[j];
                updates[u->id].effect_round_left[j] = u->effect_round_left[j];
            }
            for (int j = 0; j < MAX_SPELL_COUNT; j++) {
                updates[u->id].banned_spells[j] = u->banned_spells[j];
            }
        }
    } else if (p->type == PKT_PLAYER_ACTION) {
        net_packet_player_action *a = (net_packet_player_action *)p->content;
        LOG("New action: %d %d", a->id, a->spell);
        player_turn_action *action = &actions[action_count];
        action->player = a->id;
        action->action = a->action;
        action->target = (Vector2){a->x, a->y};
        action->spell = a->spell;
        action_count++;
    } else if (p->type == PKT_TURN_END) {
        state = RS_PLAYING_TURN;
    } else if (p->type == PKT_ROUND_START) {
        gs = GS_STARTED;
    } else if (p->type == PKT_ROUND_END) {
        net_packet_round_end *e = (net_packet_round_end *)p->content;
        LOG("Round ended");
        state = RS_PLAYING_TURN;
        winner_id = e->winner_id;
        gs = GS_ROUND_ENDING;
        FOREACH_PLAYER(i, player) {
            futures_scores[i] = e->player_scores[i];
        }
    } else if (p->type == PKT_GAME_END) {
        net_packet_round_end *e = (net_packet_round_end *)p->content;
        LOG("Game ended");
        winner_id = e->winner_id;
        gs = GS_GAME_ENDING;
        FOREACH_PLAYER(i, player) {
            futures_scores[i] = e->player_scores[i];
        }
    } else if (p->type == PKT_GAME_RESET) {
        LOG("Client: game reset");
        reset_game();
    } else if (p->type == PKT_GAME_STATS) {
        net_packet_game_stats *s = (net_packet_game_stats *)p->content;
        round_timer = s->round_timer;
    } else if (p->type == PKT_SERVER_MESSAGE) {
        net_packet_server_message *msg = (net_packet_server_message *)p->content;
        LOGL(msg->level, "From server: %s", msg->message);
        if (msg->level == LL_ERROR) {
            set_error(msg->message);
        }
    }

    free(p->content);
}

// TODO: It should also send queued packets from the client
void *network_thread(void *arg) {
    (void)arg;
    // Read incoming packets and push it to a queue
    while (true) {
        if (accepted) {
            net_packet p = {0};
            if (packet_read(&p, server_fd) < 0) {
                close(server_fd);
                connected = false;
                accepted = false;
                set_scene(SCENE_MAIN_MENU);
                break;
            }
            // Special case. We want to set the recieve time right now and not wait until we are handling the packet.
            if (p.type == PKT_PING) {
                net_packet_ping *ping = (net_packet_ping *)p.content;
                ping->recieve_time = GetTime() * 1000;
            }
            queue_push(&pkt_queue, &p);
        }
    }
    return NULL;
}

bool join_game(const char *ip, int port, const char *username) {
    if (connect_to_server(ip, port) < 0) {
        LOGL(LL_ERROR, "Could not connect to server");
        return false;
    }
    accepted = true;

    LOG("Connected to server !");
    pthread_create(&t_network, NULL, network_thread, NULL);

    reset_game();
    player_join(username);

    FD_ZERO(&master_set);
    FD_SET(server_fd, &master_set);
    fd_count = server_fd;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    return true;
}

// Scenes
//   Main menu
void init_scene_main_menu(const char *username) {
    input_set_text(&ip_buf, "127.0.0.1");
    input_set_text(&port_buf, "3000");
    input_set_text(&username_buf, username);

    for (int i = 0; i < MAIN_MENU_INPUT_COUNT; i++) {
        inputs[i]->rec = (Rectangle){server_info_card.rec.x + 8, server_info_card.rec.y + 75 + 60 * i,
                                     server_info_card.rec.width - 25, 50};
    }
    confirm_button.rec =
        (Rectangle){server_info_card.rec.x + 25, server_info_card.rec.y + server_info_card.rec.height - 100,
                    server_info_card.rec.width - 50, 75};

    spell_select_buttons = malloc(sizeof(button) * spell_count);
    spell_selection = malloc(sizeof(bool) * spell_count);
    {
        int max_row_count = 9;
        int x = player_info_card.rec.x + 16;
        int y = player_info_card.rec.y + 125;
        for (int i = 0; i < spell_count; i++) {
            int cell_x = x + 60 * (i % max_row_count);
            int cell_y = y + 60 * (i / max_row_count);
            spell_select_buttons[i] = BUTTON_TEXTURE(cell_x, cell_y, 50, 50, icons_sheet, NULL, 32);
            spell_select_buttons[i].texture_sprite = icons[all_spells[i].icon];
            spell_select_buttons[i].muted = true;
            spell_selection[i] = false;
        }
        for (int i = 0; i < MAX_SPELL_COUNT; i++) {
            spell_selection[i] = true;
            spell_select_buttons[i].color = DARKGRAY;
        }
    }
    {
        int w = player_info_card.rec.width - 150;
        int y = 125;
        buttoned_slider_init(&health_slider,
                             (Rectangle){player_info_card.rec.x + 125, player_info_card.rec.y + y, w, 50}, 100, 10);
        y += 60;
        buttoned_slider_init(&strength_slider,
                             (Rectangle){player_info_card.rec.x + 125, player_info_card.rec.y + y, w, 50}, 100, 10);
        y += 60;
        buttoned_slider_init(&speed_slider,
                             (Rectangle){player_info_card.rec.x + 125, player_info_card.rec.y + y, w, 50}, 30, 10);
        speed_slider.slider.min = -30;
    }

    int quart = (player_info_card.rec.width - 50) / 4;
    build_filepath.rec =
        (Rectangle){player_info_card.rec.x + 25, confirm_button.rec.y, quart * 2, confirm_button.rec.height};
    if (build_filepath.ptr == 0) {
        input_set_text(&build_filepath, "build01");
    }
    build_load_button.rec = (Rectangle){build_filepath.rec.x + build_filepath.rec.width, build_filepath.rec.y, quart,
                                        build_filepath.rec.height};
    build_save_button.rec = (Rectangle){build_load_button.rec.x + build_load_button.rec.width, build_filepath.rec.y,
                                        quart, build_filepath.rec.height};

    load_build(TextFormat("%s.build", input_to_text(&build_filepath)));
}

const char *try_join() {
    int total = 0;
    for (int i = 0; i < spell_count; i++) {
        total += spell_selection[i] == true;
    }

    if (total != MAX_SPELL_COUNT) {
        return "You need to select " STR(MAX_SPELL_COUNT) " spells";
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

int get_total_stats() {
    int stat_total = 0;
    stat_total += health_slider.slider.value;
    stat_total += strength_slider.slider.value;
    stat_total += strength_slider.slider.value;
    stat_total += fabs((float)speed_slider.slider.value);
    return stat_total;
}

void update_scene_main_menu() {
    if (is_console_closed()) {
        // First card
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            input_erase(inputs[selected_input]);
        } else if (IsKeyPressed(KEY_ENTER)) {
            if (set_error(try_join()) == false) {
                PlaySound(ui_button_clicked);
            }
        } else if ((IsKeyPressed(KEY_TAB) || IsKeyPressedRepeat(KEY_TAB)) && !IsKeyDown(KEY_LEFT_SHIFT)) {
            selected_input = (selected_input + 1) % MAIN_MENU_INPUT_COUNT;
        } else if ((IsKeyPressed(KEY_TAB) || IsKeyPressedRepeat(KEY_TAB)) && IsKeyDown(KEY_LEFT_SHIFT)) {
            if (selected_input == 0) {
                selected_input = MAIN_MENU_INPUT_COUNT - 1;
            } else {
                selected_input--;
            }
        }

        int key;
        while ((key = GetCharPressed())) {
            input_write(inputs[selected_input], key);
        }
    }

    for (int i = 0; i < MAIN_MENU_INPUT_COUNT; i++) {
        if (input_clicked(inputs[i])) {
            selected_input = i;
        }
    }

    confirm_button.muted = true;
    if (button_clicked(&confirm_button)) {
        if (set_error(try_join()) == false) {
            PlaySound(ui_button_clicked);
        }
    }
    confirm_button.muted = false;

    // Second card
    card_update_tabs(&player_info_card);

    if (player_info_card.selected_tab == 0) {
        for (int i = 0; i < spell_count; i++) {
            if (button_clicked(&spell_select_buttons[i])) {
                int total = 0;
                for (int j = 0; j < spell_count; j++) {
                    total += spell_selection[j] == true;
                }
                if (spell_selection[i] == false && total == MAX_SPELL_COUNT) {
                    set_error("Spell selection limit reached");
                } else {
                    spell_selection[i] = !spell_selection[i];
                    if (spell_selection[i]) {
                        spell_select_buttons[i].color = DARKGRAY;
                    } else {
                        spell_select_buttons[i].color = WHITE;
                    }
                }
            }
        }
    } else if (player_info_card.selected_tab == 1) {
        for (int i = 0; i < stat_sliders_count; i++) {
            buttoned_slider *b = stat_sliders[i];
            b->minus.disabled = b->slider.min < 0 && b->slider.value <= 0 && get_total_stats() + 10 > 200;
            if (button_clicked(&b->minus)) {
                buttoned_slider_decrement(b);
            }

            b->plus.disabled = (b->slider.value >= 0 && get_total_stats() + 10 > 200);
            if (button_clicked(&b->plus)) {
                buttoned_slider_increment(b);
            }
        }
    }

    if (button_clicked(&build_load_button)) {
        const char *filepath = TextFormat("%s.build", input_to_text(&build_filepath));
        if (FileExists(filepath)) {
            if (load_build(filepath) == false) {
                set_error("Could not load build");
            }
        } else {
            set_error("Could not find build file");
        }
    }

    if (button_clicked(&build_save_button)) {
        if (save_build(TextFormat("%s.build", input_to_text(&build_filepath))) == false) {
            set_error("Could not save build");
        }
    }
}

void render_scene_main_menu() {
    const char *version_text = TextFormat("v%s", GIT_VERSION);
    int version_width = MeasureText(version_text, 24);
    DrawText(version_text, WIDTH - version_width, 0, 24, LIGHTGRAY);
    int title_center = get_width_center((Rectangle){0, 0, WIDTH, 0}, "Duel Game", 64);
    DrawText("Duel Game", title_center, 32, 64, WHITE);
    card_render(&server_info_card);
    card_render(&player_info_card);

    for (size_t i = 0; i < MAIN_MENU_INPUT_COUNT; i++) {
        input_render(inputs[i], i == selected_input);
    }
    button_render(&confirm_button);

    if (player_info_card.selected_tab == 0) {
        int button_tooltip = -1;
        int total = 0;
        for (int i = 0; i < spell_count; i++) {
            if (button_hover(&spell_select_buttons[i])) {
                button_tooltip = i;
            }
            button_render(&spell_select_buttons[i]);
            total += spell_selection[i] == true;
        }
        DrawText(TextFormat("Total : %d/%d", total, MAX_SPELL_COUNT), player_info_card.rec.x + 8,
                 server_info_card.rec.y + 75, 32, WHITE);
        if (button_tooltip != -1) {
            render_spell_tooltip(&all_spells[button_tooltip]);
        }
    } else if (player_info_card.selected_tab == 1) {
        DrawText(TextFormat("Health", (int)health_slider.slider.value), player_info_card.rec.x + 8, health_slider.rec.y,
                 32, WHITE);
        buttoned_slider_render(&health_slider);

        DrawText(TextFormat("STR", (int)strength_slider.slider.value), player_info_card.rec.x + 8,
                 strength_slider.rec.y, 32, WHITE);
        buttoned_slider_render(&strength_slider);

        DrawText(TextFormat("Speed", (int)speed_slider.slider.value), player_info_card.rec.x + 8, speed_slider.rec.y,
                 32, WHITE);
        buttoned_slider_render(&speed_slider);

        DrawText(TextFormat("Total stats: %d / 200", get_total_stats()), player_info_card.rec.x + 8,
                 player_info_card.rec.y + 75, 32, WHITE);
    }

    button_render(&build_load_button);
    button_render(&build_save_button);
}

//   Lobby
void init_scene_lobby() {
    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        int x = player_list_card.rec.x + player_list_card.rec.width - 75;
        int y = player_list_card.rec.y + 75 + 46 * i;
        player_build_buttons[i] = BUTTON_COLOR(x, y, 50, 50, UI_BEIGE, "B", 36);
    }

    map_picker.rec = (Rectangle){server_config_card.rec.x + 125, server_config_card.rec.y + 75,
                                 server_config_card.rec.width - 150, 50};
    map_picker.option_frame = 5;
    picker_add_option(&map_picker, "Loading...");

    Rectangle round_counter_rec = {map_picker.rec.x, map_picker.rec.y + map_picker.rec.height + 25,
                                   map_picker.rec.width, map_picker.rec.height};
    buttoned_slider_init(&round_count_slider, round_counter_rec, 15, 1);
    round_count_slider.slider.min = 3;
    round_count_slider.slider.value = 3;
}

void update_scene_lobby() {
    start_game_button.disabled = master_player != current_player || player_count() < 2;

    if (selected_player_build == -1) {
        if (button_clicked(&start_game_button) && master_player == current_player) {
            net_packet_request_game_start s =
                pkt_request_game_start(map_picker.selected_option, round_count_slider.slider.value);
            send_sock(PKT_REQUEST_GAME_START, &s, server_fd);
        }

        card_update_tabs(&player_list_card);
        card_update_tabs(&server_config_card);

        if (server_config_card.selected_tab == 0) {
            if (master_player == current_player) {
                picker_update_scroll(&map_picker);
                if (picker_clicked(&map_picker)) {
                    map_picker.opened = !map_picker.opened;
                }
                int clicked_option = picker_option_clicked(&map_picker);
                if (clicked_option != -1) {
                    net_packet_update_server_configuration selection =
                        pkt_update_server_configuration(clicked_option, round_count_slider.slider.value);
                    send_sock(PKT_UPDATE_SERVER_CONFIGURATION, &selection, server_fd);
                }

                if (button_clicked(&round_count_slider.minus)) {
                    buttoned_slider_decrement(&round_count_slider);
                    net_packet_update_server_configuration selection =
                        pkt_update_server_configuration(map_picker.selected_option, round_count_slider.slider.value);
                    send_sock(PKT_UPDATE_SERVER_CONFIGURATION, &selection, server_fd);
                }

                if (button_clicked(&round_count_slider.plus)) {
                    buttoned_slider_increment(&round_count_slider);
                    net_packet_update_server_configuration selection =
                        pkt_update_server_configuration(map_picker.selected_option, round_count_slider.slider.value);
                    send_sock(PKT_UPDATE_SERVER_CONFIGURATION, &selection, server_fd);
                }
            }
        }

        FOREACH_PLAYER(i, player) {
            if (button_clicked(&player_build_buttons[i])) {
                selected_player_build = i;
            }
        }
    } else {
        if (button_clicked(&close_build_card_button)) {
            selected_player_build = -1;
        }
    }
}

void render_scene_lobby() {
    int title_center = get_width_center((Rectangle){0, 0, WIDTH, HEIGHT}, "Lobby", 64);
    DrawText("Lobby", title_center, 32, 64, WHITE);

    if (selected_player_build == -1) {
        card_render(&player_list_card);
        card_render(&server_config_card);
    }

    if (gs == GS_WAITING) {
        int idx = 0;
        // TODO: We should order names based on who connected first
        FOREACH_PLAYER(i, player) {
            int x = player_list_card.rec.x + 8;
            int y = player_list_card.rec.y + 75 + 46 * idx;
            Color c = master_player == i ? YELLOW : WHITE;
            if (i == current_player) {
                DrawText(TextFormat("- %s (you)", player->info.name), x, y, 36, c);
            } else {
                DrawText(TextFormat("- %s", player->info.name), x, y, 36, c);
            }
            button_render(&player_build_buttons[idx]);
            idx++;
        }

        if (server_config_card.selected_tab == 0) {
            if (master_player == current_player) {
                DrawText("Round", server_config_card.rec.x + 25, round_count_slider.rec.y + 8, 32, WHITE);
                buttoned_slider_render(&round_count_slider);

                DrawText("Map", server_config_card.rec.x + 25, map_picker.rec.y + 8, 32, WHITE);
                picker_render(&map_picker);
            } else {
                // TODO Include round count sync
                DrawText(TextFormat("Map: %s", selected_map), server_config_card.rec.x + 25, map_picker.rec.y, 32,
                         WHITE);
                DrawText(TextFormat("Round: %d", round_count), server_config_card.rec.x + 25, round_count_slider.rec.y,
                         32, WHITE);
            }
        }
    }

    if (current_player == master_player) {
        button_render(&start_game_button);
    }

    if (selected_player_build != -1) {
        int icon_tooltip = -1;
        card_render(&player_build_card);
        Rectangle header = {player_build_card.rec.x, player_build_card.rec.y + 75, player_build_card.rec.width, 64};
        DrawTextCenter(header, TextFormat("Viewing %s build", players[selected_player_build].info.name), 54, WHITE);
        player *p = &players[selected_player_build];
        player_info *info = &p->info;
        int x = player_build_card.rec.x + 25;
        DrawText(TextFormat("Health = %d", info->stats[STAT_HEALTH].max), x, header.y + 64, 32, WHITE);
        DrawText(TextFormat("Strength = %d", info->stats[STAT_STRENGTH].value), x, header.y + 96, 32, WHITE);
        DrawText(TextFormat("Speed = %d", info->stats[STAT_SPEED].value), x, header.y + 128, 32, WHITE);
        DrawText("Spells", x, header.y + 192, 32, WHITE);
        for (int i = 0; i < MAX_SPELL_COUNT; i++) {
            Rectangle icon = icons[all_spells[info->spells[i]].icon];
            Rectangle dst = {x + 60 * i, header.y + 224, 50, 50};
            icon_render(icon, dst);
            if (icon_hover(dst)) {
                icon_tooltip = info->spells[i];
            }
        }
        button_render(&close_build_card_button);

        if (icon_tooltip != -1) {
            render_spell_tooltip(&all_spells[icon_tooltip]);
        }
    }
}

//   In game
void init_scene_in_game() {
    for (int i = 0; i < RAINDROP_COUNT; i++) {
        raindrop_timers[i] = RAINDROP_RAND_SPAWNRATE;
        raindrop_target[i] = (Vector2){0};
        raindrop_position[i] = (Vector2){0};
        raindrop_sounds[i] = LoadSoundAlias(raindrop_sound);
        SetSoundVolume(raindrop_sounds[i], 0.01f);
        SetSoundPitch(raindrop_sounds[i], 1.0f + ((rand() % 200 - 100) / 100.f));
    }
}

void update_scene_in_game() {
    const int keybinds[] = {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I};

    if (gs == GS_STARTED || gs == GS_ROUND_ENDING || gs == GS_GAME_ENDING) {
        next_state = state;
        if (state == RS_PLAYING && is_console_closed()) {
            for (int i = 0; i < MAX_SPELL_COUNT; i++) {
                if (IsKeyPressed(keybinds[i]) && players[current_player].info.cooldowns[i] == 0) {
                    players[current_player].info.action = PA_SPELL;
                    set_selected_spell(&players[current_player], i);
                }
            }
        }

        if (state == RS_PLAYING) {
            player_move move = player_exec_action(&players[current_player]);
            if (move.action != PA_NONE) {
                next_state = RS_WAITING;
                net_packet_player_action a =
                    pkt_player_action(current_player, move.action, move.position.x, move.position.y, move.spell);
                send_sock(PKT_PLAYER_ACTION, &a, server_fd);
                players[current_player].round_move = move;
                players[current_player].info.state = RS_WAITING;
            }
        } else if (state == RS_PLAYING_TURN) {
            play_turn();
            next_state = RS_WAITING_ANIMATIONS;
        } else if (state == RS_WAITING_ANIMATIONS) {
            if (current_spell_animation != NO_ANIMATION && anim_finished(current_spell_animation)) {
                current_spell_animation = NO_ANIMATION;
                const spell *s = current_animation.spell;
                LOG("%s is casting %s", current_animation.caster->info.name, s->name);
                if (s->cast_type == CT_CAST || s->cast_type == CT_CAST_EFFECT) {
                    bool success = execute_spell(current_animation.caster, s, current_animation.target_cell,
                                                 current_animation.target);
                    if (success && s->effect != SE_NONE && current_animation.target != NULL) {
                        apply_effect(&current_animation.target->info, s);
                    }
                } else if (s->cast_type == CT_EFFECT && current_animation.target != NULL) {
                    apply_effect(&current_animation.target->info, s);
                }
            }

            if (current_spell_animation == NO_ANIMATION && !queue_empty(&spell_animation_queue)) {
                play_animation_request();
            }

            if (wait_for_animations() && queue_empty(&spell_animation_queue)) {
                if (action_step < action_count - 1) {
                    action_step++;
                    next_state = RS_PLAYING_TURN;
                } else {
                    play_effects(get_player(effect_player_turn));
                    next_state = RS_PLAYING_EFFECTS;
                }
            }
        } else if (state == RS_PLAYING_EFFECTS) {
            if (current_spell_animation != NO_ANIMATION && anim_finished(current_spell_animation)) {
                current_spell_animation = NO_ANIMATION;
                player *player = &players[effect_player_turn];
                if (player->dead == false) {
                    const spell *s = current_animation.spell;
                    execute_spell(player, s, V(player->info.x, player->info.y), player);
                }
            }
            if (current_spell_animation == NO_ANIMATION && !queue_empty(&spell_animation_queue)) {
                play_animation_request();
            }

            if (wait_for_animations() && queue_empty(&spell_animation_queue)) {
                current_spell_animation = NO_ANIMATION;
                effect_player_turn++;
                if (effect_player_turn < player_count()) {
                    play_effects(get_player(effect_player_turn));
                } else {
                    effect_player_turn = 0;
                    next_state = RS_ENDING_ROUND;
                }
            }
        } else if (state == RS_ENDING_ROUND) {
            FOREACH_PLAYER(i, player) {
                if (player->animation_state == PAS_NONE && player->info.stats[STAT_HEALTH].value == 0 &&
                    player->dead == false) {
                    player->action_animation = new_animation(AT_ONESHOT, 1.f, 1);
                    player->animation_state = PAS_DYING;
                    PlaySound(death_sound);
                }
                if (player->animation_state == PAS_DYING && anim_finished(player->action_animation)) {
                    player->dead = true;
                }
            }

            if (wait_for_animations()) {
                end_turn();
            }
        }

        for (int i = 0; i < RAINDROP_COUNT; i++) {
            raindrop_timers[i] -= GetFrameTime();
            if (raindrop_position[i].y == 0 && raindrop_timers[i] < 0) {
                raindrop_target[i] = (Vector2){rand() % game_map.width * CELL_SIZE + base_x_offset,
                                               ((rand() % (game_map.height - 4)) + 4) * CELL_SIZE + base_y_offset};
                raindrop_position[i] = (Vector2){raindrop_target[i].x + (rand() % CELL_SIZE), base_y_offset};
            }
        }
    }
}

void render_scene_in_game() {
    if (gs == GS_WAITING) {
        render_map();
        render_outter_map();
        render_infos();
    } else if (gs == GS_STARTED || gs == GS_ROUND_ENDING || gs == GS_GAME_ENDING) {
        render_map();

        // TODO: Always render currently animation player on top
        FOREACH_PLAYER(i, player) {
            render_player(player);
        }

        render_outter_map();

        if (state == RS_PLAYING) {
            render_player_actions(&players[current_player]);
        } else if (state == RS_WAITING) {
            render_player_move(&players[current_player].round_move);
        } else if (state == RS_PLAYING_TURN || state == RS_WAITING_ANIMATIONS) {
            if (players[current_player].info.state == RS_WAITING) {
                render_player_move(&players[current_player].round_move);
            }
        }

        if (current_spell_animation != NO_ANIMATION) {
            Vector2 position = grid2screen(current_animation.target_cell);
            DrawSpriteFromSheet(current_animation.animation_sprite, current_spell_animation, position, 1);
        }

        render_infos();
        state = next_state;
        FOREACH_PLAYER(i, player) {
            Vector2 screen_pos = grid2screen((Vector2){player->info.x, player->info.y});
            Rectangle player_rec = {screen_pos.x, screen_pos.y, CELL_SIZE, CELL_SIZE};
            Vector2 mp = get_mouse();
            if (CheckCollisionPointRec(mp, player_rec)) {
                set_tooltip(get_mouse(), player->info.name,
                            TextFormat("Health=%d/%d\nStrength=%d\nSpeed=%d", player->info.stats[STAT_HEALTH].value,
                                       player->info.stats[STAT_HEALTH].max, player->info.stats[STAT_STRENGTH].value,
                                       player->info.stats[STAT_SPEED].value));
            }
        }

        for (int i = 0; i < RAINDROP_COUNT; i++) {
            // Raindrop is falling
            if (raindrop_position[i].y > 0) {
                DrawRectangle(raindrop_position[i].x, raindrop_position[i].y, 10, 20, ColorAlpha(BLUE, 0.8f));
                if (raindrop_position[i].y >= raindrop_target[i].y) {
                    // TODO: Particule splash
                    PlaySound(raindrop_sounds[i]);
                    raindrop_timers[i] = RAINDROP_RAND_SPAWNRATE;
                    raindrop_position[i].y = 0;
                } else {
                    raindrop_position[i].y += 850 * GetFrameTime();
                }
            }
        }
    }
    char time_string[8] = {0};
    struct tm *time = localtime(&round_timer);
    strftime(time_string, 8, "%M:%S", time);
    int width = MeasureText(time_string, 48);
    DrawText(time_string, WIDTH - width - 8, 8, 48, WHITE);
}

//   End game

void update_scene_game_ended() {
    if (IsKeyPressed(KEY_R) && is_console_closed()) {
        if (current_player == master_player) {
            send_sock(PKT_GAME_RESET, NULL, server_fd);
        }
    }
}

void render_scene_game_ended() {
    if (winner_id == 255) {
        DrawText("Game draw", 0, 0, 64, WHITE);
    } else {
        DrawText(TextFormat("%s won the game !", players[winner_id].info.name), 0, 0, 64, WHITE);
    }

    if (current_player == 0) {
        DrawText("Press R to reset the game!", 0, 72, 64, WHITE);
    }
}

//    Editor
void init_scene_editor() {
    init_map(&game_map, MAP_WIDTH, MAP_HEIGHT, editor_map.map);
    init_map(&variants, MAP_WIDTH, MAP_HEIGHT, NULL);
    base_x_offset = (WIDTH - (CELL_SIZE * game_map.width)) / 2;
    base_y_offset = (HEIGHT - (CELL_SIZE * game_map.height)) / 2;

    init_map(&props, MAP_WIDTH, MAP_HEIGHT, editor_map.props);
    set_props_animations();
}

void update_scene_editor() {
    Vector2 over_cell = screen2grid(get_mouse());
    if (get_map(&game_map, over_cell.x, over_cell.y) != -1) {
        if (IsMouseButtonPressed(0)) {
            set_map(&game_map, over_cell.x, over_cell.y, 0);
            editor_map.map[(int)(over_cell.x + MAP_WIDTH * over_cell.y)] = 0;
        }
        if (IsMouseButtonPressed(1)) {
            set_map(&game_map, over_cell.x, over_cell.y, 1);
            editor_map.map[(int)(over_cell.x + MAP_WIDTH * over_cell.y)] = 1;
        }

        if (IsKeyPressed(KEY_R)) {
            set_map(&props, over_cell.x, over_cell.y, 0);
            clear_animations();
            set_props_animations();
            editor_map.props[(int)(over_cell.x + MAP_WIDTH * over_cell.y)] = 0;
        }

        if (IsKeyPressed(KEY_T)) {
            set_map(&props, over_cell.x, over_cell.y, 1);
            clear_animations();
            set_props_animations();
            editor_map.props[(int)(over_cell.x + MAP_WIDTH * over_cell.y)] = 1;
        }

        if (IsKeyPressed(KEY_Y)) {
            set_map(&props, over_cell.x, over_cell.y, 2);
            clear_animations();
            set_props_animations();
            editor_map.props[(int)(over_cell.x + MAP_WIDTH * over_cell.y)] = 2;
        }

        if (IsKeyPressed(KEY_Z)) {
            editor_map.spawn_positions[0][0] = over_cell.x;
            editor_map.spawn_positions[0][1] = over_cell.y;
        }
        if (IsKeyPressed(KEY_X)) {
            editor_map.spawn_positions[1][0] = over_cell.x;
            editor_map.spawn_positions[1][1] = over_cell.y;
        }
        if (IsKeyPressed(KEY_C)) {
            editor_map.spawn_positions[2][0] = over_cell.x;
            editor_map.spawn_positions[2][1] = over_cell.y;
        }
        if (IsKeyPressed(KEY_V)) {
            editor_map.spawn_positions[3][0] = over_cell.x;
            editor_map.spawn_positions[3][1] = over_cell.y;
        }
    }

    if (IsKeyPressed(KEY_S)) {
        if (save_map(editor_map_filepath, &editor_map) == false) {
            LOGL(LL_ERROR, "Error while saving map from the editor");
        }
    }
}

void render_scene_editor() {
    render_map();

    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        Vector2 spawn_point = {editor_map.spawn_positions[i][0], editor_map.spawn_positions[i][1]};
        Vector2 screen_space = grid2screen(spawn_point);
        screen_space.x += CELL_SIZE / 2.f;
        screen_space.y += CELL_SIZE / 2.f;
        DrawCircleLinesV(screen_space, CELL_SIZE / 4.f, GREEN);
        DrawText(TextFormat("%d", i), screen_space.x - 4, screen_space.y - 8, 24, WHITE);
    }

    DrawText(editor_map_filepath, 0, 36, 32, WHITE);
    DrawText("Press S to save", 0, 68, 32, WHITE);
}

bool load_editor(const char *filename) {
    if (load_map(filename, &editor_map) == false) {
        LOGL(LL_ERROR, "Could not load map %s", filename);
        return false;
    }
    editor_map_filepath = filename;
    set_scene(SCENE_EDITOR);
    init_scene_editor();
    return true;
}

// Main

int main(int argc, char **argv) {
#ifdef WINDOWS_BUILD
    LOG("Running on Windows");
#else
    LOG("Running on Linux");
#endif

    srand(time(NULL) + getpid());

    char *ip = NULL;
    int port = 3000;
    const char *username = NULL;

    POPARG(argc, argv);  // program name
    while (argc > 0) {
        const char *arg = POPARG(argc, argv);
        if (strcmp(arg, "--ip") == 0) {
            ip = POPARG(argc, argv);
        } else if (strcmp(arg, "--port") == 0) {
            const char *value = POPARG(argc, argv);
            if (!strtoint(value, &port)) {
                LOG("Error parsing port to int '%s'", value);
                exit(1);
            }
        } else if (strcmp(arg, "--username") == 0) {
            username = POPARG(argc, argv);
        } else if (strcmp(arg, "--build") == 0) {
            const char *value = POPARG(argc, argv);
            input_set_text(&build_filepath, value);
        } else if (strcmp(arg, "--editor") == 0) {
            const char *map = POPARG(argc, argv);
            if (load_editor(map) == false) {
                exit(1);
            }
        } else {
            LOG("Unknown arg : '%s'", arg);
            exit(1);
        }
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WIDTH, HEIGHT, "Duel Game");
    InitAudioDevice();
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture(WIDTH, HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);
    lightmap = LoadRenderTexture(WIDTH, HEIGHT);
    ui = LoadRenderTexture(WIDTH, HEIGHT);

    if (username == NULL) {
        username = TextFormat("User %d", rand());
    }

    load_assets();
    init_scene_main_menu(username);
    init_scene_lobby();
    init_scene_in_game();

    init_queue(&pkt_queue, sizeof(net_packet));
    init_queue(&map_queue, sizeof(map_node));
    init_queue(&spell_animation_queue, sizeof(animation_request));

    if (ip != NULL) {
        join_game(ip, port, username);
    }

    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        players[i].info.id = i;
        players[i].animation = NO_ANIMATION;
    }

    // Send ping every seconds
    float ping_counter = 1;
    while (!WindowShouldClose()) {
        step_animations();
        update_console();
        if (connected) {
            ping_counter -= GetFrameTime();
            if (ping_counter <= 0) {
                net_packet_ping ping = {.send_time = (uint64_t)(GetTime() * 1000)};
                send_sock(PKT_PING, &ping, server_fd);
                ping_counter = 1;
            }
        }

        if (error_time_remaining > 0) {
            error_time_remaining -= GetFrameTime();
        }

        net_packet p = {0};
        if (queue_pop(&pkt_queue, &p)) {
            handle_packet(&p);
        }

        // Update
        if (active_scene == SCENE_MAIN_MENU) {
            update_scene_main_menu();
        } else if (active_scene == SCENE_LOBBY) {
            update_scene_lobby();
        } else if (active_scene == SCENE_IN_GAME) {
            update_scene_in_game();
        } else if (active_scene == SCENE_GAME_ENDED) {
            update_scene_game_ended();
        } else if (active_scene == SCENE_EDITOR) {
            update_scene_editor();
        }

        BeginTextureMode(target);
        {
            ClearBackground(GetColor(0x232323FF));
            // Rendering
            if (active_scene == SCENE_MAIN_MENU) {
                render_scene_main_menu();
            } else if (active_scene == SCENE_LOBBY) {
                render_scene_lobby();
            } else if (active_scene == SCENE_IN_GAME) {
                render_scene_in_game();
            } else if (active_scene == SCENE_GAME_ENDED) {
                render_scene_game_ended();
            } else if (active_scene == SCENE_EDITOR) {
                render_scene_editor();
            }

            if (error != NULL && error_time_remaining > 0) {
                int error_center = get_width_center((Rectangle){0, 0, WIDTH, 0}, error, 32);
                DrawText(error, error_center, 96, 32, UI_RED);
            }
            render_console();
        }
        EndTextureMode();

        // Move somewhere else
        if (active_scene == SCENE_IN_GAME) {
            BeginTextureMode(lightmap);
            {
                ClearBackground((Color){0, 0, 0, 0});
                for (int y = 0; y < game_map.height; y++) {
                    for (int x = 0; x < game_map.width; x++) {
                        Vector2 pos = {base_x_offset + x * CELL_SIZE + 8, base_y_offset + y * CELL_SIZE - 8};
                        int type = get_map(&props, x, y);
                        if (type == 1) {
                            int frame = get_frame(get_map(&props_animations, x, y));
                            float size = 30 + (frame < 4) * 4;
                            float alpha = (frame < 4) ? 0.6f : 0.4;
                            DrawCircle(pos.x + 8 * 3, pos.y + 8 * 3 + size / 2, size, ColorAlpha(ORANGE, alpha));
                        }
                    }
                }
            }
            EndTextureMode();
        }

        BeginTextureMode(ui);
        {
            ClearBackground((Color){0, 0, 0, 0});
            render_tooltip();
        }
        EndTextureMode();

        BeginDrawing();
        {
            ClearBackground(GetColor(0x232323FF));
            float scale = fminf((float)GetScreenWidth() / WIDTH, (float)GetScreenHeight() / HEIGHT);
            int offset_x = (GetScreenWidth() - (WIDTH * scale)) / 2;
            int offset_y = (GetScreenHeight() - (HEIGHT * scale)) / 2;
            Rectangle src = {0, 0, WIDTH, -HEIGHT};
            Rectangle dest = {offset_x, offset_y, (WIDTH * scale), (HEIGHT * scale)};

            DrawTexturePro(target.texture, src, dest, (Vector2){0}, 0, WHITE);

            if (active_scene == SCENE_IN_GAME) {
                BeginBlendMode(BLEND_MULTIPLIED);
                {
                    DrawTexturePro(lightmap.texture, src, dest, (Vector2){0}, 0, WHITE);
                }
                EndBlendMode();
            }
            DrawTexturePro(ui.texture, src, dest, (Vector2){0}, 0, WHITE);
            DrawText(TextFormat("FPS=%d", GetFPS()), 0, 0, 24, GREEN);
            if (connected) {
                DrawText(TextFormat("Ping=%lums", last_ping), 0, 24, 24, GREEN);
            }
        }
        clear_tooltip();
        EndDrawing();
    }
    UnloadRenderTexture(target);
    CloseAudioDevice();
    CloseWindow();
}
