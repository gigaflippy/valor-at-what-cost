// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// Plain data for everything that moves.
#pragma once
#include "raylib.h"
#include <vector>

namespace valor {

enum class EType { Grunt, Scatter, Whirl, Warden };
enum class PickType { Heart, Ammo, Blank };

struct Gun { int def; int mag; int ammo; };

struct Player {
    Vector2 pos{};
    float r = 13.0f;
    int hp = 6, maxHp = 6;          // half-hearts
    int blanks = 2;
    float rollT = 0, rollCd = 0, invuln = 0;
    Vector2 rollDir{1, 0};
    std::vector<Gun> guns;
    int cur = 0;
    float fireCd = 0, reloadT = 0, recoil = 0, emptyT = 0;
    float aim = 0;
};

struct Bullet {
    Vector2 pos{}, vel{};
    float r = 6, dmg = 1;
    bool fromPlayer = false, pierce = false;
    int id = 0;
    float life = 6.0f;
    float splitT = -1.0f;           // > 0: bursts into a ring when it reaches 0
    Color col = WHITE;
    bool alive = true;
};

struct Enemy {
    EType type = EType::Grunt;
    Vector2 pos{};
    float r = 15, hp = 10, maxHp = 10, speed = 80;
    float shootT = 1, spawnT = 0.9f, flash = 0, moveT = 0;
    float burstT = 0, tick = 0, ang = 0, spin = 0;
    Vector2 dir{1, 0};
    int lastPierce = -1;
    bool alive = true;
    // boss state
    int pattern = -1, lastPattern = -1, count = 0;
    float patT = 0, restT = 1.6f, spiral = 0;
    bool enraged = false;
};

struct Particle { Vector2 pos, vel; float life, maxLife, size; Color col; };
struct Pickup   { Vector2 pos; PickType type; float t = 0; bool alive = true; };

}  // namespace valor
