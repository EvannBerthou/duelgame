#include "common.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

char *inttostr(int i) {
    static char out[4] = {0};
    memset(out, 0, 4);
    if (i == 0) {
        out[0] = '0';
        out[1] = '\0';
        return out;
    }
    int idx = 0;
    while (i > 0) {
        out[idx] = i % 10 + '0';
        i /= 10;
        idx++;
    }
    for (int j = 0, k = idx - 1; j < k; j++, k--) {
        char temp = out[j];
        out[j] = out[k];
        out[k] = temp;
    }
    return out;
}

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
     .effect = SE_DODGE,
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
        .damage_value = 25,
        .range = 3,
        .speed = 90,
    },
    {
        .name = "Focus",
        .description = "Increases next spell damage",
        .cast_animation = SA_FOCUS,
        .icon = SI_ATTACK,
        .type = ST_TARGET,
        .effect = SE_FOCUS,
        .cast_type = CT_CAST,
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
     .cast_type = CT_CAST_EFFECT,
     .type = ST_TARGET,
     .damage_value = 5,
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
        .cast_type = CT_EFFECT,
        .type = ST_TARGET,
        .damage_value = 10,
        .range = 1,
        .speed = 125,
        .cooldown = 4,
        .effect = SE_POISON,
        .effect_duration = 3,
    },
    {
        .name = "Ice",
        .description = "Damages the target and slows him down",
        .cast_animation = SA_ICE_CAST,
        .effect_animation = SA_ICE_TICK,
        .type = ST_TARGET,
        .stat = STAT_SPEED,
        .damage_value = 10,
        .stat_value = -75,
        .range = 2,
        .speed = 75,
        .cooldown = 3,
        .effect = SE_SLOW,
        .effect_duration = 3,
    },
    {
        .name = "Sacrify",
        .description = "Explodes the player but deals lot of damage around.\nCan be blocked",
        .cast_animation = SA_SACRIFY,
        .type = ST_AROUND,
        .damage_value = 200,
        .range = 3,
        .speed = 150,
    },
    // Defensive
    {
        .name = "Block",
        .description = "If casted first, will block the next attack\nIf the ennemy is hit hits the player,\nthe ennemy "
                       "will be stunned.",
        .cast_animation = SA_BLOCK,
        .type = ST_TARGET,
        .effect = SE_BLOCK,
        .range = 0,
        .speed = 120,
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
        .effect = SE_CLEANSE,
    },
    {
        .name = "Banish",
        .description = "Banish the last casted spell from the target",
        .cast_animation = SA_BANISH,
        .effect = SE_BANISH,
        .type = ST_TARGET,
        .icon = SI_WAND,
        .range = 2,
        .speed = 100,
        .cooldown = 5,
    },
    {.name = "Revert",
     .description = "Sends back the last recieved damage from the current turn\nto the original caster.\nDoes nothing "
                    "if casted first.",
     .cast_animation = SA_REVERT,
     .type = ST_TARGET,
     .range = 0,
     .speed = 100,
     .cooldown = 3},
    {.name = "Knockback",
     .description = "Pushes the target 2 tiles back.\nIf it hits an obstacle, he will be stunned",
     .cast_animation = SA_KOCKBACK,
     .type = ST_TARGET,
     .range = 2,
     .speed = 100},
    // Stats
    {
        .name = "Heal",
        .description = "Heals the player for 25HP",
        .cast_animation = SA_HEAL,
        .icon = SI_WAND,
        .type = ST_TARGET,
        .stat = STAT_HEALTH,
        .stat_value = 25,
        .range = 0,
        .speed = 100,
        .cooldown = 4,
    },
    {.name = "Fortify",
     .description = "Increases max health",
     .cast_animation = SA_FORTIFY,
     .icon = SI_WAND,
     .stat = STAT_HEALTH,
     .stat_max = true,
     .stat_value = 15,
     .range = 0,
     .speed = 100,
     .cooldown = 2},
    {
        .name = "Slow down",
        .description = "Reduces the speed of the target",
        .cast_animation = SA_SLOWDOWN,
        .icon = SI_WAND,
        .type = ST_TARGET,
        .stat = STAT_SPEED,
        .stat_max = true,
        .stat_value = -25,
        .range = 2,
        .speed = 100,
        .cooldown = 3,
    },
    {
        .name = "Speed boost",
        .description = "Increases the speed of the target",
        .cast_animation = SA_SPEEDUP,
        .icon = SI_WAND,
        .type = ST_TARGET,
        .stat = STAT_SPEED,
        .stat_max = true,
        .stat_value = 25,
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
    if (s->damage_value == 0) {
        return 0;
    }

    if (s->cast_type == CT_EFFECT && info->effect[s->effect] == false) {
        return 0;
    }

    // Atleast 1 damage until 1 hp
    if (s->effect == SE_POISON) {
        if (info->stats[STAT_HEALTH].value == 1) {
            return 0;
        }
        return fmax(1, round(info->stats[STAT_HEALTH].value * (s->damage_value / 100.f)));
    }

    if (info->turn_effect == SE_FOCUS) {
        return 12;
    }

    return s->damage_value + info->stats[STAT_AP].value / 4;
}

void apply_effect(player_info *p, const spell *s) {
    if (s->effect == SE_CLEANSE) {
        for (int i = 0; i < SE_COUNT; i++) {
            p->effect[i] = false;
            p->effect_round_left[i] = 0;
            p->spell_effect[i] = NULL;
        }
    } else if (s->effect == SE_FOCUS) {
        p->turn_effect = SE_FOCUS;
        p->turn_effect_duration_left = 1;
    } else if (s->effect == SE_BLOCK) {
        p->turn_effect = SE_BLOCK;
        p->turn_effect_duration_left = 0;
    } else if (s->effect == SE_BANISH) {
        LOG("Banning %s from %s", all_spells[p->last_spell].name, p->name);
        if (p->last_spell != NO_SPELL) {
            p->banned[p->last_spell] = true;
        } else {
            LOG("No spells to ban for %s", p->name);
        }
    } else {
        LOG("Applying effect %d to %s", s->effect, p->name);
        p->effect[s->effect] = true;
        p->effect_round_left[s->effect] = s->effect_duration;
        p->spell_effect[s->effect] = s;
    }
}

void init_queue(queue *q, size_t elem_size) {
    q->content = malloc(elem_size * MAX_QUEUE_SIZE);
    q->elem_size = elem_size;
    reset_queue(q);
}

bool queue_full(queue *q) {
    return q->size == MAX_QUEUE_SIZE;
}

bool queue_empty(queue *q) {
    return q->size == 0;
}

bool queue_push(queue *q, void *data) {
    if (queue_full(q)) {
        return false;
    }

    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    memcpy(q->content + q->rear * q->elem_size, data, q->elem_size);
    q->size++;
    return true;
}

bool queue_pop(queue *q, void *out) {
    if (queue_empty(q)) {
        return false;
    }
    memcpy(out, q->content + q->front * q->elem_size, q->elem_size);
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    return true;
}

void reset_queue(queue *q) {
    q->size = 0;
    q->front = 0;
    q->rear = -1;
}

typedef enum { MLS_NONE, MLS_HEADER, MLS_SPAWN, MLS_MAP, MLS_PROPS } map_loading_stage;

map_loading_stage loading_stage = MLS_NONE;
int spawn_point_index = 0;

char *strip(char *line) {
    while (isspace(*line)) {
        line++;
    }
    char *end = line + strlen(line) - 1;
    while (isspace(*end) && end > line) {
        *end = '\0';
        end--;
    }
    return line;
}

map_header parse_header_line(char *line) {
    char *key = strip(strtok(line, ":"));
    char *value = strip(strtok(NULL, "\0"));

    if (strcmp(key, "name") == 0) {
        return (map_header){MAP_HEADER_NAME, strdup(value)};
    } else {
        return (map_header){MAP_HEADER_UNKNOWN, NULL};
    }
}

// TODO: Add some checks on ssccanf inside @SPAWN, @MAP and @PROPS to ensure validity
bool handle_map_line(char *line, map_data *map) {
    if (strcmp(line, "@HEADER") == 0) {
        loading_stage = MLS_HEADER;
    } else if (strcmp(line, "@SPAWN") == 0) {
        loading_stage = MLS_SPAWN;
    } else if (strcmp(line, "@SPAWN") == 0) {
        loading_stage = MLS_SPAWN;
    } else if (strcmp(line, "@MAP") == 0) {
        loading_stage = MLS_MAP;
    } else if (strcmp(line, "@PROPS") == 0) {
        loading_stage = MLS_PROPS;
    } else if (strcmp(line, "@END") == 0) {
        loading_stage = MLS_NONE;
    } else {
        if (loading_stage == MLS_HEADER) {
            map_header header = parse_header_line(line);
            if (header.key != MAP_HEADER_UNKNOWN) {
                map->headers[header.key].value = header.value;
            }
        } else if (loading_stage == MLS_SPAWN) {
            assert(spawn_point_index < MAX_PLAYER_COUNT);
            sscanf(line, "%hhu %hhu", &map->spawn_positions[spawn_point_index][0],
                   &map->spawn_positions[spawn_point_index][1]);
            spawn_point_index++;
        } else if (loading_stage == MLS_MAP) {
            int idx = 0;
            while (*line != '\0') {
                if (*line != ' ') {
                    sscanf(line, "%hhu", &map->map[idx++]);
                }
                line++;
            }
            assert(idx == MAP_WIDTH * MAP_HEIGHT);
        } else if (loading_stage == MLS_PROPS) {
            int idx = 0;
            while (*line != '\0') {
                if (*line != ' ') {
                    sscanf(line, "%hhu", &map->props[idx++]);
                }
                line++;
            }
            assert(idx == MAP_WIDTH * MAP_HEIGHT);
        } else {
            return false;
        }
    }
    return true;
}

bool load_map(const char *filepath, map_data *map) {
    char fullpath[256] = {0};
    strcat(fullpath, "maps/");
    strncat(fullpath, filepath, 256 - strlen("maps/") - strlen(".map"));
    strcat(fullpath, ".map");
    LOG("Loading map '%s'", fullpath);
    FILE *f = fopen(fullpath, "r");
    if (f == NULL) {
        return false;
    }

    spawn_point_index = 0;
    for (int i = 0; i < MAP_HEIGHT * MAP_WIDTH; i++) {
        map->map[i] = 0;
        map->props[i] = 0;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    char *string = malloc(file_size + 1);
    if (string == NULL) {
        fclose(f);
        return false;
    }
    fread(string, file_size, 1, f);
    fclose(f);

    char *line = string;
    do {
        char *next = strchrnul(line, '\n');
        if (*next == '\0') {
            break;
        }
        *next = '\0';
        if (handle_map_line(line, map) == false) {
            free(string);
            return false;
        }
        line = next + 1;
    } while (true);

    // TODO: Once map is loaded we should do some checks on validty such as avoid duplicate spawn points or spawn points
    // inside walls

    free(string);
    LOG("Map loaded!");
    return true;
}

void write_string(FILE *f, const char *str) {
    size_t len = strlen(str);
    fwrite(str, len, 1, f);
}

bool save_map(const char *filepath, map_data *map) {
    char fullpath[256] = {0};
    strcat(fullpath, "maps/");
    strncat(fullpath, filepath, 256 - strlen("maps/") - strlen(".map"));
    strcat(fullpath, ".map");
    LOG("Saving map at %s", fullpath);
    FILE *f = fopen(fullpath, "w");
    if (f == NULL) {
        return false;
    }

    write_string(f, "@HEADER\n");
    write_string(f, "name: ");
    write_string(f, map->headers[MAP_HEADER_NAME].value);
    write_string(f, "\n");
    write_string(f, "@END\n");

    write_string(f, "@SPAWN\n");
    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        write_string(f, inttostr(map->spawn_positions[i][0]));
        write_string(f, " ");
        write_string(f, inttostr(map->spawn_positions[i][1]));
        write_string(f, "\n");
    }
    write_string(f, "@END\n");

    write_string(f, "@MAP\n");
    for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++) {
        write_string(f, inttostr(map->map[i]));
        write_string(f, " ");
    }
    write_string(f, "\n");
    write_string(f, "@END\n");

    write_string(f, "@PROPS\n");
    for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++) {
        write_string(f, inttostr(map->props[i]));
        write_string(f, " ");
    }
    write_string(f, "\n");
    write_string(f, "@END\n");

    fclose(f);
    LOG("Map saved!");
    return true;
}

void free_map_data(map_data *map) {
    for (int i = 0; i < MAP_HEADER_COUNT; i++) {
        free((void *)map->headers[i].value);
    }
}

bool create_map(const char *filename) {
    map_data map = {0};
    map.headers[MAP_HEADER_NAME].value = strdup(filename);
    memset(map.map, 0, sizeof(map.map));
    memset(map.props, 0, sizeof(map.props));
    bool result = save_map(filename, &map);
    if (result == false) {
        free((void *)map.headers[MAP_HEADER_NAME].value);
    }
    return result;
}
