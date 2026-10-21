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
    Camera2D cam{};                 // target = world point at the centre of the screen

    // pixel-art renderer (render.cpp)
    RenderTexture2D canvas{}, lightMap{};
    Texture2D lightTex{};
    Texture2D playerSprite{};        // data/sprites/deserter.png; id 0 = not found, shapes are drawn instead
    bool gfxReady = false;
    float pixelScale = 1;            // screen pixels per canvas pixel
    Vector2 canvasPos{};             // screen position of the canvas' top-left corner
    std::vector<Vector2> braziers;   // wall braziers on this floor (world positions)
    float muzzleT = 0;
    Vector2 muzzlePos{};
    Color muzzleCol = WHITE;
    float shake = 0, hurtFlash = 0, blankR = -1, eShotSfxT = 0;
    Vector2 blankPos{};
    std::string banner;
    float bannerT = 0;
    float titleT = 0;

    // stats
    int nextBulletId = 1, kills = 0;
    float runTime = 0;

    Sfx sShoot, sShotgun, sRail, sEShoot, sHit, sDie, sHurt, sBlank, sPickup, sRoll, sDoor, sChest, sEmpty, sBoom;

    const Theme& theme() const { return content().floors[std::clamp(floor - 1, 0, content().floorCount() - 1)]; }
    const EnemyDef& def(const Enemy& e) const { return content().enemies[e.def]; }

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

    // Where the gun is held: a little above the centre of the player's collision circle
    Vector2 handPos() const { return {pl.pos.x, pl.pos.y - 6}; }
    Gun& gun() { return pl.guns[pl.cur]; }
    const GunDef& gunDef() { return content().guns[gun().def]; }

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

    Enemy makeEnemy(int defIndex, Vector2 p);

    int pickEnemy();
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
    Color enemyColor(const Enemy& e) const;
    static Color pickupColor(PickType t);

    void draw();

    void initGraphics();
    void unloadGraphics();
    void layoutCanvas();
    Vector2 screenToWorld(Vector2 s) const;
    Vector2 worldToScreen(Vector2 w) const;
    void placeBraziers();
    void emitEmbers(float dt);
    void drawFloor(int x0, int y0, int x1, int y1);
    void drawWallTile(int x, int y);
    void drawWorldSorted(int x0, int y0, int x1, int y1);
    void drawLights(const Camera2D& view);
    void drawEmissive();
    void drawPrompts();


    void drawPickup(const Pickup& p, int pass);
    void drawChest(int room, int pass);

    void drawEnemy(const Enemy& e, int pass);

    void drawPlayer(int pass);

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
