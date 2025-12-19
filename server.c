#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "packet.h"

// Note: This is a simple blocking server for demonstration
int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(21337) };

    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 1);

    printf("Server listening on 21337...\n");
    int client_fd = accept(listen_fd, NULL, NULL);
    printf("Client connected!\n");

    struct header h;
    uint8_t buffer[2048]; 

    while(1) {
        // 1. Read Header
        int n = recv(client_fd, &h, sizeof(h), 0);
        if (n <= 0) break; // Client disconnected

        // 2. Read Body
        if (h.data_size > 0) {
            recv(client_fd, buffer, h.data_size, 0);
        }

        // 3. Logic: If it is a move packet, print it and Echo it back
        if (h.type == PACKET_TYPE_MOVE) {
            struct PacketVector2* pos = (struct PacketVector2*)buffer;
            // printf("Server sees player at: %f, %f\n", pos->x, pos->y);

            // Send it back to client so they can render the "Ghost"
            send(client_fd, &h, sizeof(h), 0);
            send(client_fd, buffer, h.data_size, 0);
        }
    }
    
    close(client_fd);
    close(listen_fd);
    return 0;
}
