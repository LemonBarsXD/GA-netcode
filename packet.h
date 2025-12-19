#pragma once

#include <stdint.h>
#include <time.h>

#define MAX_P_SIZE 2048

enum p_type {
    PACKET_TYPE_CONNECT = 0,
    PACKET_TYPE_PING    = 1,
    PACKET_TYPE_MOVE    = 2
};

#pragma pack(push, 1)
struct header {
    uint8_t type;
    uint16_t data_size;
};

struct packet {
    struct header _header;
    uint8_t data[MAX_P_SIZE];
};

struct PacketVector2 {
    float x;
    float y;
};

struct PacketEcho {
    double diff;
    clock_t time;
};
#pragma back(pop)
