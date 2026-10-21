// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
//  A top-down bullet-hell roguelike in C++17 + raylib.
//
//  Dodge-roll through bullet patterns, clear locked rooms, find guns in chests,
//  beat the Warden at the bottom of each floor, and descend. Five floors.
//
//  Everything (art and sound) is generated in code — no asset files.
// ============================================================================

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------- constants
constexpr int   SCREEN_W     = 1280;
constexpr int   SCREEN_H     = 720;
constexpr float TILE         = 40.0f;
constexpr int   ROOM_W       = 23;          // tiles, including the wall border
constexpr int   ROOM_H       = 15;
constexpr int   GRID         = 7;           // floor layout is a GRID x GRID grid of room slots
constexpr int   MAP_W        = GRID * ROOM_W;
constexpr int   MAP_H        = GRID * ROOM_H;
constexpr int   FINAL_FLOOR  = 5;

constexpr float PLAYER_SPEED = 230.0f;
constexpr float ROLL_TIME    = 0.38f;
constexpr float ROLL_SPEED   = 470.0f;
constexpr float ROLL_IFRAME_END = 0.08f;    // i-frames for all of the roll except the last 0.08s
constexpr float HIT_INVULN   = 1.2f;

constexpr int DX[4] = {1, 0, -1, 0};        // right, down, left, up
constexpr int DY[4] = {0, 1, 0, -1};

// ---------------------------------------------------------------- random
std::mt19937 rng{std::random_device{}()};
float frand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }
int   irand(int a, int b)     { return std::uniform_int_distribution<int>(a, b)(rng); }
bool  chance(float p)         { return frand(0.0f, 1.0f) < p; }

// ---------------------------------------------------------------- math helpers
Vector2 fromAngle(float a, float len = 1.0f) { return {cosf(a) * len, sinf(a) * len}; }
float   angleTo(Vector2 a, Vector2 b)        { return atan2f(b.y - a.y, b.x - a.x); }
Color   mix(Color a, Color b, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return {(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
            (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
void textCentered(const char* s, int cx, int y, int size, Color c) {
    DrawText(s, cx - MeasureText(s, size) / 2, y, size, c);
}

// ---------------------------------------------------------------- data types
enum class Tile : unsigned char { Void, Floor, Wall, Door, Pillar };
enum class RoomType { Start, Combat, Chest, Boss };
enum class State { Title, Playing, Paused, Dead, Won };
enum class EType { Grunt, Scatter, Whirl, Warden };
enum class PickType { Heart, Ammo, Blank };

struct Room {
    int gx = 0, gy = 0;
    RoomType type = RoomType::Combat;
    bool links[4] = {};
    bool visited = false, cleared = false, locked = false, chestOpened = false;
    int wavesLeft = 0;
    int depth = 0;
};

struct GunDef {
    const char* name;
    float fireDelay;
    int   mag;
    int   maxAmmo;      // reserve; -1 = infinite
    float reload;
    int   pellets;
    float spread;       // radians
    float speed;
    float damage;
    float radius;
    bool  pierce;
    Color color;
};

const GunDef GUNS[] = {
    {"Rusty Sidearm", 0.22f,  8,  -1, 0.9f, 1, 0.04f,  720.0f,  6.0f, 5.0f, false, {255, 220, 120, 255}},
    {"Scattergun",    0.65f,  6,  60, 1.3f, 6, 0.38f,  620.0f,  4.5f, 4.5f, false, {255, 160,  80, 255}},
    {"Riveter",       0.07f, 32, 320, 1.4f, 1, 0.12f,  820.0f,  3.2f, 4.0f, false, {190, 255, 130, 255}},
    {"Railspike",     0.90f,  4,  36, 1.6f, 1, 0.00f, 1300.0f, 28.0f, 6.0f, true,  {140, 220, 255, 255}},
};
constexpr int GUN_COUNT = sizeof(GUNS) / sizeof(GUNS[0]);

struct Gun { int def; int mag; int ammo; };

struct Player {
    Vector2 pos{};
    float r = 13.0f;
    int hp = 6, maxHp = 6;          // half-hearts
    int blanks = 2;
    float rollT = 0, rollCd = 0, invuln = 0;
    Vector2 rollDir{1, 0};
    std::vector<Gun> guns;
    int cur = 0;
    float fireCd = 0, reloadT = 0, recoil = 0, emptyT = 0;
    float aim = 0;
};

struct Bullet {
    Vector2 pos{}, vel{};
    float r = 6, dmg = 1;
    bool fromPlayer = false, pierce = false;
    int id = 0;
    float life = 6.0f;
    float splitT = -1.0f;           // > 0: bursts into a ring when it reaches 0
    Color col = WHITE;
    bool alive = true;
};

struct Enemy {
    EType type = EType::Grunt;
    Vector2 pos{};
    float r = 15, hp = 10, maxHp = 10, speed = 80;
    float shootT = 1, spawnT = 0.9f, flash = 0, moveT = 0;
    float burstT = 0, tick = 0, ang = 0, spin = 0;
    Vector2 dir{1, 0};
    int lastPierce = -1;
    bool alive = true;
    // boss state
    int pattern = -1, lastPattern = -1, count = 0;
    float patT = 0, restT = 1.6f, spiral = 0;
    bool enraged = false;
};

struct Particle { Vector2 pos, vel; float life, maxLife, size; Color col; };
struct Pickup   { Vector2 pos; PickType type; float t = 0; bool alive = true; };

struct Theme { Color floorA, floorB, wall, wallTop, accent, bg; };
const Theme THEMES[FINAL_FLOOR] = {
    {{46, 44, 56, 255}, {50, 48, 61, 255}, {62, 56, 74, 255}, {108, 100, 126, 255}, {255, 200,  90, 255}, {14, 13, 18, 255}},
    {{34, 46, 42, 255}, {38, 51, 46, 255}, {48, 66, 58, 255}, { 88, 118, 102, 255}, {150, 255, 170, 255}, {10, 15, 13, 255}},
    {{52, 42, 34, 255}, {57, 46, 37, 255}, {76, 60, 46, 255}, {130, 106,  82, 255}, {255, 170,  90, 255}, {16, 12,  9, 255}},
    {{56, 32, 32, 255}, {62, 36, 35, 255}, {84, 44, 40, 255}, {148,  80,  68, 255}, {255, 120,  80, 255}, {18,  8,  8, 255}},
    {{28, 30, 52, 255}, {32, 34, 58, 255}, {46, 44, 86, 255}, { 90,  86, 148, 255}, {170, 150, 255, 255}, { 8,  8, 18, 255}},
};
const char* FLOOR_NAMES[FINAL_FLOOR] = {
    "The Breach Halls", "The Rusted Gutters", "The Cinder Mines", "The Hollow Forge", "The Gunmetal Abyss"};

const Color ENEMY_BULLET = {255, 80, 110, 255};

// ---------------------------------------------------------------- procedural sound
// Every sound effect is synthesised at startup into a raylib Wave.
enum class Osc { Square, Noise, Triangle };

Wave synthWave(float f0, float f1, float dur, Osc osc, float vol) {
    const int rate = 22050;
    const int n = std::max(1, (int)(dur * rate));
    auto* data = (short*)MemAlloc(n * sizeof(short));
    float phase = 0, held = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        float f = f0 + (f1 - f0) * t;
        float prev = phase;
        phase += f / rate;
        phase -= floorf(phase);
        float s;
        switch (osc) {
            case Osc::Square:   s = phase < 0.5f ? 1.0f : -1.0f; break;
            case Osc::Noise:    if (phase < prev) held = frand(-1, 1); s = held; break;
            default:            s = 4.0f * fabsf(phase - 0.5f) - 1.0f; break;
        }
        float env = (1.0f - t) * (1.0f - t);
        float attack = std::min(1.0f, i / (rate * 0.004f));
        data[i] = (short)(s * env * attack * vol * 30000.0f);
    }
    Wave w{};
    w.frameCount = (unsigned)n;
    w.sampleRate = rate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = data;
    return w;
}

struct Sfx {
    std::vector<Sound> voices;      // voices[0] owns the data, the rest are aliases (polyphony)
    int next = 0;
    void load(float f0, float f1, float dur, Osc osc, float vol, int polyphony = 4) {
        if (!IsAudioDeviceReady()) return;
        Wave w = synthWave(f0, f1, dur, osc, vol);
        voices.push_back(LoadSoundFromWave(w));
        UnloadWave(w);
        for (int i = 1; i < polyphony; i++) voices.push_back(LoadSoundAlias(voices[0]));
    }
    void play(float pitch = 1.0f, float volume = 1.0f) {
        if (voices.empty()) return;
        Sound& s = voices[next];
        next = (next + 1) % (int)voices.size();
        SetSoundPitch(s, pitch);
        SetSoundVolume(s, volume);
        PlaySound(s);
    }
    void unload() {
        for (size_t i = 1; i < voices.size(); i++) UnloadSoundAlias(voices[i]);
        if (!voices.empty()) UnloadSound(voices[0]);
        voices.clear();
    }
};

// ============================================================================
//  GAME
// ============================================================================
struct Game {
    State state = State::Title;
    bool quit = false;
    bool debug = false, god = false;   // --debug: F1 god mode, F2 clear room, F3 jump to boss
    int floor = 1;

    // level
    std::vector<Tile> tiles;
    std::vector<int>  doorOwner;    // which room's lock controls this door tile
    std::vector<Room> rooms;
    std::vector<int>  roomAt;       // GRID*GRID -> room index or -1
    int curRoom = -1;
    float waveDelay = 0;
    bool hatchActive = false;
    Vector2 hatchPos{};

    // entities
    Player pl;
    std::vector<Bullet>   bullets, pendingBullets;
    std::vector<Enemy>    enemies;
    std::vector<Particle> parts;
    std::vector<Pickup>   pickups;

    // presentation
    Camera2D cam{};
    float shake = 0, hurtFlash = 0, blankR = -1, eShotSfxT = 0;
    Vector2 blankPos{};
    std::string banner;
    float bannerT = 0;
    float titleT = 0;

    // stats
    int nextBulletId = 1, kills = 0;
    float runTime = 0;

    Sfx sShoot, sShotgun, sRail, sEShoot, sHit, sDie, sHurt, sBlank, sPickup, sRoll, sDoor, sChest, sEmpty, sBoom;

    const Theme& theme() const { return THEMES[std::clamp(floor - 1, 0, FINAL_FLOOR - 1)]; }

    // ------------------------------------------------------------ lifecycle
    void init() {
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

    void unload() {
        for (Sfx* s : {&sShoot, &sShotgun, &sRail, &sEShoot, &sHit, &sDie, &sHurt, &sBlank,
                       &sPickup, &sRoll, &sDoor, &sChest, &sEmpty, &sBoom})
            s->unload();
    }

    void newRun() {
        floor = 1;
        pl = Player{};
        pl.guns = {{0, GUNS[0].mag, -1}};
        kills = 0;
        runTime = 0;
        buildFloor();
        state = State::Playing;
    }

    void buildFloor() {
        generateFloor();
        bullets.clear();
        pendingBullets.clear();
        enemies.clear();
        parts.clear();
        pickups.clear();
        hatchActive = false;
        waveDelay = 0;
        pl.pos = roomCenter(0);
        pl.blanks = std::max(pl.blanks, 2);
        pl.rollT = 0;
        curRoom = 0;
        rooms[0].visited = true;
        cam.target = pl.pos;
        showBanner(TextFormat("FLOOR %d  -  %s", floor, FLOOR_NAMES[floor - 1]), 3.0f);
    }

    void showBanner(const std::string& s, float t) { banner = s; bannerT = t; }

    // ------------------------------------------------------------ level generation
    void generateFloor() {
        tiles.assign(MAP_W * MAP_H, Tile::Void);
        doorOwner.assign(MAP_W * MAP_H, -1);
        rooms.clear();
        roomAt.assign(GRID * GRID, -1);

        auto addRoom = [&](int gx, int gy, int depth) {
            Room r;
            r.gx = gx; r.gy = gy; r.depth = depth;
            rooms.push_back(r);
            roomAt[gy * GRID + gx] = (int)rooms.size() - 1;
            return (int)rooms.size() - 1;
        };

        // Grow a tree of rooms from the centre. Tree = exactly one path between rooms,
        // so "depth" is the true distance from the start and the boss sits at the far end.
        const int target = std::min(7 + floor * 2, 16);
        addRoom(GRID / 2, GRID / 2, 0);
        for (int guard = 0; (int)rooms.size() < target && guard < 5000; guard++) {
            int i = irand(0, (int)rooms.size() - 1);
            int d = irand(0, 3);
            int nx = rooms[i].gx + DX[d], ny = rooms[i].gy + DY[d];
            if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID || roomAt[ny * GRID + nx] != -1) continue;
            int neighbours = 0;
            for (int k = 0; k < 4; k++) {
                int ax = nx + DX[k], ay = ny + DY[k];
                if (ax >= 0 && ay >= 0 && ax < GRID && ay < GRID && roomAt[ay * GRID + ax] != -1) neighbours++;
            }
            if (neighbours > 1 && !chance(0.15f)) continue;   // keep it branchy, not blobby
            int j = addRoom(nx, ny, rooms[i].depth + 1);
            rooms[i].links[d] = true;
            rooms[j].links[(d + 2) % 4] = true;
        }

        auto linkCount = [&](const Room& r) { int c = 0; for (bool l : r.links) c += l; return c; };

        // Boss: deepest dead-end. Chest: another dead-end if there is one.
        int boss = -1;
        for (int i = 1; i < (int)rooms.size(); i++)
            if (linkCount(rooms[i]) == 1 && (boss < 0 || rooms[i].depth > rooms[boss].depth)) boss = i;
        if (boss < 0) boss = (int)rooms.size() - 1;

        std::vector<int> leaves, others;
        for (int i = 1; i < (int)rooms.size(); i++) {
            if (i == boss) continue;
            (linkCount(rooms[i]) == 1 ? leaves : others).push_back(i);
        }
        int chest = !leaves.empty() ? leaves[irand(0, (int)leaves.size() - 1)]
                  : !others.empty() ? others[irand(0, (int)others.size() - 1)] : -1;

        rooms[0].type = RoomType::Start;   rooms[0].cleared = true;
        rooms[boss].type = RoomType::Boss;
        if (chest >= 0) { rooms[chest].type = RoomType::Chest; rooms[chest].cleared = true; }

        // Carve tiles
        for (int ri = 0; ri < (int)rooms.size(); ri++) {
            const Room& r = rooms[ri];
            int x0 = r.gx * ROOM_W, y0 = r.gy * ROOM_H;
            for (int y = 0; y < ROOM_H; y++)
                for (int x = 0; x < ROOM_W; x++) {
                    bool border = x == 0 || y == 0 || x == ROOM_W - 1 || y == ROOM_H - 1;
                    setTile(x0 + x, y0 + y, border ? Tile::Wall : Tile::Floor);
                }
            // Doors: a 3-tile gap in this room's border for each link
            for (int d = 0; d < 4; d++) {
                if (!r.links[d]) continue;
                for (int k = -1; k <= 1; k++) {
                    int tx, ty;
                    if (d == 0)      { tx = x0 + ROOM_W - 1; ty = y0 + ROOM_H / 2 + k; }
                    else if (d == 2) { tx = x0;              ty = y0 + ROOM_H / 2 + k; }
                    else if (d == 1) { tx = x0 + ROOM_W / 2 + k; ty = y0 + ROOM_H - 1; }
                    else             { tx = x0 + ROOM_W / 2 + k; ty = y0; }
                    setTile(tx, ty, Tile::Door);
                    doorOwner[ty * MAP_W + tx] = ri;
                }
            }
            // Cover pillars, kept off the cross-shaped lanes between doors so rooms stay connected
            if (r.type == RoomType::Combat) {
                int want = irand(2, 5);
                for (int tries = 0; want > 0 && tries < 60; tries++) {
                    int px = irand(3, ROOM_W - 5), py = irand(3, ROOM_H - 5);
                    bool inColLane = px + 1 >= ROOM_W / 2 - 2 && px <= ROOM_W / 2 + 2;
                    bool inRowLane = py + 1 >= ROOM_H / 2 - 2 && py <= ROOM_H / 2 + 2;
                    if (inColLane || inRowLane) continue;
                    if (tileAt(x0 + px, y0 + py) != Tile::Floor || tileAt(x0 + px + 1, y0 + py + 1) != Tile::Floor) continue;
                    for (int yy = 0; yy < 2; yy++)
                        for (int xx = 0; xx < 2; xx++) setTile(x0 + px + xx, y0 + py + yy, Tile::Pillar);
                    want--;
                }
            } else if (r.type == RoomType::Boss) {
                const int spots[4][2] = {{4, 3}, {ROOM_W - 6, 3}, {4, ROOM_H - 5}, {ROOM_W - 6, ROOM_H - 5}};
                for (auto& s : spots)
                    for (int yy = 0; yy < 2; yy++)
                        for (int xx = 0; xx < 2; xx++) setTile(x0 + s[0] + xx, y0 + s[1] + yy, Tile::Pillar);
            }
        }
    }

    // ------------------------------------------------------------ tile queries
    void setTile(int x, int y, Tile t) { if (x >= 0 && y >= 0 && x < MAP_W && y < MAP_H) tiles[y * MAP_W + x] = t; }
    Tile tileAt(int x, int y) const {
        if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return Tile::Void;
        return tiles[y * MAP_W + x];
    }
    bool solidTile(int x, int y) const {
        Tile t = tileAt(x, y);
        if (t == Tile::Floor) return false;
        if (t == Tile::Door) { int o = doorOwner[y * MAP_W + x]; return o >= 0 && rooms[o].locked; }
        return true;
    }
    bool pointSolid(Vector2 p) const { return solidTile((int)floorf(p.x / TILE), (int)floorf(p.y / TILE)); }
    bool circleSolid(Vector2 p, float r) const {
        int x0 = (int)floorf((p.x - r) / TILE), x1 = (int)floorf((p.x + r) / TILE);
        int y0 = (int)floorf((p.y - r) / TILE), y1 = (int)floorf((p.y + r) / TILE);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                if (solidTile(x, y) && CheckCollisionCircleRec(p, r, {x * TILE, y * TILE, TILE, TILE})) return true;
        return false;
    }
    // Axis-separated movement so things slide along walls. Returns true if anything blocked.
    bool moveCircle(Vector2& p, float r, Vector2 d) {
        bool blocked = false;
        Vector2 np{p.x + d.x, p.y};
        if (!circleSolid(np, r)) p = np; else blocked = true;
        np = {p.x, p.y + d.y};
        if (!circleSolid(np, r)) p = np; else blocked = true;
        return blocked;
    }

    int roomIndexAt(Vector2 p) const {
        if (p.x < 0 || p.y < 0) return -1;
        int gx = (int)(p.x / (ROOM_W * TILE)), gy = (int)(p.y / (ROOM_H * TILE));
        if (gx >= GRID || gy >= GRID) return -1;
        return roomAt[gy * GRID + gx];
    }
    Vector2 roomCenter(int i) const {
        return {(rooms[i].gx * ROOM_W + ROOM_W / 2.0f) * TILE, (rooms[i].gy * ROOM_H + ROOM_H / 2.0f) * TILE};
    }
    Rectangle roomInterior(int i) const {
        return {(rooms[i].gx * ROOM_W + 1) * TILE, (rooms[i].gy * ROOM_H + 1) * TILE, (ROOM_W - 2) * TILE, (ROOM_H - 2) * TILE};
    }

    // ------------------------------------------------------------ effects
    void burst(Vector2 p, int n, Color c, float sMin, float sMax, float life, float size, float dirA = 0, float arc = PI) {
        for (int i = 0; i < n; i++) {
            float a = dirA + frand(-arc, arc);
            float l = life * frand(0.6f, 1.2f);
            parts.push_back({p, fromAngle(a, frand(sMin, sMax)), l, l, size * frand(0.7f, 1.3f), c});
        }
    }

    // ------------------------------------------------------------ update
    void update(float dt) {
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

    void debugKeys() {
        if (IsKeyPressed(KEY_F1)) { god = !god; showBanner(god ? "GOD MODE ON" : "GOD MODE OFF", 1.0f); }
        if (IsKeyPressed(KEY_F2))
            for (auto& e : enemies) if (e.alive) { e.spawnT = 0; damageEnemy(e, 1e6f, {1, 0}); }
        if (IsKeyPressed(KEY_F3))
            for (int i = 0; i < (int)rooms.size(); i++)
                if (rooms[i].type == RoomType::Boss) {
                    pl.pos = Vector2Add(roomCenter(i), {0, TILE * 4});
                    cam.target = pl.pos;
                }
    }

    Gun& gun() { return pl.guns[pl.cur]; }
    const GunDef& gunDef() { return GUNS[gun().def]; }

    void updatePlayer(float dt) {
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
            moveCircle(pl.pos, pl.r, Vector2Scale(pl.rollDir, ROLL_SPEED * dt));
            if (chance(0.6f)) burst({pl.pos.x, pl.pos.y + pl.r}, 1, {180, 170, 160, 160}, 10, 40, 0.35f, 4);
            if (pl.rollT <= 0) pl.rollCd = 0.15f;
        } else {
            moveCircle(pl.pos, pl.r, Vector2Scale(in, PLAYER_SPEED * dt));
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

    bool startReload() {
        Gun& g = gun();
        const GunDef& d = gunDef();
        if (pl.reloadT > 0 || g.mag >= d.mag) return false;
        if (d.maxAmmo >= 0 && g.ammo <= 0) return false;
        pl.reloadT = d.reload;
        return true;
    }

    void finishReload() {
        Gun& g = gun();
        const GunDef& d = gunDef();
        int need = d.mag - g.mag;
        if (d.maxAmmo < 0) g.mag = d.mag;
        else { int take = std::min(need, g.ammo); g.mag += take; g.ammo -= take; }
        sDoor.play(2.5f, 0.4f);
    }

    void spawnBullet(Vector2 pos, float ang, float speed, float r, float dmg, bool fromPlayer, bool pierce, Color col,
                     float splitT = -1.0f, bool pending = false) {
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

    void fire() {
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

    void useBlank() {
        if (pl.blanks <= 0) return;
        pl.blanks--;
        for (auto& b : bullets)
            if (!b.fromPlayer && b.alive) { b.alive = false; burst(b.pos, 2, {150, 200, 255, 255}, 20, 80, 0.3f, 3); }
        for (auto& e : enemies) {
            Vector2 d = Vector2Subtract(e.pos, pl.pos);
            float L = Vector2Length(d);
            if (L < 260 && L > 0 && e.type != EType::Warden) moveCircle(e.pos, e.r, Vector2Scale(d, 70.0f / L));
        }
        blankR = 0;
        blankPos = pl.pos;
        shake += 0.35f;
        pl.invuln = std::max(pl.invuln, 0.4f);
        sBlank.play();
    }

    void interact() {
        if (curRoom < 0) return;
        Room& r = rooms[curRoom];
        if (r.type == RoomType::Chest && !r.chestOpened && Vector2Distance(pl.pos, roomCenter(curRoom)) < 75) {
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

    void openChest(Room& r) {
        r.chestOpened = true;
        Vector2 c = roomCenter(curRoom);
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

    // ------------------------------------------------------------ rooms & waves
    void updateRooms(float dt) {
        int ri = roomIndexAt(pl.pos);
        if (ri >= 0 && ri != curRoom) { curRoom = ri; rooms[ri].visited = true; }
        if (curRoom < 0) return;
        Room& r = rooms[curRoom];

        // Lock the doors once the player is properly inside an uncleared room
        if (!r.cleared && !r.locked && (r.type == RoomType::Combat || r.type == RoomType::Boss)) {
            Rectangle in = roomInterior(curRoom);
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

    void roomCleared() {
        Room& r = rooms[curRoom];
        Vector2 c = roomCenter(curRoom);
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

    Enemy makeEnemy(EType t, Vector2 p) {
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

    void spawnWave() {
        Room& r = rooms[curRoom];
        int x0 = r.gx * ROOM_W, y0 = r.gy * ROOM_H;
        int n = irand(3, 5) + (floor - 1) / 2;
        for (int i = 0; i < n; i++) {
            Vector2 p{};
            bool ok = false;
            for (int tries = 0; tries < 80 && !ok; tries++) {
                int tx = irand(x0 + 2, x0 + ROOM_W - 3), ty = irand(y0 + 2, y0 + ROOM_H - 3);
                p = {(tx + 0.5f) * TILE, (ty + 0.5f) * TILE};
                ok = !circleSolid(p, 22) && Vector2Distance(p, pl.pos) > 260;
            }
            if (!ok) continue;
            float whirlP = floor >= 2 ? 0.12f + 0.05f * floor : 0.0f;
            float roll = frand(0, 1);
            EType t = roll < whirlP ? EType::Whirl : roll < whirlP + 0.32f ? EType::Scatter : EType::Grunt;
            enemies.push_back(makeEnemy(t, p));
        }
    }

    void spawnBoss() {
        Vector2 c = roomCenter(curRoom);
        Enemy e = makeEnemy(EType::Warden, {c.x, c.y - TILE * 3});
        e.spawnT = 1.4f;
        enemies.push_back(e);
    }

    // ------------------------------------------------------------ enemies
    void enemyShot(Vector2 from, float ang, float speed, float r, float splitT = -1.0f) {
        spawnBullet(Vector2Add(from, fromAngle(ang, 10)), ang, speed, r, 1, false, false, ENEMY_BULLET, splitT);
        if (eShotSfxT <= 0) { sEShoot.play(frand(0.9f, 1.1f), 0.7f); eShotSfxT = 0.06f; }
    }

    void updateEnemies(float dt) {
        const float diff = 1.0f + 0.15f * (floor - 1);
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

            switch (e.type) {
                case EType::Grunt: {
                    if (e.moveT <= 0) {
                        e.moveT = frand(0.8f, 1.6f);
                        e.dir = chance(0.5f) ? Vector2{-dirP.y, dirP.x} : Vector2{dirP.y, -dirP.x};
                    }
                    if (dist > 280) mv = dirP;
                    else if (dist < 170) mv = Vector2Scale(dirP, -1);
                    else mv = Vector2Scale(e.dir, 0.7f);
                    e.shootT -= dt * diff;
                    if (e.shootT <= 0) {
                        e.shootT = frand(1.3f, 1.9f);
                        enemyShot(e.pos, aimA + frand(-0.06f, 0.06f), 250, 7);
                    }
                } break;

                case EType::Scatter: {
                    if (dist > 210) mv = dirP;
                    else if (dist < 130) mv = Vector2Scale(dirP, -1);
                    e.shootT -= dt * diff;
                    if (e.shootT <= 0) {
                        e.shootT = frand(2.1f, 2.7f);
                        for (int k = -2; k <= 2; k++) enemyShot(e.pos, aimA + k * 0.22f, 215, 8);
                    }
                } break;

                case EType::Whirl: {
                    if (e.moveT <= 0) { e.moveT = frand(1.5f, 2.5f); e.dir = fromAngle(frand(0, 2 * PI)); }
                    mv = Vector2Scale(e.dir, 0.6f);
                    if (e.burstT > 0) {
                        e.burstT -= dt;
                        e.tick -= dt;
                        if (e.tick <= 0) {
                            e.tick = 0.11f;
                            for (int k = 0; k < 4; k++) enemyShot(e.pos, e.ang + k * PI / 2, 175, 7);
                            e.ang += 0.27f;
                        }
                        if (e.burstT <= 0) e.shootT = frand(1.8f, 2.4f);
                    } else {
                        e.shootT -= dt * diff;
                        if (e.shootT <= 0) { e.burstT = 1.6f; e.tick = 0; }
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
            if (moveCircle(e.pos, e.r, Vector2Scale(mv, e.speed * dt))) e.moveT = 0;

            if (dist < e.r + pl.r * 0.7f) hurtPlayer();
        }
    }

    void updateBoss(Enemy& e, float dt, float aimA, Vector2& mv) {
        const float diff = sqrtf(1.0f + 0.15f * (floor - 1));
        Vector2 c = roomCenter(curRoom);

        if (!e.enraged && e.hp < e.maxHp * 0.5f) {
            e.enraged = true;
            showBanner("THE WARDEN IS ENRAGED", 1.8f);
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

    void damageEnemy(Enemy& e, float dmg, Vector2 vel) {
        e.hp -= dmg;
        e.flash = 1.0f;
        float a = atan2f(vel.y, vel.x);
        if (e.type != EType::Warden) moveCircle(e.pos, e.r, fromAngle(a, 4));
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
            if (chance(0.06f)) pickups.push_back({e.pos, PickType::Ammo});
        }
    }

    void hurtPlayer() {
        if (pl.invuln > 0 || pl.rollT > ROLL_IFRAME_END || god) return;
        pl.hp--;
        pl.invuln = HIT_INVULN;
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

    // ------------------------------------------------------------ bullets / pickups / particles
    void updateBullets(float dt) {
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

            if (pointSolid(b.pos)) {
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

    void updatePickups(float dt) {
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

    void updateParticles(float dt) {
        for (auto& p : parts) {
            p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
            p.vel = Vector2Scale(p.vel, std::max(0.0f, 1.0f - 4.0f * dt));
            p.life -= dt;
        }
        parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.life <= 0; }), parts.end());
    }

    // ============================================================ DRAWING
    static Color enemyColor(const Enemy& e) {
        switch (e.type) {
            case EType::Grunt:   return {214, 172, 96, 255};
            case EType::Scatter: return {226, 104, 70, 255};
            case EType::Whirl:   return {168, 110, 230, 255};
            case EType::Warden:  return e.enraged ? Color{220, 60, 70, 255} : Color{150, 160, 180, 255};
        }
        return WHITE;
    }
    static Color pickupColor(PickType t) {
        switch (t) {
            case PickType::Heart: return {240, 70, 90, 255};
            case PickType::Ammo:  return {240, 200, 90, 255};
            case PickType::Blank: return {110, 180, 255, 255};
        }
        return WHITE;
    }

    void draw() {
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

    void drawTiles(const Camera2D& view) {
        const Theme& th = theme();
        Vector2 tl = GetScreenToWorld2D({0, 0}, view);
        Vector2 br = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, view);
        int x0 = std::max(0, (int)(tl.x / TILE) - 1), y0 = std::max(0, (int)(tl.y / TILE) - 1);
        int x1 = std::min(MAP_W - 1, (int)(br.x / TILE) + 1), y1 = std::min(MAP_H - 1, (int)(br.y / TILE) + 1);

        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                Tile t = tileAt(x, y);
                if (t == Tile::Void) continue;
                Rectangle rc{x * TILE, y * TILE, TILE, TILE};
                unsigned h = (unsigned)(x * 73856093u ^ y * 19349663u);

                if (t == Tile::Floor || t == Tile::Door) {
                    DrawRectangleRec(rc, ((x + y) & 1) ? th.floorA : th.floorB);
                    if (h % 9 == 0) DrawRectangle((int)rc.x + (int)(h % 28) + 4, (int)rc.y + (int)((h >> 5) % 28) + 4, 3, 3, Fade(BLACK, 0.18f));
                    // soft shadow cast by the wall above
                    Tile above = tileAt(x, y - 1);
                    if (above == Tile::Wall || above == Tile::Pillar) DrawRectangle((int)rc.x, (int)rc.y, (int)TILE, 10, Fade(BLACK, 0.28f));
                    if (t == Tile::Door) {
                        int o = doorOwner[y * MAP_W + x];
                        if (o >= 0 && rooms[o].locked) {
                            DrawRectangleRec(rc, {70, 26, 30, 255});
                            for (int k = 0; k < 4; k++)
                                DrawRectangle((int)rc.x + 4 + k * 10, (int)rc.y + 2, 4, (int)TILE - 4, mix(th.accent, RED, 0.5f));
                        } else {
                            DrawRectangleRec(rc, Fade(th.wallTop, 0.12f));
                        }
                    }
                } else {
                    // Wall / pillar: lighter "top" with a darker front face where it meets the floor
                    bool faceVisible = tileAt(x, y + 1) == Tile::Floor || tileAt(x, y + 1) == Tile::Door;
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

    void drawRoomProps() {
        const Theme& th = theme();
        for (int i = 0; i < (int)rooms.size(); i++) {
            if (rooms[i].type != RoomType::Chest) continue;
            Vector2 c = roomCenter(i);
            DrawEllipse((int)c.x, (int)c.y + 16, 30, 9, Fade(BLACK, 0.35f));
            Color wood{120, 78, 46, 255};
            if (rooms[i].chestOpened) {
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

    void drawPickup(const Pickup& p) {
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

    void drawEnemy(const Enemy& e) {
        Color base = enemyColor(e);
        if (e.spawnT > 0) {
            float k = e.spawnT / (e.type == EType::Warden ? 1.4f : 0.9f);
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

        switch (e.type) {
            case EType::Grunt:
                DrawRectanglePro({e.pos.x, e.pos.y, 22, 6}, {-6, 3}, aimA * RAD2DEG, {50, 48, 56, 255});
                DrawCircleV(e.pos, e.r, c);
                DrawCircleV({e.pos.x, e.pos.y - e.r * 0.35f}, e.r * 0.55f, mix(c, WHITE, 0.15f));
                eyes(5, 3.5f);
                break;
            case EType::Scatter:
                DrawRectanglePro({e.pos.x, e.pos.y, 26, 10}, {-6, 5}, aimA * RAD2DEG, {60, 50, 46, 255});
                DrawRectangleRounded({e.pos.x - e.r, e.pos.y - e.r, e.r * 2, e.r * 2}, 0.35f, 6, c);
                DrawRectangle((int)(e.pos.x - e.r), (int)(e.pos.y + e.r * 0.2f), (int)(e.r * 2), 4, Fade(BLACK, 0.25f));
                eyes(6, 4);
                break;
            case EType::Whirl:
                DrawPoly(e.pos, 6, e.r * 1.15f, e.spin * RAD2DEG, c);
                DrawPoly(e.pos, 6, e.r * 0.7f, -e.spin * RAD2DEG, mix(c, BLACK, 0.3f));
                DrawCircleV(e.pos, 5, RAYWHITE);
                DrawCircleV(Vector2Add(e.pos, fromAngle(aimA, 2)), 2.5f, {20, 16, 24, 255});
                break;
            case EType::Warden: {
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
        bool tell = (e.type == EType::Grunt || e.type == EType::Scatter) ? e.shootT < 0.3f
                  : e.type == EType::Whirl ? (e.burstT <= 0 && e.shootT < 0.4f) : false;
        if (tell) textCentered("!", (int)e.pos.x, (int)(e.pos.y - e.r - 26), 22, {255, 230, 90, 255});

        if (e.type != EType::Warden && e.hp < e.maxHp) {
            float w = e.r * 2;
            DrawRectangle((int)(e.pos.x - w / 2), (int)(e.pos.y - e.r - 9), (int)w, 4, Fade(BLACK, 0.6f));
            DrawRectangle((int)(e.pos.x - w / 2), (int)(e.pos.y - e.r - 9), (int)(w * e.hp / e.maxHp), 4, {255, 90, 90, 255});
        }
    }

    void drawPlayer() {
        bool rolling = pl.rollT > 0;
        if (pl.invuln > 0 && fmodf(titleT, 0.12f) < 0.06f) return;   // i-frame flicker
        DrawEllipse((int)pl.pos.x, (int)(pl.pos.y + pl.r * 0.9f), pl.r, pl.r * 0.35f, Fade(BLACK, 0.35f));

        Color cloak{70, 150, 220, 255}, face{240, 228, 205, 255};
        if (rolling) {
            float k = 1.0f - pl.rollT / ROLL_TIME;
            for (int i = 1; i <= 3; i++)
                DrawCircleV(Vector2Subtract(pl.pos, Vector2Scale(pl.rollDir, i * 9.0f)), pl.r * 0.9f, Fade(cloak, 0.18f * (4 - i)));
            float squash = 0.75f + 0.25f * fabsf(cosf(k * PI * 2));
            DrawEllipse((int)pl.pos.x, (int)pl.pos.y, pl.r * (2 - squash), pl.r * squash, cloak);
            return;
        }

        const GunDef& d = gunDef();
        float gunLen = d.pellets > 1 ? 24.0f : d.pierce ? 30.0f : d.fireDelay < 0.1f ? 22.0f : 18.0f;
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

    void drawBullets() {
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

    // ------------------------------------------------------------ HUD
    void drawHeart(float cx, float cy, float s, int fill) {
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

    void drawHUD() {
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
        DrawText(d.name, (int)panel.x + 14, (int)panel.y + 10, 20, d.color);
        const char* ammoTxt = d.maxAmmo < 0 ? TextFormat("%d / INF", g.mag) : TextFormat("%d / %d", g.mag, g.ammo);
        DrawText(ammoTxt, (int)panel.x + 14, (int)panel.y + 38, 26, RAYWHITE);
        if (pl.emptyT > 0) DrawText("EMPTY", (int)panel.x + 170, (int)panel.y + 42, 20, {255, 90, 90, 255});
        else if (pl.reloadT > 0) DrawText("RELOAD", (int)panel.x + 170, (int)panel.y + 42, 20, Fade(RAYWHITE, 0.7f));
        if (pl.guns.size() > 1)
            DrawText(TextFormat("%d/%d  wheel / 1-4", pl.cur + 1, (int)pl.guns.size()), (int)panel.x + 14, (int)panel.y - 20, 14, Fade(RAYWHITE, 0.55f));

        // Floor label & stats
        const char* fl = TextFormat("FLOOR %d / %d", floor, FINAL_FLOOR);
        textCentered(fl, sw / 2, 14, 20, Fade(th.accent, 0.9f));
        DrawText(TextFormat("Kills %d   %02d:%02d", kills, (int)runTime / 60, (int)runTime % 60), 20, sh - 30, 16, Fade(RAYWHITE, 0.45f));

        drawMinimap(sw);

        // Boss bar
        for (auto& e : enemies) {
            if (e.type != EType::Warden || e.spawnT > 0) continue;
            float w = std::min(620.0f, sw - 340.0f);
            Rectangle bar{sw / 2.0f - w / 2, (float)sh - 54, w, 18};
            textCentered("THE WARDEN", sw / 2, (int)bar.y - 24, 18, RAYWHITE);
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

    void drawMinimap(int sw) {
        const Theme& th = theme();
        const int cw = 20, ch = 13, gap = 4;
        int ox = sw - GRID * (cw + gap) - 16, oy = 16;
        auto known = [&](int i) {
            if (rooms[i].visited) return true;
            for (int d = 0; d < 4; d++) {
                if (!rooms[i].links[d]) continue;
                int n = roomAt[(rooms[i].gy + DY[d]) * GRID + rooms[i].gx + DX[d]];
                if (n >= 0 && rooms[n].visited) return true;
            }
            return false;
        };
        DrawRectangle(ox - 8, oy - 8, GRID * (cw + gap) + 12, GRID * (ch + gap) + 12, Fade(BLACK, 0.4f));
        for (int i = 0; i < (int)rooms.size(); i++) {
            if (!known(i)) continue;
            const Room& r = rooms[i];
            int x = ox + r.gx * (cw + gap), y = oy + r.gy * (ch + gap);
            if (r.links[0]) DrawRectangle(x + cw, y + ch / 2 - 1, gap, 3, Fade(RAYWHITE, 0.5f));
            if (r.links[1]) DrawRectangle(x + cw / 2 - 1, y + ch, 3, gap, Fade(RAYWHITE, 0.5f));
            Color c = i == curRoom ? th.accent : r.visited ? Color{120, 118, 130, 255} : Color{60, 58, 68, 255};
            DrawRectangle(x, y, cw, ch, c);
            if (r.type == RoomType::Boss)  DrawCircle(x + cw / 2, y + ch / 2, 4, {230, 60, 70, 255});
            if (r.type == RoomType::Chest && !r.chestOpened) DrawRectangle(x + cw / 2 - 3, y + ch / 2 - 3, 6, 6, {240, 200, 90, 255});
        }
    }

    void drawOverlay(const char* title, const char* hint, Color accent) {
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

    void drawCrosshair() {
        if (state == State::Title) return;
        Vector2 m = GetMousePosition();
        Color c = state == State::Playing ? RAYWHITE : Fade(RAYWHITE, 0.5f);
        DrawCircleLinesV(m, 9, c);
        DrawLineEx({m.x - 16, m.y}, {m.x - 6, m.y}, 2, c);
        DrawLineEx({m.x + 6, m.y}, {m.x + 16, m.y}, 2, c);
        DrawLineEx({m.x, m.y - 16}, {m.x, m.y - 6}, 2, c);
        DrawLineEx({m.x, m.y + 6}, {m.x, m.y + 16}, 2, c);
    }

    void drawTitle() {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        Vector2 c{sw / 2.0f, sh / 2.0f - 40};
        // Decorative bullet spiral
        for (int k = 0; k < 90; k++) {
            float rad = fmodf(k * 23.0f + titleT * 90.0f, 900.0f);
            float a = k * 0.42f + titleT * 0.35f;
            Vector2 p = Vector2Add(c, fromAngle(a, rad));
            float fade = 1.0f - rad / 900.0f;
            DrawCircleV(p, 7, Fade(ENEMY_BULLET, 0.5f * fade));
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
};

}  // namespace

int main(int argc, char** argv) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_W, SCREEN_H, "Valor at What Cost");
    SetWindowMinSize(960, 640);
    SetExitKey(KEY_NULL);           // ESC is used for pause
    InitAudioDevice();
    SetTargetFPS(144);
    HideCursor();

    static Game game;               // large object; keep it off the stack
    game.init();
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--debug") game.debug = true;

    while (!WindowShouldClose() && !game.quit) {
        float dt = std::min(GetFrameTime(), 1.0f / 30.0f);   // clamp so a hitch can't tunnel things through walls
        game.update(dt);
        BeginDrawing();
        game.draw();
        EndDrawing();
    }

    game.unload();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
