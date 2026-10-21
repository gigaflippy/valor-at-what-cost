// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
#include "game/content.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace valor {

using json = nlohmann::json;

namespace {

Content g_content;

constexpr float DEG = 3.14159265358979f / 180.0f;

// Error with a readable location: "guns.json > scattergun > magazine: ..."
struct DataError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::string join(const std::string& where, const std::string& key) { return where + " > " + key; }

const json& need(const json& j, const char* key, const std::string& where) {
    if (!j.is_object() || !j.contains(key)) throw DataError(where + ": missing \"" + key + "\"");
    return j.at(key);
}

float num(const json& j, const char* key, const std::string& where) {
    const json& v = need(j, key, where);
    if (!v.is_number()) throw DataError(join(where, key) + ": expected a number");
    return v.get<float>();
}
float num(const json& j, const char* key, const std::string& where, float fallback) {
    return j.contains(key) ? num(j, key, where) : fallback;
}
int integer(const json& j, const char* key, const std::string& where) {
    const json& v = need(j, key, where);
    if (!v.is_number_integer()) throw DataError(join(where, key) + ": expected a whole number");
    return v.get<int>();
}
int integer(const json& j, const char* key, const std::string& where, int fallback) {
    return j.contains(key) ? integer(j, key, where) : fallback;
}
bool flag(const json& j, const char* key, const std::string& where, bool fallback) {
    if (!j.contains(key)) return fallback;
    if (!j.at(key).is_boolean()) throw DataError(join(where, key) + ": expected true or false");
    return j.at(key).get<bool>();
}
std::string text(const json& j, const char* key, const std::string& where) {
    const json& v = need(j, key, where);
    if (!v.is_string()) throw DataError(join(where, key) + ": expected text in quotes");
    return v.get<std::string>();
}
// [min, max] pair
void range(const json& j, const char* key, const std::string& where, float& lo, float& hi) {
    const json& v = need(j, key, where);
    if (!v.is_array() || v.size() != 2 || !v[0].is_number() || !v[1].is_number())
        throw DataError(join(where, key) + ": expected [min, max]");
    lo = v[0].get<float>();
    hi = v[1].get<float>();
    if (lo > hi) throw DataError(join(where, key) + ": min is larger than max");
}
// "#RRGGBB" or "#RRGGBBAA"
Color color(const json& j, const char* key, const std::string& where) {
    std::string s = text(j, key, where);
    bool ok = (s.size() == 7 || s.size() == 9) && s[0] == '#';
    for (size_t i = 1; ok && i < s.size(); i++) ok = std::isxdigit((unsigned char)s[i]) != 0;
    if (!ok) throw DataError(join(where, key) + ": expected a colour like \"#FFAA00\", got \"" + s + "\"");
    auto byte = [&](int i) { return (unsigned char)std::strtol(s.substr(i, 2).c_str(), nullptr, 16); };
    return {byte(1), byte(3), byte(5), s.size() == 9 ? byte(7) : (unsigned char)255};
}
Color color(const json& j, const char* key, const std::string& where, Color fallback) {
    return j.contains(key) ? color(j, key, where) : fallback;
}
void positive(float v, const std::string& where) {
    if (!(v > 0)) throw DataError(where + ": must be greater than 0");
}

json readFile(const std::string& path, const std::string& name) {
    std::ifstream f(path);
    if (!f) throw DataError(name + ": file not found at " + path);
    try {
        return json::parse(f, nullptr, true, /*ignore_comments=*/true);
    } catch (const json::parse_error& e) {
        throw DataError(name + ": not valid JSON (" + std::string(e.what()) + ")");
    }
}

const json& list(const json& root, const char* key, const std::string& file) {
    const json& v = need(root, key, file);
    if (!v.is_array() || v.empty()) throw DataError(join(file, key) + ": expected a non-empty list");
    return v;
}

// ------------------------------------------------------------------ files
void loadGuns(const std::string& dir, Content& c) {
    const std::string file = "guns.json";
    json root = readFile(dir + "/" + file, file);
    for (const json& g : list(root, "guns", file)) {
        GunDef d;
        d.id = text(g, "id", file);
        std::string w = join(file, d.id);
        d.name = text(g, "name", w);
        d.starter = flag(g, "starter", w, false);
        d.fireDelay = num(g, "fireDelay", w);          positive(d.fireDelay, join(w, "fireDelay"));
        d.mag = integer(g, "magazine", w);             positive((float)d.mag, join(w, "magazine"));
        d.maxAmmo = integer(g, "maxAmmo", w);
        d.reload = num(g, "reloadTime", w);            positive(d.reload, join(w, "reloadTime"));
        d.pellets = integer(g, "pellets", w);          positive((float)d.pellets, join(w, "pellets"));
        d.spread = num(g, "spreadDeg", w) * DEG;
        d.speed = num(g, "bulletSpeed", w);            positive(d.speed, join(w, "bulletSpeed"));
        d.damage = num(g, "damage", w);
        d.radius = num(g, "bulletRadius", w);          positive(d.radius, join(w, "bulletRadius"));
        d.pierce = flag(g, "pierce", w, false);
        d.color = color(g, "color", w);
        std::string snd = g.contains("sound") ? text(g, "sound", w) : "pistol";
        if (snd == "pistol") d.sound = GunSound::Pistol;
        else if (snd == "shotgun") d.sound = GunSound::Shotgun;
        else if (snd == "rail") d.sound = GunSound::Rail;
        else throw DataError(join(w, "sound") + ": must be pistol, shotgun or rail");
        d.volume = num(g, "volume", w, 1.0f);
        d.shake = num(g, "shake", w, 0.03f);
        d.length = num(g, "length", w, 18.0f);
        for (auto& other : c.guns)
            if (other.id == d.id) throw DataError(w + ": two guns share this id");
        c.guns.push_back(d);
    }
    if (c.starterGun() < 0) throw DataError(file + ": no gun is marked \"starter\": true");
}

void loadEnemies(const std::string& dir, Content& c) {
    const std::string file = "enemies.json";
    json root = readFile(dir + "/" + file, file);
    for (const json& e : list(root, "enemies", file)) {
        EnemyDef d;
        d.id = text(e, "id", file);
        std::string w = join(file, d.id);
        d.name = text(e, "name", w);
        std::string ai = text(e, "ai", w);
        if (ai == "shooter") d.ai = EType::Shooter;
        else if (ai == "spinner") d.ai = EType::Spinner;
        else if (ai == "warden") d.ai = EType::Warden;
        else throw DataError(join(w, "ai") + ": must be shooter, spinner or warden");
        std::string shape = e.contains("shape") ? text(e, "shape", w)
                          : d.ai == EType::Spinner ? "hexagon" : d.ai == EType::Warden ? "warden" : "round";
        if (shape == "round") d.shape = Shape::Round;
        else if (shape == "square") d.shape = Shape::Square;
        else if (shape == "hexagon") d.shape = Shape::Hexagon;
        else if (shape == "warden") d.shape = Shape::Warden;
        else throw DataError(join(w, "shape") + ": must be round, square, hexagon or warden");
        d.boss = flag(e, "boss", w, false);
        d.radius = num(e, "radius", w);     positive(d.radius, join(w, "radius"));
        d.hp = num(e, "hp", w);             positive(d.hp, join(w, "hp"));
        d.hpPerFloor = num(e, "hpPerFloor", w, 0);
        d.speed = num(e, "speed", w);
        d.color = color(e, "color", w);
        d.enragedColor = color(e, "enragedColor", w, d.color);
        d.enrageAt = num(e, "enrageAt", w, 0.5f);
        d.spawnDelay = num(e, "spawnDelay", w, 0.9f);

        if (!d.boss) {
            const json& s = need(e, "spawn", w);
            std::string ws = join(w, "spawn");
            d.spawnMinFloor = integer(s, "minFloor", ws, 1);
            d.spawnFill = flag(s, "fill", ws, false);
            if (!d.spawnFill) {
                d.spawnChance = num(s, "chance", ws);
                d.spawnChancePerFloor = num(s, "chancePerFloor", ws, 0);
            }
        }
        if (d.ai == EType::Shooter) {
            const json& m = need(e, "movement", w);
            std::string wm = join(w, "movement");
            d.keepFar = num(m, "keepFar", wm);
            d.keepNear = num(m, "keepNear", wm);
            range(m, "strafeTime", wm, d.strafeMin, d.strafeMax);
            d.strafeSpeed = num(m, "strafeSpeed", wm);
            const json& a = need(e, "attack", w);
            std::string wa = join(w, "attack");
            range(a, "interval", wa, d.fireMin, d.fireMax);
            d.count = integer(a, "count", wa);          positive((float)d.count, join(wa, "count"));
            d.spreadStep = num(a, "spreadStepDeg", wa) * DEG;
            d.aimJitter = num(a, "aimJitterDeg", wa) * DEG;
            d.bulletSpeed = num(a, "bulletSpeed", wa);
            d.bulletRadius = num(a, "bulletRadius", wa);
        } else if (d.ai == EType::Spinner) {
            const json& m = need(e, "movement", w);
            std::string wm = join(w, "movement");
            range(m, "wanderTime", wm, d.wanderMin, d.wanderMax);
            d.wanderSpeed = num(m, "wanderSpeed", wm);
            const json& a = need(e, "attack", w);
            std::string wa = join(w, "attack");
            range(a, "interval", wa, d.fireMin, d.fireMax);
            d.burstTime = num(a, "burstTime", wa);       positive(d.burstTime, join(wa, "burstTime"));
            d.burstTick = num(a, "burstTick", wa);       positive(d.burstTick, join(wa, "burstTick"));
            d.arms = integer(a, "arms", wa);             positive((float)d.arms, join(wa, "arms"));
            d.burstTurn = num(a, "turnDeg", wa) * DEG;
            d.bulletSpeed = num(a, "bulletSpeed", wa);
            d.bulletRadius = num(a, "bulletRadius", wa);
        }
        for (auto& other : c.enemies)
            if (other.id == d.id) throw DataError(w + ": two enemies share this id");
        c.enemies.push_back(d);
    }
    int fills = 0;
    for (auto& d : c.enemies) fills += d.spawnFill;
    if (fills != 1) throw DataError(file + ": exactly one regular enemy needs \"spawn\": { \"fill\": true }");
    if (c.bossEnemy() < 0) throw DataError(file + ": no enemy is marked \"boss\": true");
}

void loadFloors(const std::string& dir, Content& c) {
    const std::string file = "floors.json";
    json root = readFile(dir + "/" + file, file);
    int i = 0;
    for (const json& f : list(root, "floors", file)) {
        std::string w = join(file, "floor " + std::to_string(++i));
        Theme t;
        t.name = text(f, "name", w);
        t.floorA = color(f, "floorA", w);
        t.floorB = color(f, "floorB", w);
        t.wall = color(f, "wall", w);
        t.wallTop = color(f, "wallTop", w);
        t.accent = color(f, "accent", w);
        t.bg = color(f, "background", w);
        c.floors.push_back(t);
    }
}

void loadRules(const std::string& dir, Content& c) {
    const std::string file = "rules.json";
    json root = readFile(dir + "/" + file, file);
    Rules& r = c.rules;

    const json& p = need(root, "player", file);
    std::string w = join(file, "player");
    r.playerSpeed = num(p, "speed", w);
    r.rollTime = num(p, "rollTime", w);           positive(r.rollTime, join(w, "rollTime"));
    r.rollSpeed = num(p, "rollSpeed", w);
    r.rollIframeEnd = num(p, "rollIframeEnd", w);
    r.hitInvuln = num(p, "hitInvuln", w);
    r.halfHearts = integer(p, "halfHearts", w);   positive((float)r.halfHearts, join(w, "halfHearts"));
    r.blanksPerFloor = integer(p, "blanksPerFloor", w);
    r.maxBlanks = integer(p, "maxBlanks", w);
    r.healOnDescend = integer(p, "healOnDescend", w);

    const json& d = need(root, "difficulty", file);
    w = join(file, "difficulty");
    r.fireRatePerFloor = num(d, "fireRatePerFloor", w);
    range(d, "firstShotDelay", w, r.firstShotMin, r.firstShotMax);

    const json& wv = need(root, "waves", file);
    w = join(file, "waves");
    float lo, hi;
    range(wv, "size", w, lo, hi);
    r.waveMin = (int)lo;
    r.waveMax = (int)hi;
    r.waveBonusEvery = integer(wv, "sizeBonusEveryFloors", w);   positive((float)r.waveBonusEvery, join(w, "sizeBonusEveryFloors"));
    r.extraWaveChance = num(wv, "extraWaveChance", w);
    r.extraWaveChancePerFloor = num(wv, "extraWaveChancePerFloor", w);
    r.waveDelay = num(wv, "delayBetween", w);
    r.spawnMinDistance = num(wv, "spawnMinDistance", w);

    const json& dr = need(root, "drops", file);
    w = join(file, "drops");
    r.dropHeart = num(dr, "roomHeart", w);
    r.dropAmmo = num(dr, "roomAmmo", w);
    r.dropBlank = num(dr, "roomBlank", w);
    r.enemyAmmoDrop = num(dr, "enemyAmmo", w);
    if (r.dropHeart + r.dropAmmo + r.dropBlank > 1.0f) throw DataError(w + ": room drop chances add up to more than 1");

    r.enemyBullet = color(root, "enemyBulletColor", file);
}

bool exists(const std::string& dir) {
    std::ifstream f(dir + "/rules.json");
    return f.good();
}

std::string findDataDir() {
    if (const char* env = std::getenv("VALOR_DATA")) return env;
    std::string app = GetApplicationDirectory();               // ends with a slash
    for (const std::string& cand : {app + "data", app + "../data", app + "../../data", std::string("data")})
        if (exists(cand)) return cand;
    return app + "data";
}

}  // namespace

int Content::starterGun() const {
    for (int i = 0; i < (int)guns.size(); i++)
        if (guns[i].starter) return i;
    return -1;
}

int Content::bossEnemy() const {
    for (int i = 0; i < (int)enemies.size(); i++)
        if (enemies[i].boss) return i;
    return -1;
}

const Content& content() { return g_content; }

bool loadContent(std::string& error) {
    Content fresh;
    fresh.dir = findDataDir();
    try {
        loadGuns(fresh.dir, fresh);
        loadEnemies(fresh.dir, fresh);
        loadFloors(fresh.dir, fresh);
        loadRules(fresh.dir, fresh);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    g_content = std::move(fresh);
    return true;
}

}  // namespace valor
