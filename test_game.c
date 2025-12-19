#include <stdio.h>
#include <raylib.h>
#include <raymath.h>
#include "network.h"
#include "packet.h"

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 800
#define PLAYER_SPEED 200

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "test game");
    SetTargetFPS(60);

    int net_fd = Net_InitClient("127.0.0.1", 21337);
    if (net_fd == -1) printf("Network failed to connect (is server running?)\n");

    Vector2 pos = { SCREEN_WIDTH/2, SCREEN_HEIGHT/2 };
    Vector2 size = { 40, 40 };
    Color color = { 255, 255, 255, 255 };

    Vector2 server_pos = pos; 
    Color ghost_color = { 255, 0, 0, 100 }; 

    float speed = PLAYER_SPEED;
    uint8_t net_buffer[1024];

    while(!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        bool moved = false;

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
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

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
