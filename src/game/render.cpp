// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// Pixel-art renderer.
//
//  1. The world is drawn on a 480x270 canvas (16 px per tile) and scaled up with
//     sharp pixels. The camera moves smoothly by shifting the canvas by fractions
//     of a pixel when it is blitted, so the art never shimmers.
//  2. Walls are drawn in a three-quarter view (top cap + front face), and the
//     world is drawn row by row so characters pass behind walls and pillars.
//  3. Characters, props and bullets get a 1-pixel dark outline (pass 0 draws the
//     silhouette slightly larger in the outline colour, pass 1 draws the colours).
//  4. A light map (dark ambient + soft lights) is multiplied over the scene at 2x,
//     so lit areas can become brighter than their base colour.
//  5. Emissive things (bullets, flames, sparks) are drawn after the lighting, so
//     they always glow.
#include "game/game.hpp"
#include "core/random.hpp"
#include "core/util.hpp"
#include "rlgl.h"
#include <cmath>

namespace valor {

namespace {

const Color OUTLINE = {14, 11, 18, 255};
constexpr float OL = PX;                   // outline thickness: one canvas pixel
constexpr int CANVAS_PAD = 1;              // spare pixel on each side for sub-pixel scrolling

void oCircle(Vector2 c, float r, Color col, int pass) {
    if (pass == 0) DrawCircleV(c, r + OL, OUTLINE);
    else DrawCircleV(c, r, col);
}
void oRect(Rectangle r, Color col, int pass) {
    if (pass == 0) DrawRectangleRec({r.x - OL, r.y - OL, r.width + 2 * OL, r.height + 2 * OL}, OUTLINE);
    else DrawRectangleRec(r, col);
}
// Rectangle rotated around `pivot`, extending `back` behind it and `len` in front
void oBar(Vector2 pivot, float back, float len, float thick, float angDeg, Color col, int pass) {
    float o = pass == 0 ? OL : 0;
    DrawRectanglePro({pivot.x, pivot.y, len + back + 2 * o, thick + 2 * o}, {back + o, thick / 2 + o}, angDeg,
                     pass == 0 ? OUTLINE : col);
}
void oPoly(Vector2 c, int sides, float rad, float rotDeg, Color col, int pass) {
    if (pass == 0) DrawPoly(c, sides, rad + OL * 1.2f, rotDeg, OUTLINE);
    else DrawPoly(c, sides, rad, rotDeg, col);
}
void shadow(Vector2 feet, float w) { DrawEllipse((int)feet.x, (int)feet.y, w, w * 0.38f, Fade(BLACK, 0.38f)); }

bool isBlock(const Dungeon& d, int x, int y) {
    Tile t = d.tileAt(x, y);
    if (t == Tile::Wall || t == Tile::Pillar) return true;
    if (t == Tile::Door) { int o = d.doorOwner[y * MAP_W + x]; return o >= 0 && d.rooms[o].locked; }
    return false;
}
bool isOpenFloor(const Dungeon& d, int x, int y) {
    Tile t = d.tileAt(x, y);
    return t == Tile::Floor || (t == Tile::Door && !isBlock(d, x, y));
}
// A block shows its tall front face when there is walkable floor right below it
bool tallFace(const Dungeon& d, int x, int y) {
    if (!isBlock(d, x, y)) return false;
    if (isOpenFloor(d, x, y + 1)) return true;
    // corners: match the height of a neighbouring tall face so the wall line is continuous
    return (isBlock(d, x - 1, y) && isOpenFloor(d, x - 1, y + 1)) || (isBlock(d, x + 1, y) && isOpenFloor(d, x + 1, y + 1));
}
unsigned hash2(int x, int y) {
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ (h >> 15);
}

}  // namespace

// ============================================================ setup
void Game::initGraphics() {
    canvas = LoadRenderTexture(CANVAS_W + 2 * CANVAS_PAD, CANVAS_H + 2 * CANVAS_PAD);
    lightMap = LoadRenderTexture(CANVAS_W + 2 * CANVAS_PAD, CANVAS_H + 2 * CANVAS_PAD);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(lightMap.texture, TEXTURE_FILTER_POINT);

    // Soft round light: bright centre, smooth falloff to nothing at the edge
    const int S = 128;
    Image img = GenImageColor(S, S, BLANK);
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float dx = (x + 0.5f) / S * 2 - 1, dy = (y + 0.5f) / S * 2 - 1;
            float d = std::min(1.0f, sqrtf(dx * dx + dy * dy));
            float a = (1 - d) * (1 - d) * (1 + 2 * d);       // smoothstep-like
            ImageDrawPixel(&img, x, y, {255, 255, 255, (unsigned char)(a * 255)});
        }
    lightTex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(lightTex, TEXTURE_FILTER_BILINEAR);

    // Player sprite sheet (optional: shapes are drawn if it is missing)
    std::string sheet = content().dir + "/sprites/deserter.png";
    if (FileExists(sheet.c_str())) {
        playerSprite = LoadTexture(sheet.c_str());
        SetTextureFilter(playerSprite, TEXTURE_FILTER_POINT);
    } else {
        TraceLog(LOG_WARNING, "SPRITE: %s not found, drawing the player with shapes", sheet.c_str());
    }
    gfxReady = true;
}

void Game::unloadGraphics() {
    if (!gfxReady) return;
    UnloadRenderTexture(canvas);
    UnloadRenderTexture(lightMap);
    UnloadTexture(lightTex);
    if (playerSprite.id != 0) UnloadTexture(playerSprite);
    gfxReady = false;
}

// Biggest scale that fits the window; prefer a whole number (perfectly even pixels)
// unless that would waste more than 15% of the window.
void Game::layoutCanvas() {
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float fit = std::min(sw / CANVAS_W, sh / CANVAS_H);
    float whole = floorf(fit);
    pixelScale = (whole >= 1 && whole / fit > 0.85f) ? whole : fit;
    canvasPos = {floorf((sw - CANVAS_W * pixelScale) / 2), floorf((sh - CANVAS_H * pixelScale) / 2)};
}

Vector2 Game::screenToWorld(Vector2 s) const {
    Vector2 u = Vector2Scale(Vector2Subtract(s, canvasPos), 1.0f / pixelScale);
    return {(u.x - CANVAS_W / 2.0f) / ZOOM + cam.target.x, (u.y - CANVAS_H / 2.0f) / ZOOM + cam.target.y};
}

Vector2 Game::worldToScreen(Vector2 w) const {
    return {canvasPos.x + ((w.x - cam.target.x) * ZOOM + CANVAS_W / 2.0f) * pixelScale,
            canvasPos.y + ((w.y - cam.target.y) * ZOOM + CANVAS_H / 2.0f) * pixelScale};
}

// ============================================================ braziers & embers (Valor's own flavour)
// Two braziers on the north wall of every room, four in the boss room. They burn in
// the floor's accent colour and shed embers that drift up through the light.
void Game::placeBraziers() {
    braziers.clear();
    for (const Room& r : dun.rooms) {
        int x0 = r.gx * ROOM_W, y0 = r.gy * ROOM_H;
        std::vector<int> cols = {4, ROOM_W - 5};
        if (r.type == RoomType::Boss) cols = {3, 8, ROOM_W - 9, ROOM_W - 4};
        for (int cx : cols) {
            int x = x0 + cx;
            if (dun.tileAt(x, y0) == Tile::Wall && dun.tileAt(x, y0 + 1) == Tile::Floor)
                braziers.push_back({(x + 0.5f) * TILE, y0 * TILE + TILE * 0.55f});
        }
    }
}

void Game::emitEmbers(float dt) {
    const Color a = theme().accent;
    for (size_t i = 0; i < braziers.size(); i++) {
        Vector2 b = braziers[i];
        if (fabsf(b.x - cam.target.x) > 700 || fabsf(b.y - cam.target.y) > 450) continue;
        if (!chance(dt * 2.2f)) continue;
        float life = frand(1.6f, 2.8f);
        Particle p{{b.x + frand(-6, 6), b.y - 14}, {frand(-12, 12), frand(-55, -30)}, life, life, frand(2.0f, 3.2f),
                   mix(a, {255, 200, 120, 255}, 0.4f)};
        p.drag = 0.25f;
        parts.push_back(p);
    }
}

// ============================================================ colours
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

// ============================================================ frame
void Game::draw() {
    const Theme& th = theme();
    if (state == State::Title) {
        ClearBackground({12, 11, 16, 255});
        drawTitle();
        return;
    }
    layoutCanvas();

    // Camera snapped to whole canvas pixels; the remainder becomes a sub-pixel shift at blit time
    Vector2 shakeOff = state == State::Playing ? Vector2{frand(-1, 1) * shake * 4, frand(-1, 1) * shake * 4} : Vector2{0, 0};
    Vector2 tgtPx = {cam.target.x * ZOOM + shakeOff.x, cam.target.y * ZOOM + shakeOff.y};
    Vector2 snapped = {roundf(tgtPx.x), roundf(tgtPx.y)};
    Vector2 sub = Vector2Subtract(tgtPx, snapped);
    Camera2D view{};
    view.zoom = ZOOM;
    view.target = {snapped.x / ZOOM, snapped.y / ZOOM};
    view.offset = {CANVAS_W / 2.0f + CANVAS_PAD, CANVAS_H / 2.0f + CANVAS_PAD};

    // Visible tile range (+ margin for tall walls drawn above their tile)
    int x0 = std::max(0, (int)floorf((view.target.x - CANVAS_W / 2.0f * PX) / TILE) - 1);
    int x1 = std::min(MAP_W - 1, (int)floorf((view.target.x + CANVAS_W / 2.0f * PX) / TILE) + 1);
    int y0 = std::max(0, (int)floorf((view.target.y - CANVAS_H / 2.0f * PX) / TILE) - 1);
    int y1 = std::min(MAP_H - 1, (int)floorf((view.target.y + CANVAS_H / 2.0f * PX) / TILE) + 2);

    // 1. Scene
    BeginTextureMode(canvas);
    ClearBackground(th.bg);
    BeginMode2D(view);
    drawFloor(x0, y0, x1, y1);
    drawWorldSorted(x0, y0, x1, y1);
    EndMode2D();
    EndTextureMode();

    // 2. Light
    drawLights(view);
    BeginTextureMode(canvas);
    rlSetBlendFactors(RL_DST_COLOR, RL_SRC_COLOR, RL_FUNC_ADD);   // result = 2 * light * scene
    BeginBlendMode(BLEND_CUSTOM);
    DrawTextureRec(lightMap.texture, {0, 0, (float)lightMap.texture.width, -(float)lightMap.texture.height}, {0, 0}, WHITE);
    EndBlendMode();

    // 3. Emissive on top of the lighting
    BeginMode2D(view);
    drawEmissive();
    EndMode2D();
    EndTextureMode();

    // 4. Scale the canvas up to the window
    ClearBackground(BLACK);
    Rectangle src{CANVAS_PAD + sub.x, CANVAS_PAD - sub.y, (float)CANVAS_W, -(float)CANVAS_H};
    Rectangle dst{canvasPos.x, canvasPos.y, CANVAS_W * pixelScale, CANVAS_H * pixelScale};
    DrawTexturePro(canvas.texture, src, dst, {0, 0}, 0, WHITE);

    // 5. Crisp screen-resolution layer: prompts, HUD, menus
    drawPrompts();
    drawHUD();
    if (state == State::Paused) drawOverlay("PAUSED", "ESC - resume     X - quit to title", th.accent);
    if (state == State::Dead)   drawOverlay("YOU DIED", "R - try again     ESC - title", {255, 90, 90, 255});
    if (state == State::Won)    drawOverlay("YOU ESCAPED THE DEPTHS", "R - play again     ESC - title", th.accent);
    drawCrosshair();
}

// ============================================================ floor
void Game::drawFloor(int x0, int y0, int x1, int y1) {
    const Theme& th = theme();
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            Tile t = dun.tileAt(x, y);
            if (t != Tile::Floor && t != Tile::Door) continue;
            float fx = x * TILE, fy = y * TILE;
            unsigned h = hash2(x, y);
            // Flagstones: base colour per tile with a little variation, darker seams
            Color base = ((x + y) & 1) ? th.floorA : th.floorB;
            base = mix(base, (h & 1) ? th.wallTop : BLACK, (h >> 3) % 4 * 0.025f);
            DrawRectangleRec({fx, fy, TILE, TILE}, base);
            DrawRectangleRec({fx, fy + TILE - PX, TILE, PX}, mix(base, BLACK, 0.25f));
            DrawRectangleRec({fx + TILE - PX, fy, PX, TILE}, mix(base, BLACK, 0.18f));
            // Details: a crack or a few pebbles on some tiles
            if (h % 7 == 0) {
                float cx = fx + (h >> 8) % 10 * PX + 3 * PX, cy = fy + (h >> 12) % 10 * PX + 3 * PX;
                Color c = mix(base, BLACK, 0.35f);
                DrawRectangleRec({cx, cy, 3 * PX, PX}, c);
                DrawRectangleRec({cx + 2 * PX, cy + PX, 2 * PX, PX}, c);
            } else if (h % 11 == 0) {
                DrawRectangleRec({fx + (h >> 9) % 12 * PX + 2 * PX, fy + (h >> 14) % 12 * PX + 2 * PX, PX, PX}, mix(base, th.wallTop, 0.35f));
            }
            // Ambient occlusion under walls
            if (isBlock(dun, x, y - 1)) {
                DrawRectangleRec({fx, fy, TILE, 3 * PX}, Fade(BLACK, 0.35f));
                DrawRectangleRec({fx, fy + 3 * PX, TILE, 2 * PX}, Fade(BLACK, 0.15f));
            }
        }

    // Hatch to the next floor sits flat on the floor
    if (hatchActive) {
        float pulse = 0.5f + 0.5f * sinf(titleT * 4);
        DrawCircleV(hatchPos, 34, OUTLINE);
        DrawCircleV(hatchPos, 31, th.wall);
        DrawCircleV(hatchPos, 26, {8, 6, 10, 255});
        for (int k = 0; k < 3; k++) DrawRectangleRec({hatchPos.x - 18, hatchPos.y - 14 + k * 11, 36, 2 * PX}, th.wallTop);
        DrawRing(hatchPos, 34, 34 + PX + pulse * PX, 0, 360, 48, Fade(th.accent, 0.5f + 0.5f * pulse));
    }
}

// ============================================================ walls (three-quarter view)
// A wall block is drawn as a top cap raised above its tile plus, where floor is in
// front of it, a tall brick front face. Its base is the bottom of its tile, which
// is what the row-by-row drawing order uses for depth.
void Game::drawWallTile(int x, int y) {
    if (!isBlock(dun, x, y)) return;
    const Theme& th = theme();
    Tile t = dun.tileAt(x, y);
    bool tall = tallFace(dun, x, y);
    bool door = t == Tile::Door;
    float fx = x * TILE, fy = y * TILE;
    float capY = tall ? fy - TILE : fy - TILE * 0.5f;
    unsigned h = hash2(x, y);

    if (tall) {
        // Brick front face: 2 courses of 8 px, staggered joints
        Color face = mix(th.wall, BLACK, (h % 3) * 0.04f);
        DrawRectangleRec({fx, fy, TILE, TILE}, face);
        Color mortar = mix(th.wall, BLACK, 0.35f);
        DrawRectangleRec({fx, fy + 8 * PX, TILE, PX}, mortar);
        float off = (y & 1) ? 0 : 4 * PX;
        DrawRectangleRec({fx + off + 2 * PX, fy, PX, 8 * PX}, mortar);
        DrawRectangleRec({fx + off + 10 * PX, fy, PX, 8 * PX}, mortar);
        DrawRectangleRec({fx + 6 * PX - off * 0.5f, fy + 9 * PX, PX, 7 * PX}, mortar);
        DrawRectangleRec({fx + 14 * PX - off * 0.5f, fy + 9 * PX, PX, 7 * PX}, mortar);
        DrawRectangleRec({fx, fy, TILE, PX}, Fade(BLACK, 0.45f));                   // under the cap's lip
        DrawRectangleRec({fx, fy + TILE - PX, TILE, PX}, mix(th.wall, BLACK, 0.55f)); // where it meets the floor
        if (door) {   // portcullis
            Color bar = mix(th.accent, RED, 0.45f);
            DrawRectangleRec({fx, fy, TILE, TILE}, {58, 22, 26, 255});
            for (int k = 0; k < 4; k++) DrawRectangleRec({fx + (1 + k * 4) * PX, fy, 2 * PX, TILE}, bar);
            DrawRectangleRec({fx, fy + 6 * PX, TILE, PX}, bar);
        }
    } else if (!isBlock(dun, x, y + 1)) {
        // Back side of a wall facing away from us: just a dark strip below the cap
        DrawRectangleRec({fx, capY + TILE, TILE, fy + TILE - (capY + TILE)}, mix(th.wall, BLACK, 0.6f));
    }

    // Top cap
    Color top = t == Tile::Pillar ? mix(th.wallTop, WHITE, 0.06f) : th.wallTop;
    top = mix(top, BLACK, (h >> 4) % 3 * 0.03f);
    DrawRectangleRec({fx, capY, TILE, TILE}, top);
    DrawRectangleRec({fx, capY + TILE - PX, TILE, PX}, mix(top, WHITE, 0.22f));   // lit front edge
    if (!isBlock(dun, x, y - 1)) DrawRectangleRec({fx, capY, TILE, PX}, mix(top, BLACK, 0.3f));
    if (!isBlock(dun, x - 1, y)) DrawRectangleRec({fx, capY, PX, TILE}, mix(top, BLACK, 0.3f));
    if (!isBlock(dun, x + 1, y)) DrawRectangleRec({fx + TILE - PX, capY, PX, TILE}, mix(top, BLACK, 0.3f));
    if (door && !tall) {
        Color bar = mix(th.accent, RED, 0.45f);
        for (int k = 0; k < 4; k++) DrawRectangleRec({fx + (1 + k * 4) * PX, capY + 2 * PX, 2 * PX, TILE - 4 * PX}, bar);
    }

    // Brazier bracket on this face (the flame itself is drawn in the emissive pass)
    if (tall && !door)
        for (Vector2 b : braziers)
            if (fabsf(b.x - (fx + TILE / 2)) < 1 && fabsf(b.y - (fy + TILE * 0.55f)) < 1) {
                oRect({b.x - 4 * PX, b.y - 2 * PX, 8 * PX, 3 * PX}, {}, 0);
                oRect({b.x - PX, b.y + PX, 2 * PX, 4 * PX}, {}, 0);
                oRect({b.x - 4 * PX, b.y - 2 * PX, 8 * PX, 3 * PX}, {54, 50, 58, 255}, 1);
                oRect({b.x - PX, b.y + PX, 2 * PX, 4 * PX}, {40, 36, 44, 255}, 1);
            }
}

// ============================================================ depth-sorted world
// Rows are drawn top to bottom: first the wall blocks of a row, then every actor
// whose feet are in that row. Actors north of a wall or pillar are therefore
// covered by its raised cap; actors south of it stand in front of its face.
void Game::drawWorldSorted(int x0, int y0, int x1, int y1) {
    struct Item { float feet; int kind; int idx; };   // kind: 0 player, 1 enemy, 2 pickup, 3 chest
    std::vector<Item> items;
    if (state != State::Dead) items.push_back({pl.pos.y + pl.r, 0, 0});
    for (int i = 0; i < (int)enemies.size(); i++) items.push_back({enemies[i].pos.y + enemies[i].r, 1, i});
    for (int i = 0; i < (int)pickups.size(); i++) items.push_back({pickups[i].pos.y + 12, 2, i});
    for (int i = 0; i < (int)dun.rooms.size(); i++)
        if (dun.rooms[i].type == RoomType::Chest) items.push_back({dun.roomCenter(i).y + 18, 3, i});
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.feet < b.feet; });

    auto drawItem = [&](const Item& it) {
        for (int pass = 0; pass < 2; pass++) {
            switch (it.kind) {
                case 0: drawPlayer(pass); break;
                case 1: drawEnemy(enemies[it.idx], pass); break;
                case 2: drawPickup(pickups[it.idx], pass); break;
                case 3: drawChest(it.idx, pass); break;
            }
        }
    };

    size_t next = 0;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) drawWallTile(x, y);
        float rowEnd = (y + 1) * TILE;
        while (next < items.size() && items[next].feet <= rowEnd) drawItem(items[next++]);
    }
    while (next < items.size()) drawItem(items[next++]);
}

// ============================================================ actors
void Game::drawChest(int room, int pass) {
    const Theme& th = theme();
    Vector2 c = dun.roomCenter(room);
    Color wood{132, 84, 48, 255}, dark = mix(wood, BLACK, 0.35f);
    if (pass == 1) shadow({c.x, c.y + 18}, 30);
    if (dun.rooms[room].chestOpened) {
        oRect({c.x - 24, c.y - 8, 48, 26}, dark, pass);
        oRect({c.x - 24, c.y - 30, 48, 12}, wood, pass);
        if (pass == 1) DrawRectangleRec({c.x - 20, c.y - 6, 40, 6}, {20, 14, 12, 255});
    } else {
        float bob = roundf(sinf(titleT * 3) * 1.5f) * PX;
        oRect({c.x - 24, c.y - 16 + bob, 48, 34}, wood, pass);
        if (pass == 1) {
            DrawRectangleRec({c.x - 24, c.y - 16 + bob, 48, 3 * PX}, mix(wood, WHITE, 0.2f));
            DrawRectangleRec({c.x - 24, c.y - 2 + bob, 48, 2 * PX}, th.accent);
            DrawRectangleRec({c.x - 2 * PX, c.y - 5 * PX + bob, 4 * PX, 5 * PX}, mix(th.accent, WHITE, 0.45f));
        }
    }
}

void Game::drawPickup(const Pickup& p, int pass) {
    Vector2 pos{p.pos.x, p.pos.y + roundf(sinf(p.t * 4) * 1.2f) * PX};
    if (pass == 1) shadow({p.pos.x, p.pos.y + 12}, 9);
    Color c = pickupColor(p.type);
    switch (p.type) {
        case PickType::Heart:
            if (pass == 0) { DrawCircleV({pos.x - 5, pos.y - 3}, 8.5f, OUTLINE); DrawCircleV({pos.x + 5, pos.y - 3}, 8.5f, OUTLINE);
                             DrawTriangle({pos.x - 13.5f, pos.y - 1}, {pos.x, pos.y + 14}, {pos.x + 13.5f, pos.y - 1}, OUTLINE); }
            else drawHeart(pos.x, pos.y - 3, 0.7f, 2);
            break;
        case PickType::Ammo:
            oRect({pos.x - 10, pos.y - 8, 20, 15}, {92, 98, 62, 255}, pass);
            if (pass == 1) DrawRectangleRec({pos.x - 10, pos.y - 2, 20, 2 * PX}, c);
            break;
        case PickType::Blank:
            oCircle(pos, 9, c, pass);
            if (pass == 1) DrawRing(pos, 4, 4 + PX, 0, 360, 16, WHITE);
            break;
    }
}

void Game::drawEnemy(const Enemy& e, int pass) {
    Color base = enemyColor(e);
    if (e.spawnT > 0) {   // summoning circle
        if (pass == 1) {
            float k = e.spawnT / def(e).spawnDelay;
            DrawCircleLinesV(e.pos, e.r + k * 34, Fade(theme().accent, 1.0f - k));
            DrawCircleV(e.pos, e.r * (1.0f - k), Fade(base, 0.5f));
        }
        return;
    }
    Color c = mix(base, WHITE, e.flash);
    float aimA = angleTo(e.pos, pl.pos);
    if (pass == 1) shadow({e.pos.x, e.pos.y + e.r * 0.85f}, e.r);

    auto eyes = [&](float spacing) {
        if (pass == 0) return;
        Vector2 fwd = fromAngle(aimA, e.r * 0.3f);
        for (int s = -1; s <= 1; s += 2) {
            Vector2 ep{e.pos.x + fwd.x + s * spacing, e.pos.y + fwd.y - e.r * 0.2f};
            DrawRectangleRec({ep.x - PX, ep.y - PX, 2 * PX, 2 * PX}, RAYWHITE);
            DrawRectangleRec({ep.x - PX + (cosf(aimA) > 0 ? PX : 0), ep.y - PX + (sinf(aimA) > 0 ? PX : 0), PX, PX}, OUTLINE);
        }
    };

    switch (e.shape) {
        case Shape::Round:
            oBar(e.pos, -2, 18, 6, aimA * RAD2DEG, {58, 56, 64, 255}, pass);
            oCircle(e.pos, e.r, c, pass);
            if (pass == 1) DrawCircleV({e.pos.x - e.r * 0.3f, e.pos.y - e.r * 0.4f}, e.r * 0.35f, mix(c, WHITE, 0.25f));
            eyes(5);
            break;
        case Shape::Square:
            oBar(e.pos, -2, 22, 9, aimA * RAD2DEG, {66, 56, 52, 255}, pass);
            oRect({e.pos.x - e.r, e.pos.y - e.r, e.r * 2, e.r * 2}, c, pass);
            if (pass == 1) {
                DrawRectangleRec({e.pos.x - e.r, e.pos.y - e.r, e.r * 2, PX}, mix(c, WHITE, 0.3f));
                DrawRectangleRec({e.pos.x - e.r, e.pos.y + e.r * 0.3f, e.r * 2, 2 * PX}, mix(c, BLACK, 0.3f));
            }
            eyes(6);
            break;
        case Shape::Hexagon:
            oPoly(e.pos, 6, e.r * 1.15f, e.spin * RAD2DEG, c, pass);
            if (pass == 1) {
                DrawPoly(e.pos, 6, e.r * 0.65f, -e.spin * RAD2DEG, mix(c, BLACK, 0.35f));
                DrawRectangleRec({e.pos.x - PX, e.pos.y - PX, 2 * PX, 2 * PX}, RAYWHITE);
            }
            break;
        case Shape::Warden: {
            for (int k = 0; k < 8; k++) {
                float a = e.spin + k * PI / 4;
                oPoly(Vector2Add(e.pos, fromAngle(a, e.r + 6)), 3, 11, a * RAD2DEG, mix(c, BLACK, 0.25f), pass);
            }
            oPoly(e.pos, 8, e.r, e.spin * 20, c, pass);
            if (pass == 1) {
                DrawPoly(e.pos, 8, e.r - 2 * PX, e.spin * 20, mix(c, BLACK, 0.15f));
                DrawCircleV(e.pos, e.r * 0.55f, {24, 20, 30, 255});
                Color iris = e.enraged ? Color{255, 80, 60, 255} : theme().accent;
                Vector2 pupil = Vector2Add(e.pos, fromAngle(aimA, e.r * 0.2f));
                DrawCircleV(pupil, e.r * 0.3f, iris);
                DrawCircleV(pupil, e.r * 0.13f, {10, 8, 12, 255});
            }
        } break;
    }

    // Wind-up tell: a pixel "!" above the head
    bool tell = e.type == EType::Shooter ? e.shootT < 0.3f
              : e.type == EType::Spinner ? (e.burstT <= 0 && e.shootT < 0.4f) : false;
    if (tell) {
        float tx = roundf(e.pos.x / PX) * PX, ty = e.pos.y - e.r - 10 * PX;
        oRect({tx - PX, ty, 2 * PX, 4 * PX}, {255, 230, 90, 255}, pass);
        oRect({tx - PX, ty + 5 * PX, 2 * PX, 2 * PX}, {255, 230, 90, 255}, pass);
    }

    if (e.type != EType::Warden && e.hp < e.maxHp) {
        float w = e.r * 2, by = e.pos.y - e.r - 4 * PX;
        oRect({e.pos.x - w / 2, by, w, PX}, {50, 20, 26, 255}, pass);
        if (pass == 1) DrawRectangleRec({e.pos.x - w / 2, by, w * e.hp / e.maxHp, PX}, {255, 90, 90, 255});
    }
}

void Game::drawPlayer(int pass) {
    if (pl.invuln > 0 && fmodf(titleT, 0.12f) < 0.06f) return;   // i-frame flicker
    // The Deserter's palette, from docs/concept/deserter-sheet.png
    const Color cloak{53, 70, 103, 255}, cloakDark{38, 50, 77, 255}, face{227, 180, 140, 255}, gold{194, 149, 72, 255};
    const bool sprite = playerSprite.id != 0;
    const float feet = pl.pos.y + pl.r;
    if (pass == 1) shadow({pl.pos.x, pl.pos.y + pl.r * 0.9f}, pl.r);

    if (pl.rollT > 0) {
        float k = 1.0f - pl.rollT / content().rules.rollTime;
        if (pass == 1)
            for (int i = 1; i <= 3; i++)
                DrawCircleV(Vector2Subtract(pl.pos, Vector2Scale(pl.rollDir, i * 9.0f)), pl.r * 0.9f, Fade(cloak, 0.22f * (4 - i)));
        float squash = 0.75f + 0.25f * fabsf(cosf(k * PI * 2));
        float rx = pl.r * (2 - squash) * 1.1f, ry = pl.r * squash * 1.1f;
        if (pass == 0) DrawEllipse((int)pl.pos.x, (int)pl.pos.y, rx + OL, ry + OL, OUTLINE);
        else {
            DrawEllipse((int)pl.pos.x, (int)pl.pos.y, rx, ry, cloak);
            DrawEllipse((int)pl.pos.x, (int)(pl.pos.y - ry * 0.3f), rx * 0.5f, ry * 0.35f, mix(cloak, WHITE, 0.15f));
        }
        return;
    }

    const GunDef& d = gunDef();
    float back = pl.recoil * 4;
    float s = sinf(pl.aim), c = cosf(pl.aim);
    // Facing: 0 = toward the camera, 1 = away (back view), 2 = side (mirrored when aiming left)
    int facing = s < -0.45f ? 1 : s > 0.6f ? 0 : 2;
    bool behind = facing == 1;              // aiming up: gun goes behind the body
    Vector2 hand = handPos();
    auto drawGun = [&] {
        oBar(hand, -(6 - back), d.length + 4, 3 * PX, pl.aim * RAD2DEG, {196, 198, 210, 255}, pass);
        if (pass == 1) oBar(hand, -(6 - back) - d.length * 0.5f, d.length * 0.5f, PX, pl.aim * RAD2DEG, d.color, 1);
    };
    if (behind) drawGun();

    if (sprite) {
        // 16x24 frames; walk cycle: step A, idle, step B, idle at 8 frames a second
        static const int CYCLE[4] = {1, 0, 2, 0};
        int frame = pl.moving ? CYCLE[(int)(pl.walkT * 8) % 4] : 0;
        bool flip = facing == 2 && c < 0;
        Rectangle src{frame * 16.0f, facing * 24.0f, flip ? -16.0f : 16.0f, 24.0f};
        float x = roundf(pl.pos.x / PX) * PX - 8 * PX, y = roundf(feet / PX) * PX - 24 * PX;
        if (pass == 0) {
            const Vector2 offs[4] = {{-OL, 0}, {OL, 0}, {0, -OL}, {0, OL}};
            for (Vector2 o : offs)
                DrawTexturePro(playerSprite, src, {x + o.x, y + o.y, 16 * PX, 24 * PX}, {0, 0}, 0, OUTLINE);
        } else {
            DrawTexturePro(playerSprite, src, {x, y, 16 * PX, 24 * PX}, {0, 0}, 0, WHITE);
        }
    } else {
        // Fallback when the sprite file is missing: the shape-drawn hooded gunner
        oCircle({pl.pos.x, pl.pos.y + 1}, pl.r * 1.05f, cloak, pass);
        Vector2 head{roundf((pl.pos.x + c * 2) / PX) * PX, pl.pos.y - pl.r * 1.0f};
        oCircle(head, pl.r * 0.82f, cloakDark, pass);
        if (pass == 1) {
            DrawCircleV({pl.pos.x - pl.r * 0.35f, pl.pos.y - pl.r * 0.2f}, pl.r * 0.45f, mix(cloak, WHITE, 0.18f));
            DrawRectangleRec({pl.pos.x - pl.r, pl.pos.y + pl.r * 0.55f, pl.r * 2, 2 * PX}, cloakDark);
            DrawRectangleRec({pl.pos.x - PX, pl.pos.y - pl.r * 0.35f, 2 * PX, 2 * PX}, gold);
            if (!behind) {
                float side = c > 0 ? 1.0f : -1.0f;
                Vector2 f{head.x + side * 2 * PX, head.y + PX};
                DrawRectangleRec({f.x - 3 * PX, f.y - 2 * PX, 6 * PX, 4 * PX}, face);
                DrawRectangleRec({f.x - 2 * PX + side * PX, f.y - PX, PX, 2 * PX}, OUTLINE);
                DrawRectangleRec({f.x + side * PX + PX, f.y - PX, PX, 2 * PX}, OUTLINE);
            }
        }
    }
    if (!behind) drawGun();

    if (pl.reloadT > 0) {
        float k = 1.0f - pl.reloadT / d.reload;
        float top = sprite ? roundf(feet / PX) * PX - 27 * PX : pl.pos.y - pl.r * 2.2f;
        Rectangle bar{pl.pos.x - 8 * PX, top, 16 * PX, PX};
        oRect(bar, {50, 50, 60, 255}, pass);
        if (pass == 1) DrawRectangleRec({bar.x, bar.y, bar.width * k, PX}, RAYWHITE);
    }
}

// ============================================================ lighting
void Game::drawLights(const Camera2D& view) {
    const Theme& th = theme();
    auto light = [&](Vector2 p, float radius, Color c, float intensity) {
        DrawTexturePro(lightTex, {0, 0, (float)lightTex.width, (float)lightTex.height},
                       {p.x - radius, p.y - radius, radius * 2, radius * 2}, {0, 0}, 0, Fade(c, Clamp(intensity, 0, 1)));
    };
    // Light value 128 = neutral (2x multiply); the ambient is darker so rooms are moody
    Color ambient = mix({46, 43, 62, 255}, th.wallTop, 0.2f);

    BeginTextureMode(lightMap);
    ClearBackground(ambient);
    BeginMode2D(view);
    BeginBlendMode(BLEND_ADDITIVE);

    if (state != State::Dead) light(pl.pos, 280, {255, 236, 205, 255}, 0.5f);
    Color flame = mix(th.accent, {255, 150, 60, 255}, 0.35f);
    for (size_t i = 0; i < braziers.size(); i++) {
        float flick = 1.0f + 0.07f * sinf(titleT * 11 + i * 1.7f) + 0.04f * sinf(titleT * 23 + i);
        light({braziers[i].x, braziers[i].y + 60}, 320 * flick, flame, 0.42f);
    }
    if (muzzleT > 0) light(muzzlePos, 220, muzzleCol, muzzleT / 0.07f * 0.8f);
    if (blankR >= 0) light(blankPos, blankR, {120, 180, 255, 255}, 0.6f * (1.0f - blankR / 1000.0f));
    if (hatchActive) light(hatchPos, 160, th.accent, 0.5f);
    for (int i = 0; i < (int)dun.rooms.size(); i++)
        if (dun.rooms[i].type == RoomType::Chest && !dun.rooms[i].chestOpened) light(dun.roomCenter(i), 120, th.accent, 0.4f);
    for (auto& p : pickups) light(p.pos, 60, pickupColor(p.type), 0.35f);
    for (auto& e : enemies)
        if (e.type == EType::Warden && e.spawnT <= 0) light(e.pos, 130, e.enraged ? Color{255, 80, 60, 255} : th.accent, 0.18f);
    for (auto& b : bullets) light(b.pos, b.fromPlayer ? 40 : 46, b.col, b.fromPlayer ? 0.3f : 0.2f);

    EndBlendMode();
    EndMode2D();
    EndTextureMode();
}

// ============================================================ emissive
void Game::drawEmissive() {
    const Theme& th = theme();

    // Brazier flames: flickering pixel tongues in the floor's accent colour
    for (size_t i = 0; i < braziers.size(); i++) {
        Vector2 b = braziers[i];
        float t = titleT * 9 + i * 2.3f;
        float h1 = 5 + roundf(sinf(t) * 1.2f), h2 = 4 + roundf(sinf(t * 1.3f + 1) * 1.2f);
        Color outer = mix(th.accent, {255, 120, 40, 255}, 0.3f), inner = mix(th.accent, {255, 250, 220, 255}, 0.6f);
        DrawRectangleRec({b.x - 3 * PX, b.y - 2 * PX - h1 * PX, 3 * PX, h1 * PX}, outer);
        DrawRectangleRec({b.x, b.y - 2 * PX - h2 * PX, 3 * PX, h2 * PX}, outer);
        DrawRectangleRec({b.x - PX, b.y - 2 * PX - (h1 - 1) * PX, 2 * PX, (h1 - 2) * PX}, inner);
    }

    // Bullets: dark outline, bright body, white-hot core
    for (auto& b : bullets) {
        if (b.fromPlayer) {
            Vector2 tail = Vector2Subtract(b.pos, Vector2Scale(b.vel, 0.02f));
            DrawLineEx(tail, b.pos, b.r * 1.2f, Fade(b.col, 0.45f));
            DrawCircleV(b.pos, b.r + OL, OUTLINE);
            DrawCircleV(b.pos, b.r, b.col);
            DrawCircleV(b.pos, b.r * 0.45f, WHITE);
        } else {
            float pulse = b.splitT > 0 ? 1.0f + 0.15f * sinf(b.splitT * 30) : 1.0f;
            DrawCircleV(b.pos, b.r * pulse + OL, OUTLINE);
            DrawCircleV(b.pos, b.r * pulse, b.col);
            DrawCircleV(b.pos, b.r * 0.5f * pulse, {255, 236, 240, 255});
        }
    }

    // Sparks, embers, smoke
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto& p : parts) {
        float k = p.life / p.maxLife;
        float s = std::max(PX * 0.6f, p.size * (0.4f + 0.6f * k));
        DrawRectangleRec({roundf(p.pos.x / PX) * PX - s / 2, roundf(p.pos.y / PX) * PX - s / 2, s, s}, Fade(p.col, k));
    }
    if (muzzleT > 0) DrawCircleV(muzzlePos, 3 * PX * (muzzleT / 0.07f) + PX, Fade(muzzleCol, 0.9f));
    EndBlendMode();

    if (blankR >= 0)
        DrawRing(blankPos, std::max(0.0f, blankR - 3 * PX), blankR, 0, 360, 72, Fade({140, 200, 255, 255}, 1.0f - blankR / 1000.0f));
}

// Text prompts are drawn at screen resolution so they stay readable
void Game::drawPrompts() {
    if (state != State::Playing) return;
    for (int i = 0; i < (int)dun.rooms.size(); i++) {
        if (dun.rooms[i].type != RoomType::Chest || dun.rooms[i].chestOpened || curRoom != i) continue;
        Vector2 c = dun.roomCenter(i);
        if (Vector2Distance(pl.pos, c) < 75) {
            Vector2 s = worldToScreen({c.x, c.y - 44});
            textCentered("[E] Open", (int)s.x, (int)s.y, 20, RAYWHITE);
        }
    }
    if (hatchActive && Vector2Distance(pl.pos, hatchPos) < 60) {
        Vector2 s = worldToScreen({hatchPos.x, hatchPos.y - 60});
        textCentered("[E] Descend", (int)s.x, (int)s.y, 20, RAYWHITE);
    }
}

}  // namespace valor
