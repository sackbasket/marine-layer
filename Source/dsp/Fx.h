// Cloud effects shared with LA: shimmer reverb and Smog (tape age).
#pragma once
#include "DspUtil.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace ml
{
// linear per-block ramp for a parameter
struct Ramp
{
    float cur = 0, target = 0, step = 0; int left = 0; bool init = false;
    void set (float t, int n) { if (! init) { cur = target = t; init = true; left = 0; return; } target = t; left = n; step = (t - cur) / (float) std::max (1, n); }
    float next() { if (left > 0) { cur += step; if (--left == 0) cur = target; } return cur; }
    void snap (float t) { cur = target = t; left = 0; init = true; }
};

class Reverb
{
public:
    void prepare (double sr);
    void reset();
    void process (float* l, float* r, int n, float size, float decay, float tone, float shimmer, float motion, float preMs, float mix);
private:
    static constexpr int N = 8;
    double sr = 48000;
    DelayLine line[N], pre[2], ap[2][4], shim;
    float apLen[4] {};
    float baseLen[N] {};
    float damp[N] {};
    double modPhase = 0;
    float shimPhase = 0;
    OnePole shimLp, shimHp;
    float curSize = -1;
    Ramp mixR, shimR;
};

class Smog
{
public:
    void prepare (double sr);
    void reset (uint32_t seed = 7);
    void reseed (uint32_t seed = 7);   // restart the random parts only (transport start)
    void process (float* l, float* r, int n, float amount, float wobble, float crush, float dust, float drop);
private:
    double sr = 48000;
    DelayLine d[2];
    SmoothNoise wow, flut;
    double flutPhase = 0;
    Rng rng;
    float holdL = 0, holdR = 0, holdPhase = 0;
    OnePole lp1[2], lp2[2], hp[2], hissLp[2], dropLp[2];
    float crackle = 0, crackleEnv = 0;
    float dropGain = 1, dropTarget = 1; int dropHold = 0;
    float pinkB[2][3] {};
    Ramp amtR;
};

inline float softClip (float x)
{
    const float a = std::fabs (x);
    if (a <= 0.8f) return x;
    const float y = 0.8f + 0.2f * fastTanh ((a - 0.8f) * 5.0f);
    return x < 0 ? -y : y;
}
} // namespace ml
