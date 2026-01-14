/*
 * Packet structures
 *
 * Influenced by source engine
 * */


#pragma once

#include <stdint.h>
#include <time.h>

// Configurations
#define TICK_RATE           64
#define TICK_DELTA          (1.0f / (float)TICK_RATE) // ca. 0.0156

// packet types
#define PACKET_CONNECT      0
#define PACKET_PING         1
#define PACKET_USERINPUT    2
#define PACKET_STATE        3

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
    uint64_t tick_number;
    uint32_t buttons;      // bitmask of inputs
    float view_angle_yaw;
    float view_angle_pitch;
} user_cmd_t;

typedef struct {
    uint64_t last_processed_tick; // for reconciliation
    float x;
    float y;
} server_state_t;

typedef struct {
    clock_t time;
    double diff;
} ping_t;

#pragma pack(pop)
