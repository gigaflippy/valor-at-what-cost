// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// Game content loaded from the JSON files in data/: guns, enemies, floors and
// balance rules. Code reads these through content(); nothing here is hardcoded.
#pragma once
#include "raylib.h"
#include "game/entities.hpp"
#include <string>
#include <vector>

namespace valor {

enum class GunSound { Pistol, Shotgun, Rail };

struct GunDef {
    std::string id, name;
    bool  starter = false;
    float fireDelay = 0.2f;
    int   mag = 1;
    int   maxAmmo = -1;          // reserve; -1 = infinite
    float reload = 1.0f;
    int   pellets = 1;
    float spread = 0;            // radians
    float speed = 600;
    float damage = 1;
    float radius = 5;
    bool  pierce = false;
    Color color = WHITE;
    GunSound sound = GunSound::Pistol;
    float volume = 1.0f;
    float shake = 0.03f;
    float length = 18;           // drawn barrel length
};

struct EnemyDef {
    std::string id, name;
    EType ai = EType::Shooter;
    Shape shape = Shape::Round;
    bool  boss = false;
    float radius = 15, hp = 10, hpPerFloor = 0, speed = 80;
    Color color = WHITE, enragedColor = RED;
    // spawning (regular enemies)
    int   spawnMinFloor = 1;
    float spawnChance = 0, spawnChancePerFloor = 0;
    bool  spawnFill = false;
    // movement
    float keepFar = 0, keepNear = 0, strafeMin = 1, strafeMax = 1, strafeSpeed = 0;
    float wanderMin = 1, wanderMax = 1, wanderSpeed = 0;
    // attack
    float fireMin = 1, fireMax = 1, bulletSpeed = 200, bulletRadius = 7;
    int   count = 1;
    float spreadStep = 0, aimJitter = 0;                 // radians
    float burstTime = 0, burstTick = 0.1f, burstTurn = 0; // spinner; burstTurn in radians
    int   arms = 4;
    // boss
    float enrageAt = 0.5f, spawnDelay = 0.9f;
};

struct Theme {
    std::string name;
    Color floorA, floorB, wall, wallTop, accent, bg;
};

struct Rules {
    float playerSpeed = 230, rollTime = 0.38f, rollSpeed = 470, rollIframeEnd = 0.08f, hitInvuln = 1.2f;
    int   halfHearts = 6, blanksPerFloor = 2, maxBlanks = 9, healOnDescend = 2;
    float fireRatePerFloor = 0.15f, firstShotMin = 0.6f, firstShotMax = 1.6f;
    int   waveMin = 3, waveMax = 5, waveBonusEvery = 2;
    float extraWaveChance = 0.25f, extraWaveChancePerFloor = 0.12f, waveDelay = 0.7f, spawnMinDistance = 260;
    float dropHeart = 0.22f, dropAmmo = 0.30f, dropBlank = 0.08f, enemyAmmoDrop = 0.06f;
    Color enemyBullet = {255, 80, 110, 255};
};

struct Content {
    std::vector<GunDef>   guns;
    std::vector<EnemyDef> enemies;
    std::vector<Theme>    floors;
    Rules rules;
    std::string dir;              // where the files were loaded from

    int floorCount() const { return (int)floors.size(); }
    int starterGun() const;
    int bossEnemy() const;
};

// The loaded content. Valid after a successful loadContent().
const Content& content();

// Finds the data/ folder and loads every file. On failure, returns false and
// leaves the previously loaded content untouched; `error` says what is wrong.
bool loadContent(std::string& error);

}  // namespace valor
