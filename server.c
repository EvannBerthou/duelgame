#include <asm-generic/socket.h>
#include <errno.h>
#include <limits.h>
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

fd_set master_set, read_fds;

// Admin stuff
const char ADMIN_PASSWORD[8] = {'p', 'a', 's', 's'};
#define MAX_ADMIN 4

int admins[MAX_ADMIN] = {0};
int admin_count = 0;

bool is_admin(int fd) {
    for (int i = 0; i < admin_count; i++) {
        if (admins[i] == fd)
            return true;
    }
    return false;
}

// Number of players required to have join the game in order to start
#define LOBBY_FULL_COUNT 2

game_state gs = GS_WAITING;

typedef struct {
    uint8_t id;
    char name[9];
    uint8_t x, y;
    uint8_t health;
    uint8_t max_health;

    round_state state;
    player_action action;
    uint8_t ax, ay;
    uint8_t spell;

    spell_effect effect;
    uint8_t effect_round_left;
} player_info;

int connections[MAX_PLAYER_COUNT + MAX_ADMIN] = {0};
int connection_count = 0;

int clients[MAX_PLAYER_COUNT] = {0};
player_info players[MAX_PLAYER_COUNT] = {0};
// Stores the order in which players will do their actions
int player_round_order[MAX_PLAYER_COUNT] = {0};
int player_count = 0;

int spawn_positions[][2] = {
    {0, 0},
    //{MAP_WIDTH - 1, 0},
    {1, 0},
    {0, MAP_HEIGHT - 1},
    {MAP_WIDTH - 1, MAP_HEIGHT - 1},
};

int ci(int a) {
    if (a < 0) {
        printf("ci\n");
        exit(-a);
    }
    return a;
}

net_packet_player_update pkt_from_info(player_info *p) {
    return pkt_player_update(p->id, p->health, p->max_health, p->x, p->y, p->effect, p->effect_round_left);
}

void broadcast(net_packet_type_enum type, void *content) {
    for (int i = 0; i < player_count; i++) {
        send_sock(type, content, clients[i]);
    }
}

void damage_player(player_info *p, const spell *s) {
    p->health = fmax(p->health - s->damage, 0);
    if (s->effect != SE_NONE) {
        p->effect = s->effect;
        p->effect_round_left = s->effect_duration;
    }
    net_packet_player_update u = pkt_from_info(p);
    broadcast(PKT_PLAYER_UPDATE, &u);

    int alive_count = player_count;
    for (int i = 0; i < player_count; i++) {
        if (players[i].health <= 0) {
            alive_count--;
        }
    }
    // One player won.
    if (alive_count == 1) {
        for (int i = 0; i < player_count; i++) {
            if (players[i].health > 0) {
                printf("Player %s won the game !\n", players[i].name);
                net_packet_game_end e = pkt_game_end(players[i].id);
                broadcast(PKT_GAME_END, &e);
                break;
            }
        }
    }
    // No winner
    else if (alive_count == 0) {
        printf("Nobody won the game...\n");
        net_packet_game_end e = pkt_game_end(255);
        broadcast(PKT_GAME_END, &e);
    }
}

void play_round(player_info *player) {
    if (player->action == PA_CANT_PLAY) {
        printf("Player %d can't play this round\n", player->id);
        net_packet_player_update u = pkt_from_info(player);
        broadcast(PKT_PLAYER_UPDATE, &u);
        net_packet_player_action action = pkt_player_action(player->id, player->action, player->x, player->y, 0);
        broadcast(PKT_PLAYER_ACTION, &action);
        player->state = RS_PLAYING;
    } else if (player->action == PA_SPELL) {
        if (player->effect == SE_STUN) {
            printf("Player can't do this action because he is stunned\n");
            net_packet_player_update u = pkt_from_info(player);
            broadcast(PKT_PLAYER_UPDATE, &u);
            net_packet_player_action action = pkt_player_action(player->id, PA_CANT_PLAY, player->x, player->y, 0);
            broadcast(PKT_PLAYER_ACTION, &action);
            player->state = RS_PLAYING;
            return;
        }

        // TODO: Check that the player really has this spell in his build
        printf("Player %d is using spell %s this round\n", player->id, all_spells[player->spell].name);
        const spell *s = &all_spells[player->spell];
        net_packet_player_action action =
            pkt_player_action(player->id, player->action, player->ax, player->ay, player->spell);
        broadcast(PKT_PLAYER_ACTION, &action);
        if (s->type == ST_MOVE) {
            for (int i = 0; i < player_count; i++) {
                // The player tries to move on the same cell as another player so we cancel it
                // TODO: We could add a negative effect (stun ?)
                if (players[i].x == player->ax && players[i].y == player->ay && players[i].id != player->id) {
                    player->state = RS_PLAYING;
                    return;
                }
            }
            player->x = player->ax;
            player->y = player->ay;
            net_packet_player_update u = pkt_from_info(player);
            broadcast(PKT_PLAYER_UPDATE, &u);
        } else if (s->type == ST_TARGET) {
            for (int i = 0; i < player_count; i++) {
                player_info *other = &players[i];
                if (other->x == player->ax && other->y == player->ay) {
                    damage_player(other, s);
                }
            }
        }
        player->state = RS_PLAYING;
    } else {
        fprintf(stderr, "Unsupported player action\n");
        exit(1);
    }
}

player_info *get_player_from_fd(int fd) {
    for (int i = 0; i < player_count; i++) {
        if (clients[i] == fd) {
            return &players[i];
        }
    }
    printf("Unkown player\n");
    exit(1);
    return NULL;
}

int compare_players_action(const void *a, const void *b) {
    int player1_id = *(int *)a;
    int player2_id = *(int *)b;

    player_info *p1 = &players[player1_id];
    player_info *p2 = &players[player2_id];

    const spell *s1 = &all_spells[p1->spell];
    const spell *s2 = &all_spells[p2->spell];

    if (s1->speed == s2->speed) {
        return rand() % 2 == 0 ? 1 : -1;  // If both spell have the same speed, its random (for now ?)
    }

    return all_spells[p2->spell].speed - all_spells[p1->spell].speed;
}

void sort_actions() {
    for (int i = 0; i < player_count; i++) {
        player_round_order[i] = i;
    }
    qsort(player_round_order, player_count, sizeof(int), &compare_players_action);
}

void start_game() {
    broadcast(PKT_GAME_START, NULL);
}

void handle_message(int fd) {
    net_packet p = {0};
    if (packet_read(&p, fd) < 0) {
        FD_CLR(fd, &master_set);
        printf("Player %d left\n", fd);
        // TODO: Handle player disconnect
        return;
    }

    if (p.type == PKT_JOIN) {
        clients[player_count] = fd;
        player_count++;

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

            net_packet_player_update u = pkt_from_info(&players[i]);
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
        net_packet_player_update u = pkt_from_info(pi);
        broadcast(PKT_PLAYER_UPDATE, &u);

        net_packet_connected c = pkt_connected(last_player);
        send_sock(PKT_CONNECTED, &c, fd);
    } else if (p.type == PKT_PLAYER_BUILD) {
        // TODO: We should only recieved this packet before the game has started
        net_packet_player_build *b = (net_packet_player_build *)p.content;
        player_info *player = get_player_from_fd(fd);
        // TODO: Store spells in player_info
        printf("Player %d has %d base health and is using following spells : ", player->id, b->base_health);
        for (int i = 0; i < MAX_SPELL_COUNT; i++) {
            printf("%d ", b->spells[i]);
        }
        printf("\n");
        player->max_health = b->base_health;
        player->health = player->max_health;
        // We send a PKT_PLAYER_UPDATE to set the base_health for all clients
        net_packet_player_update u = pkt_from_info(player);
        broadcast(PKT_PLAYER_UPDATE, &u);

        // TODO: We should only start the game when every player has sent his build
        if (player_count == LOBBY_FULL_COUNT) {
            start_game();
        }
    } else if (p.type == PKT_PLAYER_ACTION) {
        net_packet_player_action *a = (net_packet_player_action *)p.content;
        printf("Player %d played : %d at %d %d\n", a->id, a->action, a->x, a->y);

        players[a->id].action = a->action;
        players[a->id].ax = a->x;
        players[a->id].ay = a->y;
        players[a->id].state = RS_WAITING;
        players[a->id].spell = a->spell;

        int all_played = true;
        for (int i = 0; i < player_count; i++) {
            if (players[i].state == RS_PLAYING)
                all_played = false;
        }

        if (all_played) {
            sort_actions();
            for (int i = 0; i < player_count; i++) {
                play_round(&players[player_round_order[i]]);
            }
            for (int i = 0; i < player_count; i++) {
                if (players[i].effect_round_left > 0) {
                    players[i].effect_round_left--;
                    if (players[i].effect_round_left == 0) {
                        players[i].effect = SE_NONE;
                    }
                }
                net_packet_player_update u = pkt_from_info(&players[i]);
                broadcast(PKT_PLAYER_UPDATE, &u);
            }
            broadcast(PKT_ROUND_END, NULL);
        }
    } else if (p.type == PKT_GAME_RESET) {
        printf("Serv: game reset\n");
        gs = GS_WAITING;
        broadcast(PKT_GAME_RESET, NULL);
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            players[i] = (player_info){0};
        }
        player_count = 0;
    } else if (p.type == PKT_ADMIN_CONNECT) {
        net_packet_admin_connect *a = (net_packet_admin_connect *)p.content;
        printf("%d is trying to connect as admin with password '%s'\n", fd, a->password);
        net_packet_admin_connect_result result = pkt_admin_connect_result(memcmp(a->password, ADMIN_PASSWORD, 8) == 0);
        send_sock(PKT_ADMIN_CONNECT_RESULT, &result, fd);
        admins[admin_count] = fd;
        admin_count++;
    } else if (p.type == PKT_ADMIN_UPDATE_PLAYER_INFO) {
        if (is_admin(fd)) {
            net_packet_admin_update_player_info *info = (net_packet_admin_update_player_info *)p.content;
            if (info->property == PIP_HEALTH) {
                players[info->id].health = info->value;
                net_packet_player_update u = pkt_from_info(&players[info->id]);
                broadcast(PKT_PLAYER_UPDATE, &u);
            }
        }
    }

    free(p.content);
}

int main(int argc, char **argv) {
    int port = 3000;
    POPARG(argc, argv);  // program name
    while (argc > 0) {
        const char *arg = POPARG(argc, argv);
        if (strcmp(arg, "--port") == 0) {
            const char *value = POPARG(argc, argv);
            if (!strtoint(value, &port)) {
                printf("Error parsing port to int '%s'\n", value);
                exit(1);
            }
        } else {
            printf("Unknown arg : '%s'\n", arg);
        }
    }

    int sockfd = ci(socket(AF_INET, SOCK_STREAM, 0));
    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    memset(&addr.sin_zero, 0, 8);

    ci(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)));
    ci(listen(sockfd, 5));

    printf("Listening on port %d\n", port);

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
                        connections[connection_count] = connfd;
                        connection_count++;
                    }
                } else {
                    handle_message(i);
                }
            }
        }
        usleep(16000);  // Sleep to avoid 100% CPU usage while I implement a better solution
    }

    printf("Client connected\n");

    close(sockfd);
}
