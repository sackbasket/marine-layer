// Marine Layer engine: memory buffer + listener + weather particles -> grains
// (formant-safe PSOLA or plain resampling, aged by how old the memory is),
// song memory + ghosts, gesture looper and the cloud effects chain.
// Plain C++ + juce_dsp, no UI code. Latency 0.
#pragma once
#include "ParamDefs.h"
#include "DspUtil.h"
#include "Gravity.h"
#include "Weather.h"
#include "Listener.h"
#include "Fx.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>
#include <array>

namespace ml
{
struct HostInfo
{
    double bpm = 120, ppq = 0, barBeats = 4;
    bool playing = false, ppqValid = false;
};

class Engine
{
public:
    static constexpr int maxGrains = 128, maxChunk = 256, maxBookmarks = 8, maxBars = 1024;
    static constexpr int ovChunk = 512;

    Engine();
    void prepare (double sampleRate, int maxBlock);
    void reset();   // clears the memory too
    // in/out may alias
    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  const juce::MidiBuffer& midi, const HostInfo& host);

    float values[numParams] {};   // effective (modulated) parameter values, written by the processor

    // ---- UI -> engine
    std::atomic<bool> emOn[4] {};
    std::atomic<float> emX[4] {}, emY[4] {};
    std::atomic<bool> holdFreeze { false };
    void addStroke (const Stroke&);
    void burst (float x, float y, float strength);
    std::atomic<int> looperCmd { 0 };   // 1 = record / overdub restart, 2 = clear
    void toggleBookmark (int bar);
    void setBookmarks (const std::vector<int>&);
    std::vector<int> getBookmarks() const;

    // ---- engine -> UI / processor
    std::atomic<int> looperState { 0 };   // 0 idle, 1 recording, 2 playing
    std::atomic<float> looperPhase { 0 }, effHandX { 0.3f }, effHandY { 0.5f }, effHandPull { 0 }, effHandGust { 0 };
    std::atomic<int> curBar { -1 };
    std::atomic<int> uiBookmarks[maxBookmarks];
    std::atomic<bool> uiBookmarkAuto[maxBookmarks];
    bool barRecorded (int bar) const { return bar >= 0 && bar < maxBars && barStart[(size_t) bar].load (std::memory_order_relaxed) >= 0; }
    std::atomic<float> inLevel { 0 }, cloudLevel { 0 }, srcLife { 0 }, freezeAmt { 0 };
    std::atomic<int> activeGrains { 0 }, ghostFlash { 0 };
    std::atomic<int> uiRoot { 0 }, uiScale { scMinor };
    std::atomic<bool> uiKeyFromMidi { false };
    std::atomic<int64_t> uiWrite { 0 };
    std::atomic<float> uiMemSeconds { 12 };
    float overviewPeak (int64_t chunk) const { return ov[(size_t) (chunk & ovMask)]; }
    int64_t recorded() const { return recordedCount; }
    double getSampleRate() const { return sr; }

    Weather weather;
    Gravity gravity;
    Listener listener;

    // exposed for tests
    float lastCloudGain = 1;
    int grainsDropped = 0;

private:
    struct Micro { double pos = 0; float t = 0, len = 0; bool on = false; };
    struct Grain
    {
        bool on = false, ghost = false, rev = false, psola = false;
        int delay = 0, len = 1, t = 0, win = 0;
        double src = 0, acc = 0;
        double rate = 1, rateMul = 1;
        float gl = 1, gr = 1, gain = 1;
        // psola
        float period = 100, fr = 1, nextMicro = 0;
        Micro micro[8]; int numMicro = 0;
        // ageing
        float wear = 0, lpA = 0, lpL = 0, lpR = 0, wowPh = 0, wowInc = 0, wowDepth = 0, flutPh = 0, bitStep = 0;
        float crackle = 0, crackleProb = 0, srcEnv = 0, dropFrom = 2, dropTo = 2, dropG = 1;
        uint32_t rs = 1;
    };
    struct HandFrame { float x, y, pull, gust; };

    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int n,
                       const juce::MidiBuffer& midi, int midiStart, const HostInfo& host);
    void startGrain (const GrainEvent& ev, float memSec, float sizeSec, float lifePitch, float transientMix);
    void startGhost (const HostInfo& host, float sizeSec);
    void renderGrains (float* l, float* r, int n);
    float snapPitch (float t, float f0) const;
    float keyMaskSnap (float midiNote) const;
    float window (int type, float x) const;
    inline float memRead (int ch, double pos) const;
    inline void memRead2 (double pos, float& l, float& r) const;
    inline float songRead (double pos) const;
    void runLooper (const HostInfo& host, int n);
    void runSongMemory (const float* mono, int n, const HostInfo& host);

    double sr = 48000;
    // memory
    std::vector<float> mem[2];
    int64_t memSize = 1, memMask = 0, writeAbs = 0, recordedCount = 0;
    float recGain = 1;
    std::vector<float> ov; int64_t ovMask = 0;
    // song memory (mono, 12 kHz-ish int16)
    std::vector<int16_t> song;
    int songDecim = 4; double songSr = 12000; int64_t songCap = 1, songPos = 0; int decimCount = 0;
    float songLp1 = 0, songLp2 = 0, songLpA = 0;
    std::array<std::atomic<int64_t>, maxBars> barStart;
    int lastBar = -1; double barEnergy = 0; int barSamples = 0; double barMean = -1;
    int bookmarks[maxBookmarks]; bool bookmarkAuto[maxBookmarks];
    juce::SpinLock bookmarkLock;
    float ghostAcc = 0;

    Grain grains[maxGrains];
    float winTab[5][1025];

    // key
    int heldMask = 0; int heldNotes[128] {};
    int effRoot = 0, effMask = 0xAD5, effScale = scMinor;

    // fx
    Reverb reverb; Smog smog;
    Svf svf[2]; OnePole tilt[2], fbLp[2];
    Ramp mixR, outRamp, cutR, duckR, driveR;
    float duckEnv = 0, cloudNorm = 1, activeAvg = 1, cloudActivity = 0;
    std::vector<float> cl, cr, fbL, fbR, dryL, dryR, monoIn;

    // hand / looper
    std::vector<HandFrame> frames;
    int loopState = 0; double recStartBeat = 0, intBeat = 0; int lastFrame = -1;
    // misc
    SmoothNoise lifePitchN, lifeSizeN, lifeDensN;
    Rng rng;
    bool wasPlaying = false;
    juce::SpinLock strokeLock;
    Stroke pendingStrokes[4]; int numPendingStrokes = 0;
    std::atomic<int> burstSeq { 0 }; std::atomic<float> burstX { 0 }, burstY { 0 }, burstS { 0 }; int lastBurstSeq = 0;
};
} // namespace ml
