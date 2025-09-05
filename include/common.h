#ifndef COMMON_H
#define COMMON_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

int strtoint(const char* str, int* out);

#define POPARG(argc, argv) (assert(argc > 0), (argc)--, *(argv)++)

typedef enum {
    LL_INFO,
    LL_WARNING,
    LL_DEBUG,
    LL_ERROR,
} log_level;

void LOG(const char* fmt, ...);
void LOGL(log_level level, const char* fmt, ...);

#define MAX_SPELL_COUNT 4
#define MAX_PLAYER_COUNT 4

typedef enum {
    ST_MOVE,
    ST_TARGET,
    ST_ZONE,
    ST_STAT, //TODO: Should just be a target ?
} spell_type_enum;

typedef enum {
    SE_NONE,
    SE_STUN,
    SE_BURN,
    SE_HEAL,
    SE_COUNT,
} spell_effect;

typedef enum { SI_UNKNOWN, SI_MOVE, SI_ATTACK, SI_WAND, SI_COUNT } spell_icon;

typedef enum {
    SA_NONE,
    SA_SLASH,
    SA_STUN,
    SA_FIREBALL,
    SA_BURN,
    SA_HEAL
} spell_animation;

typedef struct {
    const char* name;
    const char* description;
    uint8_t icon;
    spell_animation cast_animation;
    spell_animation effect_animation;

    spell_type_enum type;
    int damage;
    uint8_t range;
    uint8_t zone_size;
    uint8_t speed;
    // Number of round between 2 usages of the spell
    uint8_t cooldown;

    spell_effect effect;
    uint8_t effect_duration;  // Number of turn left before the effect is gone
} spell;

typedef enum {
    MLT_BACKGROUND,
    MLT_PROPS,
} map_layer_type;

typedef struct {
    uint8_t width, height;
    map_layer_type type;
    uint8_t* content;
} map_layer;

void init_map(map_layer* m, int width, int height, uint8_t* copy);
int get_map(map_layer* m, int x, int y);
void set_map(map_layer* m, int x, int y, int v);
void clear_map(map_layer* m);
void free_map(map_layer* m);

typedef enum {
    RS_PLAYING,
    RS_WAITING,
    RS_PLAYING_ROUND,
    RS_WAITING_ANIMATIONS,
    RS_PLAYING_EFFECTS,
    RS_COUNT,
} round_state;

typedef enum {
    GS_WAITING,
    GS_STARTED,
    GS_ROUND_ENDING,
    GS_ROUND_ENDED,
    GS_GAME_ENDING,
    GS_COUNT
} game_state;

typedef enum {
    PA_NONE,
    PA_SPELL,
    PA_STUNNED,
} player_action;

typedef struct {
    uint8_t id;
    bool connected;
    char name[9];
    uint8_t x, y;
    uint8_t base_health;
    uint8_t max_health;
    int health;
    uint8_t ad;
    uint8_t ap;

    round_state state;
    player_action action;
    uint8_t ax, ay;
    uint8_t spells[MAX_SPELL_COUNT];
    uint8_t cooldowns[MAX_SPELL_COUNT];
    uint8_t spell;

    // TODO: Multiple effects can be active at the same time
    spell_effect effect;
    uint8_t effect_round_left;
    const spell* spell_effect;
} player_info;

const char* get_log(int idx);
log_level get_level(int idx);
int get_log_count();
void clear_logs();

#endif
