#include <stdint.h>

#define NET_SIZE(...)

typedef struct {
} net_packet_ping;

typedef struct {
    uint8_t id;
    char username[8];
} net_packet_join;

typedef struct {
    uint8_t id;
} net_packet_connected;

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t type;
    uint8_t *content NET_SIZE("s->width * s->height");
} net_packet_map;

typedef struct {
} net_packet_game_start;

typedef struct {
    uint8_t id;
    uint8_t health;
    uint8_t max_health;
    uint8_t ad;
    uint8_t ap;
    uint8_t x;
    uint8_t y;
    uint8_t effect;
    uint8_t effect_round_left;
} net_packet_player_update;

// Player build
// Includes stats and spells
typedef struct {
    uint8_t id;
    uint8_t base_health;
    // We only send IDs since spells are fixed by the game and not the player
    uint8_t spells[MAX_SPELL_COUNT] NET_SIZE("MAX_SPELL_COUNT");
    uint8_t ad;
    uint8_t ap;
} net_packet_player_build;

typedef struct {
    uint8_t id;
    uint8_t action;
    uint8_t x;
    uint8_t y;
    uint8_t spell;
} net_packet_player_action;

typedef struct {
    uint8_t winner_id;
} net_packet_round_end;

typedef struct {
} net_packet_game_reset;

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
