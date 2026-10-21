// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
#include "world/dungeon.hpp"
#include "core/random.hpp"
#include <algorithm>
#include <cmath>

namespace valor {

void Dungeon::generate(int floor) {
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

Tile Dungeon::tileAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return Tile::Void;
    return tiles[y * MAP_W + x];
}

bool Dungeon::solidTile(int x, int y) const {
    Tile t = tileAt(x, y);
    if (t == Tile::Floor) return false;
    if (t == Tile::Door) { int o = doorOwner[y * MAP_W + x]; return o >= 0 && rooms[o].locked; }
    return true;
}

bool Dungeon::circleSolid(Vector2 p, float r) const {
    int x0 = (int)floorf((p.x - r) / TILE), x1 = (int)floorf((p.x + r) / TILE);
    int y0 = (int)floorf((p.y - r) / TILE), y1 = (int)floorf((p.y + r) / TILE);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (solidTile(x, y) && CheckCollisionCircleRec(p, r, {x * TILE, y * TILE, TILE, TILE})) return true;
    return false;
}

bool Dungeon::moveCircle(Vector2& p, float r, Vector2 d) {
    bool blocked = false;
    Vector2 np{p.x + d.x, p.y};
    if (!circleSolid(np, r)) p = np; else blocked = true;
    np = {p.x, p.y + d.y};
    if (!circleSolid(np, r)) p = np; else blocked = true;
    return blocked;
}

int Dungeon::roomIndexAt(Vector2 p) const {
    if (p.x < 0 || p.y < 0) return -1;
    int gx = (int)(p.x / (ROOM_W * TILE)), gy = (int)(p.y / (ROOM_H * TILE));
    if (gx >= GRID || gy >= GRID) return -1;
    return roomAt[gy * GRID + gx];
}

Vector2 Dungeon::roomCenter(int i) const {
    return {(rooms[i].gx * ROOM_W + ROOM_W / 2.0f) * TILE, (rooms[i].gy * ROOM_H + ROOM_H / 2.0f) * TILE};
}

Rectangle Dungeon::roomInterior(int i) const {
    return {(rooms[i].gx * ROOM_W + 1) * TILE, (rooms[i].gy * ROOM_H + 1) * TILE, (ROOM_W - 2) * TILE, (ROOM_H - 2) * TILE};
}

}  // namespace valor
