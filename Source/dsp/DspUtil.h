// Small DSP building blocks (shared with LA). Plain C++, no JUCE.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace ml
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clamp01 (float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float semis (float st) { return std::exp2 (st * (1.0f / 12.0f)); }

// Pade-style tanh: accurate to ~1e-3 in [-3,3], hard-limited outside
inline float fastTanh (float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// fast sine for phase in [0,1)
inline float sin01 (float p)
{
    // parabola approximation with one refinement step (error ~0.1%)
    float x = p * 2.0f - 1.0f;           // -1..1 maps to -pi..pi
    float y = 4.0f * x * (1.0f - std::fabs (x)); // ~ -sin(pi x)
    y = 0.225f * (y * std::fabs (y) - y) + y;
    return -y;
}

// deterministic random numbers
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    explicit Rng (uint32_t seed = 1) { setSeed (seed); }
    void setSeed (uint32_t seed) { s = seed * 2654435761u + 0x6d2b79f5u; if (s == 0) s = 1; next(); next(); }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }      // 0..1
    float bi() { return uni() * 2.0f - 1.0f; }                                   // -1..1
};

inline uint32_t hash32 (uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x;
}

// smooth random (cubic-interpolated random points), output -1..1
struct SmoothNoise
{
    Rng rng;
    float a = 0, b = 0, c = 0, d = 0, t = 0;
    void seed (uint32_t s) { rng.setSeed (s); a = rng.bi(); b = rng.bi(); c = rng.bi(); d = rng.bi(); t = rng.uni(); }
    // advance by `inc` cycles (rate / updateRate)
    float next (float inc)
    {
        t += inc;
        while (t >= 1.0f) { t -= 1.0f; a = b; b = c; c = d; d = rng.bi(); }
        // Catmull-Rom between b and c
        const float t2 = t * t, t3 = t2 * t;
        const float v = 0.5f * ((2.0f * b) + (-a + c) * t + (2.0f * a - 5.0f * b + 4.0f * c - d) * t2 + (-a + 3.0f * b - 3.0f * c + d) * t3);
        return clampf (v, -1.0f, 1.0f);
    }
};

struct OnePole
{
    float z = 0, a = 0;
    void setCutoff (float hz, float sr) { a = std::exp (-kTwoPi * std::min (hz, sr * 0.49f) / sr); }
    void setTime (float sec, float sr) { a = sec <= 0 ? 0.0f : std::exp (-1.0f / (sec * sr)); }
    float lp (float x) { z = x + a * (z - x); return z; }
    float hp (float x) { return x - lp (x); }
    void reset (float v = 0) { z = v; }
};

// ---------------------------------------------------------------------------
// ADSR with analog-style curves. Times in seconds.
struct Adsr
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float value = 0, sr = 48000;
    float aCoef = 0, aBase = 0, dCoef = 0, dBase = 0, rCoef = 0, rBase = 0, sus = 1;

    static float coef (float time, float sr, float ratio) { return std::exp (-std::log ((1.0f + ratio) / ratio) / std::max (1.0f, time * sr)); }
    void set (float a, float d, float s, float r)
    {
        sus = s;
        const float ra = 0.4f, rd = 0.002f;
        aCoef = coef (a, sr, ra); aBase = (1.0f + ra) * (1.0f - aCoef);
        dCoef = coef (d, sr, rd); dBase = (s - rd) * (1.0f - dCoef);
        rCoef = coef (r, sr, rd); rBase = -rd * (1.0f - rCoef);
    }
    void gate (bool on, bool retrigFromZero = false)
    {
        if (on) { if (retrigFromZero) value = 0; stage = Attack; }
        else if (stage != Idle) stage = Release;
    }
    float next()
    {
        switch (stage)
        {
            case Idle: return 0.0f;
            case Attack: value = aBase + value * aCoef; if (value >= 1.0f) { value = 1.0f; stage = Decay; } break;
            case Decay: value = dBase + value * dCoef; if (value <= sus) { value = sus; stage = Sustain; } break;
            case Sustain: value = sus; break;
            case Release: value = rBase + value * rCoef; if (value <= 0.0f) { value = 0.0f; stage = Idle; } break;
        }
        return value;
    }
    bool active() const { return stage != Idle; }
};

// ---------------------------------------------------------------------------
// Filters (per channel). Coefficients are set at control rate.
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 2, a1 = 0, a2 = 0, a3 = 0;
    void set (float hz, float res, float sr)
    {
        g = std::tan (kPi * clampf (hz, 10.0f, sr * 0.45f) / sr);
        k = 2.0f - 1.96f * clamp01 (res);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    // returns lp, writes bp/hp
    inline void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        // keep states bounded under heavy drive
        if (std::fabs (ic1) > 8.0f) ic1 = 8.0f * fastTanh (ic1 * 0.125f);
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

// 4-pole ladder (TPT, linear core solved exactly, saturating input)
struct Ladder
{
    float s[4] {}, G = 0, k = 0, comp = 1;
    void set (float hz, float res, float sr)
    {
        const float g = std::tan (kPi * clampf (hz, 10.0f, sr * 0.45f) / sr);
        G = g / (1.0f + g);
        k = 3.9f * clamp01 (res);
        comp = 1.0f + 0.55f * k;
    }
    inline float tick (float x)
    {
        const float G2 = G * G, G4 = G2 * G2;
        const float S = G2 * G * (1.0f - G) * s[0] + G2 * (1.0f - G) * s[1] + G * (1.0f - G) * s[2] + (1.0f - G) * s[3];
        float y4 = (G4 * x * comp + S) / (1.0f + k * G4);
        float u = fastTanh (x * comp - k * y4);
        for (int i = 0; i < 4; ++i)
        {
            const float v = (u - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            u = y;
        }
        return u;
    }
    void reset() { for (auto& v : s) v = 0; }
};

// fractional delay line (power-of-two size)
struct DelayLine
{
    std::vector<float> buf; int mask = 0, w = 0;
    void init (int minSize) { int n = 1; while (n < minSize) n <<= 1; buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (float delay) const   // delay in samples, >= 1
    {
        const float rp = (float) w - delay;
        const int i = (int) std::floor (rp);
        const float f = rp - (float) i;
        const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)];
        return a + (b - a) * f;
    }
    inline float readInt (int delay) const { return buf[(size_t) ((w - delay) & mask)]; }
};

inline float tempoDivisionBeats (int idx)
{
    // matches kSync
    static const float beats[] = { 32, 16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f,
                                   4.0f / 3, 2.0f / 3, 1.0f / 3, 1.0f / 6, 3, 1.5f, 0.75f, 0.375f };
    return beats[std::max (0, std::min (idx, 16))];
}
inline float delayDivisionBeats (int idx)
{
    // matches kDelay
    static const float beats[] = { 0.125f, 1.0f / 6, 0.25f, 1.0f / 3, 0.375f, 0.5f, 2.0f / 3, 0.75f, 1.0f,
                                   4.0f / 3, 1.5f, 2.0f, 3.0f, 4.0f };
    return beats[std::max (0, std::min (idx, 13))];
}
} // namespace ml
