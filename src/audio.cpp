#include "audio.h"

#include "raylib.h"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace {
constexpr int kRate = 44100;
constexpr int kAliases = 4;
constexpr float kTau = 6.28318530718f;

struct Slot {
    Sound base;
    Sound alias[kAliases];
    int next = 0;
};

Slot gSlots[(int)Sfx::Count];
bool gReady = false;

float Noise() { return (rand() / (float)RAND_MAX) * 2.0f - 1.0f; }

// Frequency sweep from f0 to f1 with an exponential decay envelope.
std::vector<float> Sweep(float seconds, float f0, float f1, float decay, float squareMix, float noiseMix) {
    int n = (int)(seconds * kRate);
    std::vector<float> s(n);
    float phase = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        float f = f0 + (f1 - f0) * t;
        phase += kTau * f / kRate;
        float sine = sinf(phase);
        float square = sine > 0 ? 1.0f : -1.0f;
        float v = sine * (1 - squareMix) + square * squareMix * 0.6f + Noise() * noiseMix;
        float env = expf(-t * decay) * fminf(1.0f, i / 80.0f);
        s[i] = v * env;
    }
    return s;
}

std::vector<float> Synth(Sfx type) {
    switch (type) {
        case Sfx::Blaster: return Sweep(0.22f, 1500, 180, 5.0f, 0.35f, 0.05f);
        case Sfx::EnemyBlaster: return Sweep(0.24f, 950, 140, 4.5f, 0.45f, 0.05f);
        case Sfx::Deflect: return Sweep(0.18f, 2200, 600, 6.0f, 0.2f, 0.1f);
        case Sfx::Hurt: return Sweep(0.3f, 320, 110, 3.5f, 0.7f, 0.05f);
        case Sfx::Jump: return Sweep(0.14f, 280, 620, 3.0f, 0.1f, 0.0f);
        case Sfx::Swing: {
            int n = (int)(0.28f * kRate);
            std::vector<float> s(n);
            float lp = 0, phase = 0;
            for (int i = 0; i < n; i++) {
                float t = (float)i / n;
                lp += (Noise() - lp) * 0.08f;
                phase += kTau * (95 + 70 * sinf(t * 3.14159f)) / kRate;
                float hum = sinf(phase) * 0.6f + sinf(phase * 2.01f) * 0.25f;
                s[i] = (lp * 2.0f + hum) * sinf(t * 3.14159f) * 0.8f;
            }
            return s;
        }
        case Sfx::Ignite: {
            int n = (int)(0.45f * kRate);
            std::vector<float> s(n);
            float phase = 0;
            for (int i = 0; i < n; i++) {
                float t = (float)i / n;
                phase += kTau * (70 + 90 * t) / kRate;
                float saw = fmodf(phase / kTau, 1.0f) * 2 - 1;
                s[i] = (saw * 0.5f + sinf(phase) * 0.5f + Noise() * 0.08f) * fminf(1.0f, t * 8) * (1 - t * 0.6f);
            }
            return s;
        }
        case Sfx::Break: {  // plastic bricks clattering: many tiny clicks
            int n = (int)(0.4f * kRate);
            std::vector<float> s(n, 0.0f);
            for (int c = 0; c < 18; c++) {
                int start = (int)((rand() / (float)RAND_MAX) * (n - 900) * (c < 4 ? 0.15f : 1.0f));
                float freq = 1800 + (rand() % 2600);
                float amp = 0.5f + (rand() / (float)RAND_MAX) * 0.5f;
                for (int i = 0; i < 800 && start + i < n; i++) {
                    float t = i / (float)kRate;
                    s[start + i] += (sinf(kTau * freq * t) * 0.6f + Noise() * 0.4f) * expf(-i / 120.0f) * amp;
                }
            }
            for (auto& v : s) v *= 0.6f;
            return s;
        }
        case Sfx::Stud: {
            int n = (int)(0.12f * kRate);
            std::vector<float> s(n);
            for (int i = 0; i < n; i++) {
                float t = (float)i / n;
                float f = t < 0.4f ? 1320.0f : 1980.0f;
                s[i] = sinf(kTau * f * i / kRate) * expf(-t * 3.0f) * 0.6f;
            }
            return s;
        }
        default: return std::vector<float>(100, 0.0f);
    }
}

Sound MakeSound(const std::vector<float>& samples) {
    Wave w = {};
    w.frameCount = (unsigned int)samples.size();
    w.sampleRate = kRate;
    w.sampleSize = 16;
    w.channels = 1;
    short* data = (short*)MemAlloc((unsigned int)(samples.size() * sizeof(short)));
    for (size_t i = 0; i < samples.size(); i++) {
        float v = fmaxf(-1.0f, fminf(1.0f, samples[i]));
        data[i] = (short)(v * 30000.0f);
    }
    w.data = data;
    Sound snd = LoadSoundFromWave(w);
    UnloadWave(w);
    return snd;
}
}  // namespace

namespace Audio {

void Init() {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;  // no audio device (e.g. headless CI) -> stay silent
    gReady = true;
    for (int i = 0; i < (int)Sfx::Count; i++) {
        gSlots[i].base = MakeSound(Synth((Sfx)i));
        for (int a = 0; a < kAliases; a++) gSlots[i].alias[a] = LoadSoundAlias(gSlots[i].base);
    }
}

void Shutdown() {
    if (!gReady) return;
    for (auto& slot : gSlots) {
        for (auto& a : slot.alias) UnloadSoundAlias(a);
        UnloadSound(slot.base);
    }
    CloseAudioDevice();
    gReady = false;
}

void Play(Sfx sfx, float pitch, float volume) {
    if (!gReady) return;
    Slot& slot = gSlots[(int)sfx];
    Sound& s = slot.alias[slot.next];
    slot.next = (slot.next + 1) % kAliases;
    SetSoundPitch(s, pitch);
    SetSoundVolume(s, volume);
    PlaySound(s);
}

}  // namespace Audio
