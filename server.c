#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "common.h"
#include "net.h"

typedef struct {
    uint8_t id;
    char name[9];
    uint8_t x, y;
    uint8_t health;

    game_state state;
    player_action action;
    uint8_t ax, ay;
} player_info;

int clients[MAX_PLAYER_COUNT] = {0};
player_info players[MAX_PLAYER_COUNT] = {0};
int player_count = 0;

int spawn_positions[][2] = {
    {0, 0},
    {MAP_WIDTH - 1, 0},
    {0, MAP_HEIGHT - 1},
    {MAP_WIDTH - 1, MAP_HEIGHT - 1},
};

int ci(int a) {
    if (a < 0) {
        exit(-a);
    }
    return a;
}

void broadcast(net_packet_type_enum type, void *content) {
    for (int i = 0; i < player_count; i++) {
        send_sock(type, content, clients[i]);
    }
}

void play_round(player_info *player) {
    if (player->action == PA_MOVE) {
        net_packet_player_update u = pkt_player_update(player->id, player->health, player->ax, player->ay);
        broadcast(PKT_PLAYER_UPDATE, &u);
        player->state = GS_PLAYING;
        player->x = player->ax;
        player->y = player->ay;
    } else if (player->action == PA_SPELL) {
        //TODO: Rework. We should check different spell types etc.
        for (int j = 0; j < player_count; j++) {
            player_info *pj = &players[j];
            // Should check spell zone etc instead of just where the player clicked
            if (pj->x == player->ax && pj->y == player->ay) {
                pj->health = fmax(pj->health - 25, 0);
                net_packet_player_update u = pkt_player_update(j, pj->health, pj->x, pj->y);
                broadcast(PKT_PLAYER_UPDATE, &u);
            }
        }
        player->state = GS_PLAYING;
    } else {
        fprintf(stderr, "Unsupported player action\n");
        exit(1);
    }
}

player_info *get_player(int fd) {
    for (int i = 0; i < player_count; i++) {
        if (clients[i] == fd) {
            return &players[i];
        }
    }
    printf("Unkown player\n");
    exit(1);
    return NULL;
}

fd_set master_set, read_fds;

void handle_message(int fd) {
    net_packet p = {0};
    if (packet_read(&p, fd) < 0) {
        FD_CLR(fd, &master_set);
        printf("Player %d left\n", fd);
        return;
    }

    if (p.type == PKT_JOIN) {
        net_packet_join *j = (net_packet_join *)p.content;
        printf("[%d] Joined %.*s\n", fd, 8, j->username);
        for (int i = 0; i < player_count; i++) {
            if (clients[i] == fd) {
                j->id = i;
                break;
            }
        }

        // We send previously connected players informations to the new player
        for (int i = 0; i < player_count - 1; i++) {
            net_packet_join other_user_join = pkt_join(i, players[i].name);
            send_sock(PKT_JOIN, &other_user_join, fd);

            net_packet_player_update u =
                pkt_player_update(players[i].id, players[i].health, players[i].x, players[i].y);
            send_sock(PKT_PLAYER_UPDATE, &u, fd);
        }

        for (int i = 0; i < player_count; i++) {
            send_sock(PKT_JOIN, j, clients[i]);
        }

        int last_player = player_count - 1;
        players[last_player].id = last_player;
        memcpy(players[last_player].name, j->username, 8);
        players[last_player].name[8] = '\0';
        players[last_player].health = 0;
        players[last_player].x = spawn_positions[last_player][0];
        players[last_player].y = spawn_positions[last_player][1];

        player_info *pi = &players[last_player];
        net_packet_player_update u = pkt_player_update(pi->id, pi->health, pi->x, pi->y);
        broadcast(PKT_PLAYER_UPDATE, &u);

        net_packet_connected c = pkt_connected(last_player);
        send_sock(PKT_CONNECTED, &c, fd);
    } else if (p.type == PKT_PLAYER_BUILD) {
        // TODO: We should only recieved this packet before the game has started
        net_packet_player_build *b = (net_packet_player_build *)p.content;
        player_info *player = get_player(fd);
        printf("Player %d has %d base health and is using following spells : ", player->id, b->base_health);
        for (int i = 0; i < MAX_SPELL_COUNT; i++) {
            printf("%d ", b->spells[i]);
        }
        printf("\n");
        player->health = b->base_health;
        // We send a PKT_PLAYER_UPDATE to set the base_health for all clients
        net_packet_player_update u = pkt_player_update(player->id, player->health, player->x, player->y);
        broadcast(PKT_PLAYER_UPDATE, &u);
    } else if (p.type == PKT_PLAYER_ACTION) {
        net_packet_player_action *a = (net_packet_player_action *)p.content;
        printf("Player %d played : %d at %d %d\n", a->id, a->action, a->x, a->y);

        players[a->id].action = a->action;
        players[a->id].ax = a->x;
        players[a->id].ay = a->y;
        players[a->id].state = GS_WAITING;

        int all_played = true;
        for (int i = 0; i < player_count; i++) {
            if (players[i].state == GS_PLAYING)
                all_played = false;
        }

        // TODO: Set a specific action order (ex: player move before they attack)
        if (all_played) {
            for (int i = 0; i < player_count; i++) {
                play_round(&players[i]);
            }
            // TODO: Send end round packet
        }
    }

    free(p.content);
}

int main() {
    int sockfd = ci(socket(AF_INET, SOCK_STREAM, 0));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(3000);
    memset(&addr.sin_zero, 0, 8);

    ci(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)));
    ci(listen(sockfd, 5));

    FD_ZERO(&master_set);
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &master_set);
    int fd_count = sockfd;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    struct sockaddr client = {0};
    socklen_t len = sizeof(client);

    while (1) {
        read_fds = master_set;
        int activity = ci(select(fd_count + 1, &read_fds, NULL, NULL, &timeout));
        if (activity < 0) {
            exit(1);
        }

        for (int i = 0; i <= fd_count; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == sockfd) {
                    // New connection
                    int connfd = ci(accept(sockfd, &client, &len));
                    FD_SET(connfd, &master_set);
                    if (connfd > fd_count) {
                        fd_count = connfd;
                    }
                    printf("new connection %d\n", connfd);
                    if (player_count == MAX_PLAYER_COUNT) {
                        printf("Full\n");
                        FD_CLR(connfd, &master_set);
                        close(connfd);
                    } else {
                        clients[player_count] = connfd;
                        player_count++;
                    }
                } else {
                    handle_message(i);
                }
            }
        }
    }

    printf("Client connected\n");

    close(sockfd);
}
