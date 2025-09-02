#include <asm-generic/socket.h>
#include <limits.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "common.h"
#include "net.h"
#include "net_protocol.h"

#define MAP_WIDTH 16
#define MAP_HEIGHT 8

#define FOREACH_PLAYER(IT, P)                                                    \
    for (int IT = 0; IT < MAX_PLAYER_COUNT; IT++)                                \
        for (player_info *P = &players[IT]; P != NULL && P->connected; P = NULL) \
            if (1)

extern const spell all_spells[];

fd_set master_set, read_fds;

// Admin stuff
const char ADMIN_PASSWORD[8] = {'p', 'a', 's', 's'};
#define MAX_ADMIN 4

int admins[MAX_ADMIN] = {0};
int admin_count = 0;

game_state gs = GS_WAITING;
int connections[MAX_PLAYER_COUNT + MAX_ADMIN] = {0};
int connection_count = 0;
int master_player = 0;

int clients[MAX_PLAYER_COUNT] = {0};
player_info players[MAX_PLAYER_COUNT] = {0};
bool player_ready[MAX_PLAYER_COUNT] = {0};
// Stores the order in which players will do their actions
int player_round_order[MAX_PLAYER_COUNT] = {0};
// int player_count = 0;
uint8_t round_scores[MAX_PLAYER_COUNT] = {0};
time_t round_start_time = 0;

bool is_admin(int fd) {
    // Master player does not have to login
    if (fd == clients[master_player]) {
        return true;
    }
    for (int i = 0; i < admin_count; i++) {
        if (admins[i] == fd)
            return true;
    }
    return false;
}

const int MAP[MAP_HEIGHT][MAP_WIDTH] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0},
    {0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0},
    {0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

const int PROPS[MAP_HEIGHT][MAP_WIDTH] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

int spawn_positions[][2] = {
    {0, 0},
    {1, 0},
    {2, 0},
    {3, 0},
};

int ci(int a) {
    if (a < 0) {
        LOG("ci\n");
        exit(-a);
    }
    return a;
}

net_packet_player_update pkt_from_info(player_info *p) {
    uint8_t health = p->health >= 0 ? p->health : 0;
    return pkt_player_update(p->id, health, p->max_health, p->ad, p->ap, p->x, p->y, p->effect, p->effect_round_left,
                             false);
}

void broadcast(net_packet_type_enum type, void *content) {
    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].connected) {
            send_sock(type, content, clients[i]);
        }
    }
}

void heal_player(player_info *p, const spell *s) {
    LOG("Healing player %s\n", p->name);
    p->health = fmin(fmax(p->health + s->damage, 0), p->max_health);
    net_packet_player_update u = pkt_from_info(p);
    broadcast(PKT_PLAYER_UPDATE, &u);
}

void damage_player(player_info *p, const spell *s) {
    p->health = fmin(fmax(p->health - s->damage, 0), p->max_health);
    if (s->effect != SE_NONE) {
        p->effect = s->effect;
        p->effect_round_left = s->effect_duration;
        p->spell_effect = s;
    }

    net_packet_player_update u = pkt_from_info(p);
    broadcast(PKT_PLAYER_UPDATE, &u);
}

void reset_player(player_info *player) {
    player->max_health = player->base_health;
    player->health = player->base_health;
    player->x = spawn_positions[player->id][0];
    player->y = spawn_positions[player->id][1];
    player->effect = SE_NONE;
    player->effect_round_left = 0;
    player->spell_effect = NULL;
    net_packet_player_update u = pkt_from_info(player);
    u.immediate = true;
    broadcast(PKT_PLAYER_UPDATE, &u);
}

void play_round(player_info *player) {
    if (player->action == PA_CANT_PLAY) {
        LOG("Player %d can't play this round\n", player->id);
        net_packet_player_update u = pkt_from_info(player);
        broadcast(PKT_PLAYER_UPDATE, &u);
        net_packet_player_action action = pkt_player_action(player->id, player->action, player->x, player->y, 0);
        broadcast(PKT_PLAYER_ACTION, &action);
        player->state = RS_PLAYING;
    } else if (player->action == PA_SPELL) {
        if (player->effect == SE_STUN) {
            LOG("Player can't do this action because he is stunned\n");
            net_packet_player_update u = pkt_from_info(player);
            broadcast(PKT_PLAYER_UPDATE, &u);
            net_packet_player_action action = pkt_player_action(player->id, PA_CANT_PLAY, player->x, player->y, 0);
            broadcast(PKT_PLAYER_ACTION, &action);
            player->state = RS_PLAYING;
            return;
        }

        LOG("Player %d is using spell %s this round\n", player->id, all_spells[player->spell].name);
        int found = false;
        for (int i = 0; i < MAX_SPELL_COUNT; i++) {
            if (player->spell == player->spells[i]) {
                found = true;
            }
        }
        // TODO: Report an error / kick the player ?
        if (found == false) {
            LOG("Player is trying to cast a spell which is not in his build\n");
            exit(0);
        }

        const spell *s = &all_spells[player->spell];
        net_packet_player_action action =
            pkt_player_action(player->id, player->action, player->ax, player->ay, player->spell);
        broadcast(PKT_PLAYER_ACTION, &action);
        if (s->type == ST_MOVE) {
            FOREACH_PLAYER(i, other) {
                // The player tries to move on the same cell as another player so we cancel it
                if (other->x == player->ax && other->y == player->ay && other->id != player->id) {
                    player->state = RS_PLAYING;
                    return;
                }
            }
            player->x = player->ax;
            player->y = player->ay;
            net_packet_player_update u = pkt_from_info(player);
            broadcast(PKT_PLAYER_UPDATE, &u);
        } else if (s->type == ST_TARGET) {
            FOREACH_PLAYER(i, other) {
                if (other->x == player->ax && other->y == player->ay) {
                    damage_player(other, s);
                }
            }
        } else if (s->type == ST_STAT) {
            if (s->effect == SE_HEAL) {
                heal_player(player, s);
            }
        } else {
            LOGL(LL_ERROR, "Unknown spell type %d from %s\n", s->type, s->name);
        }
        player->state = RS_PLAYING;
    } else {
        fprintf(stderr, "Unsupported player action\n");
        exit(1);
    }
}

player_info *get_player_from_fd(int fd) {
    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (clients[i] == fd) {
            return &players[i];
        }
    }
    LOG("Unkown player\n");
    exit(1);
    return NULL;
}

int player_count() {
    int player_count = 0;
    FOREACH_PLAYER(i, player) {
        player_count++;
    }
    return player_count;
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
    for (int i = 0; i < player_count(); i++) {
        player_round_order[i] = i;
    }
    qsort(player_round_order, player_count(), sizeof(int), &compare_players_action);
}

void start_game() {
    if (gs != GS_WAITING) {
        return;
    }

    // TODO: Check that every player is really ready (build is set, etc.)
    FOREACH_PLAYER(i, player) {
        reset_player(player);
    }
    broadcast(PKT_GAME_START, NULL);
    round_start_time = time(NULL);
}

void send_map(int fd) {
    uint8_t map[MAP_WIDTH * MAP_HEIGHT] = {0};
    for (int i = 0; i < MAP_HEIGHT; i++) {
        for (int j = 0; j < MAP_WIDTH; j++) {
            map[i * MAP_WIDTH + j] = MAP[i][j];
        }
    }
    net_packet_map m = pkt_map(MAP_WIDTH, MAP_HEIGHT, MLT_BACKGROUND, map);
    send_sock(PKT_MAP, &m, fd);

    uint8_t props[MAP_WIDTH * MAP_HEIGHT] = {0};
    for (int i = 0; i < MAP_HEIGHT; i++) {
        for (int j = 0; j < MAP_WIDTH; j++) {
            props[i * MAP_WIDTH + j] = PROPS[i][j];
        }
    }
    net_packet_map p = pkt_map(MAP_WIDTH, MAP_HEIGHT, MLT_PROPS, props);
    send_sock(PKT_MAP, &p, fd);
}

void handle_player_disconnect(int fd) {
    FD_CLR(fd, &master_set);
    LOG("Player %d left\n", fd);
    player_info *player = get_player_from_fd(fd);
    player->connected = false;
    int new_master = -1;
    FOREACH_PLAYER(i, player) {
        if (player->connected && new_master == -1) {
            new_master = player->id;
        }
    }
    net_packet_disconnect p = pkt_disconnect(player->id, new_master);
    broadcast(PKT_DISCONNECT, &p);
    master_player = new_master;
    gs = GS_WAITING;
}

void handle_message(int fd) {
    net_packet p = {0};
    if (packet_read(&p, fd) < 0) {
        handle_player_disconnect(fd);
        return;
    }

    if (p.type == PKT_PING) {
        send_sock(PKT_PING, p.content, fd);
    } else if (p.type == PKT_JOIN) {
        int new_player = -1;
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            if (players[i].connected == false) {
                players[i].connected = true;
                clients[i] = fd;
                new_player = i;
                break;
            }
        }
        assert(new_player != -1);

        net_packet_join *j = (net_packet_join *)p.content;
        LOG("[%d] Joined %.*s\n", fd, 8, j->username);
        j->id = new_player;

        // We send previously connected players informations to the new player
        FOREACH_PLAYER(i, player) {
            if (clients[i] != fd) {
                player_info *player = &players[i];
                net_packet_join other_user_join = pkt_join(i, player->name);
                send_sock(PKT_JOIN, &other_user_join, fd);

                net_packet_player_build b =
                    pkt_player_build(player->id, player->base_health, player->spells, player->ad, player->ap);
                send_sock(PKT_PLAYER_BUILD, &b, fd);

                net_packet_player_update u = pkt_from_info(player);
                send_sock(PKT_PLAYER_UPDATE, &u, fd);
            }
        }

        broadcast(PKT_JOIN, j);

        players[new_player].id = new_player;
        memcpy(players[new_player].name, j->username, 8);
        players[new_player].name[8] = '\0';

        player_info *pi = &players[new_player];

        net_packet_player_update u = pkt_from_info(pi);
        broadcast(PKT_PLAYER_UPDATE, &u);

        net_packet_connected c = pkt_connected(new_player, master_player);
        send_sock(PKT_CONNECTED, &c, fd);

        send_map(fd);
    } else if (p.type == PKT_GAME_START) {
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            round_scores[i] = 0;
        }
        round_start_time = time(NULL);
        start_game();
    } else if (p.type == PKT_PLAYER_BUILD) {
        // TODO: Handle error
        if (gs == GS_STARTED) {
            LOG("Recieved PKT_PLAYER_BUILD but game has already started\n");
            exit(0);
        }
        net_packet_player_build *b = (net_packet_player_build *)p.content;
        player_info *player = get_player_from_fd(fd);
        // We force the id to avoid a player setting the build of another player
        b->id = player->id;
        broadcast(PKT_PLAYER_BUILD, b);

        for (int i = 0; i < MAX_SPELL_COUNT; i++) {
            player->spells[i] = b->spells[i];
        }
        player->base_health = b->base_health;
        player->max_health = b->base_health;
        player->health = player->max_health;
        player->ad = b->ad;
        player->ap = b->ap;
        // We send a PKT_PLAYER_UPDATE to set the base_health for all clients
        net_packet_player_update u = pkt_from_info(player);
        broadcast(PKT_PLAYER_UPDATE, &u);
    } else if (p.type == PKT_PLAYER_ACTION) {
        net_packet_player_action *a = (net_packet_player_action *)p.content;
        LOG("Player %d played : %d at %d %d\n", a->id, a->action, a->x, a->y);

        players[a->id].action = a->action;
        players[a->id].ax = a->x;
        players[a->id].ay = a->y;
        players[a->id].state = RS_WAITING;
        players[a->id].spell = a->spell;

        int all_played = true;
        FOREACH_PLAYER(i, player) {
            if (player->state == RS_PLAYING) {
                all_played = false;
            }
        }

        if (all_played) {
            sort_actions();
            FOREACH_PLAYER(i, player) {
                play_round(&players[player_round_order[i]]);
            }
            FOREACH_PLAYER(i, player) {
                if (players->effect == SE_BURN) {
                    players->health -= player->spell_effect->damage;
                }
                if (player->effect_round_left > 0) {
                    player->effect_round_left--;
                    if (player->effect_round_left == 0) {
                        player->effect = SE_NONE;
                    }
                }
                net_packet_player_update u = pkt_from_info(&players[i]);
                broadcast(PKT_PLAYER_UPDATE, &u);
            }

            int alive_count = 0;
            FOREACH_PLAYER(i, player) {
                if (player->health > 0) {
                    alive_count++;
                }
            }

            if (alive_count < 2) {
                uint8_t end_verdict = GAME_TIE;
                if (alive_count == 1) {
                    FOREACH_PLAYER(i, player) {
                        if (player->health > 0) {
                            LOG("Player %s won the round !\n", players[i].name);
                            end_verdict = i;
                            round_scores[i]++;
                        }
                    }
                } else if (alive_count == 0) {
                    LOG("Nobody won the game...\n");
                    end_verdict = GAME_TIE;
                }

                net_packet_round_end end = pkt_round_end(end_verdict, round_scores);
                broadcast(PKT_ROUND_END, &end);

                FOREACH_PLAYER(i, player) {
                    if (round_scores[i] == 3) {
                        net_packet_game_end game_end = pkt_game_end(player->id, round_scores);
                        broadcast(PKT_GAME_END, &game_end);
                    }
                }

                gs = GS_WAITING;
            } else {
                broadcast(PKT_TURN_END, NULL);
            }
        }
    } else if (p.type == PKT_PLAYER_READY) {
        player_ready[get_player_from_fd(fd)->id] = true;
        int ready_count = 0;
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            ready_count += player_ready[i];
        }
        if (ready_count == player_count()) {
            FOREACH_PLAYER(i, player) {
                reset_player(player);
                net_packet_player_build b =
                    pkt_player_build(player->id, player->base_health, player->spells, player->ad, player->ap);
                broadcast(PKT_PLAYER_BUILD, &b);

                net_packet_player_update u = pkt_from_info(player);
                broadcast(PKT_PLAYER_UPDATE, &u);
            }
            broadcast(PKT_ROUND_START, NULL);
        }
    } else if (p.type == PKT_GAME_RESET) {
        // Only first player (owner) can reset the game
        if (fd != connections[0]) {
            player_info *player = get_player_from_fd(fd);
            LOG("%s tried to reset the game but they are not the owner.\n", player->name);
            return;
        }
        LOG("Serv: game reset\n");
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            round_scores[i] = 0;
        }
        round_start_time = time(NULL);
        gs = GS_WAITING;
        broadcast(PKT_GAME_RESET, NULL);
        // We send previously connected players informations to the new player
        FOREACH_PLAYER(i, player) {
            reset_player(player);
            net_packet_join other_user_join = pkt_join(i, player->name);
            broadcast(PKT_JOIN, &other_user_join);

            net_packet_player_build b =
                pkt_player_build(player->id, player->base_health, player->spells, player->ad, player->ap);
            broadcast(PKT_PLAYER_BUILD, &b);

            net_packet_player_update u = pkt_from_info(player);
            u.immediate = true;
            broadcast(PKT_PLAYER_UPDATE, &u);

            send_map(connections[i]);
        }
        broadcast(PKT_GAME_START, NULL);
    } else if (p.type == PKT_ADMIN_CONNECT) {
        net_packet_admin_connect *a = (net_packet_admin_connect *)p.content;
        LOG("%d is trying to connect as admin with password '%s'\n", fd, a->password);
        net_packet_admin_connect_result result = pkt_admin_connect_result(memcmp(a->password, ADMIN_PASSWORD, 8) == 0);
        send_sock(PKT_ADMIN_CONNECT_RESULT, &result, fd);
        admins[admin_count] = fd;
        admin_count++;
    } else if (p.type == PKT_ADMIN_UPDATE_PLAYER_INFO) {
        if (is_admin(fd)) {
            net_packet_admin_update_player_info *info = (net_packet_admin_update_player_info *)p.content;
            if (info->property == PIP_HEALTH) {
                players[info->id].health = info->value;
                if (players[info->id].health > players[info->id].max_health) {
                    players[info->id].max_health = info->value;
                }
                net_packet_player_update u = pkt_from_info(&players[info->id]);
                u.immediate = true;
                broadcast(PKT_PLAYER_UPDATE, &u);
            }
        } else {
            char msg[128] = {0};
            strncpy(msg, "You are not allowed to execute this command", 128);
            net_packet_server_message response = pkt_server_message(LL_ERROR, msg);
            send_sock(PKT_SERVER_MESSAGE, &response, fd);
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
                LOG("Error parsing port to int '%s'\n", value);
                exit(1);
            }
        } else {
            LOG("Unknown arg : '%s'\n", arg);
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

    LOG("Listening on port %d\n", port);

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
            LOG("here?\n");
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
                    LOG("new connection %d\n", connfd);
                    // TODO: Check if there is any space left
                    if (false) {  // player_count == MAX_PLAYER_COUNT) {
                        // TODO: Tell the player the game is full
                        LOG("Full\n");
                        FD_CLR(connfd, &master_set);
                        close(connfd);
                    } else {
                        connections[connection_count] = connfd;
                        connection_count++;
                    }
                } else {
                    handle_message(i);
                    time_t round_timer = time(NULL) - round_start_time;
                    net_packet_game_stats stats = pkt_game_stats(round_timer);
                    broadcast(PKT_GAME_STATS, &stats);
                }
            }
        }
        usleep(16000);  // TODO: Sleep to avoid 100% CPU usage while I implement a better solution
    }

    LOG("Client connected\n");

    close(sockfd);
}
