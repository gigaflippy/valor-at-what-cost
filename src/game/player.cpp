// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
#include "game/game.hpp"
#include "core/random.hpp"
#include "core/util.hpp"
#include <cmath>

namespace valor {

void Game::updatePlayer(float dt) {
    Vector2 mouseW = GetScreenToWorld2D(GetMousePosition(), cam);
    pl.aim = angleTo(pl.pos, mouseW);

    Vector2 in{0, 0};
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    in.y -= 1;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  in.y += 1;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  in.x -= 1;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) in.x += 1;
    if (Vector2Length(in) > 0) in = Vector2Normalize(in);

    pl.invuln = std::max(0.0f, pl.invuln - dt);
    pl.rollCd -= dt;
    pl.fireCd -= dt;
    pl.emptyT -= dt;
    pl.recoil = std::max(0.0f, pl.recoil - dt * 8.0f);

    // Dodge roll
    if (pl.rollT > 0) {
        pl.rollT -= dt;
        dun.moveCircle(pl.pos, pl.r, Vector2Scale(pl.rollDir, ROLL_SPEED * dt));
        if (chance(0.6f)) burst({pl.pos.x, pl.pos.y + pl.r}, 1, {180, 170, 160, 160}, 10, 40, 0.35f, 4);
        if (pl.rollT <= 0) pl.rollCd = 0.15f;
    } else {
        dun.moveCircle(pl.pos, pl.r, Vector2Scale(in, PLAYER_SPEED * dt));
        if ((IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) && pl.rollCd <= 0) {
            pl.rollDir = Vector2Length(in) > 0 ? in : fromAngle(pl.aim);
            pl.rollT = ROLL_TIME;
            sRoll.play(frand(0.9f, 1.1f));
        }
    }

    // Weapon switching
    int n = (int)pl.guns.size();
    float wheel = GetMouseWheelMove();
    int before = pl.cur;
    if (wheel != 0 && n > 1) pl.cur = (pl.cur + (wheel < 0 ? 1 : -1) + n) % n;
    for (int k = 0; k < std::min(n, 4); k++)
        if (IsKeyPressed(KEY_ONE + k)) pl.cur = k;
    if (pl.cur != before) pl.reloadT = 0;

    // Reload
    if (pl.reloadT > 0) {
        pl.reloadT -= dt;
        if (pl.reloadT <= 0) finishReload();
    }
    if (IsKeyPressed(KEY_R)) startReload();

    // Shoot
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && pl.rollT <= 0 && pl.fireCd <= 0 && pl.reloadT <= 0) {
        if (gun().mag > 0) fire();
        else if (!startReload() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { sEmpty.play(); pl.emptyT = 1.0f; }
    }

    if (IsKeyPressed(KEY_Q)) useBlank();
    if (IsKeyPressed(KEY_E)) interact();
}

bool Game::startReload() {
    Gun& g = gun();
    const GunDef& d = gunDef();
    if (pl.reloadT > 0 || g.mag >= d.mag) return false;
    if (d.maxAmmo >= 0 && g.ammo <= 0) return false;
    pl.reloadT = d.reload;
    return true;
}

void Game::finishReload() {
    Gun& g = gun();
    const GunDef& d = gunDef();
    int need = d.mag - g.mag;
    if (d.maxAmmo < 0) g.mag = d.mag;
    else { int take = std::min(need, g.ammo); g.mag += take; g.ammo -= take; }
    sDoor.play(2.5f, 0.4f);
}

void Game::spawnBullet(Vector2 pos, float ang, float speed, float r, float dmg, bool fromPlayer, bool pierce, Color col, float splitT, bool pending) {
    Bullet b;
    b.pos = pos;
    b.vel = fromAngle(ang, speed);
    b.r = r;
    b.dmg = dmg;
    b.fromPlayer = fromPlayer;
    b.pierce = pierce;
    b.id = nextBulletId++;
    b.life = fromPlayer ? 1.6f : 8.0f;
    b.splitT = splitT;
    b.col = col;
    (pending ? pendingBullets : bullets).push_back(b);
}

void Game::fire() {
    Gun& g = gun();
    const GunDef& d = gunDef();
    g.mag--;
    pl.fireCd = d.fireDelay;
    pl.recoil = 1.0f;
    Vector2 muzzle = Vector2Add(pl.pos, fromAngle(pl.aim, 26));
    for (int p = 0; p < d.pellets; p++) {
        float a = d.pellets > 1 ? pl.aim - d.spread + 2 * d.spread * (p + 0.5f) / d.pellets + frand(-0.05f, 0.05f)
                                : pl.aim + frand(-d.spread, d.spread);
        float sp = d.pellets > 1 ? d.speed * frand(0.85f, 1.1f) : d.speed;
        spawnBullet(muzzle, a, sp, d.radius, d.damage, true, d.pierce, d.color);
    }
    burst(muzzle, 4, d.color, 60, 220, 0.12f, 4, pl.aim, 0.5f);
    if (d.pellets > 1)  { sShotgun.play(frand(0.9f, 1.1f)); shake += 0.14f; }
    else if (d.pierce)  { sRail.play(frand(0.95f, 1.05f)); shake += 0.22f; }
    else                { sShoot.play(frand(0.9f, 1.15f), d.fireDelay < 0.1f ? 0.6f : 1.0f); shake += 0.03f; }
    if (g.mag == 0) startReload();
}

void Game::useBlank() {
    if (pl.blanks <= 0) return;
    pl.blanks--;
    for (auto& b : bullets)
        if (!b.fromPlayer && b.alive) { b.alive = false; burst(b.pos, 2, {150, 200, 255, 255}, 20, 80, 0.3f, 3); }
    for (auto& e : enemies) {
        Vector2 d = Vector2Subtract(e.pos, pl.pos);
        float L = Vector2Length(d);
        if (L < 260 && L > 0 && e.type != EType::Warden) dun.moveCircle(e.pos, e.r, Vector2Scale(d, 70.0f / L));
    }
    blankR = 0;
    blankPos = pl.pos;
    shake += 0.35f;
    pl.invuln = std::max(pl.invuln, 0.4f);
    sBlank.play();
}

void Game::interact() {
    if (curRoom < 0) return;
    Room& r = dun.rooms[curRoom];
    if (r.type == RoomType::Chest && !r.chestOpened && Vector2Distance(pl.pos, dun.roomCenter(curRoom)) < 75) {
        openChest(r);
        return;
    }
    if (hatchActive && Vector2Distance(pl.pos, hatchPos) < 60) {
        floor++;
        if (floor > FINAL_FLOOR) { floor = FINAL_FLOOR; state = State::Won; return; }
        pl.hp = std::min(pl.maxHp, pl.hp + 2);
        buildFloor();
    }
}

void Game::openChest(Room& r) {
    r.chestOpened = true;
    Vector2 c = dun.roomCenter(curRoom);
    std::vector<int> unowned;
    for (int i = 0; i < GUN_COUNT; i++) {
        bool owned = false;
        for (auto& g : pl.guns) owned |= g.def == i;
        if (!owned) unowned.push_back(i);
    }
    if (!unowned.empty()) {
        int d = unowned[irand(0, (int)unowned.size() - 1)];
        pl.guns.push_back({d, GUNS[d].mag, GUNS[d].maxAmmo});
        pl.cur = (int)pl.guns.size() - 1;
        pl.reloadT = 0;
        showBanner(TextFormat("You found the %s!", GUNS[d].name), 2.5f);
    } else {
        pickups.push_back({{c.x - 30, c.y + 40}, PickType::Heart});
        pickups.push_back({{c.x + 30, c.y + 40}, PickType::Ammo});
        showBanner("The chest holds supplies.", 2.0f);
    }
    burst(c, 40, theme().accent, 60, 260, 0.7f, 5);
    sChest.play();
}

}  // namespace valor
