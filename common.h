#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define MAX_SPELL_COUNT 2

typedef enum {
    ST_TARGET,
    ST_ZONE,
    ST_LINE,
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
    {.id = 0, .name = "Attack", .icon = 0, .type = ST_ZONE, .damage = 25, .range = 5, .zone_size = 3},
    {.id = 1, .name = "Hello?", .icon = 1, .type = ST_TARGET, .damage = 50, .range = 2},
};


#endif
