#include <stdio.h>
#include <raylib.h>
#include <raymath.h>
#include "network.h"
#include "packet.h"

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 800

#define PLAYER_SPEED 200.0f
#define PLAYER_SIZE 40.0f

#define PING_INTERVAL 1.0f

int main(int argc, char *argv[])
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, argv[0]);
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));

    int net_fd = Net_InitClient("127.0.0.1", 21337);
    if (net_fd == -1)
        printf("failed to connect, is the server running? :-)\n");

    Vector2 my_pos = { SCREEN_WIDTH/2, SCREEN_HEIGHT/2 };
    Vector2 server_pos = my_pos;

    Color my_color = { 255, 255, 255, 100 };
    Color ghost_color = { 255, 0, 0, 100 };

    double accumulator = 0.0;
    uint64_t client_tick = 0;
    uint8_t net_buffer[1024];

    float ping_timer = 0.0f;
    double rtt_seconds = 0;

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
                )
            ) {
                if (in_header.type == PACKET_STATE) {
                    server_state_t* state = (server_state_t*)net_buffer;
                    server_pos.x = state->x;
                    server_pos.y = state->y;

                    // simple reconciliation snap
                    if(Vector2Distance(my_pos, server_pos) > 20.0f) {
                        puts("Applied Reconciliation!");
                        my_pos = server_pos;
                    }
                }

                if (in_header.type == PACKET_PING) {
                    clock_t end_time = clock();

                    ping_t* recv_p = (ping_t*)net_buffer;

                    rtt_seconds = (double)(end_time - recv_p->time) / CLOCKS_PER_SEC;

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
                rtt_seconds * 1000),
            10,
            30,
            20,
            GREEN
        );

        DrawRectangleV(
            server_pos,
            (Vector2) {PLAYER_SIZE, PLAYER_SIZE},
            ghost_color
        );
        DrawRectangleV(
            my_pos,
            (Vector2) {PLAYER_SIZE, PLAYER_SIZE}, 
            my_color
        );

        EndDrawing();
    }

    if (net_fd != -1) {
        Net_Close(net_fd);
    }
    CloseWindow();
    return 0;
}
