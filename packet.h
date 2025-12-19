#pragma once

#include <stdint.h>

#define MAX_P_SIZE 2048

enum p_flag {
    PING  = 0x01,
    TYPE2 = 0x02,
    TYPE3 = 0x04,
    TYPE4 = 0x08,
    TYPE5 = 0x10,
    TYPE6 = 0x20,
    TYPE7 = 0x40,
    TYPE8 = 0x80,
};

enum p_type {
    PACKET_TYPE_CONNECT = 0,
    PACKET_TYPE_MOVE    = 1
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
#pragma back(pop)
