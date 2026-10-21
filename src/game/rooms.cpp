// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
#include "game/game.hpp"
#include "core/random.hpp"
#include "core/util.hpp"
#include <cmath>

namespace valor {

void Game::updateRooms(float dt) {
    int ri = dun.roomIndexAt(pl.pos);
    if (ri >= 0 && ri != curRoom) { curRoom = ri; dun.rooms[ri].visited = true; }
    if (curRoom < 0) return;
    Room& r = dun.rooms[curRoom];

    // Lock the doors once the player is properly inside an uncleared room
    if (!r.cleared && !r.locked && (r.type == RoomType::Combat || r.type == RoomType::Boss)) {
        Rectangle in = dun.roomInterior(curRoom);
        float m = TILE * 0.6f + pl.r;
        if (pl.pos.x > in.x + m && pl.pos.x < in.x + in.width - m && pl.pos.y > in.y + m && pl.pos.y < in.y + in.height - m) {
            r.locked = true;
            sDoor.play();
            shake += 0.15f;
            if (r.type == RoomType::Boss) {
                spawnBoss();
                r.wavesLeft = 0;
                showBanner("THE WARDEN", 2.5f);
            } else {
                r.wavesLeft = chance(0.25f + 0.12f * floor) ? 1 : 0;
                spawnWave();
            }
        }
    }

    if (r.locked) {
        bool anyAlive = std::any_of(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.alive; });
        if (!anyAlive) {
            if (r.wavesLeft > 0) {
                waveDelay += dt;
                if (waveDelay > 0.7f) { waveDelay = 0; r.wavesLeft--; spawnWave(); }
            } else {
                r.locked = false;
                r.cleared = true;
                sDoor.play(0.8f);
                roomCleared();
            }
        }
    }
}

void Game::roomCleared() {
    Room& r = dun.rooms[curRoom];
    Vector2 c = dun.roomCenter(curRoom);
    for (auto& b : bullets)
        if (!b.fromPlayer) b.alive = false;
    if (r.type == RoomType::Boss) {
        hatchActive = true;
        hatchPos = c;
        pickups.push_back({{c.x, c.y + 70}, PickType::Heart});
        pickups.push_back({{c.x - 50, c.y + 70}, PickType::Ammo});
        showBanner(floor == FINAL_FLOOR ? "The Warden falls. Take the hatch to escape!"
                                        : "Floor cleared! Press E at the hatch to descend.", 3.5f);
        return;
    }
    float roll = frand(0, 1);
    if (roll < 0.22f)      pickups.push_back({c, PickType::Heart});
    else if (roll < 0.52f) pickups.push_back({c, PickType::Ammo});
    else if (roll < 0.60f) pickups.push_back({c, PickType::Blank});
}

Enemy Game::makeEnemy(EType t, Vector2 p) {
    Enemy e;
    e.type = t;
    e.pos = p;
    float hpScale = 1.0f + 0.22f * (floor - 1);
    switch (t) {
        case EType::Grunt:   e.r = 15; e.hp = 16; e.speed = 85; break;
        case EType::Scatter: e.r = 17; e.hp = 26; e.speed = 60; break;
        case EType::Whirl:   e.r = 18; e.hp = 34; e.speed = 45; break;
        case EType::Warden:  e.r = 42; e.hp = 560; e.speed = 70; hpScale = 1.0f + 0.35f * (floor - 1); break;
    }
    e.hp *= hpScale;
    e.maxHp = e.hp;
    e.shootT = frand(0.6f, 1.6f);
    e.ang = frand(0, 2 * PI);
    return e;
}

void Game::spawnWave() {
    Room& r = dun.rooms[curRoom];
    int x0 = r.gx * ROOM_W, y0 = r.gy * ROOM_H;
    int n = irand(3, 5) + (floor - 1) / 2;
    for (int i = 0; i < n; i++) {
        Vector2 p{};
        bool ok = false;
        for (int tries = 0; tries < 80 && !ok; tries++) {
            int tx = irand(x0 + 2, x0 + ROOM_W - 3), ty = irand(y0 + 2, y0 + ROOM_H - 3);
            p = {(tx + 0.5f) * TILE, (ty + 0.5f) * TILE};
            ok = !dun.circleSolid(p, 22) && Vector2Distance(p, pl.pos) > 260;
        }
        if (!ok) continue;
        float whirlP = floor >= 2 ? 0.12f + 0.05f * floor : 0.0f;
        float roll = frand(0, 1);
        EType t = roll < whirlP ? EType::Whirl : roll < whirlP + 0.32f ? EType::Scatter : EType::Grunt;
        enemies.push_back(makeEnemy(t, p));
    }
}

void Game::spawnBoss() {
    Vector2 c = dun.roomCenter(curRoom);
    Enemy e = makeEnemy(EType::Warden, {c.x, c.y - TILE * 3});
    e.spawnT = 1.4f;
    enemies.push_back(e);
}

}  // namespace valor
