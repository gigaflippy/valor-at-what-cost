// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// Content tables: guns, floor themes. (Phase 0 moves these into data files.)
#pragma once
#include "raylib.h"
#include "core/config.hpp"

namespace valor {

struct GunDef {
    const char* name;
    float fireDelay;
    int   mag;
    int   maxAmmo;      // reserve; -1 = infinite
    float reload;
    int   pellets;
    float spread;       // radians
    float speed;
    float damage;
    float radius;
    bool  pierce;
    Color color;
};

inline const GunDef GUNS[] = {
    {"Rusty Sidearm", 0.22f,  8,  -1, 0.9f, 1, 0.04f,  720.0f,  6.0f, 5.0f, false, {255, 220, 120, 255}},
    {"Scattergun",    0.65f,  6,  60, 1.3f, 6, 0.38f,  620.0f,  4.5f, 4.5f, false, {255, 160,  80, 255}},
    {"Riveter",       0.07f, 32, 320, 1.4f, 1, 0.12f,  820.0f,  3.2f, 4.0f, false, {190, 255, 130, 255}},
    {"Railspike",     0.90f,  4,  36, 1.6f, 1, 0.00f, 1300.0f, 28.0f, 6.0f, true,  {140, 220, 255, 255}},
};
constexpr int GUN_COUNT = sizeof(GUNS) / sizeof(GUNS[0]);

struct Theme { Color floorA, floorB, wall, wallTop, accent, bg; };
inline const Theme THEMES[FINAL_FLOOR] = {
    {{46, 44, 56, 255}, {50, 48, 61, 255}, {62, 56, 74, 255}, {108, 100, 126, 255}, {255, 200,  90, 255}, {14, 13, 18, 255}},
    {{34, 46, 42, 255}, {38, 51, 46, 255}, {48, 66, 58, 255}, { 88, 118, 102, 255}, {150, 255, 170, 255}, {10, 15, 13, 255}},
    {{52, 42, 34, 255}, {57, 46, 37, 255}, {76, 60, 46, 255}, {130, 106,  82, 255}, {255, 170,  90, 255}, {16, 12,  9, 255}},
    {{56, 32, 32, 255}, {62, 36, 35, 255}, {84, 44, 40, 255}, {148,  80,  68, 255}, {255, 120,  80, 255}, {18,  8,  8, 255}},
    {{28, 30, 52, 255}, {32, 34, 58, 255}, {46, 44, 86, 255}, { 90,  86, 148, 255}, {170, 150, 255, 255}, { 8,  8, 18, 255}},
};
inline const char* const FLOOR_NAMES[FINAL_FLOOR] = {
    "The Breach Halls", "The Rusted Gutters", "The Cinder Mines", "The Hollow Forge", "The Gunmetal Abyss"};

inline const Color ENEMY_BULLET = {255, 80, 110, 255};

}  // namespace valor
