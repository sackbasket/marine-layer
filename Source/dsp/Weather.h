// Marine Layer weather: every grain is a particle drifting in a small 2D
// weather simulation. x = position in the memory (0 = now, 1 = oldest),
// y = pitch (-1..1). Particles live for a few seconds and "rain" grains while
// they drift. Deterministic: fixed 250 Hz step, restarts from the same seed.
#pragma once
#include "DspUtil.h"
#include "Gravity.h"
#include <atomic>
#include <cmath>

namespace ml
{
struct WeatherParams
{
    float humidity = 0.5f;     // population (can exceed 1 when Forecast pushes it)
    float sizeSec = 0.15f;     // grain length
    float reach = 0.6f;        // how far back in the memory the fog rolls
    float wind = 0, windDir = 0, turb = 0.3f, gravity = 0, temp = 0;
    float bodyForce = 0.5f;
    float handX = 0.3f, handY = 0, handPull = 0, handGust = 0;
    float drift = 1.0f;        // Life: slow organic wandering of the wind
    float lifeAmt = 0;
    float overlap = 1.0f;      // grain rate per particle
};

struct GrainEvent
{
    float x, y, z, bright, pan;
    int offset;                // sample offset inside the block
    int particle;
    float pitchOverride;       // NaN = use y
};

struct Stroke
{
    static constexpr int maxPts = 48;
    float x[maxPts] {}, y[maxPts] {};
    int n = 0;
    float ttl = 0;             // seconds left
};

struct VisParticle { float x, y, z, flash, life; uint8_t src; };

class Weather
{
public:
    static constexpr int maxParticles = 200, maxEvents = 512, maxEmitters = 4, maxStrokes = 4;
    static constexpr double stepSec = 1.0 / 250.0;

    Weather();
    void reset (uint32_t seed = 1);
    // advances the simulation over one block; grain requests go to events[]
    int process (int numSamples, double sampleRate, const WeatherParams&, const Gravity& bodies);

    // inputs from the engine (audio thread)
    void setEmitter (int i, bool on, float x, float y) { emOn[i] = on; emX[i] = x; emY[i] = y; }
    void addStroke (const Stroke& s);
    void burst (float x, float y, float strength);
    void spawnNote (float pitchSemis, int count);   // MIDI "Plays Cloud"

    GrainEvent events[maxEvents];

    // UI snapshot (double buffered)
    struct Vis { VisParticle p[maxParticles]; int n = 0; float strokeX[maxStrokes][Stroke::maxPts]; float strokeY[maxStrokes][Stroke::maxPts]; int strokeN[maxStrokes] {}; float strokeTtl[maxStrokes] {}; };
    Vis vis[2];
    std::atomic<int> visIndex { 0 };
    std::atomic<int> grainsFired { 0 };

private:
    struct Particle
    {
        bool alive = false;
        float x = 0, y = 0, vx = 0, vy = 0, z = 0.5f, age = 0, life = 4, phase = 0, flash = 0, jitter = 1, pitchOv = NAN;
        uint8_t src = 0;
        int side = 1;
    };
    void step (double dt, const WeatherParams&, const Gravity&, int sampleOffset);
    void spawn (const WeatherParams&, int src);
    float psi (float x, float y, float t) const;
    void publish();

    Particle parts[maxParticles];
    Stroke strokes[maxStrokes];
    int numEvents = 0;
    double acc = 0, simTime = 0;
    float spawnAcc = 0;
    float prevHX = 0.3f, prevHY = 0, hvx = 0, hvy = 0;
    bool emOn[maxEmitters] {}; float emX[maxEmitters] {}, emY[maxEmitters] {};
    float pendingBurstX = 0, pendingBurstY = 0, pendingBurst = 0;
    float pendingNote = NAN; int pendingNoteCount = 0;
    SmoothNoise lifeA, lifeB;
    Rng rng;
    int nextId = 0, emitterTurn = 0;
};
} // namespace ml
