#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "command.h"
#include "net.h"

#define ci(X)           \
    do {                \
        if (X == 0)     \
            goto retry; \
    } while (0)

int client_fd = 0;

int connect_to_server(const char *ip, uint16_t port) {
    printf("Connecting to %s:%d\n", ip, port);

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        return -1;
    }

    int status;
    if ((status = connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    char *ip = "127.0.0.1";
    int port = 3000;

    POPARG(argc, argv);  // program name
    while (argc > 0) {
        const char *arg = POPARG(argc, argv);
        if (strcmp(arg, "--ip") == 0) {
            ip = POPARG(argc, argv);
        } else if (strcmp(arg, "--port") == 0) {
            const char *value = POPARG(argc, argv);
            if (!strtoint(value, &port)) {
                printf("Error parsing port to int '%s'\n", value);
                exit(1);
            }
        } else {
            printf("Unknown arg : '%s'\n", arg);
            exit(1);
        }
    }

    if (connect_to_server(ip, port) < 0) {
        fprintf(stderr, "Could not connect to server\n");
        exit(1);
    }

    net_packet_admin_connect connect = pkt_admin_connect("pass");
    send_sock(PKT_ADMIN_CONNECT, &connect, client_fd);

    net_packet p = {0};
    if (packet_read(&p, client_fd) < 0) {
        return -1;
    }

    if (p.type != PKT_ADMIN_CONNECT_RESULT) {
        return -1;
    }

    net_packet_admin_connect_result *result = (net_packet_admin_connect_result *)p.content;
    if (result->success == 0) {
        printf("Could not connect to server\n");
        return -1;
    }
    printf("Successfully connected to server as admin\n");

    char buf[64] = {0};
    while (1) {
        if (fgets(buf, 63, stdin) == NULL) {
            printf("Error reading\n");
            exit(1);
        }
        buf[strcspn(buf, "\n")] = '\0';
        command_result result = handle_command(buf);
        if (result.valid == false) {
            if (result.content == NULL) {
                printf("Unknown command `%s`", buf);
            } else {
                printf("%s", (char *)result.content);
            }
        } else {
            if (result.has_packet) {
                if (result.content == NULL) {
                    printf("Error creating packet");
                } else {
                    send_sock(result.type, result.content, client_fd);
                }
                free(result.content);
            }
        }
    }
}
