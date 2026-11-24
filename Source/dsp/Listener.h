// Marine Layer listener: real-time analysis of the input.
// Per hop (512 samples) it measures level, onset strength, spectral flatness
// and pitch (normalised autocorrelation). Features are stored per memory hop so
// grains can pick tonal / transient / airy moments. A slow chroma average
// gives the key, a fast one the current chord.
#pragma once
#include "DspUtil.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>
#include <memory>

namespace ml
{
struct HopFeatures
{
    float rms = 0, onset = 0, flat = 1, f0 = 0, conf = 0;
};

class Listener
{
public:
    static constexpr int hop = 512, winSize = 2048, fftOrder = 12, fftSize = 1 << fftOrder;

    void prepare (double sampleRate, int64_t memoryCapacity);
    void reset();
    // feed live input (mono). absWrite = memory write position at the start of
    // this block, recording = memory is being written (not frozen)
    void push (const float* mono, int n, int64_t absWrite, bool recording);

    const HopFeatures& at (int64_t absSample) const { return feat[(size_t) (((absSample / hop) % numHops + numHops) % numHops)]; }
    int onsetsSinceLastCall() { const int o = onsetCount; onsetCount = 0; return o; }

    // key / chord results (also read by the UI)
    std::atomic<int> keyRoot { 0 }, chordRoot { 0 };
    std::atomic<bool> keyMinor { true }, chordMinor { true }, keyValid { false }, chordValid { false };
    std::atomic<float> level { 0 }, tonalness { 0 };
    float chromaLong[12] {}, chromaShort[12] {};
    float lastOnset = 0;

    static void estimateKey (const float* chroma, int& root, bool& minor, float& score, float* all24 = nullptr);
    static void estimateChord (const float* chroma, int& root, bool& minor, float& score);

private:
    void analyse (int64_t absWrite, bool recording);
    double sr = 48000;
    std::unique_ptr<juce::dsp::FFT> fft, chromaFft;
    static constexpr int chromaOrder = 13, chromaSize = 1 << chromaOrder;
    std::vector<float> cRing, cWin, cWork; int cPos = 0, hopCount = 0;
    void analyseChroma (float rms);
    std::vector<float> ring, win, winAcf, work, prevMag, mag;
    int ringPos = 0, fill = 0;
    int64_t numHops = 1;
    std::vector<HopFeatures> feat;
    float fluxMean = 0.01f;
    int sinceOnset = 1000, onsetCount = 0;
    int curKey = 0; bool curMinor = true; float curKeyScore = -1;
};
} // namespace ml
