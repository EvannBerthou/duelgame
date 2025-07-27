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
#include "net_protocol.h"

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

typedef uint8_t net_packet_type;

typedef struct {
    uint8_t len;
    net_packet_type type;
    void *content;
} net_packet;

#define GAME_NOT_ENDED 254
#define GAME_TIE 255

typedef enum {
    PIP_HEALTH,
} player_info_property;

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

uint8_t unpacku8(uint8_t **buf) {
    uint8_t res = **buf;
    (*buf)++;
    return res;
}

void unpacksv(uint8_t **buf, char *dest, uint8_t len) {
    for (int i = 0; i < len; i++) {
        dest[i] = **buf;
        (*buf)++;
    }
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

//TODO: We should check the size and avoid reading too much
int packet_read(net_packet *p, int fd) {
    uint8_t packet_len;
    int n = read(fd, &packet_len, sizeof(packet_len));
    if (n == 0) {
        return -1;
    }
    p->len = packet_len;

    uint8_t buf[256] = {0};
    uint8_t *b = buf;
    int packet_size_left = packet_len - 1;
    n = 0;
    while ((n = read(fd, buf, packet_size_left)) > 0) {
        packet_size_left -= n;
        b += n;
    }

    uint8_t *base = buf;
    p->type = unpacku8(&base);
    p->content = unpackstruct(p->type, base);
    return 0;
}

void send_sock(net_packet_type_enum type, void *p, int fd) {
    net_packet packet = {0};
    packet.len = 2 + get_packet_length(type, p);
    packet.type = type;
    packet.content = p;
    write_packet(&packet, fd);
}

#endif
