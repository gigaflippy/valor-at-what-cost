// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
#include "game/game.hpp"
#include "core/random.hpp"
#include "core/util.hpp"
#include <cmath>

namespace valor {

Color Game::enemyColor(const Enemy& e) const {
    return e.enraged ? def(e).enragedColor : def(e).color;
}

Color Game::pickupColor(PickType t) {
    switch (t) {
        case PickType::Heart: return {240, 70, 90, 255};
        case PickType::Ammo:  return {240, 200, 90, 255};
        case PickType::Blank: return {110, 180, 255, 255};
    }
    return WHITE;
}

void Game::draw() {
    const Theme& th = theme();
    ClearBackground(state == State::Title ? Color{12, 11, 16, 255} : th.bg);
    if (state == State::Title) { drawTitle(); return; }

    Camera2D view = cam;
    view.offset = {GetScreenWidth() / 2.0f + frand(-1, 1) * shake * 12, GetScreenHeight() / 2.0f + frand(-1, 1) * shake * 12};
    if (state != State::Playing) view.offset = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};

    BeginMode2D(view);
    drawTiles(view);
    drawRoomProps();
    for (auto& p : pickups) drawPickup(p);
    for (auto& e : enemies) drawEnemy(e);
    if (state != State::Dead) drawPlayer();
    drawBullets();
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto& p : parts) {
        float k = p.life / p.maxLife;
        DrawCircleV(p.pos, p.size * (0.4f + 0.6f * k), Fade(p.col, k));
    }
    EndBlendMode();
    if (blankR >= 0)
        DrawRing(blankPos, std::max(0.0f, blankR - 18), blankR, 0, 360, 72, Fade({140, 200, 255, 255}, 1.0f - blankR / 1000.0f));
    EndMode2D();

    drawHUD();
    if (state == State::Paused) drawOverlay("PAUSED", "ESC - resume     X - quit to title", th.accent);
    if (state == State::Dead)   drawOverlay("YOU DIED", "R - try again     ESC - title", {255, 90, 90, 255});
    if (state == State::Won)    drawOverlay("YOU ESCAPED THE DEPTHS", "R - play again     ESC - title", th.accent);
    drawCrosshair();
}

void Game::drawTiles(const Camera2D& view) {
    const Theme& th = theme();
    Vector2 tl = GetScreenToWorld2D({0, 0}, view);
    Vector2 br = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, view);
    int x0 = std::max(0, (int)(tl.x / TILE) - 1), y0 = std::max(0, (int)(tl.y / TILE) - 1);
    int x1 = std::min(MAP_W - 1, (int)(br.x / TILE) + 1), y1 = std::min(MAP_H - 1, (int)(br.y / TILE) + 1);

    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            Tile t = dun.tileAt(x, y);
            if (t == Tile::Void) continue;
            Rectangle rc{x * TILE, y * TILE, TILE, TILE};
            unsigned h = (unsigned)(x * 73856093u ^ y * 19349663u);

            if (t == Tile::Floor || t == Tile::Door) {
                DrawRectangleRec(rc, ((x + y) & 1) ? th.floorA : th.floorB);
                if (h % 9 == 0) DrawRectangle((int)rc.x + (int)(h % 28) + 4, (int)rc.y + (int)((h >> 5) % 28) + 4, 3, 3, Fade(BLACK, 0.18f));
                // soft shadow cast by the wall above
                Tile above = dun.tileAt(x, y - 1);
                if (above == Tile::Wall || above == Tile::Pillar) DrawRectangle((int)rc.x, (int)rc.y, (int)TILE, 10, Fade(BLACK, 0.28f));
                if (t == Tile::Door) {
                    int o = dun.doorOwner[y * MAP_W + x];
                    if (o >= 0 && dun.rooms[o].locked) {
                        DrawRectangleRec(rc, {70, 26, 30, 255});
                        for (int k = 0; k < 4; k++)
                            DrawRectangle((int)rc.x + 4 + k * 10, (int)rc.y + 2, 4, (int)TILE - 4, mix(th.accent, RED, 0.5f));
                    } else {
                        DrawRectangleRec(rc, Fade(th.wallTop, 0.12f));
                    }
                }
            } else {
                // Wall / pillar: lighter "top" with a darker front face where it meets the floor
                bool faceVisible = dun.tileAt(x, y + 1) == Tile::Floor || dun.tileAt(x, y + 1) == Tile::Door;
                Color top = t == Tile::Pillar ? mix(th.wallTop, WHITE, 0.08f) : th.wallTop;
                DrawRectangleRec(rc, top);
                DrawRectangleLinesEx(rc, 1, Fade(BLACK, 0.18f));
                if (faceVisible) {
                    DrawRectangle((int)rc.x, (int)(rc.y + TILE - 14), (int)TILE, 14, th.wall);
                    DrawRectangle((int)rc.x, (int)(rc.y + TILE - 14), (int)TILE, 2, Fade(BLACK, 0.25f));
                }
            }
        }
}

void Game::drawRoomProps() {
    const Theme& th = theme();
    for (int i = 0; i < (int)dun.rooms.size(); i++) {
        if (dun.rooms[i].type != RoomType::Chest) continue;
        Vector2 c = dun.roomCenter(i);
        DrawEllipse((int)c.x, (int)c.y + 16, 30, 9, Fade(BLACK, 0.35f));
        Color wood{120, 78, 46, 255};
        if (dun.rooms[i].chestOpened) {
            DrawRectangleRounded({c.x - 26, c.y - 10, 52, 28}, 0.2f, 4, mix(wood, BLACK, 0.3f));
            DrawRectangleRounded({c.x - 26, c.y - 30, 52, 14}, 0.3f, 4, wood);
            DrawRectangle((int)c.x - 26, (int)c.y - 5, 52, 4, Fade(BLACK, 0.4f));
        } else {
            float bob = sinf(titleT * 3) * 2;
            DrawRectangleRounded({c.x - 26, c.y - 18 + bob, 52, 36}, 0.2f, 4, wood);
            DrawRectangle((int)c.x - 26, (int)(c.y - 4 + bob), 52, 6, th.accent);
            DrawRectangle((int)c.x - 5, (int)(c.y - 8 + bob), 10, 12, mix(th.accent, WHITE, 0.4f));
            if (curRoom == i && Vector2Distance(pl.pos, c) < 75) textCentered("[E] Open", (int)c.x, (int)c.y - 50, 18, RAYWHITE);
        }
    }
    if (hatchActive) {
        float pulse = 0.5f + 0.5f * sinf(titleT * 4);
        DrawCircleV(hatchPos, 34, {10, 8, 12, 255});
        DrawRing(hatchPos, 30, 38, 0, 360, 48, th.wall);
        DrawRing(hatchPos, 38, 42 + pulse * 4, 0, 360, 48, Fade(th.accent, 0.5f + 0.5f * pulse));
        for (int k = 0; k < 3; k++) DrawRectangle((int)hatchPos.x - 18, (int)hatchPos.y - 14 + k * 11, 36, 4, th.wallTop);
        if (Vector2Distance(pl.pos, hatchPos) < 60) textCentered("[E] Descend", (int)hatchPos.x, (int)hatchPos.y - 66, 18, RAYWHITE);
    }
}

void Game::drawPickup(const Pickup& p) {
    Vector2 pos{p.pos.x, p.pos.y + sinf(p.t * 4) * 3};
    DrawEllipse((int)p.pos.x, (int)p.pos.y + 12, 10, 4, Fade(BLACK, 0.3f));
    Color c = pickupColor(p.type);
    switch (p.type) {
        case PickType::Heart: drawHeart(pos.x, pos.y - 3, 0.7f, 2); break;
        case PickType::Ammo:
            DrawRectangleRounded({pos.x - 11, pos.y - 8, 22, 16}, 0.25f, 4, {90, 96, 60, 255});
            DrawRectangle((int)pos.x - 11, (int)pos.y - 2, 22, 4, c);
            break;
        case PickType::Blank:
            DrawCircleV(pos, 10, c);
            DrawRing(pos, 6, 8, 0, 360, 24, WHITE);
            break;
    }
}

void Game::drawEnemy(const Enemy& e) {
    Color base = enemyColor(e);
    if (e.spawnT > 0) {
        float k = e.spawnT / def(e).spawnDelay;
        DrawCircleLinesV(e.pos, e.r + k * 34, Fade(theme().accent, 1.0f - k));
        DrawCircleV(e.pos, e.r * (1.0f - k), Fade(base, 0.5f));
        return;
    }
    Color c = mix(base, WHITE, e.flash);
    float aimA = angleTo(e.pos, pl.pos);
    DrawEllipse((int)e.pos.x, (int)(e.pos.y + e.r * 0.85f), e.r * 0.95f, e.r * 0.35f, Fade(BLACK, 0.35f));

    auto eyes = [&](float spacing, float size) {
        Vector2 fwd = fromAngle(aimA, e.r * 0.3f);
        for (int s = -1; s <= 1; s += 2) {
            Vector2 ep{e.pos.x + fwd.x + s * spacing, e.pos.y + fwd.y - e.r * 0.15f};
            DrawCircleV(ep, size, RAYWHITE);
            DrawCircleV(Vector2Add(ep, fromAngle(aimA, size * 0.4f)), size * 0.5f, {20, 16, 24, 255});
        }
    };

    switch (e.shape) {
        case Shape::Round:
            DrawRectanglePro({e.pos.x, e.pos.y, 22, 6}, {-6, 3}, aimA * RAD2DEG, {50, 48, 56, 255});
            DrawCircleV(e.pos, e.r, c);
            DrawCircleV({e.pos.x, e.pos.y - e.r * 0.35f}, e.r * 0.55f, mix(c, WHITE, 0.15f));
            eyes(5, 3.5f);
            break;
        case Shape::Square:
            DrawRectanglePro({e.pos.x, e.pos.y, 26, 10}, {-6, 5}, aimA * RAD2DEG, {60, 50, 46, 255});
            DrawRectangleRounded({e.pos.x - e.r, e.pos.y - e.r, e.r * 2, e.r * 2}, 0.35f, 6, c);
            DrawRectangle((int)(e.pos.x - e.r), (int)(e.pos.y + e.r * 0.2f), (int)(e.r * 2), 4, Fade(BLACK, 0.25f));
            eyes(6, 4);
            break;
        case Shape::Hexagon:
            DrawPoly(e.pos, 6, e.r * 1.15f, e.spin * RAD2DEG, c);
            DrawPoly(e.pos, 6, e.r * 0.7f, -e.spin * RAD2DEG, mix(c, BLACK, 0.3f));
            DrawCircleV(e.pos, 5, RAYWHITE);
            DrawCircleV(Vector2Add(e.pos, fromAngle(aimA, 2)), 2.5f, {20, 16, 24, 255});
            break;
        case Shape::Warden: {
            for (int k = 0; k < 8; k++) {
                float a = e.spin + k * PI / 4;
                DrawPoly(Vector2Add(e.pos, fromAngle(a, e.r + 6)), 3, 11, a * RAD2DEG, mix(c, BLACK, 0.25f));
            }
            DrawPoly(e.pos, 8, e.r, e.spin * 20, c);
            DrawPolyLinesEx(e.pos, 8, e.r, e.spin * 20, 3, mix(c, BLACK, 0.4f));
            DrawCircleV(e.pos, e.r * 0.55f, {24, 20, 30, 255});
            Color iris = e.enraged ? Color{255, 80, 60, 255} : theme().accent;
            Vector2 pupil = Vector2Add(e.pos, fromAngle(aimA, e.r * 0.2f));
            DrawCircleV(pupil, e.r * 0.3f, iris);
            DrawCircleV(pupil, e.r * 0.13f, {10, 8, 12, 255});
        } break;
    }

    // Wind-up tell before a shot
    bool tell = e.type == EType::Shooter ? e.shootT < 0.3f
              : e.type == EType::Spinner ? (e.burstT <= 0 && e.shootT < 0.4f) : false;
    if (tell) textCentered("!", (int)e.pos.x, (int)(e.pos.y - e.r - 26), 22, {255, 230, 90, 255});

    if (e.type != EType::Warden && e.hp < e.maxHp) {
        float w = e.r * 2;
        DrawRectangle((int)(e.pos.x - w / 2), (int)(e.pos.y - e.r - 9), (int)w, 4, Fade(BLACK, 0.6f));
        DrawRectangle((int)(e.pos.x - w / 2), (int)(e.pos.y - e.r - 9), (int)(w * e.hp / e.maxHp), 4, {255, 90, 90, 255});
    }
}

void Game::drawPlayer() {
    bool rolling = pl.rollT > 0;
    if (pl.invuln > 0 && fmodf(titleT, 0.12f) < 0.06f) return;   // i-frame flicker
    DrawEllipse((int)pl.pos.x, (int)(pl.pos.y + pl.r * 0.9f), pl.r, pl.r * 0.35f, Fade(BLACK, 0.35f));

    Color cloak{70, 150, 220, 255}, face{240, 228, 205, 255};
    if (rolling) {
        float k = 1.0f - pl.rollT / content().rules.rollTime;
        for (int i = 1; i <= 3; i++)
            DrawCircleV(Vector2Subtract(pl.pos, Vector2Scale(pl.rollDir, i * 9.0f)), pl.r * 0.9f, Fade(cloak, 0.18f * (4 - i)));
        float squash = 0.75f + 0.25f * fabsf(cosf(k * PI * 2));
        DrawEllipse((int)pl.pos.x, (int)pl.pos.y, pl.r * (2 - squash), pl.r * squash, cloak);
        return;
    }

    const GunDef& d = gunDef();
    float gunLen = d.length;
    bool behind = sinf(pl.aim) < -0.2f;   // aiming up: gun is drawn behind the body
    auto drawGun = [&] {
        float back = pl.recoil * 4;
        DrawRectanglePro({pl.pos.x, pl.pos.y + 3, gunLen, 8}, {-(8 - back), 4}, pl.aim * RAD2DEG, {20, 18, 24, 255});
        DrawRectanglePro({pl.pos.x, pl.pos.y + 3, gunLen - 2, 6}, {-(9 - back), 3}, pl.aim * RAD2DEG, {176, 178, 190, 255});
        DrawRectanglePro({pl.pos.x, pl.pos.y + 3, gunLen * 0.5f, 3}, {-(8 - back) - gunLen * 0.45f, 1.5f}, pl.aim * RAD2DEG, d.color);
    };
    if (behind) drawGun();
    DrawCircleV(pl.pos, pl.r, cloak);
    Vector2 headOff = fromAngle(pl.aim, 3.0f);
    Vector2 head{pl.pos.x + headOff.x, pl.pos.y - 5 + headOff.y * 0.5f};
    DrawCircleV(head, pl.r * 0.62f, face);
    if (!behind) {
        Vector2 visor = Vector2Add(head, fromAngle(pl.aim, 3.5f));
        DrawCircleV(visor, 2.6f, {30, 34, 50, 255});
        drawGun();
    }

    if (pl.reloadT > 0) {
        float k = 1.0f - pl.reloadT / d.reload;
        DrawRectangle((int)pl.pos.x - 22, (int)pl.pos.y - 34, 44, 6, Fade(BLACK, 0.7f));
        DrawRectangle((int)pl.pos.x - 21, (int)pl.pos.y - 33, (int)(42 * k), 4, RAYWHITE);
    }
}

void Game::drawBullets() {
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto& b : bullets) {
        if (b.fromPlayer) {
            Vector2 tail = Vector2Subtract(b.pos, Vector2Scale(b.vel, 0.025f));
            DrawLineEx(tail, b.pos, b.r * 1.4f, Fade(b.col, 0.35f));
        } else {
            DrawCircleV(b.pos, b.r + 4, Fade(b.col, 0.25f));
        }
    }
    EndBlendMode();
    for (auto& b : bullets) {
        if (b.fromPlayer) {
            DrawCircleV(b.pos, b.r, b.col);
        } else {
            float pulse = b.splitT > 0 ? 1.0f + 0.15f * sinf(b.splitT * 30) : 1.0f;
            DrawCircleV(b.pos, b.r * pulse, b.col);
            DrawCircleV(b.pos, b.r * 0.5f * pulse, {255, 235, 240, 255});
        }
    }
}

}  // namespace valor
