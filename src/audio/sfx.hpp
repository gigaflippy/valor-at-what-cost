// ============================================================================
//  VALOR AT WHAT COST
//  Copyright (c) 2026 Georgi D. Georgiev (github.com/gigaflippy). All rights reserved.
// ============================================================================
// Procedural sound effects: every sound is synthesised at startup.
#pragma once
#include "raylib.h"
#include <vector>

namespace valor {

enum class Osc { Square, Noise, Triangle };

Wave synthWave(float f0, float f1, float dur, Osc osc, float vol);

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

}  // namespace valor
