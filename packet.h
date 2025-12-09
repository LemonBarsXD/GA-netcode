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

struct header {
    uint8_t type;
    uint8_t flags;

    uint64_t size; // data size in byte(s)
    uint64_t id;
};

struct data {
    uint8_t p_data[MAX_P_SIZE];
};

struct packet {
    struct header _header;
    struct data _data;
};


