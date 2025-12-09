#include <stdio.h>
#include <raylib.h>
#include <raymath.h>

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 800

#define PLAYER_SPEED 200

int main(void) {

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "test game");
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));

    Vector2 pos;
    pos.x = SCREEN_WIDTH/ 2;
    pos.y = SCREEN_HEIGHT / 2;

    Vector2 size = { 40, 40 };
    Color color = { 255,255,255,255 };

    float speed = PLAYER_SPEED;

    while(!WindowShouldClose()) {
        BeginDrawing();

        float deltaTime = GetFrameTime();

        if(IsKeyDown(KEY_W)) pos.y -= speed*deltaTime;
        if(IsKeyDown(KEY_S)) pos.y += speed*deltaTime;
        if(IsKeyDown(KEY_A)) pos.x -= speed*deltaTime;
        if(IsKeyDown(KEY_D)) pos.x += speed*deltaTime;

        DrawRectangleV( pos, size, color );

        ClearBackground(BLACK);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
