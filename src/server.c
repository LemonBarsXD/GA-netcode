#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#include "packet.h"
#include "network.h"
#include "client.h"
#include "cfg.h"

#define PLAYER_SPEED 5.f

client_t clients[MAX_PLAYERS];

pthread_mutex_t global_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int is_server_running = 1;

void* svloop(void* arg) {
    int listen_fd = *(int*)arg;
    fd_set readfds;
    int max_fd;

    header_t in_header;
    uint8_t buffer[2048];

    while(is_server_running) {
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        max_fd = listen_fd;

        pthread_mutex_lock(&global_clients_mutex);
        for(int i = 0; i < MAX_PLAYERS; ++i) {
            if(clients[i].active && clients[i].fd > 0) {
                FD_SET(clients[i].fd, &readfds);
                if(clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }
        pthread_mutex_unlock(&global_clients_mutex);
        // printf("unlocked after max_fd\n");

        if(select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1) {
            perror("select error");
            break;
        }

        if(FD_ISSET(listen_fd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);

            if(new_fd < 0) {
                perror("accept failed");
            } else {
                printf("Accepted fd: %d\n", new_fd);
            }

            if(new_fd >= 0) {
                pthread_mutex_lock(&global_clients_mutex);
                printf("New connection from: %s\n", inet_ntoa(client_addr.sin_addr));

                // find active slot
                int found_slot = 0;
                for(int i = 0; i < MAX_PLAYERS; ++i) {
                    if(!clients[i].active) {
                        cl_initqueue(&clients[i]);
                        clients[i].fd = new_fd;
                        clients[i].active = 1;
                        clients[i].state.x = 0;
                        clients[i].state.y = 0;
                        clients[i].state.z = 0.5f;
                        clients[i].state.entindex = (uint16_t)i;
                        clients[i].recv_buf_len = 0;
                        found_slot = 1;

                        // set fd to non-blocking
                        int flags = fcntl(new_fd, F_GETFL, 0);
                        if(fcntl(new_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                            perror("fcntl O_NONBLOCK");
                        }
                        break;
                    }
                }

                pthread_mutex_unlock(&global_clients_mutex);
                // printf("unlocked after accept\n");
                if (!found_slot) {
                    printf("Server full, kicking client: %d\n", new_fd);
                    net_close(new_fd);
                }
            }
        }

        // get data from clients
        pthread_mutex_lock(&global_clients_mutex);
        for(int i = 0; i < MAX_PLAYERS; ++i) {
            if(clients[i].active && FD_ISSET(clients[i].fd, &readfds)) {

                int res = net_recvpacket(&clients[i], &in_header, buffer, sizeof(buffer));
                if(res == 1) {
                    switch(in_header.type) {
                        case PACKET_CONNECT:
                            {
                                net_handshake_t* req_hs = (net_handshake_t*)buffer;
                                net_handshake_t hs = {.entindex = clients[i].state.entindex, .version = VERSION};
                                net_sendpacket(clients[i].fd, PACKET_CONNECT, &hs, sizeof(hs));
                                printf("Client %d connected with version %d\n", clients[i].fd, req_hs->version);

                                // broadcast new client to others
                                net_broadcast(
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
                                net_sendpacket(clients[i].fd, PACKET_FULL_STATE, &full_state, sizeof(full_state));

                                break;
                            }

                        case PACKET_USERINPUT:
                            {
                                user_cmd_t* cmd = (user_cmd_t*)buffer;
                                if (cl_pushcmd(&clients[i], cmd) == -1) {
                                    printf("Client %d command queue full!\n", clients[i].fd);
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
                                net_sendpacket(clients[i].fd, PACKET_PING, &resp_ping, sizeof(resp_ping));
                                break;
                            }

                        case PACKET_DISCONNECT:
                            {
                                printf("Client disconnected by packet: %d\n", clients[i].fd);
                                net_disconnect_t dc = {.entindex=clients[i].state.entindex, .reason=1};
                                net_broadcast(
                                    clients,
                                    MAX_PLAYERS,
                                    clients[i].fd,
                                    PACKET_DISCONNECT,
                                    &dc,
                                    sizeof(dc)
                                );
                                clients[i].active = 0;
                                pthread_mutex_destroy(&clients[i].cmd_mutex);
                                net_close(clients[i].fd);
                                break;
                            }
                    }
                } else if (res == -1 || res == -2) {
                    printf("Client disconnected (error/invalid packet): %d\n", clients[i].fd);
                    net_disconnect_t dc = {.entindex=clients[i].state.entindex, .reason=2};
                    net_broadcast(
                        clients,
                        MAX_PLAYERS,
                        clients[i].fd,
                        PACKET_DISCONNECT,
                        &dc,
                        sizeof(dc)
                    );
                    clients[i].active = 0;
                    pthread_mutex_destroy(&clients[i].cmd_mutex);
                    net_close(clients[i].fd);
                }
            }
        }
        pthread_mutex_unlock(&global_clients_mutex);
        // printf("unlocked after data\n");
    }
    return NULL;
}

int main() {
    int port = 21337;
    int listen_fd = net_initserver(port);

    if(listen_fd == -1) {
        printf("failed to create server!\n");
    }

    printf("Server listening on %d...\n", port);

    for(int i = 0; i < MAX_PLAYERS; ++i) {
        clients[i].active = 0;
        clients[i].fd = -1;
    }

    pthread_t net_thread_id;
    if (pthread_create(&net_thread_id, NULL, svloop, &listen_fd) != 0) {
        perror("Failed to create network thread");
        return 1;
    }

    double accumulator = 0.0f;

    struct timeval prev_tv;
    gettimeofday(&prev_tv, NULL);

    while(is_server_running) {
        struct timeval now;
        gettimeofday(&now, NULL);
        double dt = (now.tv_sec - prev_tv.tv_sec) + (double)(now.tv_usec - prev_tv.tv_usec) / 1000000.0f;
        accumulator += dt;
        prev_tv = now;

        // process ticks
        while (accumulator >= TICK_DELTA) {
            pthread_mutex_lock(&global_clients_mutex);
            for(int j = 0; j < MAX_PLAYERS; ++j) {
                if (!clients[j].active) continue;
                user_cmd_t cmd;
                while(cl_popcmd(&clients[j], &cmd)) {
                    // calculate physics
                    float yaw_rad = cmd.view_angle_yaw * (3.14159265f / 180.0f); // Deg2Rad
                    float move_amt = PLAYER_SPEED * TICK_DELTA;

                    float fwd_x = sinf(yaw_rad);
                    float fwd_y = cosf(yaw_rad);

                    float right_x = cosf(yaw_rad);
                    float right_y = -sinf(yaw_rad);

                    if (cmd.buttons & IN_FORWARD) {
                        clients[j].state.x += fwd_x * move_amt;
                        clients[j].state.y += fwd_y * move_amt;
                    }
                    if (cmd.buttons & IN_BACKWARD) {
                        clients[j].state.x -= fwd_x * move_amt;
                        clients[j].state.y -= fwd_y * move_amt;
                    }
                    if (cmd.buttons & IN_RIGHT) {
                        clients[j].state.x += right_x * move_amt;
                        clients[j].state.y += right_y * move_amt;
                    }
                    if (cmd.buttons & IN_LEFT) {
                        clients[j].state.x -= right_x * move_amt;
                        clients[j].state.y -= right_y * move_amt;
                    }

                    clients[j].state.last_processed_tick = cmd.tick_number;
                    clients[j].state.view_angle_yaw = cmd.view_angle_yaw;
                    clients[j].state.view_angle_pitch = cmd.view_angle_pitch;
                }
            }

            // broadcast full state to all
            sv_full_state_t full_state = {0};
            int count = 0;
            for (int j = 0; j < MAX_PLAYERS; ++j) {
                if (clients[j].active) {
                    full_state.states[count] = clients[j].state;
                    count++;
                }
            }
            full_state.num_states = count;
            net_broadcast(clients, MAX_PLAYERS, -1, PACKET_FULL_STATE, &full_state, sizeof(full_state));

            pthread_mutex_unlock(&global_clients_mutex);
            // printf("unlocked after ticks\n");
            accumulator -= TICK_DELTA;
        }
    }

    pthread_join(net_thread_id, NULL);
    for(int i = 0; i < MAX_PLAYERS; ++i) {
        if(clients[i].active) {
            net_close(clients[i].fd);
        }
    }
    net_close(listen_fd);
    return 0;
}
