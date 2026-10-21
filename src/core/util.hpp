// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// Small math and drawing helpers.
#pragma once
#include "raylib.h"
#include "raymath.h"
#include <cctype>
#include <cmath>
#include <string>

namespace valor {

inline Vector2 fromAngle(float a, float len = 1.0f) { return {cosf(a) * len, sinf(a) * len}; }
inline float   angleTo(Vector2 a, Vector2 b)        { return atan2f(b.y - a.y, b.x - a.x); }
inline Color   mix(Color a, Color b, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return {(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
            (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
inline void textCentered(const char* s, int cx, int y, int size, Color c) {
    DrawText(s, cx - MeasureText(s, size) / 2, y, size, c);
}

inline std::string upper(std::string s) {
    for (char& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}

}  // namespace valor
