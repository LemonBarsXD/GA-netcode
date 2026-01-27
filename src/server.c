#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#include "packet.h"
#include "network.h"
#include "cfg.h"

#define PLAYER_SPEED 200.0f

client_t clients[MAX_PLAYERS];

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

    if(bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        return -1;
    }

    if(listen(listen_fd, 1) == -1) {
        perror("listen failed");
        return -1;
    }

    printf("Server listening on 21337...\n");

    for(int i = 0; i < MAX_PLAYERS; ++i) {
        clients[i].active = 0;
        clients[i].fd = -1;
    }

    header_t in_header;
    uint8_t buffer[2048];

    double accumulator = 0.0;

    struct timeval prev_tv;
    gettimeofday(&prev_tv, NULL);

    while(1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        double dt = (now.tv_sec - prev_tv.tv_sec) + (double)(now.tv_usec - prev_tv.tv_usec) / 1000000.0f;
        accumulator += dt;
        prev_tv = now;

        fd_set readfds;
        FD_ZERO(&readfds);

        FD_SET(listen_fd, &readfds);
        int max_fd = listen_fd;

        for(int i = 0; i < MAX_PLAYERS; ++i) {
            if(clients[i].active && clients[i].fd > 0) {
                FD_SET(clients[i].fd, &readfds);
                if(clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1) {
            perror("select failed, continuing...");
            continue;
        }

        // check for new connections
        // give it a slot
        // or kick if server is full (no slots available)
        if(FD_ISSET(listen_fd, &readfds)) {
            int new_fd = accept(listen_fd, NULL, NULL);
            if(new_fd != -1) {
                printf("New connection: %d\n", new_fd);

                // set fd to non-blocking
                int flags = fcntl(new_fd, F_GETFL, 0);
                fcntl(new_fd, F_SETFL, flags | O_NONBLOCK);

                // find active slot
                int found_slot = 0;
                for(int i = 0; i < MAX_PLAYERS; ++i) {
                    if(!clients[i].active) {
                        clients[i].fd = new_fd;
                        clients[i].active = 1;
                        clients[i].state.x = 500;
                        clients[i].state.y = 400;
                        clients[i].state.entindex = (uint16_t)i;
                        clients[i].recv_buf_len = 0;
                        found_slot = 1;
                        break;
                    }
                }
                // if no slot was found
                if (!found_slot) {
                    printf("Server full, kicking client: %d\n", new_fd);
                    Net_Close(new_fd);
                }
            }
        }

        // get data from clients
        for(int i = 0; i < MAX_PLAYERS; ++i) {
            if(clients[i].active && FD_ISSET(clients[i].fd, &readfds)) {

                int res = Net_ReceivePacket(&clients[i], &in_header, buffer, sizeof(buffer));
                if(res == 1) {
                    if(in_header.type != PACKET_USERINPUT) {
                        printf("packet received: %d\n", in_header.type);
                    }
                    switch(in_header.type) {
                        case PACKET_CONNECT:
                            {
                                net_handshake_t* req_hs = (net_handshake_t*)buffer;
                                net_handshake_t hs = {.entindex = clients[i].state.entindex, .version = VERSION};
                                Net_SendPacket(clients[i].fd, PACKET_CONNECT, &hs, sizeof(hs));
                                printf("Client %d connected with version %d\n", clients[i].fd, req_hs->version);

                                // broadcast new client to others
                                Net_Broadcast(
                                    clients,
                                    MAX_PLAYERS,
                                    clients[i].fd,
                                    PACKET_STATE,
                                    &clients[i].state,
                                    sizeof(clients[i].state)
                                );

                                // send full state to new client
                                sv_full_state_t full_state = {0};
                                int count = 0;
                                for (int j = 0; j < MAX_PLAYERS; ++j) {
                                    if(clients[j].active && j != i) {
                                        full_state.states[count] = clients[j].state;
                                        count++;
                                    }
                                }
                                full_state.num_states = count;
                                Net_SendPacket(clients[i].fd, PACKET_FULL_STATE, &full_state, sizeof(full_state));

                                break;
                            }

                        case PACKET_USERINPUT:
                            {
                                user_cmd_t* cmd = (user_cmd_t*)buffer;
                                // calculate physics
                                float move_amt = PLAYER_SPEED * TICK_DELTA;
                                if (cmd->buttons & IN_FORWARD)  clients[i].state.y -= move_amt;
                                if (cmd->buttons & IN_BACKWARD) clients[i].state.y += move_amt;
                                if (cmd->buttons & IN_LEFT)     clients[i].state.x -= move_amt;
                                if (cmd->buttons & IN_RIGHT)    clients[i].state.x += move_amt;
                                clients[i].state.last_processed_tick = cmd->tick_number;

                                break;
                            }

                        case PACKET_PING:
                            {
                                ping_t* recv_ping = (ping_t*) buffer;
                                // calculate time difference
                                struct timespec now;
                                clock_gettime(CLOCK_MONOTONIC, &now);
                                uint64_t time_t = TIMESPEC_TO_NSEC(now);
                                uint64_t diff_t = time_t - recv_ping->time;
                                ping_t resp_ping = {.diff = diff_t, .time = recv_ping->time};
                                Net_SendPacket(clients[i].fd, PACKET_PING, &resp_ping, sizeof(resp_ping));
                                break;
                            }

                        case PACKET_DISCONNECT:
                            {
                                printf("Client disconnected by packet: %d\n", clients[i].fd);
                                clients[i].active = 0;
                                net_disconnect_t dc = {.entindex=clients[i].state.entindex, .reason=1};
                                Net_Broadcast(
                                    clients,
                                    MAX_PLAYERS,
                                    clients[i].fd,
                                    PACKET_DISCONNECT,
                                    &dc,
                                    sizeof(dc)
                                );
                                Net_Close(clients[i].fd);
                                break;
                            }
                    }
                } else if (res == -1 || res == -2) {
                    printf("Client disconnected (error/invalid packet): %d\n", clients[i].fd);
                    net_disconnect_t dc = {.entindex=clients[i].state.entindex, .reason=2};
                    Net_Broadcast(
                        clients,
                        MAX_PLAYERS,
                        clients[i].fd,
                        PACKET_DISCONNECT,
                        &dc,
                        sizeof(dc)
                    );
                    clients[i].active = 0;
                    Net_Close(clients[i].fd);
                }
            }
        }

        // Process ticks
        while (accumulator >= TICK_DELTA) {
            // TODO: Process queued cmds here if implemented

            // Broadcast full state to all
            sv_full_state_t full_state = {0};
            int count = 0;
            for (int j = 0; j < MAX_PLAYERS; ++j) {
                if (clients[j].active) {
                    full_state.states[count] = clients[j].state;
                    count++;
                }
            }
            full_state.num_states = count;
            Net_Broadcast(clients, MAX_PLAYERS, -1, PACKET_FULL_STATE, &full_state, sizeof(full_state));
            accumulator -= TICK_DELTA;
        }
    }

    for(int i = 0; i < MAX_PLAYERS; ++i) {
        if(clients[i].active) {
            Net_Close(clients[i].fd);
        }
    }
    Net_Close(listen_fd);
    return 0;
}
