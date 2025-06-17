#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

int strtoint(const char *str, int *out) {
    char *endptr = NULL;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (errno == ERANGE || val > INT_MAX || val < INT_MIN)
        return 0;
    if (endptr == str || *endptr != '\0')
        return 0;

    *out = (int)val;
    return 1;
}

#define POPARG(argc, argv) (assert(argc > 0), (argc)--, *(argv)++)

#define MAX_SPELL_COUNT 8

typedef enum {
    ST_MOVE,
    ST_TARGET,
    ST_ZONE,
} spell_type_enum;

typedef enum {
    SE_NONE,
    SE_STUN,
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

// TODO: Add spells with effects (stun, root, DOT, kb)
const spell all_spells[] = {
    {
        .id = 0,
        .name = "Move",
        .icon = 0,
        .type = ST_MOVE,
        .range = 1,
        .speed = 100,
        .cooldown = 0,
    },
    {.id = 1,
     .name = "Target",
     .icon = 1,
     .type = ST_TARGET,
     .damage = 25,
     .range = 1,
     .zone_size = 3,
     .speed = 90,
     .cooldown = 0},
    {.id = 2,
     .name = "Zone",
     .icon = 2,
     .type = ST_TARGET,
     .damage = 50,
     .range = 2,
     .speed = 80,
     .cooldown = 0},
    {.id = 3,
     .name = "Move Slow",
     .icon = 0,
     .type = ST_MOVE,
     .range = 1,
     .speed = 50,
     .cooldown = 0},
    {.id = 4,
     .name = "Stun",
     .icon = 0,
     .type = ST_TARGET,
     .range = 1,
     .speed = 150,
     .cooldown = 4,
     .effect = SE_STUN,
     .effect_duration = 3},
};

#endif
