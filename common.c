#include "common.h"

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

// TODO: Add spells with effects (stun, root, DOT, kb)
const spell all_spells[] = {
    {
        .id = 0,
        .name = "Move",
        .description = "Moves the player",
        .icon = SI_MOVE,
        .type = ST_MOVE,
        .range = 3,
        .speed = 100,
        .cooldown = 0,
    },
    {.id = 1, .name = "Target", .icon = SI_ATTACK, .type = ST_TARGET, .damage = 25, .range = 3, .speed = 90, .cooldown = 0},
    {.id = 2, .name = "Zone", .icon = SI_WAND, .type = ST_TARGET, .damage = 50, .range = 2, .speed = 80, .cooldown = 0},
    {.id = 3, .name = "Move Slow", .icon = SI_MOVE, .type = ST_MOVE, .range = 1, .speed = 50, .cooldown = 0},
    {.id = 4,
     .name = "Stun",
     .icon = SI_UNKNOWN,
     .type = ST_TARGET,
     .range = 1,
     .speed = 150,
     .cooldown = 4,
     .effect = SE_STUN,
     .effect_duration = 3},
    {.id = 5,
     .name = "Burn",
     .icon = SI_UNKNOWN,
     .type = ST_TARGET,
     .damage = 5,
     .range = 1,
     .speed = 150,
     .cooldown = 4,
     .effect = SE_BURN,
     .effect_duration = 3},
    {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
    {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
    {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
};

const int spell_count = (int)(sizeof(all_spells) / sizeof(all_spells[0]));

void init_map(map_layer *m, int width, int height, uint8_t *copy) {
    m->width = width;
    m->height = height;
    if (m->content != NULL) {
        free(m->content);
    }
    m->content = malloc(m->width * m->height);
    if (copy != NULL) {
        for (int i = 0; i < width * height; i++) {
            m->content[i] = copy[i];
        }
    } else {
        clear_map(m);
    }
}

int get_map(map_layer *m, int x, int y) {
    if (x >= m->width || x < 0 || y >= m->height || y < 0)
        return -1;
    return m->content[y * m->width + x];
}

void set_map(map_layer *m, int x, int y, int v) {
    if (x >= m->width || x < 0 || y >= m->height || y < 0)
        return;
    m->content[y * m->width + x] = v;
}

void clear_map(map_layer *m) {
    for (int i = 0; i < m->width * m->height; i++) {
        m->content[i] = 0;
    }
}

void free_map(map_layer *m) {
    m->width = 0;
    m->height = 0;
    free(m->content);
    m->content = NULL;
}
