#include <stdio.h>
#include <unistd.h>
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

    int net_fd = net_initclient("127.0.0.1", 21337);
    if (net_fd == -1) {
        printf("Failed to connect, is the server running? :-)\n");
    }

    printf("Connected! Sending connect packet...\n");
    net_handshake_t hs = {0};
    hs.version = VERSION;
    net_sendpacket(
        net_fd,
        PACKET_CONNECT,
        &hs,
        sizeof(hs)
    );
    printf("Connect packet sent (%zu bytes)\n", sizeof(net_handshake_t) + sizeof(header_t));

    client_t my_client = {
        .state.entindex=-1,
        .active=1,
        .fd=net_fd,
        .recv_buf_len = 0
    };

    Vector2 my_pos = { SCREEN_WIDTH/2, SCREEN_HEIGHT/2 };
    Vector2 my_server_ghost = my_pos;
    remote_player_t other_players[MAX_PLAYERS] = {0};

    Color my_color = { 255, 255, 255, 100 };
    Color ghost_color = { 255, 0, 0, 100 };
    Color others_color = { 0, 126, 126, 100 };

    double accumulator = 0.0;
    uint64_t client_tick = 0;
    uint8_t net_buffer[1024];

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
            net_ping(net_fd);

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
            net_sendpacket(
                net_fd,
                PACKET_USERINPUT,
                &cmd,
                sizeof(cmd)
            );

            // decrease accumulator
            accumulator -= TICK_DELTA;
        }


        if (net_fd != -1) {

            // handle incoming packets
            header_t in_header;
            int res;

            while((res = net_recvpacket(&my_client, &in_header, net_buffer, 1024)) == 1) {
                switch(in_header.type) {
                    case PACKET_CONNECT:
                        {
                            if(in_header.data_size == sizeof(net_handshake_t)) {
                                net_handshake_t* hs = (net_handshake_t*)net_buffer;
                                my_client.state.entindex = hs->entindex;
                                printf(
                                    "Handshake comlete! \nMy ID: %d \nGame version: %d\n",
                                    hs->entindex,
                                    hs->version
                                );
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
                            sv_state_t* state = (sv_state_t*)net_buffer;

                            int id = (int)state->entindex;

                            if (id < 0 || id >= MAX_PLAYERS) continue;

                            if(id == (int)my_client.state.entindex) {
                                my_server_ghost.x = state->x;
                                my_server_ghost.y = state->y;

                                // simple reconciliation
                                float dist = Vector2Distance(my_pos, my_server_ghost);
                                if(dist > 20.0f) {
                                    printf("Reconciliation dist: %f\n", dist);
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

                    case PACKET_FULL_STATE:
                        {
                            sv_full_state_t* full_state = (sv_full_state_t*)net_buffer;


                            for(int i = 0; i < (int)full_state->num_states; ++i) {

                                int id = full_state->states[i].entindex;

                                if (id < 0 || id >= MAX_PLAYERS) continue;

                                if(id == (int)my_client.state.entindex) {
                                    my_server_ghost.x = full_state->states[i].x;
                                    my_server_ghost.y = full_state->states[i].y;

                                    // simple reconciliation
                                    float dist = Vector2Distance(my_pos, my_server_ghost);
                                    if(dist > 20.0f) {
                                        printf("Reconciliation to: %f\n", dist);
                                        my_pos = my_server_ghost;
                                    }
                                }
                                else {
                                    other_players[id].active = 1;
                                    other_players[id].pos.x = full_state->states[i].x;
                                    other_players[id].pos.y = full_state->states[i].y;
                                }
                            }
                            break;
                        }

                    case PACKET_PING:
                        {
                            ping_t* recv_p = (ping_t*)net_buffer;
                            struct timespec now;
                            clock_gettime(CLOCK_MONOTONIC, &now);
                            uint64_t current_time_ns = TIMESPEC_TO_NSEC(now);
                            rtt_ms = (current_time_ns - recv_p->time) / 1000000.0f;
                            break;
                        }
                }

                if (res == -1 || res == -2) {
                    printf("Server disconnected (error/invalid packet)\n");
                    break;
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
            if(other_players[i].active && i != (int)my_client.state.entindex) {
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

    net_disconnect_t dc = {.entindex=my_client.state.entindex, .reason=1};
    net_sendpacket(net_fd, PACKET_DISCONNECT, &dc, sizeof(dc));
    usleep(10000);

    if (net_fd != -1) {
        net_close(net_fd);
    }
    CloseWindow();
    return 0;
}
