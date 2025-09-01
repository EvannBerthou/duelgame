#include "command.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "net_protocol.h"

#define GETTOKI(X)                                                                                             \
    const char *X##str = strtok(NULL, " ");                                                                    \
    if (X##str == NULL) {                                                                                      \
        return (command_result){.valid = false, .has_packet = false, .content = (void *)command_usage(command)}; \
    }                                                                                                          \
    int X = 0;                                                                                                 \
    strtoint(X##str, &X);                                                                                      \
    do {                                                                                                       \
    } while (0)

#define GETTOKS(X)                                                                                               \
    const char *X = strtok(NULL, " ");                                                                           \
    if (X == NULL) {                                                                                             \
        return (command_result){.valid = false, .has_packet = false, .content = (void *)command_usage(command)}; \
    }                                                                                                            \
    do {                                                                                                         \
    } while (0)

static bool streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

command_type get_command_type(char *str) {
    char *tok = strtok(str, " ");
    if (streq(tok, "update")) {
        return CT_ADMIN_UPDATE_PLAYER_INFO;
    } else if (streq(tok, "clear")) {
        return CT_CLEAR;
    } else {
        return CT_UNKNOWN;
    }
}

const char *command_usage(command_type command) {
    if (command == CT_ADMIN_UPDATE_PLAYER_INFO) {
        return "update <player_id> <property_id> <value>";
    }
    return "Unknown command";
}

command_result handle_command(char *str) {
    command_type command = get_command_type(str);
    if (command == CT_ADMIN_UPDATE_PLAYER_INFO) {
        GETTOKI(id);
        GETTOKI(prop);
        GETTOKI(value);

        net_packet_admin_update_player_info *update = malloc(sizeof(net_packet_admin_update_player_info));
        *update = pkt_admin_update_player_info(id, prop, value);
        return (command_result){
            .valid = true, .has_packet = true, .type = PKT_ADMIN_UPDATE_PLAYER_INFO, .content = update};
    } else if (command == CT_CLEAR) {
        clear_logs();
        return (command_result){.valid = true, .has_packet = false, .content = NULL};
    } else {
        return (command_result){.valid = false, .has_packet = false};
    }
}
