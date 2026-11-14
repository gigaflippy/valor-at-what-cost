// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// Game feel: debris that stays on the floor (shell casings, enemy remains, shards)
// and breakable props (pots, crates).
#include "game/game.hpp"
#include "core/random.hpp"
#include "core/util.hpp"
#include <cmath>

namespace valor {

namespace {
constexpr size_t MAX_DEBRIS = 600;
constexpr float PROP_R = 9.0f;      // touch radius of a prop
}

void Game::addDebris(Vector2 pos, Vector2 vel, float z, float vz, Color col, DebrisKind kind) {
    if (debris.size() >= MAX_DEBRIS) debris.erase(debris.begin(), debris.begin() + MAX_DEBRIS / 6);   // oldest first
    debris.push_back({pos, vel, z, vz, col, kind, kind == DebrisKind::Stain});
}

// One spent casing per shot, thrown out to the side of the gun
void Game::spawnCasing() {
    const GunDef& d = gunDef();
    Color col = d.sound == GunSound::Shotgun ? Color{190, 60, 50, 255}
              : d.sound == GunSound::Rail    ? Color{120, 180, 220, 255} : Color{214, 170, 74, 255};
    float side = pl.aim + (cosf(pl.aim) >= 0 ? -1.0f : 1.0f) * (PI / 2 + frand(-0.3f, 0.5f));
    addDebris(handPos(), fromAngle(side, frand(50, 110)), 14.0f, frand(120, 190), col, DebrisKind::Casing);
}

// A dead enemy leaves a dark stain and a few pieces in its own colour
void Game::spawnRemains(const Enemy& e) {
    bool boss = e.type == EType::Warden;
    Color c = mix(enemyColor(e), BLACK, 0.25f);
    int stains = boss ? 4 : 1, bits = boss ? 26 : 6;
    for (int i = 0; i < stains; i++)
        addDebris({e.pos.x + (boss ? frand(-30, 30) : 0.0f), e.pos.y + e.r * 0.6f + (boss ? frand(-20, 20) : 0.0f)}, {0, 0}, 0, 0,
                  BLACK, DebrisKind::Stain);
    for (int i = 0; i < bits; i++)
        addDebris(e.pos, fromAngle(frand(0, 2 * PI), frand(40, boss ? 320.0f : 190.0f)), frand(4, 12), frand(90, 230),
                  chance(0.3f) ? mix(c, WHITE, 0.3f) : c, DebrisKind::Bit);
}

// Pots and crates along the edges of every room except the boss room
void Game::placeProps() {
    props.clear();
    for (int ri = 0; ri < (int)dun.rooms.size(); ri++) {
        const Room& r = dun.rooms[ri];
        if (r.type == RoomType::Boss) continue;
        int x0 = r.gx * ROOM_W, y0 = r.gy * ROOM_H;
        Vector2 centre = dun.roomCenter(ri);
        int groups = r.type == RoomType::Combat ? irand(3, 6) : irand(2, 3);
        for (int g = 0, tries = 0; g < groups && tries < 40; tries++) {
            // A tile on the edge of the room, clear of the door lanes
            bool horizontal = chance(0.6f);
            int tx = horizontal ? irand(1, ROOM_W - 2) : (chance(0.5f) ? 1 : ROOM_W - 2);
            int ty = horizontal ? (chance(0.5f) ? 1 : ROOM_H - 2) : irand(1, ROOM_H - 2);
            if (abs(tx - ROOM_W / 2) <= 2 || abs(ty - ROOM_H / 2) <= 2) continue;
            if (dun.tileAt(x0 + tx, y0 + ty) != Tile::Floor) continue;
            Vector2 base{(x0 + tx + 0.5f) * TILE, (y0 + ty + 0.5f) * TILE};
            if (Vector2Distance(base, centre) < 120) continue;
            PropKind kind = chance(0.55f) ? PropKind::Pot : PropKind::Crate;
            int n = irand(1, 3);
            for (int k = 0; k < n; k++)
                props.push_back({{base.x + (k - (n - 1) / 2.0f) * 15.0f + frand(-2, 2), base.y + frand(-7, 7)},
                                 chance(0.8f) ? kind : (kind == PropKind::Pot ? PropKind::Crate : PropKind::Pot)});
            g++;
        }
    }
}

void Game::breakProp(Prop& p) {
    p.alive = false;
    bool pot = p.kind == PropKind::Pot;
    Color c = pot ? Color{176, 112, 78, 255} : Color{140, 96, 58, 255};
    for (int i = 0; i < (pot ? 6 : 8); i++)
        addDebris({p.pos.x, p.pos.y - 8}, fromAngle(frand(0, 2 * PI), frand(30, 150)), frand(4, 14), frand(80, 200),
                  mix(c, chance(0.5f) ? BLACK : WHITE, frand(0.0f, 0.3f)), DebrisKind::Shard);
    burst({p.pos.x, p.pos.y - 8}, 6, {170, 160, 150, 120}, 20, 90, 0.35f, 4);
    sHit.play(pot ? frand(1.5f, 1.9f) : frand(0.55f, 0.7f), 0.45f);
    shake += 0.03f;
}

void Game::updateFeel(float dt) {
    // Debris: fly, bounce off the floor and walls, then lie still
    for (auto& d : debris) {
        if (d.rest) continue;
        Vector2 next = Vector2Add(d.pos, Vector2Scale(d.vel, dt));
        if (dun.pointSolid(next)) d.vel = Vector2Scale(d.vel, -0.4f);
        else d.pos = next;
        d.z += d.vz * dt;
        d.vz -= 900.0f * dt;
        if (d.z <= 0) {
            d.z = 0;
            if (d.vz < -70.0f) { d.vz *= -0.4f; d.vel = Vector2Scale(d.vel, 0.5f); }
            else d.rest = true;
        }
    }

    // Props break when the player touches them or any bullet hits them
    for (auto& p : props) {
        if (!p.alive || fabsf(p.pos.x - pl.pos.x) > 700 || fabsf(p.pos.y - pl.pos.y) > 450) continue;
        Vector2 body{p.pos.x, p.pos.y - 8};
        if (CheckCollisionCircles(pl.pos, pl.r, body, PROP_R)) { breakProp(p); continue; }
        for (auto& b : bullets) {
            if (!b.alive || !CheckCollisionCircles(b.pos, b.r, body, PROP_R)) continue;
            if (!b.pierce) b.alive = false;
            breakProp(p);
            break;
        }
    }
    props.erase(std::remove_if(props.begin(), props.end(), [](const Prop& p) { return !p.alive; }), props.end());
}

}  // namespace valor
