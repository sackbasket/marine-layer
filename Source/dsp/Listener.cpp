#include "Listener.h"
#include <cmath>
#include <algorithm>

namespace ml
{
void Listener::prepare (double s, int64_t memCap)
{
    sr = s;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    ring.assign (winSize, 0.0f);
    win.resize (winSize);
    for (int i = 0; i < winSize; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (i + 0.5f) / winSize);
    // autocorrelation of the window, to un-bias the signal autocorrelation
    winAcf.assign (winSize, 0.0f);
    for (int t = 0; t < winSize; ++t)
    {
        double a = 0;
        for (int i = 0; i + t < winSize; ++i) a += (double) win[(size_t) i] * win[(size_t) (i + t)];
        winAcf[(size_t) t] = (float) a;
    }
    work.assign (fftSize * 2, 0.0f);
    mag.assign (fftSize / 2, 0.0f);
    prevMag.assign (fftSize / 2, 0.0f);
    numHops = std::max<int64_t> (1, memCap / hop + 2);
    feat.assign ((size_t) numHops, HopFeatures {});
    chromaFft = std::make_unique<juce::dsp::FFT> (chromaOrder);
    cRing.assign (chromaSize, 0.0f);
    cWin.resize (chromaSize);
    for (int i = 0; i < chromaSize; ++i) cWin[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (i + 0.5f) / chromaSize);
    cWork.assign (chromaSize * 2, 0.0f);
    reset();
}

void Listener::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    std::fill (prevMag.begin(), prevMag.end(), 0.0f);
    for (auto& f : feat) f = HopFeatures {};
    std::fill (cRing.begin(), cRing.end(), 0.0f); cPos = 0; hopCount = 0;
    ringPos = 0; fill = 0; fluxMean = 0.01f; sinceOnset = 1000; onsetCount = 0;
    for (auto& c : chromaLong) c = 0;
    for (auto& c : chromaShort) c = 0;
    curKeyScore = -1;
    keyValid = false; chordValid = false;
}

void Listener::push (const float* x, int n, int64_t absWrite, bool recording)
{
    for (int i = 0; i < n; ++i)
    {
        ring[(size_t) ringPos] = x[i];
        ringPos = (ringPos + 1) % winSize;
        cRing[(size_t) cPos] = x[i];
        cPos = (cPos + 1) % chromaSize;
        if (++fill >= hop)
        {
            fill = 0;
            analyse (absWrite + (recording ? i : 0), recording);
        }
    }
}

void Listener::estimateKey (const float* c, int& root, bool& minor, float& score, float* all)
{
    static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float mnr[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    auto corr = [&] (const float* prof, int r)
    {
        float mc = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mc += c[i]; mp += prof[i]; }
        mc /= 12; mp /= 12;
        float num = 0, dc = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float a = c[(i + r) % 12] - mc, b = prof[i] - mp;
            num += a * b; dc += a * a; dp += b * b;
        }
        return num / std::sqrt (dc * dp + 1e-12f);
    };
    float total = 0; for (int i = 0; i < 12; ++i) total += c[i];
    score = -2; root = 0; minor = true;
    for (int r = 0; r < 12; ++r)
        for (int m = 0; m < 2; ++m)
        {
            float s = corr (m ? mnr : maj, r);
            // a strong tonic triad settles major / minor ambiguity (relative keys)
            const float third = c[(r + (m ? 3 : 4)) % 12], tonic = c[r], fifth = c[(r + 7) % 12];
            s += 3.0f * (tonic + 0.6f * fifth + 0.8f * third) / (total + 1e-9f);
            if (all) all[r * 2 + m] = s;
            if (s > score) { score = s; root = r; minor = m != 0; }
        }
}

void Listener::estimateChord (const float* c, int& root, bool& minor, float& score)
{
    float total = 0; for (int i = 0; i < 12; ++i) total += c[i];
    score = 0; root = 0; minor = false;
    if (total <= 1e-9f) return;
    for (int r = 0; r < 12; ++r)
        for (int m = 0; m < 2; ++m)
        {
            const float s = (c[r] + c[(r + (m ? 3 : 4)) % 12] + 0.9f * c[(r + 7) % 12]
                             - 0.5f * (c[(r + (m ? 4 : 3)) % 12] + c[(r + 1) % 12] + c[(r + 6) % 12])) / total;
            if (s > score) { score = s; root = r; minor = m != 0; }
        }
}

void Listener::analyse (int64_t absWrite, bool recording)
{
    // window the latest 2048 samples into a 4096 zero-padded frame
    std::fill (work.begin(), work.end(), 0.0f);
    double e = 0;
    for (int i = 0; i < winSize; ++i)
    {
        const float v = ring[(size_t) ((ringPos + i) % winSize)];
        e += (double) v * v;
        work[(size_t) i] = v * win[(size_t) i];
    }
    const float rms = (float) std::sqrt (e / winSize);
    fft->performRealOnlyForwardTransform (work.data(), true);

    // magnitudes, flux, flatness, chroma
    const int nb = fftSize / 2;
    float flux = 0, logSum = 0, linSum = 0; int flatN = 0;
    const int lo = (int) (100.0 * fftSize / sr), hi = std::min (nb - 1, (int) (10000.0 * fftSize / sr));
    for (int k = 0; k < nb; ++k)
    {
        const float re = work[(size_t) (2 * k)], im = work[(size_t) (2 * k + 1)];
        const float m = std::sqrt (re * re + im * im);
        mag[(size_t) k] = m;
        const float lm = std::log1p (m * 10.0f), lp = std::log1p (prevMag[(size_t) k] * 10.0f);
        if (lm > lp) flux += lm - lp;
        prevMag[(size_t) k] = m;
        if (k >= lo && k <= hi) { logSum += std::log (m + 1e-9f); linSum += m; ++flatN; }
    }
    const float flat = flatN > 0 ? std::exp (logSum / flatN) / (linSum / flatN + 1e-12f) : 1.0f;

    // onset: flux against its running mean
    const float onsetStrength = flux / (fluxMean + 1e-6f);
    fluxMean += (flux - fluxMean) * 0.06f;
    ++sinceOnset;
    const bool loud = rms > 0.003f;
    if (loud && onsetStrength > 2.2f && sinceOnset > (int) (0.07 * sr / hop)) { sinceOnset = 0; ++onsetCount; }
    lastOnset = loud ? onsetStrength : 0.0f;

    // pitch: autocorrelation = inverse FFT of the power spectrum
    for (int k = 0; k < nb; ++k)
    {
        work[(size_t) (2 * k)] = mag[(size_t) k] * mag[(size_t) k];
        work[(size_t) (2 * k + 1)] = 0;
    }
    work[(size_t) (2 * nb)] = 0; work[(size_t) (2 * nb + 1)] = 0;
    fft->performRealOnlyInverseTransform (work.data());
    float f0 = 0, conf = 0;
    if (loud && work[0] > 1e-12f)
    {
        const int tMin = std::max (2, (int) (sr / 1000.0)), tMax = std::min (winSize / 2, (int) (sr / 55.0));
        const float r0 = work[0] / winAcf[0];
        float best = 0; int bestT = 0;
        float vals[1100];
        const int span = std::min (tMax, 1099);
        for (int t = tMin; t <= span; ++t)
        {
            const float v = (work[(size_t) t] / winAcf[(size_t) t]) / r0;
            vals[t] = v;
            if (v > best) { best = v; bestT = t; }
        }
        if (bestT > 0)
        {
            // smallest lag whose local peak reaches 90 % of the best: avoids octave-down errors
            int pick = bestT;
            for (int t = tMin + 1; t < span; ++t)
                if (vals[t] >= 0.9f * best && vals[t] >= vals[t - 1] && vals[t] >= vals[t + 1]) { pick = t; break; }
            float d = 0;
            if (pick > tMin && pick < span)
            {
                const float a = vals[pick - 1], b = vals[pick], c = vals[pick + 1];
                const float den = a - 2 * b + c;
                if (std::fabs (den) > 1e-9f) d = clampf (0.5f * (a - c) / den, -0.5f, 0.5f);
            }
            conf = clamp01 (vals[pick]);
            if (conf > 0.55f) f0 = (float) (sr / (pick + d));
        }
    }

    // store per memory hop (centre of the analysis window)
    if (recording)
    {
        auto& h = feat[(size_t) ((((absWrite - winSize / 2) / hop) % numHops + numHops) % numHops)];
        h.rms = rms; h.onset = lastOnset; h.flat = flat; h.f0 = f0; h.conf = conf;
    }
    level.store (rms);
    tonalness.store (tonalness.load() * 0.9f + 0.1f * (conf * (1.0f - flat)));

    if (++hopCount >= 2) { hopCount = 0; analyseChroma (rms); }
}

// chroma from a long window (170 ms) with peak picking, so low notes separate
void Listener::analyseChroma (float rms)
{
    if (rms <= 0.003f) return;
    std::fill (cWork.begin(), cWork.end(), 0.0f);
    for (int i = 0; i < chromaSize; ++i) cWork[(size_t) i] = cRing[(size_t) ((cPos + i) % chromaSize)] * cWin[(size_t) i];
    chromaFft->performFrequencyOnlyForwardTransform (cWork.data(), true);
    const int nb = chromaSize / 2;
    float mx = 0;
    const int lo = std::max (2, (int) (90.0 * chromaSize / sr)), hi = std::min (nb - 2, (int) (4500.0 * chromaSize / sr));
    for (int k = lo; k <= hi; ++k) mx = std::max (mx, cWork[(size_t) k]);
    if (mx <= 1e-9f) return;
    float frameChroma[12] {};
    for (int k = lo; k <= hi; ++k)
    {
        const float a = cWork[(size_t) (k - 1)], b = cWork[(size_t) k], c = cWork[(size_t) (k + 1)];
        if (b < a || b < c || b < mx * 0.02f) continue;
        const float den = a - 2 * b + c;
        const float d = std::fabs (den) > 1e-12f ? clampf (0.5f * (a - c) / den, -0.5f, 0.5f) : 0.0f;
        const float f = (float) ((k + d) * sr / chromaSize);
        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        const int n = (int) std::lround (midi);
        if (std::fabs (midi - (float) n) > 0.35f) continue;   // between notes: not tonal
        const float w = std::sqrt (b / mx) * (1.0f - 0.6f * clamp01 ((f - 1000.0f) / 3500.0f));
        frameChroma[((n % 12) + 12) % 12] += w;
    }
    float tot = 0; for (float v : frameChroma) tot += v;
    if (tot <= 1e-9f) return;
    const float hopSec = (float) (2 * hop / sr);
    const float aL = std::exp (-hopSec / 4.0f), aS = std::exp (-hopSec / 0.6f);
    for (int i = 0; i < 12; ++i)
    {
        const float v = frameChroma[i] / tot;
        chromaLong[i] = chromaLong[i] * aL + v * (1 - aL);
        chromaShort[i] = chromaShort[i] * aS + v * (1 - aS);
    }
    int r; bool mn; float sc;
    float all[24];
    estimateKey (chromaLong, r, mn, sc, all);
    // hysteresis against the current key's present score
    if (! keyValid.load() || sc > all[curKey * 2 + (curMinor ? 1 : 0)] + 0.04f) { curKey = r; curMinor = mn; }
    keyRoot = curKey; keyMinor = curMinor; keyValid = true;
    estimateChord (chromaShort, r, mn, sc);
    chordRoot = r; chordMinor = mn; chordValid = sc > 0.25f;
}
} // namespace ml
