#ifndef COMMON_H
#define COMMON_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int strtoint(const char* str, int* out);

#define POPARG(argc, argv) (assert(argc > 0), (argc)--, *(argv)++)

#define MAX_QUEUE_SIZE 256

typedef struct {
    void* content;
    int elem_size;

    int front;
    int rear;
    int size;
} queue;

typedef enum {
    LL_INFO,
    LL_WARNING,
    LL_DEBUG,
    LL_ERROR,
} log_level;

void LOG(const char* fmt, ...);
void LOGL(log_level level, const char* fmt, ...);

#define MAX_SPELL_COUNT 8
#define MAX_PLAYER_COUNT 4

typedef enum {
    ST_UNKNOWN,
    ST_MOVE,
    ST_TARGET,
    ST_ZONE,
    ST_AROUND,
} spell_type_enum;

typedef enum {
    SE_NONE,
    SE_STUN,
    SE_BURN,
    SE_POISON,
    SE_SLOW,
    SE_COUNT,

    SE_CLEANSE = 100,
    SE_DODGE,
    SE_FOCUS,
    SE_BLOCK,
    SE_BANISH,
    SE_REVERT,
} spell_effect;

typedef enum { SI_UNKNOWN, SI_MOVE, SI_ATTACK, SI_WAND, SI_COUNT } spell_icon;

typedef enum {
    SA_NONE,
    SA_DODGE_READY,
    SA_SLASH,
    SA_FOCUS,
    SA_STUN,
    SA_FIREBALL,
    SA_BURN,
    SA_POISON_CAST,
    SA_POISON_TICK,
    SA_ICE_CAST,
    SA_ICE_TICK,
    SA_SACRIFY,
    SA_BLOCK,
    SA_CLEANSE,
    SA_BANISH,
    SA_REVERT,
    SA_KOCKBACK,
    SA_HEAL,
    SA_FORTIFY,
    SA_SLOWDOWN,
    SA_SPEEDUP,
} spell_animation;

typedef enum {
    STAT_HEALTH,
    STAT_AP,
    STAT_AD,
    STAT_SPEED,
    STAT_CRIT,
    STAT_COUNT
} stat_type;

typedef enum {
    CT_CAST,         // Apply the spell only on cast turn
    CT_EFFECT,       // Apply the spell only as an effect
    CT_CAST_EFFECT,  // Apply the spell both on cast turn and as an effect
} cast_type;

typedef struct {
    const char* name;
    const char* description;
    uint8_t icon;
    spell_animation cast_animation;
    spell_animation effect_animation;
    cast_type cast_type;

    spell_type_enum type;
    stat_type stat;
    bool stat_max;
    int damage_value;
    int stat_value;
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
    RS_PLAYING_TURN,
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
    uint8_t base;
    uint8_t max;
    uint8_t value;
} net_player_stat;

typedef struct {
    uint8_t id;
    bool connected;
    char name[9];
    uint8_t x, y;
    net_player_stat stats[STAT_COUNT];

    round_state state;
    player_action action;
    uint8_t ax, ay;
    uint8_t spells[MAX_SPELL_COUNT];
    uint8_t cooldowns[MAX_SPELL_COUNT];
    uint8_t spell;

    uint8_t effect[SE_COUNT];
    uint8_t effect_round_left[SE_COUNT];
    const spell* spell_effect[SE_COUNT];

    // Special effects that trigger on future actions. Ex: Dodge, Focus
    uint8_t turn_effect;
    uint8_t turn_effect_duration_left;
} player_info;

const char* get_log(int idx);
log_level get_level(int idx);
int get_log_count();
void clear_logs();

int get_spell_damage(player_info* info, const spell* s);
void apply_effect(player_info* p, const spell* s);

// Queue


void init_queue(queue* q, size_t elem_size);
bool queue_full(queue* q);
bool queue_empty(queue* q);
bool queue_push(queue* q, void* data);
bool queue_pop(queue* q, void* out);
void reset_queue(queue *q);

#endif
