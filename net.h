#ifndef NET_H
#define NET_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

#define MAP_WIDTH 16
#define MAP_HEIGHT 8

#define MAX_PLAYER_COUNT 4

typedef enum {
    RS_PLAYING = 0,
    RS_WAITING = 1,
    RS_COUNT,
} round_state;

typedef enum {
    GS_WAITING,
    GS_STARTED,
    GS_ENDED,
    GS_COUNT
} game_state;

typedef enum {
    PA_NONE,
    PA_SPELL,
} player_action;

//TODO: Fix type IDs for the protocol
typedef enum {
    PKT_PING,
    PKT_JOIN,
    PKT_CONNECTED,
    PKT_GAME_START,
    PKT_PLAYER_UPDATE,
    PKT_PLAYER_BUILD,
    PKT_PLAYER_ACTION,
    PKT_ROUND_END,
    PKT_GAME_END,
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
} net_packet_game_start;

typedef struct {
    uint8_t id;
    uint8_t health;
    uint8_t x, y;
} net_packet_player_update;

net_packet_player_update pkt_player_update(uint8_t id, uint8_t health, uint8_t x, uint8_t y) {
    return (net_packet_player_update){id, health, x, y};
}

// Player build
// Includes stats and spells
typedef struct {
    uint8_t base_health;
    uint8_t spells[MAX_SPELL_COUNT]; // We only send IDs since spells are fixed by the game and not the player
} net_packet_player_build;

net_packet_player_build pkt_player_build(uint8_t base_health, uint8_t *spells) {
    net_packet_player_build packet = {.base_health = base_health};
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
        case PKT_PLAYER_UPDATE: {
            net_packet_player_update *p = (net_packet_player_update*)content;
            buf = packu8(buf, p->id);
            buf = packu8(buf, p->health);
            buf = packu8(buf, p->x);
            buf = packu8(buf, p->y);
            return buf;
        } break;
        case PKT_PLAYER_BUILD: {
            net_packet_player_build *p = (net_packet_player_build*)content;
            buf = packu8(buf, p->base_health);
            for (int i = 0; i < MAX_SPELL_COUNT; i++) {
                buf = packu8(buf, p->spells[i]);
            }
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
        default: exit(1);
    }
    return buf;
}

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
        case PKT_PLAYER_UPDATE: {
            net_packet_player_update *p = malloc(sizeof(net_packet_player_update));
            if (p == NULL) exit(1);
            p->id = buf[0];
            p->health = buf[1];
            p->x = buf[2];
            p->y = buf[3];
            return p;
        } break;
        case PKT_PLAYER_BUILD: {
            net_packet_player_build *p = malloc(sizeof(net_packet_player_build));
            if (p == NULL) exit(1);
            p->base_health = buf[0];
            for (int i = 0; i < MAX_SPELL_COUNT; i++) {
                p->spells[i] = buf[1 + i];
            }
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
        default: exit(1);
    }

    return NULL;
}

void write_packet(net_packet *p, int fd) {
    char buf[20] = {0};
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
        case PKT_PLAYER_UPDATE: return 4;
        case PKT_PLAYER_BUILD: return 2;
        case PKT_PLAYER_ACTION: return 5;
        case PKT_ROUND_END: return 0;
        case PKT_GAME_END: return 1;
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
