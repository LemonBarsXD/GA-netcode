#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

int Net_Ping(int fd) {
    if(fd < 0) {
        return -1;
    }

    clock_t start_t = clock();

    struct PacketEcho pkt; 
    pkt.diff = 0;
    pkt.time = start_t;

    struct header h;
    h.type = PACKET_TYPE_PING;
    h.data_size = sizeof(pkt);

    if(send(fd, &h, sizeof(h), 0) == -1) {
        perror("send error");
        return -1;
    }

    if (send(fd, &pkt, h.data_size, 0) == -1) {
        perror("send error");
        return -1;
    }

    return 0;
}

int Net_InitClient(const char* ip, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket error");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        return -1;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect error");
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    printf("[NETW] connected to server.\n");
    return fd;
}

int Net_SendPacket(int fd, uint8_t type, void* data, uint16_t size) 
{
    struct header h;
    h.type = type;
    h.data_size = size;

    if (send(fd, &h, sizeof(h), 0) == -1) {
        return -1;
    }

    if (size > 0 && data != NULL) {
        if (send(fd, data, size, 0) == -1) {
            return -1;
        }
    }
    return 0;
}

int Net_ReceivePacket(int fd, struct header* out_header, void* out_data_buffer, int max_buffer_size)
{
    ssize_t bytes_read = recv(fd, out_header, sizeof(struct header), 0);

    if (bytes_read > 0) {
        if (out_header->data_size > 0) {
            recv(fd, out_data_buffer, out_header->data_size, 0);
        }

        return 1;
    }

    if (bytes_read == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }

        perror("[NETW] recv error");
    } else {
        printf("[NETW] Server closed connection.\n");
    }

    return 0; 
}

void Net_Close(int fd) 
{
    close(fd);
}
