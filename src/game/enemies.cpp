// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
#include "game/game.hpp"
#include "core/random.hpp"
#include "core/util.hpp"
#include <cmath>

namespace valor {

void Game::enemyShot(Vector2 from, float ang, float speed, float r, float splitT) {
    spawnBullet(Vector2Add(from, fromAngle(ang, 10)), ang, speed, r, 1, false, false, content().rules.enemyBullet, splitT);
    if (eShotSfxT <= 0) { sEShoot.play(frand(0.9f, 1.1f), 0.7f); eShotSfxT = 0.06f; }
}

void Game::updateEnemies(float dt) {
    const float diff = 1.0f + content().rules.fireRatePerFloor * (floor - 1);
    for (auto& e : enemies) {
        if (!e.alive) continue;
        e.flash = std::max(0.0f, e.flash - dt * 6.0f);
        if (e.spawnT > 0) { e.spawnT -= dt; continue; }

        Vector2 toP = Vector2Subtract(pl.pos, e.pos);
        float dist = Vector2Length(toP);
        Vector2 dirP = dist > 0.001f ? Vector2Scale(toP, 1.0f / dist) : Vector2{1, 0};
        float aimA = atan2f(toP.y, toP.x);
        Vector2 mv{0, 0};
        e.moveT -= dt;

        const EnemyDef& d = def(e);
        switch (e.type) {
            // Keeps between keepNear and keepFar from the player, strafing in between,
            // and fires a fan of `count` bullets (count 1 = single aimed shot).
            case EType::Shooter: {
                if (d.strafeSpeed > 0 && e.moveT <= 0) {
                    e.moveT = frand(d.strafeMin, d.strafeMax);
                    e.dir = chance(0.5f) ? Vector2{-dirP.y, dirP.x} : Vector2{dirP.y, -dirP.x};
                }
                if (dist > d.keepFar) mv = dirP;
                else if (dist < d.keepNear) mv = Vector2Scale(dirP, -1);
                else mv = Vector2Scale(e.dir, d.strafeSpeed);
                e.shootT -= dt * diff;
                if (e.shootT <= 0) {
                    e.shootT = frand(d.fireMin, d.fireMax);
                    float jitter = d.aimJitter > 0 ? frand(-d.aimJitter, d.aimJitter) : 0.0f;
                    for (int k = 0; k < d.count; k++)
                        enemyShot(e.pos, aimA + (k - (d.count - 1) / 2.0f) * d.spreadStep + jitter, d.bulletSpeed, d.bulletRadius);
                }
            } break;

            // Wanders, then fires a rotating burst of `arms` streams.
            case EType::Spinner: {
                if (e.moveT <= 0) { e.moveT = frand(d.wanderMin, d.wanderMax); e.dir = fromAngle(frand(0, 2 * PI)); }
                mv = Vector2Scale(e.dir, d.wanderSpeed);
                if (e.burstT > 0) {
                    e.burstT -= dt;
                    e.tick -= dt;
                    if (e.tick <= 0) {
                        e.tick = d.burstTick;
                        for (int k = 0; k < d.arms; k++) enemyShot(e.pos, e.ang + k * 2 * PI / d.arms, d.bulletSpeed, d.bulletRadius);
                        e.ang += d.burstTurn;
                    }
                    if (e.burstT <= 0) e.shootT = frand(d.fireMin, d.fireMax);
                } else {
                    e.shootT -= dt * diff;
                    if (e.shootT <= 0) { e.burstT = d.burstTime; e.tick = 0; }
                }
                e.spin += dt * (e.burstT > 0 ? 9.0f : 1.5f);
            } break;

            case EType::Warden: updateBoss(e, dt, aimA, mv); break;
        }

        // Soft separation so enemies don't stack
        for (auto& o : enemies) {
            if (&o == &e || !o.alive) continue;
            Vector2 d = Vector2Subtract(e.pos, o.pos);
            float L = Vector2Length(d), minD = e.r + o.r;
            if (L > 0.001f && L < minD) mv = Vector2Add(mv, Vector2Scale(d, (minD - L) / minD * 1.5f / L));
        }
        if (dun.moveCircle(e.pos, e.r, Vector2Scale(mv, e.speed * dt))) e.moveT = 0;

        if (dist < e.r + pl.r * 0.7f) hurtPlayer();
    }
}

void Game::updateBoss(Enemy& e, float dt, float aimA, Vector2& mv) {
    const float diff = sqrtf(1.0f + content().rules.fireRatePerFloor * (floor - 1));
    Vector2 c = dun.roomCenter(curRoom);

    if (!e.enraged && e.hp < e.maxHp * def(e).enrageAt) {
        e.enraged = true;
        showBanner(upper(def(e).name) + " IS ENRAGED", 1.8f);
        shake += 0.6f;
        sBoom.play(1.3f);
        for (int k = 0; k < 36; k++) enemyShot(e.pos, k * 2 * PI / 36, 160, 8);
        e.pattern = -1;
        e.restT = 1.0f;
    }
    const float rate = (e.enraged ? 1.3f : 1.0f) * diff;

    Vector2 target{c.x + sinf(runTime * 0.7f) * TILE * 4, c.y - TILE * 1.5f + cosf(runTime * 0.9f) * TILE * 1.5f};
    Vector2 to = Vector2Subtract(target, e.pos);
    float L = Vector2Length(to);
    if (L > 4) mv = Vector2Scale(to, std::min(1.0f, L / 80.0f) / L);
    e.spin += dt * (e.enraged ? 2.5f : 1.2f);

    if (e.pattern < 0) {
        e.restT -= dt * rate;
        if (e.restT <= 0) {
            int options = e.enraged ? 4 : 3;
            do { e.pattern = irand(0, options - 1); } while (e.pattern == e.lastPattern);
            e.lastPattern = e.pattern;
            e.patT = 0; e.tick = 0; e.count = 0;
        }
        return;
    }

    e.patT += dt * rate;
    e.tick -= dt * rate;
    auto endPattern = [&] { e.pattern = -1; e.restT = e.enraged ? 0.7f : 1.1f; };

    switch (e.pattern) {
        case 0: {   // Concentric rings, alternating offset so there are gaps to roll through
            if (e.tick <= 0) {
                int n = e.enraged ? 28 : 20;
                float off = (e.count % 2) * PI / n;
                for (int k = 0; k < n; k++) enemyShot(e.pos, off + k * 2 * PI / n, 185, 8);
                e.count++;
                e.tick = 0.45f;
                shake += 0.06f;
            }
            if (e.count >= (e.enraged ? 4 : 3)) endPattern();
        } break;
        case 1: {   // Spiral arms
            if (e.tick <= 0) {
                int arms = e.enraged ? 3 : 2;
                for (int a = 0; a < arms; a++) enemyShot(e.pos, e.spiral + a * 2 * PI / arms, 200, 7);
                e.spiral += 0.21f;
                e.tick = 0.075f;
            }
            if (e.patT > 3.2f) endPattern();
        } break;
        case 2: {   // Aimed fans
            if (e.tick <= 0) {
                int n = e.enraged ? 9 : 7;
                for (int k = 0; k < n; k++) enemyShot(e.pos, aimA + (k - (n - 1) / 2.0f) * 0.12f, 265, 8);
                e.count++;
                e.tick = 0.5f;
            }
            if (e.count >= 4) endPattern();
        } break;
        case 3: {   // Mortars that burst into rings
            if (e.tick <= 0) {
                for (int k = -1; k <= 1; k++) enemyShot(e.pos, aimA + k * 0.6f, 150, 15, 0.9f + frand(0, 0.3f));
                e.count++;
                e.tick = 0.75f;
            }
            if (e.count >= 3) endPattern();
        } break;
    }
}

void Game::damageEnemy(Enemy& e, float dmg, Vector2 vel) {
    e.hp -= dmg;
    e.flash = 1.0f;
    float a = atan2f(vel.y, vel.x);
    if (e.type != EType::Warden) dun.moveCircle(e.pos, e.r, fromAngle(a, 4));
    burst(e.pos, 3, {255, 230, 200, 255}, 40, 160, 0.2f, 3, a, 0.6f);
    sHit.play(frand(0.9f, 1.25f), 0.5f);
    if (e.hp > 0) return;

    e.alive = false;
    kills++;
    if (e.type == EType::Warden) {
        burst(e.pos, 120, theme().accent, 80, 520, 1.2f, 7);
        burst(e.pos, 60, WHITE, 40, 300, 0.8f, 4);
        shake += 1.0f;
        sBoom.play();
        for (auto& b : bullets)
            if (!b.fromPlayer) b.alive = false;
    } else {
        burst(e.pos, 22, enemyColor(e), 60, 280, 0.5f, 5);
        sDie.play(frand(0.9f, 1.2f));
        shake += 0.1f;
        if (chance(content().rules.enemyAmmoDrop)) pickups.push_back({e.pos, PickType::Ammo});
    }
}

void Game::hurtPlayer() {
    if (pl.invuln > 0 || pl.rollT > content().rules.rollIframeEnd || god) return;
    pl.hp--;
    pl.invuln = content().rules.hitInvuln;
    hurtFlash = 1.0f;
    shake += 0.5f;
    sHurt.play();
    burst(pl.pos, 25, {255, 70, 70, 255}, 60, 260, 0.5f, 5);
    // Clear bullets right around the player so a single mistake doesn't cascade
    for (auto& b : bullets)
        if (!b.fromPlayer && Vector2Distance(b.pos, pl.pos) < 110) b.alive = false;
    if (pl.hp <= 0) {
        state = State::Dead;
        burst(pl.pos, 80, {120, 190, 255, 255}, 60, 400, 1.2f, 6);
        sBoom.play(0.7f);
    }
}

}  // namespace valor
