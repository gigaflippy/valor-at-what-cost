// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// The Game owns all run state. Its methods are split by topic across src/game/*.cpp.
#pragma once
#include "raylib.h"
#include "raymath.h"
#include "audio/sfx.hpp"
#include "core/config.hpp"
#include "game/content.hpp"
#include "game/entities.hpp"
#include "world/dungeon.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace valor {

enum class State { Title, Playing, Paused, Dead, Won };

struct Game {
    Dungeon dun;
    State state = State::Title;
    bool quit = false;
    bool debug = false, god = false;   // --debug: F1 god mode, F2 clear room, F3 jump to boss
    int floor = 1;

    // level
    int curRoom = -1;
    float waveDelay = 0;
    bool hatchActive = false;
    Vector2 hatchPos{};

    // entities
    Player pl;
    std::vector<Bullet>   bullets, pendingBullets;
    std::vector<Enemy>    enemies;
    std::vector<Particle> parts;
    std::vector<Pickup>   pickups;

    // presentation
    Camera2D cam{};
    float shake = 0, hurtFlash = 0, blankR = -1, eShotSfxT = 0;
    Vector2 blankPos{};
    std::string banner;
    float bannerT = 0;
    float titleT = 0;

    // stats
    int nextBulletId = 1, kills = 0;
    float runTime = 0;

    Sfx sShoot, sShotgun, sRail, sEShoot, sHit, sDie, sHurt, sBlank, sPickup, sRoll, sDoor, sChest, sEmpty, sBoom;

    const Theme& theme() const { return THEMES[std::clamp(floor - 1, 0, FINAL_FLOOR - 1)]; }

    // ------------------------------------------------------------ lifecycle
    void init();

    void unload();

    void newRun();

    void buildFloor();

    void showBanner(const std::string& s, float t) { banner = s; bannerT = t; }

    // Axis-separated movement so things slide along walls. Returns true if anything blocked.

    // ------------------------------------------------------------ effects
    void burst(Vector2 p, int n, Color c, float sMin, float sMax, float life, float size, float dirA = 0, float arc = PI);

    // ------------------------------------------------------------ update
    void update(float dt);

    void debugKeys();

    Gun& gun() { return pl.guns[pl.cur]; }
    const GunDef& gunDef() { return GUNS[gun().def]; }

    void updatePlayer(float dt);

    bool startReload();

    void finishReload();

    void spawnBullet(Vector2 pos, float ang, float speed, float r, float dmg, bool fromPlayer, bool pierce, Color col, float splitT = -1.0f, bool pending = false);

    void fire();

    void useBlank();

    void interact();

    void openChest(Room& r);

    // ------------------------------------------------------------ rooms & waves
    void updateRooms(float dt);

    void roomCleared();

    Enemy makeEnemy(EType t, Vector2 p);

    void spawnWave();

    void spawnBoss();

    // ------------------------------------------------------------ enemies
    void enemyShot(Vector2 from, float ang, float speed, float r, float splitT = -1.0f);

    void updateEnemies(float dt);

    void updateBoss(Enemy& e, float dt, float aimA, Vector2& mv);

    void damageEnemy(Enemy& e, float dmg, Vector2 vel);

    void hurtPlayer();

    // ------------------------------------------------------------ bullets / pickups / particles
    void updateBullets(float dt);

    void updatePickups(float dt);

    void updateParticles(float dt);

    // ============================================================ DRAWING
    static Color enemyColor(const Enemy& e);
    static Color pickupColor(PickType t);

    void draw();

    void drawTiles(const Camera2D& view);

    void drawRoomProps();

    void drawPickup(const Pickup& p);

    void drawEnemy(const Enemy& e);

    void drawPlayer();

    void drawBullets();

    // ------------------------------------------------------------ HUD
    void drawHeart(float cx, float cy, float s, int fill);

    void drawHUD();

    void drawMinimap(int sw);

    void drawOverlay(const char* title, const char* hint, Color accent);

    void drawCrosshair();

    void drawTitle();
};

}  // namespace valor
