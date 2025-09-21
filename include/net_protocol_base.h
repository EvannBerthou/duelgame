#include <stdint.h>
#include <stdio.h>
#include "common.h"

#define NET_SIZE(...)

// Hacky to allow redefinition of net_player_stat. Builder will parse it so it
// knows about the structure but the compiler will not see it.
// TODO: remove this
#if 0
typedef struct {
    uint8_t base;
    uint8_t max;
    uint8_t value;
} net_player_stat;
#endif

typedef struct {
    uint64_t send_time;
    uint64_t recieve_time;
} net_packet_ping;

typedef struct {
    uint8_t id;
    char username[8];
} net_packet_join;

typedef struct {
    uint8_t id;
    uint8_t master;  // Is the player the lobby's master ?
} net_packet_connected;

typedef struct {
    uint8_t id;
    uint8_t new_master;
} net_packet_disconnect;

//TODO: Better way to handle strings in net_protocol_builder
typedef struct {
    uint8_t map_count;
    uint8_t* map_names NET_SIZE("s->map_count * 32");
} net_packet_server_map_list;

typedef struct {
    uint8_t map_index;
} net_packet_update_selected_map;

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t type;
    uint8_t* content NET_SIZE("s->width * s->height");
} net_packet_map;

typedef struct {
    char map_name[32];
} net_packet_request_game_start;

typedef struct {
} net_packet_game_start;

typedef struct {
    uint8_t id;
    net_player_stat stats[STAT_COUNT] NET_SIZE(
        "sizeof(net_player_stat) * STAT_COUNT");
    uint8_t x;
    uint8_t y;
    uint8_t effect[SE_COUNT] NET_SIZE("SE_COUNT");
    uint8_t effect_round_left[SE_COUNT] NET_SIZE("SE_COUNT");
    // Should the game updates the status right as the packet is recieved (true)
    // or wait for the end of the round (false) ?
    uint8_t immediate;
} net_packet_player_update;

// Player build
// Includes stats and spells
typedef struct {
    uint8_t id;
    uint8_t health;
    uint8_t spells[MAX_SPELL_COUNT] NET_SIZE("MAX_SPELL_COUNT");
    uint8_t ad;
    uint8_t ap;
    uint8_t speed;
} net_packet_player_build;

typedef struct {
    uint8_t id;
    uint8_t action;
    uint8_t x;
    uint8_t y;
    uint8_t spell;
} net_packet_player_action;

typedef struct {
} net_packet_turn_end;

typedef struct {
} net_packet_round_start;

typedef struct {
    uint8_t winner_id;
    uint8_t player_scores[MAX_PLAYER_COUNT] NET_SIZE("MAX_PLAYER_COUNT");
} net_packet_round_end;

typedef struct {
    uint8_t winner_id;
    uint8_t player_scores[MAX_PLAYER_COUNT] NET_SIZE("MAX_PLAYER_COUNT");
} net_packet_game_end;

typedef struct {
} net_packet_player_ready;

typedef struct {
} net_packet_game_reset;

typedef struct {
    uint64_t round_timer;
} net_packet_game_stats;

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

typedef struct {
    uint8_t level;  // Log Level
    char message[128];
} net_packet_server_message;
