#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "packet.h"
#include "network.h"

#define PLAYER_SPEED 200.0f

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(21337)
    };

    // allow port reuse
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 1);

    printf("Server listening on 21337...\n");
    int client_fd = accept(listen_fd, NULL, NULL);
    printf("Client connected.\n");

    // "real" player position
    server_state_t player_state = {
        .x = 500,
        .y = 400,
        .last_processed_tick = 0,
    };

    header_t in_header;
    uint8_t buffer[2048];

    while(1) {
        // blocking recv
        // not for prod
        if (recv(client_fd, &in_header, sizeof(in_header), 0) <= 0)
            break;

        if (in_header.data_size > 0) {
            recv(client_fd, buffer, in_header.data_size, 0);
        }

        if (in_header.type == PACKET_USERINPUT) {
            user_cmd_t* cmd = (user_cmd_t*)buffer;

            // calculate physics
            float move_amt = PLAYER_SPEED * TICK_DELTA;

            if (cmd->buttons & IN_FORWARD)  player_state.y -= move_amt;
            if (cmd->buttons & IN_BACKWARD) player_state.y += move_amt;
            if (cmd->buttons & IN_LEFT)     player_state.x -= move_amt;
            if (cmd->buttons & IN_RIGHT)    player_state.x += move_amt;

            player_state.last_processed_tick = cmd->tick_number;

            // send server state
            Net_SendPacket(
                client_fd,
                PACKET_STATE,
                &player_state,
                sizeof(player_state)
            );
        }

        if (in_header.type == PACKET_PING) {
            ping_t* recv_ping = (ping_t*) buffer;

            // puts("ping recieved!");

            // calculate time difference
            clock_t resp_t = clock();
            double diff_t = (double) (resp_t - recv_ping->time) / CLOCKS_PER_SEC;

            ping_t resp_ping = {
                .diff = diff_t,
                .time = recv_ping->time // sending time
            };

            Net_SendPacket(
                client_fd,
                PACKET_PING,
                &resp_ping,
                sizeof(resp_ping)
            );
        }
    }

    close(client_fd);
    close(listen_fd);
    return 0;
}
