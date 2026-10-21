// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
//  A top-down bullet-hell roguelike in C++17 + raylib.
//
//  Dodge-roll through bullet patterns, clear locked rooms, find guns in chests,
//  beat the Warden at the bottom of each floor, and descend. Five floors.
//  Everything (art and sound) is generated in code: no asset files.
//
//  Layout:
//    core/   constants, random, math helpers
//    world/  dungeon generation and collision
//    audio/  procedural sound effects
//    game/   game state and logic, split by topic
// ============================================================================

#include "raylib.h"
#include "game/game.hpp"
#include <algorithm>
#include <string>

using namespace valor;

int main(int argc, char** argv) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(CANVAS_W * 2, CANVAS_H * 2, "Valor at What Cost");
    // Open at the largest whole multiple of the 480x270 canvas that fits the monitor
    {
        int m = GetCurrentMonitor();
        int mw = GetMonitorWidth(m), mh = GetMonitorHeight(m);
        int scale = 2;
        while (CANVAS_W * (scale + 1) <= mw * 0.92f && CANVAS_H * (scale + 1) <= mh * 0.88f) scale++;
        if (mw > 0 && mh > 0) {
            SetWindowSize(CANVAS_W * scale, CANVAS_H * scale);
            SetWindowPosition((mw - CANVAS_W * scale) / 2, (mh - CANVAS_H * scale) / 2);
        }
    }
    SetWindowMinSize(CANVAS_W, CANVAS_H);
    SetExitKey(KEY_NULL);           // ESC is used for pause
    InitAudioDevice();
    SetTargetFPS(144);
    HideCursor();

    // Load guns, enemies, floors and rules from data/. If anything is wrong,
    // show the exact problem instead of starting with broken content.
    std::string dataError;
    if (!loadContent(dataError)) {
        TraceLog(LOG_ERROR, "DATA: %s", dataError.c_str());
        ShowCursor();
        while (!WindowShouldClose() && !IsKeyPressed(KEY_ESCAPE)) {
            BeginDrawing();
            ClearBackground({20, 12, 14, 255});
            DrawText("Could not load game data", 40, 40, 32, {255, 110, 110, 255});
            DrawText(dataError.c_str(), 40, 96, 20, RAYWHITE);
            DrawText("Fix the file and restart the game. ESC to quit.", 40, 150, 18, GRAY);
            EndDrawing();
        }
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }
    TraceLog(LOG_INFO, "DATA: loaded %d guns, %d enemies, %d floors from %s", (int)content().guns.size(),
             (int)content().enemies.size(), content().floorCount(), content().dir.c_str());

    static Game game;               // large object; keep it off the stack
    game.init();
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--debug") game.debug = true;

    while (!WindowShouldClose() && !game.quit) {
        float dt = std::min(GetFrameTime(), 1.0f / 30.0f);   // clamp so a hitch can't tunnel things through walls
        game.update(dt);
        BeginDrawing();
        game.draw();
        EndDrawing();
    }

    game.unload();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
