#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H
// This file was auto-generated by src/net_protocol_builder.c
#include <stdint.h>
#include "common.h"

#define NET_SIZE(...)

typedef struct {
} net_packet_ping;

typedef struct {
    uint8_t id;
    char username[8];
} net_packet_join;

typedef struct {
    uint8_t id;
} net_packet_connected;

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t type;
    uint8_t *content NET_SIZE("s->width * s->height");
} net_packet_map;

typedef struct {
} net_packet_game_start;

typedef struct {
    uint8_t id;
    uint8_t health;
    uint8_t max_health;
    uint8_t ad;
    uint8_t ap;
    uint8_t x;
    uint8_t y;
    uint8_t effect;
    uint8_t effect_round_left;
} net_packet_player_update;

// Player build
// Includes stats and spells
typedef struct {
    uint8_t id;
    uint8_t base_health;
    // We only send IDs since spells are fixed by the game and not the player
    uint8_t spells[MAX_SPELL_COUNT] NET_SIZE("MAX_SPELL_COUNT");
    uint8_t ad;
    uint8_t ap;
} net_packet_player_build;

typedef struct {
    uint8_t id;
    uint8_t action;
    uint8_t x;
    uint8_t y;
    uint8_t spell;
} net_packet_player_action;

typedef struct {
    uint8_t winner_id;
} net_packet_round_end;

typedef struct {
} net_packet_game_reset;

typedef struct {
    char password[8];
} net_packet_admin_connect;

typedef struct {
    uint8_t success;
} net_packet_admin_connect_result;

typedef struct {
    uint8_t id;
    uint8_t property;
    uint8_t value;
} net_packet_admin_update_player_info;

typedef enum {
    PKT_PING,
    PKT_JOIN,
    PKT_CONNECTED,
    PKT_MAP,
    PKT_GAME_START,
    PKT_PLAYER_UPDATE,
    PKT_PLAYER_BUILD,
    PKT_PLAYER_ACTION,
    PKT_ROUND_END,
    PKT_GAME_RESET,
    PKT_ADMIN_CONNECT,
    PKT_ADMIN_CONNECT_RESULT,
    PKT_ADMIN_UPDATE_PLAYER_INFO,
} net_packet_type_enum;
uint8_t get_packet_length(net_packet_type_enum type, void *p) {
    switch (type) {
        case PKT_PING: return 0;
        case PKT_JOIN: return 9;
        case PKT_CONNECTED: return 1;
        case PKT_MAP: {
            net_packet_map *s = (net_packet_map*)p;
            (void)s;
            return 3 + (s->width * s->height);
        }
        case PKT_GAME_START: return 0;
        case PKT_PLAYER_UPDATE: return 9;
        case PKT_PLAYER_BUILD: {
            net_packet_player_build *s = (net_packet_player_build*)p;
            (void)s;
            return 4 + (MAX_SPELL_COUNT);
        }
        case PKT_PLAYER_ACTION: return 5;
        case PKT_ROUND_END: return 1;
        case PKT_GAME_RESET: return 0;
        case PKT_ADMIN_CONNECT: return 8;
        case PKT_ADMIN_CONNECT_RESULT: return 1;
        case PKT_ADMIN_UPDATE_PLAYER_INFO: return 3;
        default: { fprintf(stderr, "Unknown type\n"); exit(1); }
    }
}
net_packet_ping pkt_ping() {
    net_packet_ping result = {};
    net_packet_ping *s = &result;
    (void)s;
    return result;
}
net_packet_join pkt_join(uint8_t id, const char *username) {
    net_packet_join result = {};
    net_packet_join *s = &result;
    (void)s;
    s->id = id;
    memcpy(s->username, username, 8);
    s->username[8 - 1] = 0;
    return result;
}
net_packet_connected pkt_connected(uint8_t id) {
    net_packet_connected result = {};
    net_packet_connected *s = &result;
    (void)s;
    s->id = id;
    return result;
}
net_packet_map pkt_map(uint8_t width, uint8_t height, uint8_t type, uint8_t *content) {
    net_packet_map result = {};
    net_packet_map *s = &result;
    (void)s;
    s->width = width;
    s->height = height;
    s->type = type;
    s->content = content;
    return result;
}
net_packet_game_start pkt_game_start() {
    net_packet_game_start result = {};
    net_packet_game_start *s = &result;
    (void)s;
    return result;
}
net_packet_player_update pkt_player_update(uint8_t id, uint8_t health, uint8_t max_health, uint8_t ad, uint8_t ap, uint8_t x, uint8_t y, uint8_t effect, uint8_t effect_round_left) {
    net_packet_player_update result = {};
    net_packet_player_update *s = &result;
    (void)s;
    s->id = id;
    s->health = health;
    s->max_health = max_health;
    s->ad = ad;
    s->ap = ap;
    s->x = x;
    s->y = y;
    s->effect = effect;
    s->effect_round_left = effect_round_left;
    return result;
}
net_packet_player_build pkt_player_build(uint8_t id, uint8_t base_health, uint8_t *spells, uint8_t ad, uint8_t ap) {
    net_packet_player_build result = {};
    net_packet_player_build *s = &result;
    (void)s;
    s->id = id;
    s->base_health = base_health;
    memcpy(s->spells, spells, MAX_SPELL_COUNT);
    s->ad = ad;
    s->ap = ap;
    return result;
}
net_packet_player_action pkt_player_action(uint8_t id, uint8_t action, uint8_t x, uint8_t y, uint8_t spell) {
    net_packet_player_action result = {};
    net_packet_player_action *s = &result;
    (void)s;
    s->id = id;
    s->action = action;
    s->x = x;
    s->y = y;
    s->spell = spell;
    return result;
}
net_packet_round_end pkt_round_end(uint8_t winner_id) {
    net_packet_round_end result = {};
    net_packet_round_end *s = &result;
    (void)s;
    s->winner_id = winner_id;
    return result;
}
net_packet_game_reset pkt_game_reset() {
    net_packet_game_reset result = {};
    net_packet_game_reset *s = &result;
    (void)s;
    return result;
}
net_packet_admin_connect pkt_admin_connect(const char *password) {
    net_packet_admin_connect result = {};
    net_packet_admin_connect *s = &result;
    (void)s;
    memcpy(s->password, password, 8);
    s->password[8 - 1] = 0;
    return result;
}
net_packet_admin_connect_result pkt_admin_connect_result(uint8_t success) {
    net_packet_admin_connect_result result = {};
    net_packet_admin_connect_result *s = &result;
    (void)s;
    s->success = success;
    return result;
}
net_packet_admin_update_player_info pkt_admin_update_player_info(uint8_t id, uint8_t property, uint8_t value) {
    net_packet_admin_update_player_info result = {};
    net_packet_admin_update_player_info *s = &result;
    (void)s;
    s->id = id;
    s->property = property;
    s->value = value;
    return result;
}
char *packu8(char *buf, uint8_t u);
char *packsv(char *buf, char *str, int len);
char *packstruct(char *buf, void *content, net_packet_type_enum type) {
    switch (type) {
        case PKT_PING: {
            return buf;
        } break;
        case PKT_JOIN: {
            net_packet_join *s = (net_packet_join*)content;
            buf = packu8(buf, s->id);
            buf = packsv(buf, s->username, 8);
            return buf;
        } break;
        case PKT_CONNECTED: {
            net_packet_connected *s = (net_packet_connected*)content;
            buf = packu8(buf, s->id);
            return buf;
        } break;
        case PKT_MAP: {
            net_packet_map *s = (net_packet_map*)content;
            buf = packu8(buf, s->width);
            buf = packu8(buf, s->height);
            buf = packu8(buf, s->type);
            for (int i = 0; i < s->width * s->height; i++) {
                buf = packu8(buf, s->content[i]);
            }
            return buf;
        } break;
        case PKT_GAME_START: {
            return buf;
        } break;
        case PKT_PLAYER_UPDATE: {
            net_packet_player_update *s = (net_packet_player_update*)content;
            buf = packu8(buf, s->id);
            buf = packu8(buf, s->health);
            buf = packu8(buf, s->max_health);
            buf = packu8(buf, s->ad);
            buf = packu8(buf, s->ap);
            buf = packu8(buf, s->x);
            buf = packu8(buf, s->y);
            buf = packu8(buf, s->effect);
            buf = packu8(buf, s->effect_round_left);
            return buf;
        } break;
        case PKT_PLAYER_BUILD: {
            net_packet_player_build *s = (net_packet_player_build*)content;
            buf = packu8(buf, s->id);
            buf = packu8(buf, s->base_health);
            for (int i = 0; i < MAX_SPELL_COUNT; i++) {
                buf = packu8(buf, s->spells[i]);
            }
            buf = packu8(buf, s->ad);
            buf = packu8(buf, s->ap);
            return buf;
        } break;
        case PKT_PLAYER_ACTION: {
            net_packet_player_action *s = (net_packet_player_action*)content;
            buf = packu8(buf, s->id);
            buf = packu8(buf, s->action);
            buf = packu8(buf, s->x);
            buf = packu8(buf, s->y);
            buf = packu8(buf, s->spell);
            return buf;
        } break;
        case PKT_ROUND_END: {
            net_packet_round_end *s = (net_packet_round_end*)content;
            buf = packu8(buf, s->winner_id);
            return buf;
        } break;
        case PKT_GAME_RESET: {
            return buf;
        } break;
        case PKT_ADMIN_CONNECT: {
            net_packet_admin_connect *s = (net_packet_admin_connect*)content;
            buf = packsv(buf, s->password, 8);
            return buf;
        } break;
        case PKT_ADMIN_CONNECT_RESULT: {
            net_packet_admin_connect_result *s = (net_packet_admin_connect_result*)content;
            buf = packu8(buf, s->success);
            return buf;
        } break;
        case PKT_ADMIN_UPDATE_PLAYER_INFO: {
            net_packet_admin_update_player_info *s = (net_packet_admin_update_player_info*)content;
            buf = packu8(buf, s->id);
            buf = packu8(buf, s->property);
            buf = packu8(buf, s->value);
            return buf;
        } break;
    }
    return buf;
}
void unpacksv(uint8_t **buf, char *dest, uint8_t len);
uint8_t unpacku8(uint8_t **buf);
void *unpackstruct(net_packet_type_enum type, uint8_t *buf) {
    uint8_t **base = &buf;
    switch (type) {
        case PKT_PING: return NULL;
        case PKT_JOIN: {
            net_packet_join *s = malloc(sizeof(net_packet_join));
            if (s == NULL) exit(1);
            s->id = unpacku8(base);
            unpacksv(base, s->username, 8);
            return s;
        } break;
        case PKT_CONNECTED: {
            net_packet_connected *s = malloc(sizeof(net_packet_connected));
            if (s == NULL) exit(1);
            s->id = unpacku8(base);
            return s;
        } break;
        case PKT_MAP: {
            net_packet_map *s = malloc(sizeof(net_packet_map));
            if (s == NULL) exit(1);
            s->width = unpacku8(base);
            s->height = unpacku8(base);
            s->type = unpacku8(base);
            s->content = malloc(s->width * s->height);
            for (int i = 0; i < s->width * s->height; i++) {
                s->content[i] = unpacku8(base);
            }
            return s;
        } break;
        case PKT_GAME_START: return NULL;
        case PKT_PLAYER_UPDATE: {
            net_packet_player_update *s = malloc(sizeof(net_packet_player_update));
            if (s == NULL) exit(1);
            s->id = unpacku8(base);
            s->health = unpacku8(base);
            s->max_health = unpacku8(base);
            s->ad = unpacku8(base);
            s->ap = unpacku8(base);
            s->x = unpacku8(base);
            s->y = unpacku8(base);
            s->effect = unpacku8(base);
            s->effect_round_left = unpacku8(base);
            return s;
        } break;
        case PKT_PLAYER_BUILD: {
            net_packet_player_build *s = malloc(sizeof(net_packet_player_build));
            if (s == NULL) exit(1);
            s->id = unpacku8(base);
            s->base_health = unpacku8(base);
            for (int i = 0; i < MAX_SPELL_COUNT; i++) {
                s->spells[i] = unpacku8(base);
            }
            s->ad = unpacku8(base);
            s->ap = unpacku8(base);
            return s;
        } break;
        case PKT_PLAYER_ACTION: {
            net_packet_player_action *s = malloc(sizeof(net_packet_player_action));
            if (s == NULL) exit(1);
            s->id = unpacku8(base);
            s->action = unpacku8(base);
            s->x = unpacku8(base);
            s->y = unpacku8(base);
            s->spell = unpacku8(base);
            return s;
        } break;
        case PKT_ROUND_END: {
            net_packet_round_end *s = malloc(sizeof(net_packet_round_end));
            if (s == NULL) exit(1);
            s->winner_id = unpacku8(base);
            return s;
        } break;
        case PKT_GAME_RESET: return NULL;
        case PKT_ADMIN_CONNECT: {
            net_packet_admin_connect *s = malloc(sizeof(net_packet_admin_connect));
            if (s == NULL) exit(1);
            unpacksv(base, s->password, 8);
            return s;
        } break;
        case PKT_ADMIN_CONNECT_RESULT: {
            net_packet_admin_connect_result *s = malloc(sizeof(net_packet_admin_connect_result));
            if (s == NULL) exit(1);
            s->success = unpacku8(base);
            return s;
        } break;
        case PKT_ADMIN_UPDATE_PLAYER_INFO: {
            net_packet_admin_update_player_info *s = malloc(sizeof(net_packet_admin_update_player_info));
            if (s == NULL) exit(1);
            s->id = unpacku8(base);
            s->property = unpacku8(base);
            s->value = unpacku8(base);
            return s;
        } break;
    }
    return NULL;
}
#endif
