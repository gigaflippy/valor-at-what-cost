// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
#include "audio/sfx.hpp"
#include "core/random.hpp"
#include <algorithm>
#include <cmath>

namespace valor {

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

}  // namespace valor
