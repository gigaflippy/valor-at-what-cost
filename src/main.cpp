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
    InitWindow(SCREEN_W, SCREEN_H, "Valor at What Cost");
    SetWindowMinSize(960, 640);
    SetExitKey(KEY_NULL);           // ESC is used for pause
    InitAudioDevice();
    SetTargetFPS(144);
    HideCursor();

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
