/*
 * Packets and structures etc.
 * */

#pragma once

#include <stdint.h>
#include <time.h>

// time macros
#define NS_PER_SEC 1000000000ULL
#define TIMESPEC_TO_NSEC(ts) \
    ((uint64_t)(ts).tv_sec * NS_PER_SEC + (uint64_t)(ts).tv_nsec)

#define NSEC_TO_TIMESPEC(ns) \
    (struct timespec){ .tv_sec = (time_t)((ns) / NS_PER_SEC), .tv_nsec = (long)((ns) % NS_PER_SEC) }

// packet types
#define PACKET_DISCONNECT   0
#define PACKET_CONNECT      1
#define PACKET_PING         2
#define PACKET_USERINPUT    3
#define PACKET_STATE        4
#define PACKET_FULL_STATE   5

// input bitmasks
#define IN_FORWARD          (1 << 0)
#define IN_BACKWARD         (1 << 1)
#define IN_LEFT             (1 << 2)
#define IN_RIGHT            (1 << 3)
#define IN_SPRINT           (1 << 4)
#define IN_JUMP             (1 << 5)
#define IN_LMB              (1 << 6)
#define IN_RMB              (1 << 7)


#pragma pack(push, 1)

typedef struct {
    uint64_t timestamp;
    uint16_t data_size;
    uint8_t  type;
} header_t;

typedef struct {
    uint64_t last_processed_tick; // for reconciliation
    uint32_t entindex;
    float x;
    float y;
    float z;
} server_state_t;

typedef struct {
    uint64_t tick_number;
    uint32_t buttons;      // bitmask of inputs
    float view_angle_yaw;
    float view_angle_pitch;
} user_cmd_t;

typedef struct {
    server_state_t state;
    user_cmd_t* cmdqueue;       // for per-client buffering
    uint8_t recv_buf[4096];     // --||--
    size_t recv_buf_len;        // --||--
    int fd;
    int active;
} client_t;

typedef struct {
    uint64_t time;
    uint64_t diff;
} ping_t;

typedef struct {
    client_t* clients;
    uint32_t  entindex;
    uint32_t  version;
} net_handshake_t;

typedef struct {
    uint32_t entindex;
    uint32_t reason;
} net_disconnect_t;

#pragma pack(pop)
