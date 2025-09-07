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
    // Mouvement
    {
        .name = "Move",
        .description = "Moves the player",
        .icon = SI_MOVE,
        .type = ST_MOVE,
        .range = 2,
        .speed = 100,
    },
    {
        .name = "Move Slow",
        .description = "Moves the player",
        .icon = SI_MOVE,
        .type = ST_MOVE,
        .range = 4,
        .speed = 50,
    },
    {
        .name = "Dash",
        .description = "Moves the player",
        .icon = SI_MOVE,
        .type = ST_MOVE,
        .range = 1,
        .speed = 150,
    },
    {.name = "Dodge",
     .description = "Moves the player only if he would have taken damage.\nAvoids damages. Does nothing if no damage "
                    "should have been taken.",
     .icon = SI_MOVE,
     .type = ST_MOVE,
     .range = 3,
     .speed = 200,
     .cooldown = 2},
    // Attacks
    {
        .name = "Target",
        .description = "Auto-Attack on single target",
        .cast_animation = SA_SLASH,
        .icon = SI_ATTACK,
        .type = ST_TARGET,
        .value = 25,
        .range = 3,
        .speed = 90,
    },
    {
        .name = "Focus",
        .description = "Increases next spell damage",
        .cast_animation = SA_FOCUS,
        .icon = SI_ATTACK,
        .type = ST_TARGET,
        .range = 0,
        .speed = 100,
        .cooldown = 2,
    },
    {.name = "Stun",
     .description = "Stuns the target",
     .cast_animation = SA_STUN,
     .type = ST_TARGET,
     .range = 1,
     .speed = 150,
     .cooldown = 4,
     .effect = SE_STUN,
     .effect_duration = 3},
    {.name = "Burn",
     .description = "Apply burning effect on target. Flat damage.",
     .cast_animation = SA_FIREBALL,
     .effect_animation = SA_BURN,
     .type = ST_TARGET,
     .value = 5,
     .range = 1,
     .speed = 150,
     .cooldown = 4,
     .effect = SE_BURN,
     .effect_duration = 3},
    {
        .name = "Poison",
        .description = "Apply poison effect on target.\n% remaining health scaling damage",
        .cast_animation = SA_POISON_CAST,
        .effect_animation = SA_POISON_TICK,
        .type = ST_TARGET,
        .value = 5,
        .range = 1,
        .speed = 125,
        .cooldown = 4,
        .effect = SE_POISON,
        .effect_duration = 3,
    },
    {
        .name = "Ice", //TODO: Damage + stat value
        .description = "Damages the target and slows him down",
        .cast_animation = SA_ICE_CAST,
        .effect_animation = SA_ICE_TICK,
        .type = ST_STAT,
        .stat = STAT_SPEED,
        .value = -25,
        .range = 2,
        .speed = 75,
        .cooldown = 3,
        .effect = SE_SLOW,
        .effect_duration = 3,
    },
    {
        .name = "Sacrify",
        .description = "Explodes the player but deals lot of damage around. Can be blocked",
        .cast_animation = SA_SACRIFY,
        .type = ST_AROUND,
        .value = 200,
        .range = 3,
        .speed = 150,
    },
    // Defensive
    {
        .name = "Block",
        .description = "If casted first, will block the next attack\nIf the ennemy is hit hits the player, the ennemy "
                       "will be stunned.",
        .cast_animation = SA_BLOCK,  // TODO
        .type = ST_TARGET,
        .range = 0,
        .speed = 50,
        .cooldown = 2,
    },
    {
        .name = "Cleanse",
        .description = "Removes all effect on the player",
        .cast_animation = SA_CLEANSE,
        .type = ST_TARGET,
        .range = 0,
        .speed = 100,
        .cooldown = 3,
    },
    {
        .name = "Banish",
        .description = "Banish the last casted spell from the target",
        .cast_animation = SA_BANISH,
        .type = ST_TARGET,
        .icon = SI_WAND,
        .range = 2,
        .speed = 100,
        .cooldown = 5,
    },
    {.name = "Revert",
     .description = "Sends back the last recieved damage from the current turn to the original caster.\nDoes nothing "
                    "if casted first.",
     .cast_animation = SA_REVERT,
     .type = ST_TARGET,
     .range = 0,
     .cooldown = 3},
    // Stats
    {
        .name = "Heal",
        .description = "Heals the player for 25HP",
        .cast_animation = SA_HEAL,
        .icon = SI_WAND,
        .type = ST_STAT,
        .stat = STAT_HEALTH,
        .value = 25,
        .range = 0,
        .speed = 100,
        .cooldown = 4,
    },
    {.name = "Fortify",
     .description = "Increases max health",
     .cast_animation = SA_FORTIFY,
     .icon = SI_WAND,
     .type = ST_STAT,
     .stat = STAT_HEALTH,
     .stat_max = true,
     .value = 15,
     .range = 0,
     .speed = 100,
     .cooldown = 2},
    {
        .name = "Slow down",
        .description = "Reduces the speed of the target",
        .cast_animation = SA_SLOWDOWN,
        .icon = SI_WAND,
        .type = ST_STAT,
        .stat = STAT_SPEED,
        .stat_max = true,
        .value = -25,
        .range = 2,
        .speed = 100,
        .cooldown = 3,
    },
    {
        .name = "Speed boost",
        .description = "Increases the speed of the target",
        .cast_animation = SA_SPEEDUP,
        .icon = SI_WAND,
        .type = ST_STAT,
        .stat = STAT_SPEED,
        .stat_max = true,
        .value = 25,
        .range = 2,
        .speed = 100,
        .cooldown = 3,
    }};

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
    return s->value + info->stats[STAT_AP].value / 4;
}
