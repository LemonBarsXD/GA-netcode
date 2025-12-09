#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "packet.h"

// #define DEBUG
#define DEF_PORT 21337

static void 
client(int port) 
{
    printf("[NETW] Starting client.\n");

#ifdef DEBUG
    puts("[DEBUG] creating socket...");
#endif
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    const char *ipv4_addr = "127.0.0.1";

    if(inet_pton(AF_INET, ipv4_addr, &addr.sin_addr) != 1) {
        perror("client: address convert error");
        return;
    }

    socklen_t addr_len = sizeof(addr);

    printf("[NETW] Connecting to %s on port %d\n", ipv4_addr, htons(addr.sin_port));
    if(connect( fd, (const struct sockaddr*) &addr, addr_len )) {
        perror("client: connect error");
        return;
    }

#ifdef DEBUG
    puts("[DEBUG] sending...");
#endif

    //const char *msg = "Hello from a client!";

    struct packet pct = { 0 };
    pct._header.size = 19;
    pct._header.id = 1;

    uint8_t tmp[] = {
        0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9,
        0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12
    };

    memcpy(pct._data.p_data, tmp, sizeof(tmp));


    int bytes_sent = (int) send(fd, &pct, sizeof(pct), 0 );
    if(bytes_sent == -1) {
        perror("client: send error");
        return;
    }

    printf("[NETW] Sent %d byte(s)\n", bytes_sent);

#ifdef DEBUG
    puts("[DEBUG] closing...");
#endif
    close( fd );
}

static void
server()
{
    printf("[NETW] Starting server.\n");

#ifdef DEBUG
    puts("[DEBUG] creating socket...");
#endif
    const int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = ntohs(21337);
    socklen_t addr_len = sizeof(addr);

#ifdef DEBUG
    puts("[DEBUG] binding socket...");
#endif
    if(bind( fd, (const struct sockaddr*) &addr, addr_len )) {
        perror("server: bind error");
        return;
    }

    printf("[NETW] Server is on port %d!\n", (int) ntohs(addr.sin_port));

#ifdef DEBUG
    puts("[DEBUG] listening for socket connection...");
#endif
    if(listen( fd, 1 )) {
        perror("server: listen error");
        return;
    }

#ifdef DEBUG
    puts("[DEBUG] waiting for connections to accept...");
#endif
    const int cfd = accept( fd, (struct sockaddr*) &addr, &addr_len );
    if(cfd == -1) {
        perror("server: accept error");
        return;
    }

#ifdef DEBUG
    puts("[DEBUG] recieveing...");
#endif
    struct packet pct;
    const size_t msg_len = recv( cfd, &pct, sizeof(pct), 0 );

    printf("[NETW] recieved! \n[NETW] recv size: %zd bytes\n", msg_len);

#ifdef DEBUG
    printf("type: %u\n", pct._header.type);
    printf("flags: %u\n", pct._header.flags);
    printf("size: %lu\n", pct._header.size);
    printf("type: %lu\n", pct._header.id);
#endif
    for(size_t i = 0; i < pct._header.size; i++) {
        printf("%02X ", pct._data.p_data[i]);
    }
    puts("");
#ifdef DEBUG
    puts("[DEBUG] closing...");
#endif
    close( cfd );
    close( fd );
}

int 
main(int argc, char *argv[]) 
{
    if(argc > 1 && !strcmp(argv[1], "client")) {
        if(argc > 3) {
            fprintf(stderr, "too many args!");
            return -1;
        }

        if(argc == 2) {
            client(DEF_PORT);
        } else {
        int port;
        sscanf(argv[2], "%d", &port);

        client(port);
        }
    } else {
        server();
    }

    return 0;
}
