#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "packet.h"
#include "network.h"

#define PLAYER_SPEED 200.0f
#define MAX_PLAYERS 6 // temp, will be changed

typedef struct {
    int fd;
    int active;
    server_state_t state;
} client_t;


client_t clients[MAX_PLAYERS] = {0};

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
            if(clients[i].active) {
                FD_SET(clients[i].fd, &readfds);
                if(clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
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
                // find active slot
                int found_slot = 0;
                for(int i = 0; i < MAX_PLAYERS; ++i) {
                    if(!clients[i].active) {
                        clients[i].fd = new_fd;
                        clients[i].active = 1;
                        clients[i].state.x = 500;
                        clients[i].state.y = 400;
                        clients[i].state.entindex = (uint16_t)i;
                        found_slot = 1;
                        break;
                    }
                }
                // if no slot was found
                if (!found_slot) {
                    printf("Server full, kicking client: %d\n", new_fd);
                    close(new_fd);
                }
            }
        }

        // get data from clients
        for(int i = 0; i < MAX_PLAYERS; ++i) {
            if(clients[i].active && FD_ISSET(clients[i].fd, &readfds)) {
                if (recv(clients[i].fd, &in_header, sizeof(in_header), 0) <= 0) {
                    printf("Client disconnect: %d\n", clients[i].fd);
                    close(clients[i].fd);
                    clients[i].active = 0;
                    continue;
                }

                // validate data_size
                // fixed loop to not assume full data receivment
                if (in_header.data_size > 0) {
                    int bytes_received = 0;
                    while (bytes_received < in_header.data_size) {
                        int r = recv(
                            clients[i].fd,
                            buffer + bytes_received,
                            in_header.data_size - bytes_received,
                            0
                        );
                        if (r <= 0) {
                            puts("Client disconnected or error!\n");
                            close(clients[i].fd);
                            clients[i].active = 0;
                            break; // skip to next
                        }
                        bytes_received += r;
                    }
                    // if we errored continue
                    if(!clients[i].active) continue;
                }

                switch (in_header.type) {
                    case PACKET_CONNECT:
                        {
                            printf("Client %d requesting handshake...\n", clients[i].fd);
                            net_handshake_t resp_hs = {
                                .entindex = i,
                                .version = 1
                            };
                            Net_SendPacket(
                                clients[i].fd,
                                PACKET_CONNECT,
                                &resp_hs,
                                sizeof(resp_hs)
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
                            // broadcast
                            // (sending everyones data to everyone)
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
                            double diff_t = (double) (clock() - recv_ping->time) / CLOCKS_PER_SEC;
                            ping_t resp_ping = {
                                .diff = diff_t,
                                .time = recv_ping->time
                            };
                            Net_SendPacket(
                                clients[i].fd,
                                PACKET_PING,
                                &resp_ping,
                                sizeof(resp_ping)
                            );
                            break;
                        }

                    case PACKET_DISCONNECT:
                        {
                            printf("Client disconnected (packet): %d\n", clients[i].fd);
                            clients[i].active = 0;
                            close(clients[i].fd);
                            break;
                        }
                }
            }
        }
    }

    for(int i = 0; i < MAX_PLAYERS; ++i) {
        if(clients[i].active) {
            close(clients[i].fd);
        }
    }
    close(listen_fd);
    return 0;
}
