// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// Shared random number generator and helpers.
#pragma once
#include <random>

namespace valor {

inline std::mt19937 rng{std::random_device{}()};
inline float frand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }
inline int   irand(int a, int b)     { return std::uniform_int_distribution<int>(a, b)(rng); }
inline bool  chance(float p)         { return frand(0.0f, 1.0f) < p; }

}  // namespace valor
