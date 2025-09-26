#include <dirent.h>
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

void handle_player_disconnect(int fd);

#define FOREACH_PLAYER(P)                                                              \
    for (int iterator = 0; iterator < MAX_PLAYER_COUNT; iterator++)                    \
        for (player_info *P = &players[iterator]; P != NULL && P->connected; P = NULL) \
            if (1)

extern const spell all_spells[];

// Server management
fd_set master_set, read_fds;
int connections[MAX_PLAYER_COUNT] = {0};
int connection_count = 0;
const char *server_password = "";

// Players
int clients[MAX_PLAYER_COUNT] = {0};
player_info players[MAX_PLAYER_COUNT] = {0};
bool player_ready[MAX_PLAYER_COUNT] = {0};
int master_player = 0;

// Game logic
game_state gs = GS_WAITING;
// Stores the order in which players will do their actions
int player_round_order[MAX_PLAYER_COUNT] = {0};
uint8_t round_scores[MAX_PLAYER_COUNT] = {0};
time_t round_start_time = 0;
uint8_t max_round_count = 3;

// Admin stuff
const char ADMIN_PASSWORD[8] = {'p', 'a', 's', 's'};

// Utils

int ci(int a) {
    if (a < 0) {
        LOG("ci");
        exit(-a);
    }
    return a;
}

void broadcast_packet(net_packet *packet) {
    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].connected) {
            send_sock(packet, clients[i]);
        }
    }
}

#define broadcast(PACKET_FUNC)                  \
    do {                                        \
        net_packet p##__LINE__ = (PACKET_FUNC); \
        broadcast_packet(&p##__LINE__);         \
    } while (0)

int player_count() {
    int player_count = 0;
    FOREACH_PLAYER(player) {
        player_count++;
    }
    return player_count;
}

// Map
map_data current_map = {0};
const char *all_maps[256] = {0};
uint8_t *map_names_network = NULL;
int map_count = 0;
int selected_map_idx = 0;

void send_map(int fd) {
    uint8_t map[MAP_WIDTH * MAP_HEIGHT] = {0};
    uint8_t props[MAP_WIDTH * MAP_HEIGHT] = {0};
    for (int i = 0; i < MAP_HEIGHT * MAP_WIDTH; i++) {
        map[i] = current_map.map[i];
        props[i] = current_map.props[i];
    }

    send_packet(pkt_map(MAP_WIDTH, MAP_HEIGHT, MLT_BACKGROUND, map), fd);
    send_packet(pkt_map(MAP_WIDTH, MAP_HEIGHT, MLT_PROPS, props), fd);
}

// Player

net_packet pkt_from_info(player_info *p) {
    return pkt_player_update(p->id, p->stats, p->x, p->y, p->effect, p->effect_round_left, p->banned, false);
}

void update_stats(player_info *p, const spell *s) {
    if (s->stat_max) {
        p->stats[s->stat].max = fmin(p->stats[s->stat].max + s->stat_value, 200);
        p->stats[s->stat].value = fmin(p->stats[s->stat].value + s->stat_value, 200);
    } else {
        p->stats[s->stat].value = fmin(fmax(p->stats[s->stat].value + s->stat_value, 0), p->stats[s->stat].max);
    }
    broadcast(pkt_from_info(p));
}

void damage_player(player_info *from, player_info *to, const spell *s) {
    int damage = get_spell_damage(from, s);
    to->stats[STAT_HEALTH].value = fmin(fmax(to->stats[STAT_HEALTH].value - damage, 0), to->stats[STAT_HEALTH].max);
    if (s->effect != SE_NONE) {
        apply_effect(to, s);
    }
}

void reset_player(player_info *player) {
    for (int i = 0; i < STAT_COUNT; i++) {
        player->stats[i].max = player->stats[i].base;
        player->stats[i].value = player->stats[i].base;
    }
    player->x = current_map.spawn_positions[player->id][0];
    player->y = current_map.spawn_positions[player->id][1];
    for (int i = 0; i < SE_COUNT; i++) {
        player->effect[i] = false;
        player->effect_round_left[i] = 0;
        player->spell_effect[i] = NULL;
    }
    player->turn_effect = SE_NONE;
    player->turn_effect_duration_left = 0;
    player->last_spell = NO_SPELL;
    player->spell = NO_SPELL;
    for (int i = 0; i < MAX_SPELL_COUNT; i++) {
        player->banned[i] = false;
    }

    net_packet u = pkt_from_info(player);
    ((net_packet_player_update *)u.content)->immediate = true;
    broadcast_packet(&u);
}

player_info *get_player_from_fd(int fd) {
    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (clients[i] == fd) {
            return &players[i];
        }
    }
    LOG("Unknown player with fd=%d", fd);
    exit(1);
}

bool is_admin(int fd) {
    // Master player does not have to login
    if (fd == clients[master_player]) {
        return true;
    }
    return get_player_from_fd(fd)->admin;
}

player_info *player_on_cell(int x, int y) {
    FOREACH_PLAYER(player) {
        if (player->x == x && player->y == y) {
            return player;
        }
    }
    return NULL;
}

// Game logic

void play_turn(player_info *player) {
    if (player->action == PA_STUNNED) {
        LOG("Player %d can't play this round", player->id);
        broadcast(pkt_from_info(player));
        broadcast(pkt_player_action(player->id, player->action, player->x, player->y, 0));
        player->state = RS_PLAYING;
    } else if (player->action == PA_SPELL) {
        if (player->effect[SE_STUN]) {
            LOG("Player can't do this action because he is stunned");
            broadcast(pkt_from_info(player));
            broadcast(pkt_player_action(player->id, PA_STUNNED, player->x, player->y, 0));
            player->state = RS_PLAYING;
            return;
        }

        LOG("Player %d is using spell %s this round", player->id, all_spells[player->spell].name);
        int build_spell_index = -1;
        for (int i = 0; i < MAX_SPELL_COUNT; i++) {
            if (player->spell == player->spells[i]) {
                build_spell_index = i;
            }
        }
        if (build_spell_index == -1) {
            LOG("Player is trying to cast a spell which is not in his build");
            handle_player_disconnect(clients[player->id]);
            return;
        }

        const spell *s = &all_spells[player->spell];
        net_packet action_packet = pkt_player_action(player->id, player->action, player->ax, player->ay, player->spell);
        net_packet_player_action *action = (net_packet_player_action *)action_packet.content;

        if (player->banned[action->spell]) {
            LOG("Player %s can't cast %s spell because it is banned.", player->name, s->name);
            broadcast(pkt_from_info(player));
            broadcast(pkt_player_action(player->id, PA_STUNNED, player->x, player->y, 0));
            player->state = RS_PLAYING;
            return;
        }

        broadcast_packet(&action_packet);

        player->last_spell = player->spell;

        if (s->type == ST_MOVE) {
            if (s->effect == SE_DODGE) {
                player->turn_effect = SE_DODGE;
                player->turn_effect_duration_left = 0;
                LOG("Player %d is ready to dodge", player->id);
            } else {
                if (player_on_cell(player->ax, player->ay)) {
                    player->state = RS_PLAYING;
                    return;
                }
                player->x = player->ax;
                player->y = player->ay;
            }
        } else if (s->type == ST_TARGET) {
            if (s->effect == SE_BANISH) {
                LOG("Banning %d for %s", build_spell_index, player->name);
                player->banned[build_spell_index] = true;
            }
            player_info *other = player_on_cell(player->ax, player->ay);
            if (other == NULL) {
                player->state = RS_PLAYING;
                return;
            }

            if (other->turn_effect == SE_DODGE) {
                if (player_on_cell(other->ax, other->ay)) {
                    player->state = RS_PLAYING;
                    return;
                }
                LOG("Player %d dodged!", player->id);
                other->x = other->ax;
                other->y = other->ay;
            } else if (other->turn_effect == SE_BLOCK) {
                LOG("Player %s blocked the attack and %s is stunned", other->name, player->name);
                player->effect[SE_STUN] = true;
                player->effect_round_left[SE_STUN] = 2;
                player->spell_effect[SE_STUN] = &all_spells[6];  // TODO: Should not be hardcoded
            } else {
                if (s->cast_type == CT_CAST || s->cast_type == CT_CAST_EFFECT) {
                    damage_player(player, other, s);
                    if (s->stat_value != 0) {
                        update_stats(other, s);
                    }
                } else if (s->cast_type == CT_EFFECT) {
                    apply_effect(other, s);
                }
            }
        } else {
            LOGL(LL_ERROR, "Unknown spell type %d from %s", s->type, s->name);
        }
        player->state = RS_PLAYING;
    } else {
        fprintf(stderr, "Unsupported player action\n");
        exit(1);
    }
}

int compare_players_action(const void *a, const void *b) {
    int player1_id = *(int *)a;
    int player2_id = *(int *)b;

    player_info *p1 = &players[player1_id];
    player_info *p2 = &players[player2_id];

    const spell *s1 = &all_spells[p1->spell];
    const spell *s2 = &all_spells[p2->spell];

    int s1_final_speed = p1->stats[STAT_SPEED].value + s1->speed;
    int s2_final_speed = p2->stats[STAT_SPEED].value + s2->speed;

    if (s1_final_speed == s2_final_speed) {
        return rand() % 2 == 0 ? 1 : -1;  // If both spell have the same speed, its random (for now ?)
    }

    return s2_final_speed - s1_final_speed;
}

void sort_actions() {
    for (int i = 0; i < player_count(); i++) {
        player_round_order[i] = i;
    }
    qsort(player_round_order, player_count(), sizeof(int), &compare_players_action);
}

void execute_turn() {
    sort_actions();

    // Action execution
    FOREACH_PLAYER(player) {
        play_turn(&players[player_round_order[player->id]]);
    }

    // Effect tick
    FOREACH_PLAYER(player) {
        for (int i = 0; i < SE_COUNT; i++) {
            if (player->effect[i] && (player->spell_effect[i]->cast_type == CT_EFFECT ||
                                      player->spell_effect[i]->cast_type == CT_CAST_EFFECT)) {
                int damage = get_spell_damage(player, player->spell_effect[i]);
                player->stats[STAT_HEALTH].value = fmax(player->stats[STAT_HEALTH].value - damage, 0);
            }

            if (player->effect_round_left[i] > 0) {
                player->effect_round_left[i]--;
                if (player->effect_round_left[i] == 0) {
                    // Slow effect is not permanant
                    if (i == SE_SLOW) {
                        player->stats[STAT_SPEED].value = player->stats[STAT_SPEED].max;
                    }
                    player->effect[i] = false;
                }
            }
        }
        broadcast(pkt_from_info(player));
    }

    FOREACH_PLAYER(player) {
        if (player->turn_effect_duration_left == 0) {
            player->turn_effect = SE_NONE;
        } else {
            player->turn_effect_duration_left--;
        }
    }

    int alive_count = 0;
    FOREACH_PLAYER(player) {
        alive_count += player->stats[STAT_HEALTH].value > 0;
    }

    if (alive_count >= 2) {
        broadcast(pkt_turn_end());
        return;
    }

    uint8_t end_verdict = GAME_TIE;
    if (alive_count == 1) {
        FOREACH_PLAYER(player) {
            if (player->stats[STAT_HEALTH].value > 0) {
                LOG("Player %s won the round !", players[player->id].name);
                end_verdict = player->id;
                round_scores[player->id]++;
            }
        }
    } else if (alive_count == 0) {
        LOG("Nobody won the game...");
        end_verdict = GAME_TIE;
    }

    broadcast(pkt_round_end(end_verdict, round_scores));

    FOREACH_PLAYER(player) {
        if (round_scores[player->id] == max_round_count) {
            broadcast(pkt_game_end(player->id, round_scores));
        }
    }

    gs = GS_WAITING;
}

void start_game() {
    if (gs != GS_WAITING) {
        return;
    }

    // TODO: Check that every player is really ready (build is set, etc.)
    FOREACH_PLAYER(player) {
        reset_player(player);
        send_map(clients[player->id]);
    }
    broadcast(pkt_game_start());
    round_start_time = time(NULL);
}

// Network

void handle_player_disconnect(int fd) {
    FD_CLR(fd, &master_set);
    LOG("Player %d left", fd);
    player_info *player = get_player_from_fd(fd);
    player->connected = false;
    int new_master = -1;
    FOREACH_PLAYER(player) {
        if (player->connected && new_master == -1) {
            new_master = player->id;
        }
    }
    // Everyone left the game
    if (new_master == -1) {
        new_master = 0;
    }
    broadcast(pkt_disconnect(player->id, new_master));
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
        send_sock(&p, fd);
    } else if (p.type == PKT_JOIN) {
        net_packet_join *j = (net_packet_join *)p.content;

        if (server_password != NULL && strncmp(server_password, j->password, 8) != 0) {
            LOG("Wrong password for %.8s => '%.8s'", j->username, j->password);
            char msg[128] = {0};
            strncpy(msg, "Wrong password", 128);
            net_packet msg_p = pkt_server_message(LL_ERROR, msg);
            send_sock(&msg_p, fd);
            FD_CLR(fd, &master_set);
            close(fd);
            return;
        }

        int new_player_id = -1;
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            if (players[i].connected == false) {
                players[i].connected = true;
                clients[i] = fd;
                new_player_id = i;
                break;
            }
        }
        assert(new_player_id != -1);

        memcpy(players[new_player_id].name, j->username, 8);
        players[new_player_id].name[8] = '\0';
        LOG("New player %s joined with ID=%d", players[new_player_id].name, new_player_id);

        // We refresh player list for everyone
        FOREACH_PLAYER(player) {
            broadcast(pkt_player_joined(player->id, player->name));
            broadcast(pkt_player_build(player->id, player->stats[STAT_HEALTH].base, player->spells,
                                       player->stats[STAT_STRENGTH].value, player->stats[STAT_SPEED].value));
            broadcast(pkt_from_info(player));
        }

        player_info *pi = &players[new_player_id];
        broadcast(pkt_from_info(pi));
        send_packet(pkt_connected(new_player_id, master_player), fd);
        send_packet(pkt_server_map_list(map_count, map_names_network), fd);
        broadcast(pkt_update_server_configuration(selected_map_idx, max_round_count));
    } else if (p.type == PKT_UPDATE_SERVER_CONFIGURATION) {
        net_packet_update_server_configuration *config = (net_packet_update_server_configuration *)p.content;
        if (config->map_index >= map_count) {
            return;
        }
        if (config->round_count < 3 || config->round_count > 15) {
            return;
        }
        selected_map_idx = config->map_index;
        max_round_count = config->round_count;
        broadcast_packet(&p);
    } else if (p.type == PKT_REQUEST_GAME_START) {
        net_packet_request_game_start *s = (net_packet_request_game_start *)p.content;
        if (s->map_id >= map_count) {
            return;
        }
        if (s->round_count < 3 || s->round_count > 15) {
            return;
        }

        const char *map = all_maps[s->map_id];
        if (load_map(map, &current_map) == false) {
            LOGL(LL_ERROR, "Error loading map '%s'", map);
            return;
        }
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            round_scores[i] = 0;
        }
        round_start_time = time(NULL);
        max_round_count = s->round_count;
        start_game();
    } else if (p.type == PKT_PLAYER_BUILD) {
        // TODO: Handle error
        if (gs == GS_STARTED) {
            LOG("Recieved PKT_PLAYER_BUILD but game has already started");
            exit(0);
        }
        net_packet_player_build *b = (net_packet_player_build *)p.content;
        player_info *player = get_player_from_fd(fd);
        // We force the id to avoid a player setting the build of another player
        b->id = player->id;
        broadcast_packet(&p);

        for (int i = 0; i < MAX_SPELL_COUNT; i++) {
            player->spells[i] = b->spells[i];
        }
        // TODO: Check that total values are not over the limit
        player->stats[STAT_HEALTH].base = b->health;
        player->stats[STAT_HEALTH].max = b->health;
        player->stats[STAT_HEALTH].value = b->health;

        player->stats[STAT_STRENGTH].base = b->strength;
        player->stats[STAT_STRENGTH].max = b->strength;
        player->stats[STAT_STRENGTH].value = b->strength;

        player->stats[STAT_SPEED].base = b->speed;
        player->stats[STAT_SPEED].max = b->speed;
        player->stats[STAT_SPEED].value = b->speed;
        // We send a PKT_PLAYER_UPDATE to set the base_health for all clients
        broadcast(pkt_from_info(player));
    } else if (p.type == PKT_PLAYER_ACTION) {
        net_packet_player_action *a = (net_packet_player_action *)p.content;
        LOG("Player %d played : %d at %d %d", a->id, a->action, a->x, a->y);

        players[a->id].action = a->action;
        players[a->id].ax = a->x;
        players[a->id].ay = a->y;
        players[a->id].state = RS_WAITING;
        players[a->id].spell = a->spell;

        int all_played = true;
        FOREACH_PLAYER(player) {
            if (player->state == RS_PLAYING) {
                all_played = false;
            }
        }

        // TODO: Rework this part ?
        if (all_played) {
            execute_turn();
        }
    } else if (p.type == PKT_PLAYER_READY) {
        player_ready[get_player_from_fd(fd)->id] = true;
        int ready_count = 0;
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            ready_count += player_ready[i];
        }
        if (ready_count == player_count()) {
            FOREACH_PLAYER(player) {
                reset_player(player);
                broadcast(pkt_player_build(player->id, player->stats[STAT_HEALTH].base, player->spells,
                                           player->stats[STAT_STRENGTH].value, player->stats[STAT_SPEED].value));
                broadcast(pkt_from_info(player));
            }
            broadcast(pkt_round_start());
        }
    } else if (p.type == PKT_GAME_RESET) {
        // Only first player (owner) can reset the game
        if (fd != connections[0]) {
            player_info *player = get_player_from_fd(fd);
            LOG("%s tried to reset the game but they are not the owner.", player->name);
            return;
        }
        LOG("Serv: game reset");
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            round_scores[i] = 0;
        }
        round_start_time = time(NULL);
        gs = GS_WAITING;
        broadcast(pkt_game_reset());
        // We send previously connected players informations to the new player
        FOREACH_PLAYER(player) {
            reset_player(player);
            broadcast(pkt_player_joined(player->id, player->name));

            broadcast(pkt_player_build(player->id, player->stats[STAT_HEALTH].base, player->spells,
                                       player->stats[STAT_STRENGTH].value, player->stats[STAT_SPEED].value));

            net_packet u = pkt_from_info(player);
            ((net_packet_player_update *)u.content)->immediate = true;
            broadcast_packet(&u);

            send_map(connections[player->id]);
        }
        broadcast(pkt_game_start());
    } else if (p.type == PKT_ADMIN_UPDATE_PLAYER_INFO) {
        if (is_admin(fd)) {
            net_packet_admin_update_player_info *info = (net_packet_admin_update_player_info *)p.content;
            if (info->property == PIP_HEALTH) {
                players[info->id].stats[STAT_HEALTH].value = info->value;
                if (players[info->id].stats[STAT_HEALTH].value > players[info->id].stats[STAT_HEALTH].max) {
                    players[info->id].stats[STAT_HEALTH].max = info->value;
                }
                net_packet u = pkt_from_info(&players[info->id]);
                ((net_packet_player_update *)u.content)->immediate = true;
                broadcast_packet(&u);
            }
        } else {
            char msg[128] = {0};
            strncpy(msg, "You are not allowed to execute this command", 128);
            net_packet response = pkt_server_message(LL_ERROR, msg);
            send_sock(&response, fd);
        }
    }
}

// Main

int sort_string(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

// TODO: We should only parse header
const char *extract_map_name(const char *filepath) {
    map_data m = {0};
    if (load_map(filepath, &m) == false) {
        return NULL;
    }
    const char *res = m.headers[MAP_HEADER_NAME].value == NULL ? NULL : strdup(m.headers[MAP_HEADER_NAME].value);
    free_map_data(&m);
    return res;
}

bool ends_with(const char *s, const char *suffix) {
    s = strrchr(s, '.');
    if (s == NULL) {
        return false;
    }
    return strcmp(s, suffix) == 0;
}

void load_maps() {
    const char *map_names[256] = {0};
    DIR *d;
    struct dirent *dir;

    if ((d = opendir("maps"))) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG && ends_with(dir->d_name, ".map")) {
                char *filename = strdup(dir->d_name);
                filename[strlen(filename) - strlen(".map")] = '\0';
                all_maps[map_count] = filename;
                map_names[map_count] = extract_map_name(filename);
                if (all_maps[map_count] == NULL) {
                    LOGL(LL_ERROR, "Error while loading map %s", dir->d_name);
                    exit(-1);
                }
                map_count++;
                if (map_count == 256) {
                    LOGL(LL_ERROR, "Too many maps to load");
                    break;
                }
            }
        }
        closedir(d);
    }

    qsort(map_names, map_count, sizeof(all_maps[0]), sort_string);
    map_names_network = calloc(map_count, 32);
    for (int i = 0; i < map_count; i++) {
        memcpy(map_names_network + i * 32, map_names[i], fmin(32, strlen(map_names[i])));
    }
    for (int i = 0; i < map_count; i++) {
        free((void *)map_names[i]);
    }
}

int main(int argc, char **argv) {
    int port = 3000;
    POPARG(argc, argv);
    while (argc > 0) {
        const char *arg = POPARG(argc, argv);
        if (strcmp(arg, "--port") == 0) {
            const char *value = POPARG(argc, argv);
            if (!strtoint(value, &port)) {
                LOG("Error parsing port to int '%s'", value);
                exit(1);
            }
        } else if (strcmp(arg, "--pass") == 0) {
            server_password = POPARG(argc, argv);
        } else {
            LOG("Unknown arg : '%s'", arg);
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

    LOG("Listening on port %d", port);

    FD_ZERO(&master_set);
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &master_set);
    int fd_count = sockfd;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    struct sockaddr client = {0};
    socklen_t len = sizeof(client);

    load_maps();

    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        players[i].id = i;
    }

    while (1) {
        read_fds = master_set;
        int activity = ci(select(fd_count + 1, &read_fds, NULL, NULL, &timeout));
        if (activity < 0) {
            LOGL(LL_ERROR, "Error in select");
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
                    LOG("new connection %d", connfd);
                    // TODO: Check if there is any space left
                    if (false) {  // player_count == MAX_PLAYER_COUNT) {
                        // TODO: Tell the player the game is full
                        LOG("Full");
                        FD_CLR(connfd, &master_set);
                        close(connfd);
                    } else {
                        connections[connection_count] = connfd;
                        connection_count++;
                    }
                } else {
                    handle_message(i);
                    time_t round_timer = time(NULL) - round_start_time;
                    broadcast(pkt_game_stats(round_timer));
                }
            }
        }
        usleep(16000);  // TODO: Sleep to avoid 100% CPU usage while I implement a better solution
    }

    LOG("Client connected");

    close(sockfd);
}
