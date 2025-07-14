#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

int strtoint(const char *str, int *out);

#define POPARG(argc, argv) (assert(argc > 0), (argc)--, *(argv)++)

#define MAX_SPELL_COUNT 4

typedef enum {
    ST_MOVE,
    ST_TARGET,
    ST_ZONE,
} spell_type_enum;

typedef enum {
    SE_NONE,
    SE_STUN,
    SE_BURN,
    SE_COUNT,
} spell_effect;

typedef struct {
    uint8_t id;
    const char* name;
    uint8_t icon;

    spell_type_enum type;
    uint8_t damage;
    uint8_t range;
    uint8_t zone_size;
    uint8_t speed;
    // Number of round between 2 usages of the spell
    // TODO: Should also be server-sided
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
    uint8_t *content;
} map_layer;

void init_map(map_layer *m, int width, int height, uint8_t *copy);
int get_map(map_layer *m, int x, int y);
void set_map(map_layer *m, int x, int y, int v);
void clear_map(map_layer *m);
void free_map(map_layer *m);

#endif
