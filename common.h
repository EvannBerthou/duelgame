#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define MAX_SPELL_COUNT 2

typedef enum {
    ST_MOVE,
    ST_TARGET,
    ST_ZONE,
} spell_type_enum;

typedef uint8_t spell_type;

typedef struct {
    uint8_t id;
    const char *name;
    uint8_t icon;

    spell_type type;
    uint8_t damage;
    uint8_t range;
    uint8_t zone_size;
} spell;

const spell all_spells[] = {
    {.id = 0, .name = "Move", .icon = 0, .type = ST_MOVE, .range = 1},
    {.id = 1, .name = "Target", .icon = 1, .type = ST_TARGET, .damage = 25, .range = 1, .zone_size = 3},
    {.id = 2, .name = "Zone", .icon = 2, .type = ST_TARGET, .damage = 50, .range = 2},
};


#endif
