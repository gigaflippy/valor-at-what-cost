// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// Floor layout: a tree of rooms carved into a tile map, plus collision queries.
#pragma once
#include "raylib.h"
#include "core/config.hpp"
#include <cmath>
#include <vector>

namespace valor {

enum class Tile : unsigned char { Void, Floor, Wall, Door, Pillar };
enum class RoomType { Start, Combat, Chest, Boss };

struct Room {
    int gx = 0, gy = 0;
    RoomType type = RoomType::Combat;
    bool links[4] = {};
    bool visited = false, cleared = false, locked = false, chestOpened = false;
    int wavesLeft = 0;
    int depth = 0;
};

class Dungeon {
public:
    std::vector<Tile> tiles;
    std::vector<int>  doorOwner;    // which room's lock controls this door tile
    std::vector<Room> rooms;
    std::vector<int>  roomAt;       // GRID*GRID -> room index or -1
    void generate(int floor);
    void setTile(int x, int y, Tile t) { if (x >= 0 && y >= 0 && x < MAP_W && y < MAP_H) tiles[y * MAP_W + x] = t; }
    Tile tileAt(int x, int y) const;
    bool solidTile(int x, int y) const;
    bool pointSolid(Vector2 p) const { return solidTile((int)floorf(p.x / TILE), (int)floorf(p.y / TILE)); }
    bool circleSolid(Vector2 p, float r) const;
    bool moveCircle(Vector2& p, float r, Vector2 d);
    int roomIndexAt(Vector2 p) const;
    Vector2 roomCenter(int i) const;
    Rectangle roomInterior(int i) const;
};

}  // namespace valor
