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
                showBanner(upper(content().enemies[content().bossEnemy()].name), 2.5f);
            } else {
                const Rules& rl = content().rules;
                r.wavesLeft = chance(rl.extraWaveChance + rl.extraWaveChancePerFloor * floor) ? 1 : 0;
                spawnWave();
            }
        }
    }

    if (r.locked) {
        bool anyAlive = std::any_of(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.alive; });
        if (!anyAlive) {
            if (r.wavesLeft > 0) {
                waveDelay += dt;
                if (waveDelay > content().rules.waveDelay) { waveDelay = 0; r.wavesLeft--; spawnWave(); }
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
        showBanner(floor == content().floorCount() ? "The Warden falls. Take the hatch to escape!"
                                        : "Floor cleared! Press E at the hatch to descend.", 3.5f);
        return;
    }
    const Rules& rl = content().rules;
    float roll = frand(0, 1);
    if (roll < rl.dropHeart)                                  pickups.push_back({c, PickType::Heart});
    else if (roll < rl.dropHeart + rl.dropAmmo)               pickups.push_back({c, PickType::Ammo});
    else if (roll < rl.dropHeart + rl.dropAmmo + rl.dropBlank) pickups.push_back({c, PickType::Blank});
}

Enemy Game::makeEnemy(int defIndex, Vector2 p) {
    const EnemyDef& d = content().enemies[defIndex];
    Enemy e;
    e.def = defIndex;
    e.type = d.ai;
    e.shape = d.shape;
    e.pos = p;
    e.r = d.radius;
    e.speed = d.speed;
    e.hp = d.hp * (1.0f + d.hpPerFloor * (floor - 1));
    e.maxHp = e.hp;
    e.spawnT = d.spawnDelay;
    e.shootT = frand(content().rules.firstShotMin, content().rules.firstShotMax);
    e.ang = frand(0, 2 * PI);
    return e;
}

// Rolls a regular enemy for this floor: each listed enemy (in data order) gets
// chance + chancePerFloor * floor; the "fill" enemy takes what is left.
int Game::pickEnemy() {
    const auto& defs = content().enemies;
    float roll = frand(0, 1), acc = 0;
    int fill = 0;
    for (int i = 0; i < (int)defs.size(); i++) {
        const EnemyDef& d = defs[i];
        if (d.boss) continue;
        if (d.spawnFill) { fill = i; continue; }
        if (floor < d.spawnMinFloor) continue;
        acc += d.spawnChance + d.spawnChancePerFloor * floor;
        if (roll < acc) return i;
    }
    return fill;
}

void Game::spawnWave() {
    Room& r = dun.rooms[curRoom];
    int x0 = r.gx * ROOM_W, y0 = r.gy * ROOM_H;
    const Rules& rl = content().rules;
    int n = irand(rl.waveMin, rl.waveMax) + (floor - 1) / rl.waveBonusEvery;
    for (int i = 0; i < n; i++) {
        Vector2 p{};
        bool ok = false;
        for (int tries = 0; tries < 80 && !ok; tries++) {
            int tx = irand(x0 + 2, x0 + ROOM_W - 3), ty = irand(y0 + 2, y0 + ROOM_H - 3);
            p = {(tx + 0.5f) * TILE, (ty + 0.5f) * TILE};
            ok = !dun.circleSolid(p, 22) && Vector2Distance(p, pl.pos) > rl.spawnMinDistance;
        }
        if (!ok) continue;
        enemies.push_back(makeEnemy(pickEnemy(), p));
    }
}

void Game::spawnBoss() {
    Vector2 c = dun.roomCenter(curRoom);
    enemies.push_back(makeEnemy(content().bossEnemy(), {c.x, c.y - TILE * 3}));
}

}  // namespace valor
