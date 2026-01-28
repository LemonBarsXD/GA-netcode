#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>

#include "network.h"


int
net_ping(int fd)
{
    // returns:
    // 0  = success
    // -1 = failure/error

    if(fd < 0) {
        return -1;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    ping_t p = {.diff=0, .time=TIMESPEC_TO_NSEC(now)};

    net_sendpacket(fd, PACKET_PING, &p, sizeof(p));

    return 0;
}

int
net_initserver(int port)
{
    // returns:
    // > 0 = server fd
    // -1  = failure/error

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };

    // allow port reuse
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        return -1;
    }

    if(listen(fd, 1) == -1) {
        perror("listen failed");
        return -1;
    }

    return fd;
}

int
net_initclient(const char* ip, int port)
{
    // returns:
    // > 0 = client fd
    // -1  = failure/error

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

    int opt = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt));

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect error");
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    return fd;
}

int
net_send(int fd, void* data, uint16_t size) 
{
    // returns:
    // 0  = success
    // -1 = failure/error

    uint8_t* ptr = (uint8_t*)data;
    uint16_t bytes_sent = 0;

    while (bytes_sent < size) {
        int sent = send(fd, ptr + bytes_sent, size - bytes_sent, 0);
        if (sent == -1) return -1;
        bytes_sent += sent;
    }

    return 0;
}

int
net_sendpacket(int fd, uint8_t type, void* data, uint16_t size)
{
    // returns:
    // 0  = success
    // -1 = failure/error

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    header_t h = {.type=type, .data_size=size, .timestamp=TIMESPEC_TO_NSEC(now)};

    if (net_send(fd, &h, sizeof(h)) == -1) {
        return -1;
    }

    if (size > 0 && data != NULL) {
        if (net_send(fd, data, size) == -1) {
            return -1;
        }
    }

    printf("[%zu.%ld] sent packet type %d to fd %d\n", now.tv_sec, now.tv_nsec, type, fd);

    return 0;
}

int
net_broadcast(client_t* clients, int amount, int fd_exclude, uint8_t type, void* buffer, uint16_t size)
{
    // returns:
    // > 0 = amount of packets sent to active clients
    // -1  = there is no amount

    if(amount <= 0) return -1;

    int count = 0;
    for(int i = 0; i < amount; ++i) {
        if(clients[i].active && clients[i].fd != fd_exclude) {
            if(net_sendpacket(clients[i].fd, type, buffer, size) != -1){
                count++;
            } else {
                printf("Broadcast FD %d: send failed: %s", clients[i].fd, strerror(errno));
            }
        }
    }
    return count;
}

int
net_recvpacket(client_t* client, header_t* out_header, void* out_data_buffer, int max_buffer_size)
{
    // returns:
    //  1  = complete packet received
    //  0  = nothing / would block
    // -1  = disconnect / EOF
    // -2  = header only (need more data next time)
    // < -2 = various errors (-errno-4)

    struct timespec header_time;
    clock_gettime(CLOCK_MONOTONIC, &header_time);

    uint8_t* buf = client->recv_buf;
    size_t*  len = &client->recv_buf_len;

    while(1) {
        if(*len < sizeof(header_t)) {
            int r = recv(client->fd, buf + *len, sizeof(header_t) - *len, 0);
            printf("[%zu.%ld] header recv on fd %d → %d bytes (len now %zu)\n",
                   header_time.tv_sec, header_time.tv_nsec, client->fd, r, *len);
            if(r == 0) return -1;
            if(r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
                return -errno-4; // posix errors start at 1 so minus 4 to align with ours.
                // https://en.wikipedia.org/wiki/Errno.h for more info
            }
            *len += r;
            if (*len < sizeof(header_t)) return 0;
        }

        header_t temp_header;
        memcpy(&temp_header, buf, sizeof(header_t));

        switch(temp_header.type) {
            case PACKET_CONNECT: {
                if(temp_header.data_size != sizeof(net_handshake_t)) return -2;
                break;
            }
            case PACKET_USERINPUT: {
                if(temp_header.data_size != sizeof(user_cmd_t)) return -2;
                break;
            }
            case PACKET_PING: {
                if(temp_header.data_size != sizeof(ping_t)) return -2;
                break;
            }
            case PACKET_DISCONNECT: {
                if(temp_header.data_size != sizeof(net_disconnect_t)) return -2;
                break;
            }
            case PACKET_STATE: {
                if(temp_header.data_size != sizeof(sv_state_t)) return -2;
                break;
            }
            case PACKET_FULL_STATE: {
                if(temp_header.data_size != sizeof(sv_full_state_t)) return -2;
                break;
            }
            default: {
                return -2;
                break;
            }
        }

        size_t total_needed = sizeof(header_t) + temp_header.data_size;
        if (temp_header.data_size > max_buffer_size) {
            *len = 0;
            return -2;
        }

        struct timespec data_time;
        clock_gettime(CLOCK_MONOTONIC, &data_time);

        if (*len < total_needed) {
            ssize_t r = recv(client->fd, buf + *len, total_needed - *len, 0);
            printf("[%zu.%ld] payload recv on fd %d → %d bytes (total len %zu/%zu)\n",
                   data_time.tv_sec, data_time.tv_nsec, client->fd, r, *len, total_needed);
            if (r == 0) return -1;
            if (r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
                return -errno-4; // align
            }
            *len += r;
            if (*len < total_needed) return 0;
        }

        memcpy(out_header, buf, sizeof(header_t));
        if (temp_header.data_size > 0) {
            memcpy(out_data_buffer, buf + sizeof(header_t), temp_header.data_size);
        }

        memmove(buf, buf + total_needed, *len - total_needed);
        *len -= total_needed;

        return 1;
    }
}

void
net_close(int fd)
{
    if(close(fd) == -1) {
        printf("FD %d: close failed: %s", fd, strerror(errno));
    }
}
