#include "command.h"
#include <stdbool.h>
#include <string.h>
#include "common.h"
#include "net_protocol.h"

bool load_editor(const char *filename);

#define GETTOKI(X)                                                                                               \
    const char *X##str = strtok(NULL, " ");                                                                      \
    if (X##str == NULL) {                                                                                        \
        return (command_result){.valid = false, .has_packet = false, .content = (void *)command_usage(command)}; \
    }                                                                                                            \
    int X = 0;                                                                                                   \
    strtoint(X##str, &X);                                                                                        \
    do {                                                                                                         \
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
    } else if (streq(tok, "editor")) {
        return CT_LOAD_EDITOR;
    } else {
        return CT_UNKNOWN;
    }
}

const char *command_usage(command_type command) {
    if (command == CT_ADMIN_UPDATE_PLAYER_INFO) {
        return "update <player_id> <property_id> <value>";
    } else if (command == CT_LOAD_EDITOR) {
        return "editor <map_name>";
    }
    return "Unknown command";
}

command_result handle_command(char *str) {
    command_type command = get_command_type(str);
    if (command == CT_ADMIN_UPDATE_PLAYER_INFO) {
        GETTOKI(id);
        GETTOKI(prop);
        GETTOKI(value);

        // TODO: Fix
        //  net_packet_admin_update_player_info *update = malloc(sizeof(net_packet_admin_update_player_info));
        //  *update = pkt_admin_update_player_info(id, prop, value);
        //  return (command_result){
        //      .valid = true, .has_packet = true, .type = PKT_ADMIN_UPDATE_PLAYER_INFO, .content = update};
        return (command_result){0};
    } else if (command == CT_CLEAR) {
        clear_logs();
        return (command_result){.valid = true, .has_packet = false, .content = NULL};
    } else if (command == CT_LOAD_EDITOR) {
        GETTOKS(filename);
        if (strcmp(filename, "") == 0) {
            return (command_result){.valid = false, .content = (void *)command_usage(command)};
        }
        if (load_editor(filename) == false) {
            create_map(filename);
            load_editor(filename);
        }
        return (command_result){.valid = true};
    } else {
        return (command_result){.valid = false, .has_packet = false};
    }
}
