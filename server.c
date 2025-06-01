#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "common.h"

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
            net_packet_join other_user_join = {0};
            other_user_join.id = i;
            memcpy(other_user_join.username, players[i].name, 8);
            send_sock(PKT_JOIN, &other_user_join, fd);

            net_packet_player_update u = {0};
            u.id = players[i].id;
            u.health = players[i].health;
            u.x = players[i].x;
            u.y = players[i].y;
            send_sock(PKT_PLAYER_UPDATE, &u, fd);
        }

        for (int i = 0; i < player_count; i++) {
            send_sock(PKT_JOIN, j, clients[i]);
        }

        int last_player = player_count - 1;
        players[last_player].id = last_player;
        memcpy(players[last_player].name, j->username, 8);
        players[last_player].name[8] = '\0';
        players[last_player].health = 100;
        players[last_player].x = spawn_positions[last_player][0];
        players[last_player].y = spawn_positions[last_player][1];

        net_packet_player_update u = {0};
        u.id = players[last_player].id;
        u.health = players[last_player].health;
        u.x = players[last_player].x;
        u.y = players[last_player].y;

        for (int i = 0; i < player_count; i++) {
            send_sock(PKT_PLAYER_UPDATE, &u, clients[i]);
        }

        net_packet_connected c = {0};
        c.id = last_player;
        send_sock(PKT_CONNECTED, &c, fd);
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

        //TODO: Set a specific action order (ex: player move before they attack)
        if (all_played) {
            for (int i = 0; i < player_count; i++) {
                if (players[i].action == PA_MOVE) {
                    net_packet_player_update u = {0};
                    u.id = i;
                    u.health = players[i].health;
                    u.x = players[i].ax;
                    u.y = players[i].ay;

                    for (int j = 0; j < player_count; j++) {
                        send_sock(PKT_PLAYER_UPDATE, &u, clients[j]);
                    }
                    players[i].state = GS_PLAYING;
                    players[i].x = players[i].ax;
                    players[i].y = players[i].ay;
                } else if (players[i].action == PA_ATTACK) {
                    for (int j = 0; j < player_count; j++) {
                        if (players[j].x == players[i].ax && players[j].y == players[i].ay) {
                            players[j].health -= 25;
                            net_packet_player_update u = {0};
                            u.id = j;
                            u.health = players[j].health;
                            u.x = players[j].x;
                            u.y = players[j].y;
                            for (int k = 0; k < player_count; k++) {
                                send_sock(PKT_PLAYER_UPDATE, &u, clients[k]);
                            }
                        }
                    }
                    players[i].state = GS_PLAYING;
                } else {
                    fprintf(stderr, "Unsupported player action\n");
                    exit(1);
                }
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
