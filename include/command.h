#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>

typedef struct {
    bool valid;
    bool has_packet;
    int type;
    void *content;
} command_result;

typedef enum {
    CT_UNKNOWN,
    CT_ADMIN_UPDATE_PLAYER_INFO,
    CT_CLEAR,
    CT_LOAD_EDITOR,
} command_type;

command_result handle_command(char *str);

#endif
