// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// Global tuning constants.
#pragma once

namespace valor {

constexpr int   SCREEN_W     = 1280;
constexpr int   SCREEN_H     = 720;
constexpr float TILE         = 40.0f;
constexpr int   ROOM_W       = 23;          // tiles, including the wall border
constexpr int   ROOM_H       = 15;
constexpr int   GRID         = 7;           // floor layout is a GRID x GRID grid of room slots
constexpr int   MAP_W        = GRID * ROOM_W;
constexpr int   MAP_H        = GRID * ROOM_H;
constexpr int   FINAL_FLOOR  = 5;

constexpr float PLAYER_SPEED = 230.0f;
constexpr float ROLL_TIME    = 0.38f;
constexpr float ROLL_SPEED   = 470.0f;
constexpr float ROLL_IFRAME_END = 0.08f;    // i-frames for all of the roll except the last 0.08s
constexpr float HIT_INVULN   = 1.2f;

constexpr int DX[4] = {1, 0, -1, 0};        // right, down, left, up
constexpr int DY[4] = {0, 1, 0, -1};

}  // namespace valor
