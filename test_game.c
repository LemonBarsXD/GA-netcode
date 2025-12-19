#include <stdio.h>
#include <raylib.h>
#include <raymath.h>
#include "network.h"
#include "packet.h"

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 800
#define PLAYER_SPEED 200

#define PING_INTERVAL 1.0f

int main(int argc, char *argv[])
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, argv[0]);
    SetTargetFPS(60);

    int net_fd = Net_InitClient("127.0.0.1", 21337);
    if (net_fd == -1) {
        printf("failed to connect, is the server running? :-)\n");
    }

    Vector2 pos = { SCREEN_WIDTH/2, SCREEN_HEIGHT/2 };
    Vector2 size = { 40, 40 };
    Color color = { 255, 255, 255, 100 };

    Vector2 server_pos = pos; 
    Color ghost_color = { 255, 0, 0, 100 }; 

    float speed = PLAYER_SPEED;
    uint8_t net_buffer[1024];

    float ping_timer = 0.0f;
    double rtt_seconds = 0;

    while(!WindowShouldClose()) {


        float deltaTime = GetFrameTime();
        bool moved = false;

        ping_timer += deltaTime;

        if (ping_timer >= PING_INTERVAL) {
            printf("pinging...\n");
            Net_Ping(net_fd);

            ping_timer -= PING_INTERVAL; 
        }

        if(IsKeyDown(KEY_W)) { pos.y -= speed*deltaTime; moved = true; }
        if(IsKeyDown(KEY_S)) { pos.y += speed*deltaTime; moved = true; }
        if(IsKeyDown(KEY_A)) { pos.x -= speed*deltaTime; moved = true; }
        if(IsKeyDown(KEY_D)) { pos.x += speed*deltaTime; moved = true; }

        if (moved && net_fd != -1) {
            struct PacketVector2 pkt;
            pkt.x = pos.x;
            pkt.y = pos.y;
            Net_SendPacket(net_fd, PACKET_TYPE_MOVE, &pkt, sizeof(pkt));
        }

        if (net_fd != -1) {
            struct header in_header;
            // incoming packets THIS FRAME
            while(Net_ReceivePacket(net_fd, &in_header, net_buffer, 1024)) {
                if (in_header.type == PACKET_TYPE_MOVE) {
                    struct PacketVector2* ghost_update = (struct PacketVector2*)net_buffer;
                    server_pos.x = ghost_update->x;
                    server_pos.y = ghost_update->y;
                }
                if (in_header.type == PACKET_TYPE_PING) {
                    clock_t end_t = clock();

                    struct PacketEcho* recv_p = (struct PacketEcho*)net_buffer;

                    rtt_seconds = (double)(end_t - recv_p->time) / CLOCKS_PER_SEC;

                }
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        DrawText(
            TextFormat("ping: %.3f ms", rtt_seconds * 1000),
            SCREEN_WIDTH/2 - 30, 0, 20, WHITE
        );
        DrawRectangleV(server_pos, size, ghost_color);
        DrawRectangleV(pos, size, color);

        EndDrawing();
    }

    if (net_fd != -1) {
        Net_Close(net_fd);
    }

    CloseWindow();

    return 0;
}
