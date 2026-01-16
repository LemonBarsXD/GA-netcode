/*
 * Packet structures
 *
 * Influenced by source engine
 * */


#pragma once

#include <stdint.h>
#include <time.h>

// configuration
#define TICK_RATE           64
#define TICK_DELTA          (1.0f / (float)TICK_RATE) // ca. 0.0156

// packet types
#define PACKET_DISCONNECT   0
#define PACKET_CONNECT      1
#define PACKET_PING         2
#define PACKET_USERINPUT    3
#define PACKET_STATE        4

// input bitmasks
#define IN_FORWARD          (1 << 0)
#define IN_BACKWARD         (1 << 1)
#define IN_LEFT             (1 << 2)
#define IN_RIGHT            (1 << 3)
#define IN_SPRINT           (1 << 4)
#define IN_JUMP             (1 << 5)



#pragma pack(push, 1)

typedef struct {
    uint16_t data_size;
    uint8_t type;
} header_t;

typedef struct {
    uint32_t entindex;
    uint32_t version;
} net_handshake_t;

typedef struct {
    uint64_t tick_number;
    uint32_t buttons;      // bitmask of inputs
    float view_angle_yaw;
    float view_angle_pitch;
} user_cmd_t;

typedef struct {
    uint64_t last_processed_tick; // for reconciliation
    uint32_t entindex;
    float x;
    float y;
    float z;
} server_state_t;

typedef struct {
    clock_t time;
    double diff;
} ping_t;

#pragma pack(pop)
