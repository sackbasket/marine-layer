// Offline engine tests: run on Linux before pushing and on the macOS runner.
#include "../Source/dsp/Engine.h"
#include "../Source/dsp/Presets.h"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <random>
#include <memory>
#include <cstdlib>

using namespace ml;

static int failures = 0;
#define CHECK(cond, ...) do { if (cond) { std::printf ("  ok   "); std::printf (__VA_ARGS__); std::printf ("\n"); } \
                              else { ++failures; std::printf ("  FAIL "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static constexpr double SR = 48000.0;
static constexpr int BS = 512;

struct Rig
{
    std::unique_ptr<Engine> e = std::make_unique<Engine>();
    juce::MidiBuffer midi;
    HostInfo host;
    Rig() { e->prepare (SR, BS); defaultValues (e->values); host.playing = false; }
    void setPreset (int i)
    {
        defaultValues (e->values);
        for (auto& kv : factoryPresets()[(size_t) i].values) e->values[kv.first] = kv.second;
    }
    // processes `seconds` of audio from gen(t, ch) -> returns output peak, stores output
    template <typename Gen>
    float run (double seconds, Gen gen, std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr, std::vector<float>* inCopy = nullptr)
    {
        const int total = (int) (seconds * SR);
        std::vector<float> l (BS), r (BS);
        float pk = 0;
        for (int done = 0; done < total; done += BS)
        {
            const int n = std::min (BS, total - done);
            for (int i = 0; i < n; ++i) { const double t = (done + i) / SR; l[(size_t) i] = gen (t, 0); r[(size_t) i] = gen (t, 1); }
            if (inCopy) inCopy->insert (inCopy->end(), l.begin(), l.begin() + n);
            e->process (l.data(), r.data(), l.data(), r.data(), n, midi, host);
            if (host.playing) host.ppq += n / SR * host.bpm / 60.0;
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) return 1e9f;
                pk = std::max (pk, std::max (std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i])));
            }
            if (outL) outL->insert (outL->end(), l.begin(), l.begin() + n);
            if (outR) outR->insert (outR->end(), r.begin(), r.begin() + n);
        }
        return pk;
    }
};

static float noiseGen (double t, int ch)
{
    static std::mt19937 rng (5);
    static std::normal_distribution<float> nd (0.0f, 0.15f);
    (void) t; (void) ch;
    return nd (rng);
}

// a little "song": chord pad + plucks + noise hits
static float songGen (double t, int ch)
{
    const double f[] = { 130.81, 155.56, 196.0, 261.63 };
    double v = 0;
    for (double x : f) v += 0.08 * std::sin (2 * M_PI * x * t) + 0.03 * std::sin (4 * M_PI * x * t);
    const double beat = std::fmod (t, 0.5);
    v += 0.4 * std::exp (-beat * 30.0) * std::sin (2 * M_PI * 60 * t);
    v += ch * 0.0;
    return (float) v;
}

static void testSilence()
{
    std::printf ("silence in -> silence out\n");
    Rig r; r.setPreset (0);
    const float pk = r.run (6.0, [] (double, int) { return 0.0f; });
    CHECK (pk < 1e-4f, "peak %.2e", pk);
}

static void testTransparent()
{
    std::printf ("mix 0 is bit-transparent\n");
    Rig r; r.setPreset (0);
    r.e->values[mix] = 0;
    std::vector<float> in, out;
    r.run (3.0, noiseGen, &out, nullptr, &in);
    double maxd = 0;
    for (size_t i = 0; i < in.size(); ++i) maxd = std::max (maxd, (double) std::fabs (in[i] - out[i]));
    CHECK (maxd == 0.0, "max difference %.3e", maxd);
}

static float rms (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; to = std::min (to, v.size());
    for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return (float) std::sqrt (s / std::max<size_t> (1, to - from));
}

static void testFreeze()
{
    std::printf ("freeze holds the memory\n");
    Rig r; r.setPreset (0);
    r.e->values[mix] = 1;
    r.run (2.0, [] (double t, int) { return (float) (0.4 * std::sin (2 * M_PI * 220 * t)); });
    r.e->values[freeze] = 1;
    r.run (0.2, [] (double t, int) { return (float) (0.4 * std::sin (2 * M_PI * 220 * t)); });
    const auto w0 = r.e->uiWrite.load();
    std::vector<float> out;
    r.run (6.0, [] (double, int) { return 0.0f; }, &out);
    const auto w1 = r.e->uiWrite.load();
    const float late = rms (out, (size_t) (4 * SR), (size_t) (6 * SR));
    CHECK (w0 == w1, "record head stopped (%lld -> %lld)", (long long) w0, (long long) w1);
    CHECK (late > 0.01f, "cloud still sounding 4-6 s after the input stopped (rms %.3f)", late);
}

static void testFeedbackBounded()
{
    std::printf ("feedback at max stays bounded\n");
    Rig r; r.setPreset (0);
    r.e->values[feedback] = 1; r.e->values[forecast] = 1; r.e->values[mix] = 1; r.e->values[humidity] = 1;
    r.e->values[shimmer] = 1; r.e->values[space] = 1; r.e->values[revDecay] = 20; r.e->values[drive] = 1;
    const float pk = r.run (25.0, noiseGen);
    CHECK (pk < 4.0f, "peak %.2f over 25 s", pk);
}

static void testNoClicks()
{
    std::printf ("no clicks at grain boundaries\n");
    for (int w = 0; w < 5; ++w)
    {
        Rig r; r.setPreset (0);
        auto& v = r.e->values;
        v[mix] = 1; v[space] = 0; v[shimmer] = 0; v[smog] = 0; v[drive] = 0; v[nostalgia] = 0; v[reverse] = 0;
        v[forecast] = 0.25f; v[tone] = -0.05f; v[feedback] = 0; v[fsafe] = 0; v[pspread] = 0; v[pitch] = 0; v[snap] = 0;
        v[window] = (float) w; v[glide] = 0; v[life] = 0;
        r.run (3.0, [] (double t, int) { return (float) (0.5 * std::sin (2 * M_PI * 220 * t)); });
        v[freeze] = 1;
        std::vector<float> out;
        r.run (4.0, [] (double, int) { return 0.0f; }, &out);
        float pk = 0, d2 = 0;
        for (size_t i = (size_t) SR; i < out.size(); ++i)
        {
            pk = std::max (pk, std::fabs (out[i]));
            d2 = std::max (d2, std::fabs (out[i] - 2 * out[i - 1] + out[i - 2]));
        }
        // a clean 220 Hz sine has a second difference of ~0.0008 x its level
        CHECK (pk > 0.01f && d2 < 0.012f * pk, "%s window: peak %.3f, max 2nd difference %.5f (%.4f of peak)", kWindow[w], pk, d2, d2 / std::max (1e-9f, pk));
    }
}

static std::vector<float> chord (const int* notes, int n, double seconds)
{
    std::vector<float> v ((size_t) (seconds * SR));
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = i / SR;
        double s = 0;
        for (int k = 0; k < n; ++k)
        {
            const double f = 440.0 * std::pow (2.0, (notes[k] - 69) / 12.0);
            for (int h = 1; h <= 6; ++h) s += 0.1 / h * std::sin (2 * M_PI * f * h * t);
        }
        v[i] = (float) s;
    }
    return v;
}

static void testKey()
{
    std::printf ("key detection\n");
    const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    struct C { int notes[3]; int root; bool minor; const char* label; };
    const C cases[] = { { { 48, 51, 55 }, 0, true, "C minor" }, { { 57, 61, 64 }, 9, false, "A major" },
                        { { 50, 53, 57 }, 2, true, "D minor" }, { { 55, 59, 62 }, 7, false, "G major" } };
    for (auto& c : cases)
    {
        Listener L; L.prepare (SR, 1 << 21);
        auto v = chord (c.notes, 3, 5.0);
        for (size_t i = 0; i < v.size(); i += BS) L.push (v.data() + i, (int) std::min<size_t> (BS, v.size() - i), (int64_t) i, true);
        if (std::getenv ("ML_DEBUG")) { for (int k = 0; k < 12; ++k) std::printf ("%.3f ", L.chromaLong[k]); std::printf ("\n"); }
        const int root = L.keyRoot.load(); const bool mn = L.keyMinor.load();
        CHECK (root == c.root && mn == c.minor, "%s chord -> key %s %s, chord %s %s", c.label, names[root], mn ? "minor" : "major",
               names[L.chordRoot.load()], L.chordMinor.load() ? "minor" : "major");
    }
    // pitch tracking
    Listener L; L.prepare (SR, 1 << 21);
    const int n1[] = { 57 };
    auto v = chord (n1, 1, 1.0);
    for (size_t i = 0; i < v.size(); i += BS) L.push (v.data() + i, BS, (int64_t) i, true);
    const auto& h = L.at ((int64_t) (0.8 * SR));
    CHECK (std::fabs (h.f0 - 220.0f) < 2.0f, "A3 pitch %.1f Hz (confidence %.2f)", h.f0, h.conf);
}

// spectral envelope sampled at the harmonics of 260 Hz (the +12 st output's partials,
// which are also every second partial of the 130 Hz input), in dB around their mean
static std::vector<float> envelope (const std::vector<float>& x, size_t from)
{
    const int N = 1 << 15;
    std::vector<float> buf ((size_t) N * 2, 0.0f);
    juce::dsp::FFT fft (15);
    std::vector<double> acc (N / 2, 0.0);
    int frames = 0;
    for (size_t s = from; s + N <= x.size(); s += N / 2)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = x[s + (size_t) i] * (float) (0.5 - 0.5 * std::cos (2 * M_PI * i / N));
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int k = 0; k < N / 2; ++k) acc[(size_t) k] += (double) buf[(size_t) k] * buf[(size_t) k];
        ++frames;
    }
    std::vector<float> h;
    for (int k = 1; k * 260.0 < 3000.0; ++k)
    {
        const int c = (int) std::lround (k * 260.0 * N / SR), w = (int) (8.0 * N / SR);
        double e = 0; for (int j = c - w; j <= c + w; ++j) e = std::max (e, acc[(size_t) j]);
        h.push_back ((float) (10.0 * std::log10 (e + 1e-20)));
    }
    float m = 0; for (float v : h) m += v; m /= (float) h.size();
    for (auto& v : h) v -= m;
    return h;
}

static float voice (double t, int ch)
{
    static float last = 0;
    if (ch == 1) return last;
    // 130 Hz glottal pulse train through two formant resonators (700 Hz, 1150 Hz)
    static double ph = 0; static double y1 = 0, y2 = 0, z1 = 0, z2 = 0; static double lastT = -1;
    if (t < lastT) { ph = 0; y1 = y2 = z1 = z2 = 0; }
    lastT = t;
    ph += 130.0 / SR;
    double x = 0; if (ph >= 1.0) { ph -= 1.0; x = 1.0; }
    auto reson = [] (double in, double f, double bw, double& a1, double& a2)
    {
        const double r = std::exp (-M_PI * bw / SR), c = 2 * r * std::cos (2 * M_PI * f / SR);
        const double y = in + c * a1 - r * r * a2; a2 = a1; a1 = y; return y;
    };
    const double v = reson (x, 700, 110, y1, y2) * 0.6 + reson (x, 1150, 120, z1, z2) * 0.4;
    last = (float) (v * 0.05);
    return last;
}

static void testFormant()
{
    std::printf ("formant-safe pitch keeps the spectral envelope\n");
    if (std::getenv ("ML_DEBUG")) std::printf ("    (harmonics of 260 Hz below 3 kHz)\n");
    float err[2] {};
    for (int safe = 0; safe < 2; ++safe)
    {
        Rig r; r.setPreset (0);
        auto& v = r.e->values;
        v[mix] = 1; v[space] = 0; v[shimmer] = 0; v[smog] = 0; v[drive] = 0; v[nostalgia] = 0; v[reverse] = 0; v[feedback] = 0;
        v[forecast] = 0.0f; v[tone] = -0.15f; v[fsafe] = (float) safe; v[pitch] = 12; v[pspread] = 0; v[snap] = 0; v[glide] = 0;
        v[window] = 1; v[gsize] = 300; v[humidity] = 0.7f; v[life] = 0; v[reach] = 0.5f;
        std::vector<float> in, out;
        r.run (3.0, voice, nullptr, nullptr, &in);
        v[freeze] = 1;
        r.run (6.0, [] (double, int) { return 0.0f; }, &out);
        auto a = envelope (in, (size_t) SR), b = envelope (out, (size_t) (2 * SR));
        float e = 0; for (size_t i = 0; i < a.size(); ++i) e += std::fabs (a[i] - b[i]);
        if (std::getenv ("ML_DEBUG")) { for (size_t i = 0; i < a.size(); ++i) std::printf ("%5.1f/%5.1f ", a[i], b[i]); std::printf ("\n"); }
        err[safe] = e / a.size();
    }
    CHECK (err[1] < 4.0f && err[1] < err[0], "+12 st: envelope error %.1f dB formant-safe vs %.1f dB plain resampling", err[1], err[0]);
}

static void testParticles()
{
    std::printf ("particle simulation is deterministic and bounded\n");
    Weather a, b; Gravity g;
    g.setParams (0, bodyFirefly, 1, 0.7f); g.setParams (1, bodyPendulum, 1, 0.7f);
    WeatherParams p; p.humidity = 1.4f; p.turb = 1; p.wind = 1; p.gravity = 0.5f; p.temp = 0.5f; p.bodyForce = 1; p.handPull = 1; p.handGust = 1;
    bool same = true, bounded = true; long events = 0;
    for (int blk = 0; blk < 3000; ++blk)
    {
        p.handX = 0.5f + 0.4f * std::sin (blk * 0.05f); p.handY = 0.7f * std::cos (blk * 0.07f);
        g.process (BS, SR);
        const int na = a.process (BS, SR, p, g), nb = b.process (BS, SR, p, g);
        events += na;
        if (na != nb) same = false;
        for (int k = 0; k < std::min (na, nb); ++k)
        {
            if (a.events[k].x != b.events[k].x || a.events[k].y != b.events[k].y || a.events[k].offset != b.events[k].offset) same = false;
            if (! (a.events[k].x >= 0 && a.events[k].x <= 1.05f && std::fabs (a.events[k].y) <= 1.0f && a.events[k].offset >= 0 && a.events[k].offset < BS)) bounded = false;
        }
    }
    CHECK (same, "two runs give identical grains (%ld grains)", events);
    CHECK (bounded, "all grains inside the scene");
}

static void testLooper()
{
    std::printf ("gesture looper replays exactly in time\n");
    Rig r; r.setPreset (0);
    r.host.playing = true; r.host.ppqValid = true; r.host.bpm = 120; r.host.ppq = 0;
    r.e->values[loopLen] = 0;   // 1 bar = 2 s at 120
    r.e->looperCmd = 1;
    std::vector<float> rec;
    const int blocks = (int) (2.0 * SR / BS);
    std::vector<float> l (BS), rr (BS);
    for (int b = 0; b < blocks + 2; ++b)
    {
        const float x = 0.5f + 0.4f * std::sin (b * 0.11f);
        r.e->values[handX] = x; r.e->values[handPull] = 1;
        r.e->process (l.data(), rr.data(), l.data(), rr.data(), BS, r.midi, r.host);
        rec.push_back (r.e->effHandX.load());
        r.host.ppq += BS / SR * 2.0;
    }
    r.e->values[handPull] = 0; r.e->values[handX] = 0.1f;
    // frame written by recording block b: (idx(b-1), idx(b)], idx = frame at the block's start
    const double blk = BS / SR * 2.0;
    auto idxAt = [] (double ppq) { return (int) (std::fmod (ppq, 4.0) / 4.0 * 384.0); };
    float maxErr = 0; int compared = 0;
    for (int b = 0; b < blocks; ++b)
    {
        r.e->process (l.data(), rr.data(), l.data(), rr.data(), BS, r.midi, r.host);
        const int f = idxAt (r.host.ppq);
        int src = -1;
        for (int k = 0; k < blocks && src < 0; ++k) if (idxAt (k * blk) >= f) src = k;
        if (src >= 1)
        {
            maxErr = std::max (maxErr, std::fabs (r.e->effHandX.load() - rec[(size_t) src]));
            ++compared;
        }
        r.host.ppq += blk;
    }
    CHECK (r.e->looperState.load() == 2 && compared > 100 && maxErr < 1e-6f, "state %d, %d blocks compared, max error %.4f", r.e->looperState.load(), compared, maxErr);
}

static void testCpu()
{
    std::printf ("CPU worst case\n");
    Rig r; r.setPreset (0);
    auto& v = r.e->values;
    v[humidity] = 1; v[forecast] = 0.7f; v[gsize] = 1000; v[fsafe] = 1; v[listen] = 0; v[pitch] = 12; v[mix] = 1;
    v[space] = 1; v[shimmer] = 1; v[smog] = 1; v[drive] = 1; v[fltType] = 1; v[nostalgia] = 1; v[ghosts] = 1; v[feedback] = 0.5f;
    for (int b = 0; b < 4; ++b) v[g1Type + b * bodyStride] = (float) (1 + b);
    r.host.playing = true; r.host.ppqValid = true;
    r.run (12.0, voice);
    int maxGrains = 0;
    const auto t0 = std::chrono::steady_clock::now();
    const double secs = std::getenv ("ML_ONLY_CPU") ? 1.0 : 10.0;
    std::vector<float> l (BS), rr (BS);
    for (int done = 0; done < (int) (secs * SR); done += BS)
    {
        for (int i = 0; i < BS; ++i) { l[(size_t) i] = voice ((done + i) / SR + 4.0, 0); rr[(size_t) i] = l[(size_t) i]; }
        r.e->process (l.data(), rr.data(), l.data(), rr.data(), BS, r.midi, r.host);
        r.host.ppq += BS / SR * 2.0;
        maxGrains = std::max (maxGrains, r.e->activeGrains.load());
    }
    const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    // target: under 40 % on the user's Mac. Build machines are slower and shared, so the
    // hard limit here only catches big regressions; the number is printed either way.
    CHECK (el / secs < 0.80, "%.1f %% of one core (up to %d grains at once, target < 40 %% on Apple Silicon)", 100.0 * el / secs, maxGrains);
}

static void testPresetsAndDice()
{
    std::printf ("every preset + 200 dice rolls stay finite\n");
    float worst = 0; int bad = 0;
    const auto& ps = factoryPresets();
    for (size_t i = 0; i < ps.size(); ++i)
    {
        Rig r; r.setPreset ((int) i);
        r.host.playing = true; r.host.ppqValid = true;
        const float pk = r.run (3.0, songGen);
        worst = std::max (worst, pk);
        if (! (pk < 8.0f)) { ++bad; std::printf ("    preset %s peak %.2f\n", ps[i].name, pk); }
    }
    CHECK (bad == 0, "%zu presets, loudest peak %.2f", ps.size(), worst);
    worst = 0; bad = 0;
    for (int d = 0; d < 200; ++d)
    {
        Rig r; r.setPreset (0);
        rollDice (r.e->values, d % 4 == 3, (uint32_t) (d * 7919 + 1));
        r.e->values[mix] = 1;
        const float pk = r.run (1.0, songGen);
        worst = std::max (worst, pk);
        if (! (pk < 8.0f)) ++bad;
    }
    CHECK (bad == 0, "200 rolls, loudest peak %.2f", worst);
}

static void testRepeatable()
{
    std::printf ("same input -> same output\n");
    Rig a, b; a.setPreset (0); b.setPreset (0);
    std::vector<float> oa, ob;
    a.run (4.0, songGen, &oa);
    b.run (4.0, songGen, &ob);
    double maxd = 0;
    for (size_t i = 0; i < oa.size(); ++i) maxd = std::max (maxd, (double) std::fabs (oa[i] - ob[i]));
    CHECK (maxd == 0.0, "max difference %.3e", maxd);
}

static void testGhosts()
{
    std::printf ("ghosts return bookmarked bars\n");
    Rig r; r.setPreset (0);
    r.host.playing = true; r.host.ppqValid = true; r.host.bpm = 120;
    r.e->values[ghosts] = 1; r.e->values[ghostAge] = 0; r.e->values[ghostRate] = 1; r.e->values[humidity] = 0;
    r.e->toggleBookmark (1);
    r.run (24.0, songGen);
    CHECK (r.e->ghostFlash.load() > 10, "%d ghost grains played", r.e->ghostFlash.load());
}

int main()
{
    if (std::getenv ("ML_ONLY_KEY")) { testKey(); testFormant(); return 0; }
    if (std::getenv ("ML_ONLY_CPU")) { testCpu(); return 0; }
    testSilence();
    testTransparent();
    testFreeze();
    testFeedbackBounded();
    testNoClicks();
    testKey();
    testFormant();
    testParticles();
    testLooper();
    testGhosts();
    testRepeatable();
    testPresetsAndDice();
    testCpu();
    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
