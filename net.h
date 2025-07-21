#ifndef NET_H
#define NET_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"

#define MAX_PLAYER_COUNT 4

typedef enum {
    RS_PLAYING,
    RS_WAITING,
    RS_PLAYING_ROUND,
    RS_WAITING_ANIMATIONS,
    RS_PLAYING_EFFECTS,
    RS_COUNT,
} round_state;

typedef enum {
    GS_WAITING,
    GS_STARTED,
    GS_ENDING,
    GS_ENDED,
    GS_COUNT
} game_state;

typedef enum {
    PA_NONE,
    PA_SPELL,
    PA_CANT_PLAY,
} player_action;

//TODO: Fixed type IDs for the protocol when more mature
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
    PKT_GAME_END,
    PKT_GAME_RESET,
    PKT_ADMIN_CONNECT,
    PKT_ADMIN_CONNECT_RESULT,
    PKT_ADMIN_UPDATE_PLAYER_INFO,
    PKT_COUNT
} net_packet_type_enum;

typedef uint8_t net_packet_type;

typedef struct {
    uint8_t len;
    net_packet_type type;
    void *content;
} net_packet;

typedef struct {
    uint8_t id;
    char username[8];
} net_packet_join;

net_packet_join pkt_join(uint8_t id, const char *username) {
    assert(strlen(username) <= 8);
    net_packet_join p = {.id = id};
    memcpy(p.username, username, 8);
    return p;
}

typedef struct {
    uint8_t id;
} net_packet_connected;

net_packet_connected pkt_connected(uint8_t id) {
    return (net_packet_connected){id};
}

typedef struct {
    uint8_t width, height;
    uint8_t type;
    uint8_t *content;
} net_packet_map;

net_packet_map pkt_map(uint8_t width, uint8_t height, uint8_t type, uint8_t *content) {
    return (net_packet_map){width, height, type, content};
}

typedef struct {
    uint8_t id;
    uint8_t health;
    uint8_t max_health;
    uint8_t ad;
    uint8_t ap;
    uint8_t x, y;
    uint8_t effect;
    uint8_t effect_round_left;
} net_packet_player_update;

net_packet_player_update pkt_player_update(uint8_t id, uint8_t health, uint8_t max_health, uint8_t ad, uint8_t ap, uint8_t x, uint8_t y, uint8_t effect, uint8_t erl) {
    return (net_packet_player_update){id, health, max_health, ad, ap, x, y, effect, erl};
}

// Player build
// Includes stats and spells
typedef struct {
    uint8_t base_health;
    uint8_t spells[MAX_SPELL_COUNT]; // We only send IDs since spells are fixed by the game and not the player
    uint8_t ad;
    uint8_t ap;
} net_packet_player_build;

net_packet_player_build pkt_player_build(uint8_t base_health, uint8_t *spells, uint8_t ad, uint8_t ap) {
    net_packet_player_build packet = {.base_health = base_health, .ad = ad, .ap = ap};
    for (int i = 0; i < MAX_SPELL_COUNT; i++) {
        packet.spells[i] = spells[i];
    }
    return packet;
}

typedef struct {
    uint8_t id;
    uint8_t action;
    uint8_t x, y;
    uint8_t spell;
} net_packet_player_action;

net_packet_player_action pkt_player_action(uint8_t id, uint8_t action, uint8_t x, uint8_t y, uint8_t spell) {
    return (net_packet_player_action){id, action, x, y, spell};
}

typedef struct {
    uint8_t winner_id;
} net_packet_game_end;

net_packet_game_end pkt_game_end(uint8_t winner_id) {
    return (net_packet_game_end) {winner_id};
}

typedef struct {
    char password[8];
} net_packet_admin_connect;

net_packet_admin_connect pkt_admin_connect(const char *password) {
    net_packet_admin_connect p = {0};
    for (int i = 0; i < 8; i++) {
        if (password[i] == '\0')
            break;
        p.password[i] = password[i];
    }
    return p;
}

typedef struct {
    uint8_t success;
} net_packet_admin_connect_result;

net_packet_admin_connect_result pkt_admin_connect_result(uint8_t result) {
    return (net_packet_admin_connect_result){result};
}

typedef enum {
    PIP_HEALTH,
} player_info_property;

typedef struct {
    uint8_t id;
    uint8_t property;
    uint8_t value;
} net_packet_admin_update_player_info;

net_packet_admin_update_player_info pkt_admin_update_player_info(uint8_t id, player_info_property prop, uint8_t value) {
    return (net_packet_admin_update_player_info){id, (uint8_t)prop, value};
}

char *packu8(char *buf, uint8_t u) {
    *buf = u;
    return buf + sizeof(uint8_t);
}

char *packsv(char *buf, char *str, int len) {
    while (len-- > 0) {
        *buf++ = *str++;
    }
    return buf;
}

char *packstruct(char *buf, void *content, net_packet_type_enum type) {
    switch (type) {
        case PKT_PING: return buf;
        case PKT_JOIN: {
            net_packet_join *p = (net_packet_join*)content;
            buf = packu8(buf, p->id);
            return packsv(buf, p->username, 8);
        } break;
        case PKT_CONNECTED: {
            net_packet_connected *p = (net_packet_connected*)content;
            return packu8(buf, p->id);
        } break;
        case PKT_GAME_START: return buf;
        case PKT_MAP: {
            net_packet_map *p = (net_packet_map*)content;
            buf = packu8(buf, p->width);
            buf = packu8(buf, p->height);
            buf = packu8(buf, p->type);
            for (int i = 0; i < p->width * p->height; i++) {
                buf = packu8(buf, p->content[i]);
            }
            return buf;
        }
        case PKT_PLAYER_UPDATE: {
            net_packet_player_update *p = (net_packet_player_update*)content;
            buf = packu8(buf, p->id);
            buf = packu8(buf, p->health);
            buf = packu8(buf, p->max_health);
            buf = packu8(buf, p->ad);
            buf = packu8(buf, p->ap);
            buf = packu8(buf, p->x);
            buf = packu8(buf, p->y);
            buf = packu8(buf, p->effect);
            buf = packu8(buf, p->effect_round_left);
            return buf;
        } break;
        case PKT_PLAYER_BUILD: {
            net_packet_player_build *p = (net_packet_player_build*)content;
            buf = packu8(buf, p->base_health);
            for (int i = 0; i < MAX_SPELL_COUNT; i++) {
                buf = packu8(buf, p->spells[i]);
            }
            buf = packu8(buf, p->ad);
            buf = packu8(buf, p->ap);
            return buf;
        } break;
        case PKT_PLAYER_ACTION: {
            net_packet_player_action *p = (net_packet_player_action*)content;
            buf = packu8(buf, p->id);
            buf = packu8(buf, p->action);
            buf = packu8(buf, p->x);
            buf = packu8(buf, p->y);
            buf = packu8(buf, p->spell);
            return buf;
        } break;
        case PKT_ROUND_END: return buf;
        case PKT_GAME_END: {
            net_packet_game_end *p = (net_packet_game_end*)content;
            buf = packu8(buf, p->winner_id);
            return buf;
        } break;
        case PKT_GAME_RESET: return buf;
        case PKT_ADMIN_CONNECT: {
            net_packet_admin_connect *p = (net_packet_admin_connect*)content;
            return packsv(buf, p->password, 8);
        } break;
        case PKT_ADMIN_CONNECT_RESULT: {
            net_packet_admin_connect_result *p = (net_packet_admin_connect_result*)content;
            return packu8(buf, p->success);
        } break;
        case PKT_ADMIN_UPDATE_PLAYER_INFO: {
            net_packet_admin_update_player_info *p = (net_packet_admin_update_player_info*)content;
            buf = packu8(buf, p->id);
            buf = packu8(buf, p->property);
            buf = packu8(buf, p->value);
            return buf;
        } break;
        default: exit(1);
    }
    return buf;
}

//TODO: Instead of using fixed offets, we should do something like `unpacku8` which keeps track of offset
void *unpackstruct(net_packet_type_enum type, char *buf) {
    switch (type) {
        case PKT_PING: return NULL;
        case PKT_JOIN: {
            net_packet_join *p = malloc(sizeof(net_packet_join));
            if (p == NULL) exit(1);
            p->id = buf[0];
            memcpy(p->username, buf + 1, 8);
            return p;
        } break;
        case PKT_CONNECTED: {
            net_packet_connected *p = malloc(sizeof(net_packet_connected));
            if (p == NULL) exit(1);
            p->id = buf[0];
            return p;
        } break;
        case PKT_GAME_START: return NULL;
        case PKT_MAP: {
            net_packet_map *p = malloc(sizeof(net_packet_map));
            if (p == NULL) exit(1);
            p->width = buf[0];
            p->height = buf[1];
            p->type = buf[2];
            p->content = malloc(p->width * p->height);
            for (int i = 0; i < p->width * p->height; i++) {
                p->content[i] = buf[3 + i];
            }
            return p;
        }
        case PKT_PLAYER_UPDATE: {
            net_packet_player_update *p = malloc(sizeof(net_packet_player_update));
            if (p == NULL) exit(1);
            p->id = buf[0];
            p->health = buf[1];
            p->max_health = buf[2];
            p->ad = buf[3];
            p->ap = buf[4];
            p->x = buf[5];
            p->y = buf[6];
            p->effect = buf[7];
            p->effect_round_left = buf[8];
            return p;
        } break;
        case PKT_PLAYER_BUILD: {
            net_packet_player_build *p = malloc(sizeof(net_packet_player_build));
            if (p == NULL) exit(1);
            p->base_health = buf[0];
            for (int i = 0; i < MAX_SPELL_COUNT; i++) {
                p->spells[i] = buf[1 + i];
            }
            p->ad = buf[1 + MAX_SPELL_COUNT];
            p->ap = buf[1 + MAX_SPELL_COUNT + 1];
            return p;
        } break;
        case PKT_PLAYER_ACTION: {
            net_packet_player_action *p = malloc(sizeof(net_packet_player_action));
            if (p == NULL) exit(1);
            p->id = buf[0];
            p->action = buf[1];
            p->x = buf[2];
            p->y = buf[3];
            p->spell = buf[4];
            return p;
        } break;
        case PKT_ROUND_END: return NULL;
        case PKT_GAME_END: {
            net_packet_game_end *p = malloc(sizeof(net_packet_game_end));
            if (p == NULL) exit(1);
            p->winner_id = buf[0];
            return p;
        } break;
        case PKT_GAME_RESET: return NULL;
        case PKT_ADMIN_CONNECT: {
            net_packet_admin_connect *p = malloc(sizeof(net_packet_admin_connect));
            if (p == NULL) exit(1);
            memcpy(p->password, buf, 8);
            return p;
        } break;
        case PKT_ADMIN_CONNECT_RESULT: {
            net_packet_admin_connect_result *p = malloc(sizeof(net_packet_admin_connect_result));
            if (p == NULL) exit(1);
            p->success = buf[0];
            return p;
        } break;
        case PKT_ADMIN_UPDATE_PLAYER_INFO: {
            net_packet_admin_update_player_info *p = malloc(sizeof(net_packet_admin_update_player_info));
            if (p == NULL) exit(1);
            p->id = buf[0];
            p->property = buf[1];
            p->value = buf[2];
            return p;
        } break;
        default: exit(1);
    }

    return NULL;
}

void write_packet(net_packet *p, int fd) {
    static char buf[256] = {0};
    char *b = buf;
    b = packu8(b, p->len);
    b = packu8(b, p->type);
    b = packstruct(b, p->content, p->type);

    b = buf;
    int packet_size_left = p->len;
    int n = 0;
    while ((n = write(fd, b, packet_size_left)) > 0) {
        packet_size_left -= n;
        b += n;
    }
    if (n < 0) {
        fprintf(stderr, "Error sending packet\n");
        exit(1);
    }
}

int packet_read(net_packet *p, int fd) {
    uint8_t packet_len;
    int n = read(fd, &packet_len, sizeof(packet_len));
    if (n == 0) {
        return -1;
    }
    p->len = packet_len;

    char buf[256] = {0};
    char *b = buf;
    int packet_size_left = packet_len - 1;
    n = 0;
    while ((n = read(fd, buf, packet_size_left)) > 0) {
        packet_size_left -= n;
        b += n;
    }

    p->type = buf[0];
    p->content = unpackstruct(p->type, buf + 1);
    return 0;
}

uint8_t get_packet_length(net_packet_type_enum type, void *p) {
    (void)p;
    switch (type) {
        case PKT_PING: return 0;
        case PKT_JOIN: return 9;
        case PKT_CONNECTED: return 1;
        case PKT_GAME_START: return 0;
        case PKT_MAP: {
            net_packet_map *map = (net_packet_map*)p;
            int map_length = map->width * map->height;
            return 3 + map_length;
        }
        case PKT_PLAYER_UPDATE: return 9;
        case PKT_PLAYER_BUILD: return 3 + MAX_SPELL_COUNT;
        case PKT_PLAYER_ACTION: return 5;
        case PKT_ROUND_END: return 0;
        case PKT_GAME_END: return 1;
        case PKT_GAME_RESET: return 0;
        case PKT_ADMIN_CONNECT: return 8;
        case PKT_ADMIN_CONNECT_RESULT: return 1;
        case PKT_ADMIN_UPDATE_PLAYER_INFO: return 3;
        default: { fprintf(stderr, "Unknown type\n"); exit(1); }
    }
}

void send_sock(net_packet_type_enum type, void *p, int fd) {
    net_packet packet = {0};
    packet.len = 2 + get_packet_length(type, p);
    packet.type = type;
    packet.content = p;
    write_packet(&packet, fd);
}

#endif
