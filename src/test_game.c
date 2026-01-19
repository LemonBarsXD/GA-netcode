#include <stdio.h>
#include <raylib.h>
#include <raymath.h>

#include "network.h"
#include "packet.h"
#include "cfg.h"

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 800

#define PLAYER_SPEED 200.0f
#define PLAYER_SIZE 40.0f

#define PING_INTERVAL 1.0f


// simple struct to track everyone else
typedef struct {
    int active;
    Vector2 pos;
    Color color;
} remote_player_t;

int main(int argc, char *argv[])
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, argv[0]);
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));

    int net_fd = Net_InitClient("127.0.0.1", 21337);
    if (net_fd == -1) {
        printf("failed to connect, is the server running? :-)\n");
    } else {
        Net_SendPacket(
            net_fd,
            PACKET_CONNECT,
            NULL,
            0
        );
    }

    Vector2 my_pos = { SCREEN_WIDTH/2, SCREEN_HEIGHT/2 };
    Vector2 my_server_ghost = my_pos;
    remote_player_t other_players[MAX_PLAYERS] = {0};
    int my_entindex = -1;

    Color my_color = { 255, 255, 255, 100 };
    Color ghost_color = { 255, 0, 0, 100 };
    Color others_color = { 0, 126, 126, 100 };

    double accumulator = 0.0;
    uint64_t client_tick = 0;
    uint8_t net_buffer[1024];

    uint8_t client_recv_buf[4096];
    size_t client_recv_buf_len = 0;

    float ping_timer = 0.0f;
    double rtt_ms = 0;

    while(!WindowShouldClose()) {

        float dt = GetFrameTime();

        // this prevents lag from
        // freezing the game if the computer lags for a long period
        // since physics (usually) takes longer to compute.
        // its complicated...
        if (dt > 0.25) dt = 0.25;
        accumulator += dt;
        ping_timer += dt;

        // ping the server
        if (ping_timer >= PING_INTERVAL) {
            // printf("pinging...\n");
            Net_Ping(net_fd);

            ping_timer -= PING_INTERVAL;
        }

        // tick loop.
        // runs the loop depending on what the tickrate is set to
        // times a second
        while(accumulator >= TICK_DELTA) {

            user_cmd_t cmd = {0};
            cmd.tick_number = ++client_tick;

            // build the bitmask
            if(IsKeyDown(KEY_W)) cmd.buttons |= IN_FORWARD;
            if(IsKeyDown(KEY_S)) cmd.buttons |= IN_BACKWARD;
            if(IsKeyDown(KEY_A)) cmd.buttons |= IN_LEFT;
            if(IsKeyDown(KEY_D)) cmd.buttons |= IN_RIGHT;

            // client side prediction
            if(cmd.buttons & IN_FORWARD) {
                my_pos.y -= PLAYER_SPEED * TICK_DELTA;
            }

            if(cmd.buttons & IN_BACKWARD) {
                my_pos.y += PLAYER_SPEED * TICK_DELTA;
            }

            if(cmd.buttons & IN_LEFT) {
                my_pos.x -= PLAYER_SPEED * TICK_DELTA;
            }

            if(cmd.buttons & IN_RIGHT) {
                my_pos.x += PLAYER_SPEED * TICK_DELTA;
            }

            // send packet
            if(net_fd != 0) {
               Net_SendPacket(
                    net_fd,
                    PACKET_USERINPUT,
                    &cmd,
                    sizeof(cmd)
                );
            }

            // decrease accumulator
            accumulator -= TICK_DELTA;
        }


        if (net_fd != -1) {

            // handle incoming packets
            header_t in_header;
            while(
                Net_ReceivePacket(
                    net_fd,
                    &in_header,
                    net_buffer,
                    1024
                ) == 1
            )
            {
                switch(in_header.type) {
                    case PACKET_CONNECT:
                        {
                            net_handshake_t* hs = (net_handshake_t*)net_buffer;
                            printf(
                                "Handshake comlete! \nMy ID: %d \nGame version: %d\n",
                                hs->entindex,
                                hs->version
                            );
                            my_entindex = hs->entindex;

                            for(int i = 0; i < MAX_PLAYERS; ++i) {
                                server_state_t state = hs->clients[i].state;
                                uint32_t id = hs->clients[i].state.entindex;
                                other_players[id].active = 1;
                                other_players[id].pos.x = state.x;
                                other_players[id].pos.y = state.y;
                            }
                            break;
                        }

                    case PACKET_DISCONNECT:
                        {
                            net_disconnect_t* dc = (net_disconnect_t*)net_buffer;
                            printf("Client %d disconnected! (%d)\n", dc->entindex, dc->reason);
                            if(dc->entindex < MAX_PLAYERS) {
                                other_players[dc->entindex].active = 0;
                            }
                            break;
                        }

                    case PACKET_STATE:
                        {
                            server_state_t* state = (server_state_t*)net_buffer;

                            int id = state->entindex;

                            if (id < 0 || id >= MAX_PLAYERS) continue;

                            if(id == my_entindex) {
                                my_server_ghost.x = state->x;
                                my_server_ghost.y = state->y;

                                // simple reconciliation
                                float dist = Vector2Distance(my_pos, my_server_ghost);
                                if(dist > 20.0f) {
                                    printf("Reconciliation to: %f\n", dist);
                                    my_pos = my_server_ghost;
                                }
                            }
                            else {
                                other_players[id].active = 1;
                                other_players[id].pos.x = state->x;
                                other_players[id].pos.y = state->y;
                            }
                            break;
                        }

                    case PACKET_PING:
                        {
                            ping_t* recv_p = (ping_t*)net_buffer;
                            struct timespec now;
                            clock_gettime(CLOCK_MONOTONIC, &now);
                            uint64_t current_time_ns = TIMESPEC_TO_NSEC(now);
                            break;
                        }
                }
            }
        }

        // TODO: Interpolation
        BeginDrawing();
        ClearBackground(BLACK);

        DrawText(
            TextFormat(
                "FPS: %i  Tick: %i",
                GetFPS(),
                client_tick),
            10,
            10,
            20,
            GREEN
        );
        DrawText(
            TextFormat(
                "Ping: %.3f ms",
                rtt_ms),
            10,
            30,
            20,
            GREEN
        );

        DrawRectangleV(
            my_server_ghost,
            (Vector2) {PLAYER_SIZE, PLAYER_SIZE},
            ghost_color
        );
        DrawRectangleV(
            my_pos,
            (Vector2) {PLAYER_SIZE, PLAYER_SIZE},
            my_color
        );

        for(int i = 0; i < MAX_PLAYERS; ++i) {
            if(other_players[i].active && i != my_entindex) {
                DrawRectangleV(
                    other_players[i].pos,
                    (Vector2){PLAYER_SIZE, PLAYER_SIZE},
                    others_color
                );
                DrawText(TextFormat("%d", i), other_players[i].pos.x, other_players[i].pos.y - 20, 20, WHITE);
            }
        }

        EndDrawing();
    }


    Net_SendPacket(net_fd, PACKET_DISCONNECT, NULL, 0);

    if (net_fd != -1) {
        Net_Close(net_fd);
    }
    CloseWindow();
    return 0;
}
