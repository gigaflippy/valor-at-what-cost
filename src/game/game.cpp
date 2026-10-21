// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
#include "game/game.hpp"
#include "core/random.hpp"
#include "core/util.hpp"
#include <cmath>

namespace valor {

void Game::init() {
    cam.zoom = 1.0f;
    cam.offset = {SCREEN_W / 2.0f, SCREEN_H / 2.0f};
    sShoot.load(900, 260, 0.07f, Osc::Square, 0.22f);
    sShotgun.load(2600, 300, 0.20f, Osc::Noise, 0.40f);
    sRail.load(1800, 120, 0.30f, Osc::Triangle, 0.45f);
    sEShoot.load(520, 340, 0.06f, Osc::Triangle, 0.18f, 6);
    sHit.load(240, 90, 0.07f, Osc::Square, 0.25f);
    sDie.load(1400, 80, 0.30f, Osc::Noise, 0.35f);
    sHurt.load(320, 50, 0.40f, Osc::Square, 0.45f, 2);
    sBlank.load(300, 3200, 0.50f, Osc::Noise, 0.40f, 2);
    sPickup.load(600, 1300, 0.12f, Osc::Triangle, 0.35f, 2);
    sRoll.load(700, 180, 0.12f, Osc::Noise, 0.15f, 2);
    sDoor.load(130, 70, 0.25f, Osc::Square, 0.30f, 2);
    sChest.load(400, 1000, 0.35f, Osc::Triangle, 0.40f, 2);
    sEmpty.load(1800, 1500, 0.03f, Osc::Square, 0.15f, 2);
    sBoom.load(500, 30, 0.80f, Osc::Noise, 0.55f, 2);
}

void Game::unload() {
    for (Sfx* s : {&sShoot, &sShotgun, &sRail, &sEShoot, &sHit, &sDie, &sHurt, &sBlank,
                   &sPickup, &sRoll, &sDoor, &sChest, &sEmpty, &sBoom})
        s->unload();
}

void Game::newRun() {
    floor = 1;
    pl = Player{};
    pl.guns = {{0, GUNS[0].mag, -1}};
    kills = 0;
    runTime = 0;
    buildFloor();
    state = State::Playing;
}

void Game::buildFloor() {
    dun.generate(floor);
    bullets.clear();
    pendingBullets.clear();
    enemies.clear();
    parts.clear();
    pickups.clear();
    hatchActive = false;
    waveDelay = 0;
    pl.pos = dun.roomCenter(0);
    pl.blanks = std::max(pl.blanks, 2);
    pl.rollT = 0;
    curRoom = 0;
    dun.rooms[0].visited = true;
    cam.target = pl.pos;
    showBanner(TextFormat("FLOOR %d  -  %s", floor, FLOOR_NAMES[floor - 1]), 3.0f);
}

void Game::burst(Vector2 p, int n, Color c, float sMin, float sMax, float life, float size, float dirA, float arc) {
    for (int i = 0; i < n; i++) {
        float a = dirA + frand(-arc, arc);
        float l = life * frand(0.6f, 1.2f);
        parts.push_back({p, fromAngle(a, frand(sMin, sMax)), l, l, size * frand(0.7f, 1.3f), c});
    }
}

void Game::update(float dt) {
    titleT += dt;
    switch (state) {
        case State::Title:
            if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) newRun();
            if (IsKeyPressed(KEY_ESCAPE)) quit = true;
            return;
        case State::Dead:
        case State::Won:
            updateParticles(dt);
            if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_ENTER)) newRun();
            if (IsKeyPressed(KEY_ESCAPE)) state = State::Title;
            return;
        case State::Paused:
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) state = State::Playing;
            if (IsKeyPressed(KEY_X)) state = State::Title;
            return;
        case State::Playing:
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) { state = State::Paused; return; }
            break;
    }

    if (debug) debugKeys();
    runTime += dt;
    eShotSfxT -= dt;
    updatePlayer(dt);
    if (state != State::Playing) return;      // descended to victory this frame
    updateRooms(dt);
    updateEnemies(dt);
    updateBullets(dt);
    updatePickups(dt);
    updateParticles(dt);

    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return !e.alive; }), enemies.end());
    pickups.erase(std::remove_if(pickups.begin(), pickups.end(), [](const Pickup& p) { return !p.alive; }), pickups.end());

    shake = std::max(0.0f, shake - dt * 2.5f);
    shake = std::min(shake, 1.2f);
    hurtFlash = std::max(0.0f, hurtFlash - dt * 2.0f);
    bannerT -= dt;
    if (blankR >= 0) { blankR += dt * 1500.0f; if (blankR > 1000.0f) blankR = -1; }

    // Camera leads toward the cursor a little, like most twin-stick shooters
    Vector2 mouseW = GetScreenToWorld2D(GetMousePosition(), cam);
    Vector2 look = Vector2Add(pl.pos, Vector2Scale(Vector2Subtract(mouseW, pl.pos), 0.22f));
    cam.target = Vector2Lerp(cam.target, look, 1.0f - expf(-10.0f * dt));
}

void Game::debugKeys() {
    if (IsKeyPressed(KEY_F1)) { god = !god; showBanner(god ? "GOD MODE ON" : "GOD MODE OFF", 1.0f); }
    if (IsKeyPressed(KEY_F2))
        for (auto& e : enemies) if (e.alive) { e.spawnT = 0; damageEnemy(e, 1e6f, {1, 0}); }
    if (IsKeyPressed(KEY_F3))
        for (int i = 0; i < (int)dun.rooms.size(); i++)
            if (dun.rooms[i].type == RoomType::Boss) {
                pl.pos = Vector2Add(dun.roomCenter(i), {0, TILE * 4});
                cam.target = pl.pos;
            }
}

}  // namespace valor
