// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
#include "game/game.hpp"
#include "core/random.hpp"
#include "core/util.hpp"
#include <cmath>

namespace valor {

void Game::updateBullets(float dt) {
    for (auto& b : bullets) {
        if (!b.alive) continue;
        b.pos = Vector2Add(b.pos, Vector2Scale(b.vel, dt));
        b.life -= dt;
        if (b.life <= 0) { b.alive = false; continue; }

        if (b.splitT > 0) {
            b.splitT -= dt;
            if (b.splitT <= 0) {
                b.alive = false;
                for (int k = 0; k < 12; k++)
                    spawnBullet(b.pos, k * 2 * PI / 12, 170, 7, 1, false, false, ENEMY_BULLET, -1, true);
                burst(b.pos, 10, ENEMY_BULLET, 40, 200, 0.3f, 4);
                continue;
            }
        }

        if (dun.pointSolid(b.pos)) {
            b.alive = false;
            burst(b.pos, 3, b.col, 30, 120, 0.2f, 3);
            continue;
        }

        if (b.fromPlayer) {
            for (auto& e : enemies) {
                if (!e.alive || e.spawnT > 0) continue;
                if (b.pierce && e.lastPierce == b.id) continue;
                if (CheckCollisionCircles(b.pos, b.r, e.pos, e.r)) {
                    damageEnemy(e, b.dmg, b.vel);
                    if (b.pierce) e.lastPierce = b.id;
                    else { b.alive = false; break; }
                }
            }
        } else if (CheckCollisionCircles(b.pos, b.r * 0.8f, pl.pos, pl.r * 0.55f)) {
            if (pl.invuln <= 0 && pl.rollT <= ROLL_IFRAME_END) {
                b.alive = false;
                hurtPlayer();
                if (state != State::Playing) return;
            }
        }
    }
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b) { return !b.alive; }), bullets.end());
    bullets.insert(bullets.end(), pendingBullets.begin(), pendingBullets.end());
    pendingBullets.clear();
}

void Game::updatePickups(float dt) {
    for (auto& p : pickups) {
        p.t += dt;
        float d = Vector2Distance(p.pos, pl.pos);
        if (d < 90 && d > 1) p.pos = Vector2MoveTowards(p.pos, pl.pos, 160 * dt);
        if (d > pl.r + 14) continue;
        switch (p.type) {
            case PickType::Heart:
                if (pl.hp >= pl.maxHp) continue;
                pl.hp = std::min(pl.maxHp, pl.hp + 2);
                break;
            case PickType::Ammo:
                for (auto& g : pl.guns)
                    if (GUNS[g.def].maxAmmo > 0) g.ammo = std::min(GUNS[g.def].maxAmmo, g.ammo + GUNS[g.def].maxAmmo / 2);
                break;
            case PickType::Blank:
                pl.blanks = std::min(pl.blanks + 1, 9);
                break;
        }
        p.alive = false;
        sPickup.play();
        burst(p.pos, 12, pickupColor(p.type), 40, 160, 0.4f, 4);
    }
}

void Game::updateParticles(float dt) {
    for (auto& p : parts) {
        p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
        p.vel = Vector2Scale(p.vel, std::max(0.0f, 1.0f - 4.0f * dt));
        p.life -= dt;
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.life <= 0; }), parts.end());
}

}  // namespace valor
