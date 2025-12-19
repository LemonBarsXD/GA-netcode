#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "packet.h"

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(21337) 
    };

    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 1);

    printf("server listening on 21337...\n");
    int client_fd = accept(listen_fd, NULL, NULL);
    printf("client connected!\n");

    struct header h;
    uint8_t buffer[2048]; 

    while(1) {
        int n = recv(client_fd, &h, sizeof(h), 0);
        if (n <= 0) break;

        if (h.data_size > 0) {
            recv(client_fd, buffer, h.data_size, 0);
        }

        if (h.type == PACKET_TYPE_MOVE) {
            struct PacketVector2* pos = (struct PacketVector2*)buffer;
            // printf("Server sees player at: %f, %f\n", pos->x, pos->y); // debug

            send(client_fd, &h, sizeof(h), 0);
            send(client_fd, buffer, h.data_size, 0);
        }

        if (h.type == PACKET_TYPE_PING) {
            struct PacketEcho* recv_echo = (struct PacketEcho*)buffer;

            puts("ping recieved!");

            clock_t resp_t = recv_echo->time;

            double diff_t = (double) (resp_t - recv_echo->time) / CLOCKS_PER_SEC;

            struct PacketEcho pkt;
            pkt.diff = diff_t;
            pkt.time = resp_t;

            struct header resp_h;
            resp_h.type = PACKET_TYPE_PING;
            resp_h.data_size = sizeof(pkt);

            send(client_fd, &resp_h, sizeof(resp_h)  , 0);
            send(client_fd, &pkt   , sizeof(pkt), 0);
        }
    }

    close(client_fd);
    close(listen_fd);

    return 0;
}
