#include <stdio.h>
#include <string.h>
#include <unistd.h>
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

    int ret_val = 0;

    while(1) {

        // define and zero file descriptors
        fd_set readfds;
        FD_ZERO(&readfds);

        // add listener
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

        struct timeval timeout = {.tv_sec=0, .tv_usec=TICK_DELTA*NS_PER_SEC/1000}; // to microseconds
        if (select(max_fd + 1, &readfds, NULL, NULL, &timeout) == -1) {
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

                int res = Net_ReceivePacket(&clients[i], &in_header, buffer, 2048);

                if(res == -1) {
                    printf("Client %d closed connection!\n", clients[i].state.entindex);
                    net_disconnect_t dc = {.entindex=clients[i].state.entindex, .reason=0};
                    Net_Broadcast(clients, MAX_PLAYERS, PACKET_DISCONNECT, 0, &dc, sizeof(dc));
                    Net_Close(clients[i].fd);
                }
                else if (res != 1 && res != 0) {
                    printf("Client %d error: %s\n", clients[i].state.entindex, strerror(res));
                    net_disconnect_t dc = {.entindex=clients[i].state.entindex, .reason=res};
                    Net_Broadcast(clients, MAX_PLAYERS, PACKET_DISCONNECT, 0, &dc, sizeof(dc));
                    Net_Close(clients[i].fd);
                }

                switch (in_header.type) {
                    case PACKET_CONNECT:
                        {
                            printf("Client %d requesting handshake...\n", clients[i].fd);

                            net_handshake_t resp_hs = {.entindex = i, .version = VERSION,};
                            Net_SendPacket(clients[i].fd, PACKET_CONNECT, &resp_hs, sizeof(resp_hs));

                            sv_full_state_t full_state;
                            for(int j = 0; j < MAX_PLAYERS; ++j) {
                                if(clients[j].active) {
                                    full_state.states[j] = clients[j].state;
                                }
                            }
                            Net_SendPacket(clients[i].fd, PACKET_FULL_STATE, &full_state, sizeof(full_state));

                            Net_Broadcast(
                                clients,
                                MAX_PLAYERS,
                                PACKET_STATE,
                                clients[i].fd,
                                &clients[i].state,
                                sizeof(clients[i].state)
                            );
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

                            // sending everyones data to everyone
                            for(int l = 0; l < MAX_PLAYERS; ++l) {
                                if(!clients[l].active) continue;

                                for (int j = 0; j < MAX_PLAYERS; ++j) {
                                    if(!clients[j].active) continue;

                                    Net_SendPacket(
                                        clients[l].fd,
                                        PACKET_STATE,
                                        &clients[j].state,
                                        sizeof(clients[j].state)
                                    );
                                }
                            }

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
                            Net_Broadcast(clients, MAX_PLAYERS, 0, PACKET_DISCONNECT, &dc, sizeof(dc));
                            Net_Close(clients[i].fd);
                            break;
                        }
                }
            }
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
