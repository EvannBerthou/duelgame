#include "common.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_LOG_HISTORY 8192

char *log_history[MAX_LOG_HISTORY] = {0};
log_level log_levels[MAX_LOG_HISTORY] = {0};
int log_history_ptr = 0;

void LOG_(log_level level, const char *fmt, va_list args) {
#ifndef DEBUG
    if (level == LL_DEBUG) {
        return;
    }
#endif
    char formatted[1024] = {0};
    int len = vsnprintf(formatted, sizeof(formatted), fmt, args);
    int prefix = snprintf(NULL, 0, "[%s(%d)]", LOG_PREFIX, getpid());
    log_history[log_history_ptr] = malloc(len + prefix + 1);
    snprintf(log_history[log_history_ptr], len + prefix + 1, "[%s(%d)]%s", LOG_PREFIX, getpid(), formatted);
    log_levels[log_history_ptr] = level;

    printf("%s\n", log_history[log_history_ptr]);
    log_history_ptr++;
    if (log_history_ptr == MAX_LOG_HISTORY) {
        // TODO: Free previous log
        log_history_ptr = 0;
    }
}

void LOG(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LOG_(LL_INFO, fmt, args);
    va_end(args);
}

void LOGL(log_level level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LOG_(level, fmt, args);
    va_end(args);
}

const char *get_log(int idx) {
    if (idx < log_history_ptr && idx >= 0) {
        return log_history[idx];
    }
    return NULL;
}

log_level get_level(int idx) {
    if (idx < log_history_ptr && idx >= 0) {
        return log_levels[idx];
    }
    return LL_INFO;
}

int get_log_count() {
    return log_history_ptr;
}

void clear_logs() {
    log_history_ptr = 0;
}

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
        .name = "Move",
        .description = "Moves the player",
        .icon = SI_MOVE,
        .type = ST_MOVE,
        .range = 3,
        .speed = 100,
        .cooldown = 0,
    },
    {.name = "Target",
     .cast_animation = SA_SLASH,
     .icon = SI_ATTACK,
     .type = ST_TARGET,
     .damage = 25,
     .range = 3,
     .speed = 90,
     .cooldown = 0},
    {.name = "Move Slow", .icon = SI_MOVE, .type = ST_MOVE, .range = 1, .speed = 50, .cooldown = 0},
    {.name = "Stun",
     .cast_animation = SA_STUN,
     .icon = SI_UNKNOWN,
     .type = ST_TARGET,
     .range = 1,
     .speed = 150,
     .cooldown = 4,
     .effect = SE_STUN,
     .effect_duration = 3},
    {.name = "Burn",
     .cast_animation = SA_FIREBALL,
     .effect_animation = SA_BURN,
     .icon = SI_UNKNOWN,
     .type = ST_TARGET,
     .damage = 5,
     .range = 1,
     .speed = 150,
     .cooldown = 4,
     .effect = SE_BURN,
     .effect_duration = 3},
    {.name = "Heal",
     .description = "Heals the player for 25HP",
     .cast_animation = SA_HEAL,
     .icon = SI_WAND,
     .type = ST_STAT,
     .damage = 25,
     .range = 0,
     .speed = 100,
     .cooldown = 4,
     .effect = SE_HEAL,
     .effect_duration = 0},
    {0},
    {0},
    {0},
    {0},
    {0}};

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

int get_spell_damage(player_info *info, const spell *s) {
    return s->damage + info->stats[STAT_AP].value / 4;
}
