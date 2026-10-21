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

// Player stats, difficulty and drop rates live in data/rules.json.

constexpr int DX[4] = {1, 0, -1, 0};        // right, down, left, up
constexpr int DY[4] = {0, 1, 0, -1};

}  // namespace valor
