#ifndef NET_H
#define NET_H

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef WINDOWS_BUILD
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif
#include <unistd.h>
#include "common.h"
#define NET_PROTOCOL_IMPLEMENTATION
#include "net_protocol.h"

#define send_packet(PACKET_FUNC, FD)            \
    do {                                        \
        net_packet p##__LINE__ = (PACKET_FUNC); \
        send_sock(&p##__LINE__, FD);            \
    } while (0)

#define GAME_TIE 255

typedef enum {
    PIP_HEALTH,
} player_info_property;

char* packu8(char* buf, uint8_t u) {
    *buf++ = u;
    return buf;
}

char* packu64(char* buf, uint64_t u) {
    *buf++ = u >> 56;
    *buf++ = u >> 48;
    *buf++ = u >> 40;
    *buf++ = u >> 32;
    *buf++ = u >> 24;
    *buf++ = u >> 16;
    *buf++ = u >> 8;
    *buf++ = u;
    return buf;
}

char* packsv(char* buf, char* str, int len) {
    while (len-- > 0) {
        *buf++ = *str++;
    }
    return buf;
}

uint8_t unpacku8(uint8_t** buf) {
    uint8_t res = **buf;
    (*buf)++;
    return res;
}

uint64_t unpacku64(uint8_t** buf) {
    uint8_t* b = *buf;
    uint64_t res = ((unsigned long long int)b[0] << 56) |
                   ((unsigned long long int)b[1] << 48) |
                   ((unsigned long long int)b[2] << 40) |
                   ((unsigned long long int)b[3] << 32) |
                   ((unsigned long long int)b[4] << 24) |
                   ((unsigned long long int)b[5] << 16) |
                   ((unsigned long long int)b[6] << 8) | b[7];
    (*buf) += sizeof(uint64_t);
    return res;
}

void unpacksv(uint8_t** buf, char* dest, uint8_t len) {
    for (int i = 0; i < len; i++) {
        dest[i] = **buf;
        (*buf)++;
    }
}

#ifdef WINDOWS_BUILD
#define send_data(fd, buf, len) send(fd, (char*)buf, len, 0)
#else
#define send_data(fd, buf, len) send(fd, buf, len, MSG_NOSIGNAL)
#endif

void write_packet(net_packet* p, int fd) {
    if (p->len > MAX_PACKET_SIZE) {
        LOGL(LL_ERROR, "Packet is too big (%d / %d)", p->len, MAX_PACKET_SIZE);
        return;
    }
    static char buf[MAX_PACKET_SIZE] = {0};
    char* b = buf;
    b = packu64(b, p->len);
    b = packu8(b, p->type);
    b = packstruct(b, p->content, p->type);

    b = buf;
    uint64_t packet_size_left = p->len;
    int n = 0;
    while ((n = send_data(fd, b, packet_size_left)) > 0) {
        packet_size_left -= n;
        b += n;
    }

    if (n < 0) {
        LOGL(LL_ERROR, "Error sending packet of type %d %s", p->type,
             strerror(errno));
    }
}

#ifdef WINDOWS_BUILD
#define recv_data(fd, buf, len) recv(fd, (char*)buf, len, 0)
#else
#define recv_data(fd, buf, len) read(fd, buf, len)
#endif

// TODO: We should check the size and avoid reading too much
int packet_read(net_packet* p, int fd) {
    uint8_t packet_len_buf[8] = {0};
    int n = recv_data(fd, &packet_len_buf, sizeof(packet_len_buf));
    if (n == 0) {
        LOG("Client %d disconnected (read)", fd);
        return -1;
    } else if (n < 0) {
        LOGL(LL_ERROR, "Error recieving packet header %s", strerror(errno));
        return -1;
    }
    uint8_t* len_buf = packet_len_buf;
    p->len = unpacku64(&len_buf);
    if (p->len > MAX_PACKET_SIZE) {
        return -1;
    }

    uint8_t buf[MAX_PACKET_SIZE] = {0};
    uint8_t* b = buf;
    int packet_size_left = p->len - sizeof(p->len);
    n = 0;
    while ((n = recv_data(fd, b, packet_size_left)) > 0) {
        packet_size_left -= n;
        b += n;
    }

    if (n < 0) {
        LOGL(LL_ERROR, "Error recieving packet of type %d %s", p->type,
             strerror(errno));
        return -1;
    }

    uint8_t* base = buf;
    p->type = unpacku8(&base);
    // TODO: Avoid allocation
    void* content = unpackstruct(p->type, base);
    if (content) {
        memcpy(p->content, content, p->len);
    }
    free(content);
    return 0;
}

void send_sock(net_packet* packet, int fd) {
    if (fd == 0) {
        LOGL(LL_ERROR, "Can't send packet when not connected");
        return;
    }
    packet->len += sizeof(packet->len) + sizeof(packet->type);
    write_packet(packet, fd);
}

#endif
