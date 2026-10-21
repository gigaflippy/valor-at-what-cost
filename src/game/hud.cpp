// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
#include "game/game.hpp"
#include "core/random.hpp"
#include "core/util.hpp"
#include <cmath>

namespace valor {

void Game::drawHeart(float cx, float cy, float s, int fill) {
    // fill: 2 = full, 1 = half, 0 = empty
    auto shape = [&](Color c) {
        DrawCircleV({cx - 7 * s, cy}, 7.6f * s, c);
        DrawCircleV({cx + 7 * s, cy}, 7.6f * s, c);
        DrawTriangle({cx - 14.6f * s, cy + 2 * s}, {cx, cy + 18 * s}, {cx + 14.6f * s, cy + 2 * s}, c);
    };
    Color red{232, 60, 80, 255};
    shape(fill == 2 ? red : Color{60, 30, 40, 255});
    if (fill == 1) {
        BeginScissorMode((int)(cx - 16 * s), (int)(cy - 10 * s), (int)(16 * s), (int)(32 * s));
        shape(red);
        EndScissorMode();
    }
}

void Game::drawHUD() {
    const Theme& th = theme();
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    if (hurtFlash > 0) DrawRectangle(0, 0, sw, sh, Fade(RED, hurtFlash * 0.22f));
    if (pl.hp <= 2 && state == State::Playing) {
        float a = 0.10f + 0.08f * sinf(titleT * 6);
        DrawRectangleGradientV(0, 0, sw, 120, Fade(RED, a), BLANK);
        DrawRectangleGradientV(0, sh - 120, sw, 120, BLANK, Fade(RED, a));
    }

    // Hearts
    for (int i = 0; i < pl.maxHp / 2; i++) {
        int fill = std::clamp(pl.hp - i * 2, 0, 2);
        drawHeart(36.0f + i * 40, 32, 1.0f, fill);
    }
    // Blanks
    for (int i = 0; i < pl.blanks; i++) {
        Vector2 c{30.0f + i * 26, 76};
        DrawCircleV(c, 10, {90, 160, 240, 255});
        DrawRing(c, 5, 7, 0, 360, 20, RAYWHITE);
    }
    DrawText("Q", 30 + pl.blanks * 26 - 4, 68, 16, Fade(RAYWHITE, 0.5f));

    // Gun panel
    const GunDef& d = gunDef();
    const Gun& g = gun();
    Rectangle panel{(float)sw - 290, (float)sh - 96, 270, 76};
    DrawRectangleRounded(panel, 0.15f, 6, Fade(BLACK, 0.55f));
    DrawRectangleRoundedLinesEx(panel, 0.15f, 6, 2, Fade(d.color, 0.6f));
    DrawText(d.name.c_str(), (int)panel.x + 14, (int)panel.y + 10, 20, d.color);
    const char* ammoTxt = d.maxAmmo < 0 ? TextFormat("%d / INF", g.mag) : TextFormat("%d / %d", g.mag, g.ammo);
    DrawText(ammoTxt, (int)panel.x + 14, (int)panel.y + 38, 26, RAYWHITE);
    if (pl.emptyT > 0) DrawText("EMPTY", (int)panel.x + 170, (int)panel.y + 42, 20, {255, 90, 90, 255});
    else if (pl.reloadT > 0) DrawText("RELOAD", (int)panel.x + 170, (int)panel.y + 42, 20, Fade(RAYWHITE, 0.7f));
    if (pl.guns.size() > 1)
        DrawText(TextFormat("%d/%d  wheel / 1-4", pl.cur + 1, (int)pl.guns.size()), (int)panel.x + 14, (int)panel.y - 20, 14, Fade(RAYWHITE, 0.55f));

    // Floor label & stats
    const char* fl = TextFormat("FLOOR %d / %d", floor, content().floorCount());
    textCentered(fl, sw / 2, 14, 20, Fade(th.accent, 0.9f));
    DrawText(TextFormat("Kills %d   %02d:%02d", kills, (int)runTime / 60, (int)runTime % 60), 20, sh - 30, 16, Fade(RAYWHITE, 0.45f));

    drawMinimap(sw);

    // Boss bar
    for (auto& e : enemies) {
        if (e.type != EType::Warden || e.spawnT > 0) continue;
        float w = std::min(620.0f, sw - 340.0f);
        Rectangle bar{sw / 2.0f - w / 2, (float)sh - 54, w, 18};
        textCentered(upper(def(e).name).c_str(), sw / 2, (int)bar.y - 24, 18, RAYWHITE);
        DrawRectangleRec(bar, Fade(BLACK, 0.7f));
        DrawRectangle((int)bar.x + 2, (int)bar.y + 2, (int)((bar.width - 4) * std::max(0.0f, e.hp / e.maxHp)), (int)bar.height - 4,
                      e.enraged ? Color{230, 60, 70, 255} : th.accent);
    }

    // Banner
    if (bannerT > 0 && state == State::Playing) {
        float a = std::min(1.0f, bannerT * 2.0f);
        int y = sh / 2 - 170;
        int w = MeasureText(banner.c_str(), 30) + 60;
        DrawRectangle(sw / 2 - w / 2, y - 12, w, 54, Fade(BLACK, 0.5f * a));
        textCentered(banner.c_str(), sw / 2, y, 30, Fade(RAYWHITE, a));
    }
}

void Game::drawMinimap(int sw) {
    const Theme& th = theme();
    const int cw = 20, ch = 13, gap = 4;
    int ox = sw - GRID * (cw + gap) - 16, oy = 16;
    auto known = [&](int i) {
        if (dun.rooms[i].visited) return true;
        for (int d = 0; d < 4; d++) {
            if (!dun.rooms[i].links[d]) continue;
            int n = dun.roomAt[(dun.rooms[i].gy + DY[d]) * GRID + dun.rooms[i].gx + DX[d]];
            if (n >= 0 && dun.rooms[n].visited) return true;
        }
        return false;
    };
    DrawRectangle(ox - 8, oy - 8, GRID * (cw + gap) + 12, GRID * (ch + gap) + 12, Fade(BLACK, 0.4f));
    for (int i = 0; i < (int)dun.rooms.size(); i++) {
        if (!known(i)) continue;
        const Room& r = dun.rooms[i];
        int x = ox + r.gx * (cw + gap), y = oy + r.gy * (ch + gap);
        if (r.links[0]) DrawRectangle(x + cw, y + ch / 2 - 1, gap, 3, Fade(RAYWHITE, 0.5f));
        if (r.links[1]) DrawRectangle(x + cw / 2 - 1, y + ch, 3, gap, Fade(RAYWHITE, 0.5f));
        Color c = i == curRoom ? th.accent : r.visited ? Color{120, 118, 130, 255} : Color{60, 58, 68, 255};
        DrawRectangle(x, y, cw, ch, c);
        if (r.type == RoomType::Boss)  DrawCircle(x + cw / 2, y + ch / 2, 4, {230, 60, 70, 255});
        if (r.type == RoomType::Chest && !r.chestOpened) DrawRectangle(x + cw / 2 - 3, y + ch / 2 - 3, 6, 6, {240, 200, 90, 255});
    }
}

void Game::drawOverlay(const char* title, const char* hint, Color accent) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.65f));
    textCentered(title, sw / 2, sh / 2 - 110, 56, accent);
    if (state != State::Paused) {
        textCentered(TextFormat("Floor reached: %d   -   Kills: %d   -   Time: %02d:%02d", floor, kills, (int)runTime / 60,
                                (int)runTime % 60),
                     sw / 2, sh / 2 - 20, 22, RAYWHITE);
    }
    textCentered(hint, sw / 2, sh / 2 + 40, 22, Fade(RAYWHITE, 0.75f));
}

void Game::drawCrosshair() {
    if (state == State::Title) return;
    Vector2 m = GetMousePosition();
    Color c = state == State::Playing ? RAYWHITE : Fade(RAYWHITE, 0.5f);
    DrawCircleLinesV(m, 9, c);
    DrawLineEx({m.x - 16, m.y}, {m.x - 6, m.y}, 2, c);
    DrawLineEx({m.x + 6, m.y}, {m.x + 16, m.y}, 2, c);
    DrawLineEx({m.x, m.y - 16}, {m.x, m.y - 6}, 2, c);
    DrawLineEx({m.x, m.y + 6}, {m.x, m.y + 16}, 2, c);
}

void Game::drawTitle() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Vector2 c{sw / 2.0f, sh / 2.0f - 40};
    // Decorative bullet spiral
    for (int k = 0; k < 90; k++) {
        float rad = fmodf(k * 23.0f + titleT * 90.0f, 900.0f);
        float a = k * 0.42f + titleT * 0.35f;
        Vector2 p = Vector2Add(c, fromAngle(a, rad));
        float fade = 1.0f - rad / 900.0f;
        DrawCircleV(p, 7, Fade(content().rules.enemyBullet, 0.5f * fade));
        DrawCircleV(p, 3, Fade(WHITE, 0.6f * fade));
    }
    textCentered("VALOR AT WHAT COST", sw / 2 + 4, sh / 2 - 182, 64, Fade(BLACK, 0.6f));
    textCentered("VALOR AT WHAT COST", sw / 2, sh / 2 - 186, 64, {255, 200, 90, 255});
    textCentered("a bullet-hell roguelike by Georgi", sw / 2, sh / 2 - 104, 22, Fade(RAYWHITE, 0.7f));

    const char* lines[] = {
        "WASD  move              MOUSE  aim",
        "LMB   shoot             R      reload",
        "SPACE / RMB  dodge roll (invincible while rolling)",
        "Q     blank (clears all enemy bullets)",
        "E     open chests / take the hatch",
        "WHEEL / 1-4  switch guns      ESC  pause",
    };
    int y = sh / 2 - 30;
    Rectangle box{sw / 2.0f - 300, (float)y - 18, 600, 6 * 28 + 30.0f};
    DrawRectangleRounded(box, 0.08f, 6, Fade(BLACK, 0.6f));
    for (auto* l : lines) { DrawText(l, sw / 2 - 270, y, 20, RAYWHITE); y += 28; }

    float blink = 0.55f + 0.45f * sinf(titleT * 4);
    textCentered("Press ENTER or click to descend", sw / 2, sh - 110, 26, Fade({255, 200, 90, 255}, blink));
    textCentered("ESC to quit", sw / 2, sh - 70, 18, Fade(RAYWHITE, 0.4f));
}

}  // namespace valor
