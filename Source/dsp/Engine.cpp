#include "Engine.h"
#include <cmath>
#include <algorithm>

namespace ml
{
namespace
{
// Forecast stops: Clear, Haze, Fog, Drizzle, Storm
struct Stop { float dens, size, turb, wind, grav, fb, smog, shim, space, tone, rev; };
const Stop kStops[5] = {
    //  dens  size  turb  wind  grav  fb    smog  shim  space tone   reverse
    { 0.40f, 1.30f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.10f, 0.00f, 0.15f, 0.00f },   // Clear
    { 0.75f, 1.15f, 0.05f, 0.05f, 0.00f, 0.04f, 0.04f, 0.25f, 0.10f, 0.05f, 0.05f },   // Haze
    { 1.00f, 1.00f, 0.10f, 0.08f, 0.00f, 0.10f, 0.10f, 0.12f, 0.18f,-0.20f, 0.10f },   // Fog
    { 1.15f, 0.55f, 0.25f, 0.25f, 0.45f, 0.12f, 0.15f, 0.05f, 0.12f, 0.25f, 0.10f },   // Drizzle (grains fall like rain)
    { 1.45f, 0.35f, 0.70f, 0.75f, 0.20f, 0.20f, 0.30f, 0.00f, 0.15f, 0.05f, 0.30f },   // Storm
};
Stop forecastStop (float w)
{
    const float f = clamp01 (w) * 4.0f;
    const int i = std::min (3, (int) f);
    const float t = f - (float) i;
    const Stop& a = kStops[i]; const Stop& b = kStops[i + 1];
    return { lerp (a.dens, b.dens, t), lerp (a.size, b.size, t), lerp (a.turb, b.turb, t), lerp (a.wind, b.wind, t),
             lerp (a.grav, b.grav, t), lerp (a.fb, b.fb, t), lerp (a.smog, b.smog, t), lerp (a.shim, b.shim, t),
             lerp (a.space, b.space, t), lerp (a.tone, b.tone, t), lerp (a.rev, b.rev, t) };
}

int scaleMaskFor (int s, bool minor)
{
    switch (s)
    {
        case scMinor:      return 0b010110101101;   // 0 2 3 5 7 8 10
        case scMajor:      return 0b101010110101;   // 0 2 4 5 7 9 11
        case scPhrygian:   return 0b010110101011;   // 0 1 3 5 7 8 10
        case scHarmMinor:  return 0b100110101101;   // 0 2 3 5 7 8 11
        case scDorian:     return 0b011010101101;   // 0 2 3 5 7 9 10
        case scPentaMinor: return 0b010010101001;   // 0 3 5 7 10
        case scAuto:       return minor ? 0b010110101101 : 0b101010110101;
        default:           return 0xfff;
    }
}

inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b = w + a;
    return ((((a * t) - b) * t + c) * t + x0);
}
} // namespace

Engine::Engine()
{
    for (int i = 0; i < maxBookmarks; ++i) { bookmarks[i] = -1; bookmarkAuto[i] = false; uiBookmarks[i] = -1; uiBookmarkAuto[i] = false; }
    for (auto& b : barStart) b.store (-1);
    for (int w = 0; w < 5; ++w)
        for (int i = 0; i <= 1024; ++i)
        {
            const float x = i / 1024.0f;
            float v = 0;
            switch (w)
            {
                case 0: { const float e0 = std::exp (-0.5f * (0.5f / 0.16f) * (0.5f / 0.16f));
                          v = (std::exp (-0.5f * ((x - 0.5f) / 0.16f) * ((x - 0.5f) / 0.16f)) - e0) / (1.0f - e0); break; }
                case 1: v = 0.5f - 0.5f * std::cos (kTwoPi * x); break;
                case 2: { const float a = 0.25f;
                          v = x < a ? 0.5f - 0.5f * std::cos (kPi * x / a) : (x > 1 - a ? 0.5f - 0.5f * std::cos (kPi * (1 - x) / a) : 1.0f); break; }
                case 3: case 4:
                {
                    const float y = w == 3 ? x : 1.0f - x;
                    const float a = 0.03f;
                    const float att = y < a ? std::sin (kPi * 0.5f * y / a) : 1.0f;
                    const float dec = y < a ? 1.0f : std::exp (-(y - a) * 4.5f);
                    const float tail = y > 0.85f ? 0.5f + 0.5f * std::cos (kPi * (y - 0.85f) / 0.15f) : 1.0f;
                    v = att * dec * tail;
                    break;
                }
                default: break;
            }
            winTab[w][i] = std::max (0.0f, v);
        }
    winTab[0][0] = winTab[0][1024] = 0;
    // sensible defaults until the processor writes values
    const auto* t = paramTable();
    for (int i = 0; i < numParams; ++i) values[i] = t[i].def;
}

void Engine::prepare (double s, int)
{
    sr = s;
    int64_t m = 1; while (m < (int64_t) ((30.0 + 8.0) * sr)) m <<= 1;
    memSize = m; memMask = m - 1;
    for (auto& c : mem) c.assign ((size_t) m, 0.0f);
    ov.assign ((size_t) (m / ovChunk), 0.0f); ovMask = (int64_t) ov.size() - 1;
    listener.prepare (sr, m);
    songDecim = std::max (1, (int) std::lround (sr / 12000.0));
    songSr = sr / songDecim;
    songCap = (int64_t) (8.5 * 60.0 * songSr);
    song.assign ((size_t) songCap, 0);
    songLpA = std::exp (-kTwoPi * (float) std::min (5000.0, songSr * 0.42) / (float) sr);
    reverb.prepare (sr); smog.prepare (sr);
    for (auto* v : { &cl, &cr, &fbL, &fbR, &dryL, &dryR, &monoIn }) v->assign (maxChunk, 0.0f);
    frames.assign (96 * 32, HandFrame { 0.3f, 0.5f, 0, 0 });
    reset();
}

void Engine::reset()
{
    for (auto& c : mem) std::fill (c.begin(), c.end(), 0.0f);
    std::fill (ov.begin(), ov.end(), 0.0f);
    std::fill (song.begin(), song.end(), (int16_t) 0);
    for (auto& b : barStart) b.store (-1);
    writeAbs = 0; recordedCount = 0; recGain = 1; songPos = 0; decimCount = 0; songLp1 = songLp2 = 0;
    lastBar = -1; barEnergy = 0; barSamples = 0; barMean = -1; ghostAcc = 0;
    for (auto& g : grains) g = Grain {};
    listener.reset();
    reverb.reset(); smog.reset();
    for (int c = 0; c < 2; ++c) { svf[c].reset(); tilt[c].reset(); fbLp[c].reset(); }
    std::fill (fbL.begin(), fbL.end(), 0.0f); std::fill (fbR.begin(), fbR.end(), 0.0f);
    weather.reset (1); gravity.reset();
    lifePitchN.seed (11); lifeSizeN.seed (12); lifeDensN.seed (13); rng.setSeed (99);
    duckEnv = 0; cloudNorm = 1; activeAvg = 1;
    loopState = 0; intBeat = 0; lastFrame = -1;
    for (auto* r : { &mixR, &outRamp, &cutR, &duckR, &driveR }) r->init = false;
}

// ----------------------------------------------------------------------------- UI hooks
void Engine::addStroke (const Stroke& s)
{
    const juce::SpinLock::ScopedLockType sl (strokeLock);
    if (numPendingStrokes < 4) pendingStrokes[numPendingStrokes++] = s;
}

void Engine::burst (float x, float y, float s)
{
    burstX = x; burstY = y; burstS = s;
    burstSeq.fetch_add (1);
}

void Engine::toggleBookmark (int bar)
{
    const juce::SpinLock::ScopedLockType sl (bookmarkLock);
    for (int i = 0; i < maxBookmarks; ++i)
        if (bookmarks[i] == bar) { bookmarks[i] = -1; uiBookmarks[i] = -1; return; }
    int slot = -1;
    for (int i = 0; i < maxBookmarks && slot < 0; ++i) if (bookmarks[i] < 0) slot = i;
    for (int i = 0; i < maxBookmarks && slot < 0; ++i) if (bookmarkAuto[i]) slot = i;
    if (slot < 0) slot = 0;
    bookmarks[slot] = bar; bookmarkAuto[slot] = false;
    uiBookmarks[slot] = bar; uiBookmarkAuto[slot] = false;
}

void Engine::setBookmarks (const std::vector<int>& v)
{
    const juce::SpinLock::ScopedLockType sl (bookmarkLock);
    for (int i = 0; i < maxBookmarks; ++i)
    {
        bookmarks[i] = i < (int) v.size() ? v[(size_t) i] : -1;
        bookmarkAuto[i] = false;
        uiBookmarks[i] = bookmarks[i]; uiBookmarkAuto[i] = false;
    }
}

std::vector<int> Engine::getBookmarks() const
{
    std::vector<int> v;
    for (int i = 0; i < maxBookmarks; ++i) if (uiBookmarks[i].load() >= 0) v.push_back (uiBookmarks[i].load());
    return v;
}

// ----------------------------------------------------------------------------- helpers
inline float Engine::window (int type, float x) const
{
    const float p = clamp01 (x) * 1024.0f;
    const int i = std::min (1023, (int) p);
    const float f = p - (float) i;
    const float* w = winTab[type];
    return w[i] + (w[i + 1] - w[i]) * f;
}

inline int64_t floorToInt (double x) { const int64_t i = (int64_t) x; return (double) i > x ? i - 1 : i; }
inline float roundFast (float x) { return (float) (int) (x + (x >= 0 ? 0.5f : -0.5f)); }

inline float Engine::memRead (int ch, double pos) const
{
    const int64_t i = floorToInt (pos);
    const float f = (float) (pos - (double) i);
    const float* m = mem[ch].data();
    return hermite (m[(i - 1) & memMask], m[i & memMask], m[(i + 1) & memMask], m[(i + 2) & memMask], f);
}

inline void Engine::memRead2 (double pos, float& l, float& r) const
{
    const int64_t i = floorToInt (pos);
    const float f = (float) (pos - (double) i);
    const int64_t a = (i - 1) & memMask, b = i & memMask, c = (i + 1) & memMask, d = (i + 2) & memMask;
    const float* m0 = mem[0].data(); const float* m1 = mem[1].data();
    l = hermite (m0[a], m0[b], m0[c], m0[d], f);
    r = hermite (m1[a], m1[b], m1[c], m1[d], f);
}

inline float Engine::songRead (double pos) const
{
    int64_t i = floorToInt (pos);
    const float f = (float) (pos - (double) i);
    i = ((i % songCap) + songCap) % songCap;
    const int64_t j = (i + 1) % songCap;
    const float a = song[(size_t) i] * (1.0f / 32767.0f), b = song[(size_t) j] * (1.0f / 32767.0f);
    return a + (b - a) * f;
}

float Engine::keyMaskSnap (float note) const
{
    // nearest note (in semitones) whose pitch class is in the effective scale
    const float base = std::floor (note);
    float best = note; float bestD = 1e9f;
    for (int k = -6; k <= 7; ++k)
    {
        const int n = (int) base + k;
        const int pc = (((n - effRoot) % 12) + 12) % 12;
        if (! (effMask & (1 << pc))) continue;
        const float d = std::fabs ((float) n - note);
        if (d < bestD) { bestD = d; best = (float) n; }
    }
    return best;
}

float Engine::snapPitch (float t, float f0) const
{
    const float amt = clamp01 (values[snap]);
    if (amt <= 0.001f || effScale == scFree) return t;
    float target = t;
    if (effScale == scOctaves) target = std::round (t / 12.0f) * 12.0f;
    else if (effScale == scFifths)
    {
        const float o = std::floor (t / 12.0f) * 12.0f;
        float best = t, bd = 1e9f;
        for (float c : { o - 5.0f, o, o + 7.0f, o + 12.0f, o + 19.0f })
            if (std::fabs (c - t) < bd) { bd = std::fabs (c - t); best = c; }
        target = best;
    }
    else if (f0 > 30.0f)
    {
        const float note = 69.0f + 12.0f * std::log2 (f0 / 440.0f);
        target = t + (keyMaskSnap (note + t) - (note + t));
    }
    else
    {
        // no pitch known: move by consonant intervals only (unison, fourth, fifth, octaves)
        const float o = std::floor (t / 12.0f) * 12.0f;
        float best = t, bd = 1e9f;
        for (float c : { o, o + 5.0f, o + 7.0f, o + 12.0f })
            if (std::fabs (c - t) < bd) { bd = std::fabs (c - t); best = c; }
        target = best;
    }
    return lerp (t, target, amt);
}

// ----------------------------------------------------------------------------- grains
void Engine::startGrain (const GrainEvent& ev, float memSec, float sizeSec, float lifePitch, float transientMix)
{
    Grain* g = nullptr;
    for (auto& q : grains) if (! q.on) { g = &q; break; }
    if (! g) { ++grainsDropped; return; }

    const double memSamples = std::min ((double) recordedCount, (double) memSec * sr);
    if (memSamples < 0.06 * sr) return;
    const int64_t head = writeAbs;
    int len = (int) clampf (sizeSec * (0.5f + ev.z) * (float) sr, 0.006f * (float) sr, 2.0f * (float) sr);

    // where in the memory: x = 0 now .. 1 oldest
    double back = (double) clamp01 (ev.x) * memSamples;
    double centre = (double) head - back;

    // listening: look for a matching moment near the target
    int mode = (int) values[listen];
    bool forced = false;
    if (transientMix > 0 && mode != 2 && rng.uni() < transientMix) { mode = 2; forced = true; }
    bool alignStart = false;
    if (mode != 0)
    {
        const int hop = Listener::hop;
        const int range = (int) (0.4 * sr / hop);
        bool found = false;
        for (int k = 0; k <= range && ! found; ++k)
            for (int sgn = -1; sgn <= 1 && ! found; sgn += 2)
            {
                if (k == 0 && sgn > 0) continue;
                const double c = centre + (double) (sgn * k * hop);
                if (c > (double) head - 0.02 * sr || c < (double) head - memSamples + 0.02 * sr) continue;
                const auto& h = listener.at ((int64_t) c);
                bool ok = false;
                if (mode == 1) ok = h.conf > 0.72f && h.flat < 0.35f && h.rms > 0.004f;
                else if (mode == 2) ok = h.onset > 2.2f && h.rms > 0.004f;
                else if (mode == 3) ok = h.flat > 0.32f && h.conf < 0.65f && h.rms > 0.002f;
                if (ok) { centre = (double) (((int64_t) c / hop) * hop); found = true; }
            }
        if (! found && ! forced) return;   // nothing fitting: the fog stays thinner
        alignStart = found && mode == 2;
        back = (double) head - centre;
    }

    const auto& feat = listener.at ((int64_t) centre);
    const float f0 = (feat.conf > 0.6f) ? feat.f0 : 0.0f;

    // ageing: older memories wear faster with Nostalgia
    const float nost = clamp01 (values[nostalgia]);
    const float backSec = (float) (back / sr);
    float wear = clamp01 (backSec / (1.5f + 28.0f * (1.0f - nost) * (1.0f - nost))) * std::min (1.0f, nost * 2.2f);
    wear = std::pow (wear, 0.8f);

    // pitch
    float t;
    if (! std::isnan (ev.pitchOverride)) t = ev.pitchOverride;
    else t = snapPitch (values[pitch] + ev.y * values[pspread] + lifePitch, f0);
    g->rs = hash32 ((uint32_t) (head + ev.particle * 7919));
    Rng r ((uint32_t) g->rs);
    const float detuneCents = wear * 28.0f * r.bi();
    double rate = std::exp2 ((t + detuneCents * 0.01f) / 12.0);
    const float gl = values[glide];
    g->rateMul = std::exp2 ((double) gl / 12.0 / std::max (1, len));
    g->rev = r.uni() < clamp01 (values[reverse] + forecastStop (values[forecast]).rev);

    g->psola = values[fsafe] > 0.5f && f0 > 50.0f;
    g->fr = (float) std::exp2 (values[formant] / 12.0);
    if (g->psola)
    {
        g->period = (float) (sr / f0);
        g->nextMicro = 0;
        g->numMicro = 0;
    }
    else g->fr = 1;

    // place the read span safely behind the record head
    const double avgRate = rate * std::exp2 ((double) gl / 24.0);
    double span = g->psola ? (double) len + 2.0 * g->period + 4 : (double) len * avgRate + 4;
    const double maxSpan = memSamples - 0.01 * sr;
    if (span > maxSpan) { const double k = maxSpan / span; len = std::max (64, (int) (len * k)); span = maxSpan; }
    double start = alignStart ? centre - 0.002 * sr : centre - span * 0.5;
    const double newest = (double) head - 8.0 - span - (g->psola ? g->period : 0.0);
    const double oldest = (double) head - memSamples + 8.0 + (g->psola ? g->period : 0.0);
    start = std::max (oldest, std::min (newest, start));
    g->src = g->rev ? start + span - 4 : start;
    g->acc = 0;
    g->rate = rate;
    g->len = len; g->t = 0;
    g->delay = ev.offset;
    g->win = (int) values[ml::window];
    g->ghost = false;

    float pan = clampf (ev.pan * values[width], -1.0f, 1.0f);
    const float a = (pan + 1.0f) * kPi * 0.25f;
    g->gl = std::cos (a) * 1.41421356f; g->gr = std::sin (a) * 1.41421356f;
    g->gain = 1.0f;

    // wear details
    g->wear = wear;
    const float cutoff = clampf (19000.0f * std::pow (0.06f, wear) * ev.bright, 250.0f, 20000.0f);
    g->lpA = cutoff >= 17000.0f ? 0.0f : std::exp (-kTwoPi * cutoff / (float) sr);
    g->lpL = g->lpR = 0;
    g->wowPh = r.uni(); g->wowInc = (0.4f + 1.4f * r.uni()) / (float) sr;
    g->flutPh = r.uni();
    g->wowDepth = wear * 0.006f;
    g->bitStep = wear > 0.45f ? std::exp2 (-(15.0f - (wear - 0.45f) / 0.55f * 10.0f)) : 0.0f;
    g->crackleProb = wear * wear * 18.0f / (float) sr;
    g->crackle = 0; g->srcEnv = 0;
    if (r.uni() < wear * 0.3f) { g->dropFrom = r.uni() * 0.7f; g->dropTo = g->dropFrom + 0.1f + 0.25f * r.uni(); }
    else g->dropFrom = g->dropTo = 2.0f;
    g->dropG = 1;
    g->on = true;
}

void Engine::startGhost (const HostInfo& host, float)
{
    Grain* g = nullptr;
    for (auto& q : grains) if (! q.on) { g = &q; break; }
    if (! g) return;
    const double barSec = host.barBeats * 60.0 / std::max (20.0, host.bpm);
    const double barSong = barSec * songSr;
    const float age = clamp01 (values[ghostAge]);
    const double minAge = (4.0 + 28.0 * age) * barSong;
    int cand[maxBookmarks]; int nc = 0;
    for (int i = 0; i < maxBookmarks; ++i)
    {
        const int b = bookmarks[i];
        if (b < 0 || b >= maxBars) continue;
        const int64_t s = barStart[(size_t) b].load (std::memory_order_relaxed);
        if (s < 0) continue;
        const double old = (double) (songPos - s);
        if (old >= minAge && old < (double) songCap - 2.0 * barSong) cand[nc++] = b;
    }
    if (nc == 0) return;
    const int b = cand[(int) (rng.uni() * nc) % nc];
    // same position inside the bar as now: ghosts come back in time with the song
    const double phase = host.ppq / host.barBeats - std::floor (host.ppq / host.barBeats);
    const double pos = (double) barStart[(size_t) b].load() + (phase + 0.06 * rng.bi()) * barSong;
    const float lenSec = 0.25f + 0.45f * rng.uni();
    const float roll = rng.uni();
    const float t = roll < 0.6f ? 0.0f : (roll < 0.8f ? 12.0f : (roll < 0.9f ? -12.0f : 7.0f));
    g->ghost = true; g->psola = false; g->rev = rng.uni() < 0.15f;
    g->len = (int) (lenSec * sr); g->t = 0; g->delay = 0; g->acc = 0;
    g->rate = std::exp2 (t / 12.0) * songSr / sr;
    g->rateMul = 1;
    g->src = g->rev ? pos + g->len * g->rate : pos;
    g->win = 1;
    const float pan = 0.8f * rng.bi();
    const float a = (pan + 1.0f) * kPi * 0.25f;
    g->gl = std::cos (a) * 1.414f; g->gr = std::sin (a) * 1.414f;
    g->gain = clamp01 (values[ghosts]) * 1.1f;
    const float wear = 0.5f + 0.5f * age;
    g->wear = wear;
    g->lpA = std::exp (-kTwoPi * (4500.0f * std::pow (0.3f, wear)) / (float) sr);
    g->lpL = g->lpR = 0;
    g->wowPh = rng.uni(); g->wowInc = 0.7f / (float) sr; g->flutPh = 0; g->wowDepth = wear * 0.008f;
    g->bitStep = std::exp2 (-11.0f + 3.0f * wear);
    g->crackleProb = wear * 30.0f / (float) sr; g->crackle = 0; g->srcEnv = 0;
    g->dropFrom = g->dropTo = 2.0f; g->dropG = 1;
    g->rs = rng.next();
    g->on = true;
    ghostFlash.fetch_add (1);
}

void Engine::renderGrains (float* outL, float* outR, int n)
{
    int active = 0;
    for (auto& g : grains)
    {
        if (! g.on) continue;
        ++active;
        Rng r (g.rs);
        const double dir = g.rev ? -1.0 : 1.0;
        for (int i = 0; i < n; ++i)
        {
            if (g.delay > 0) { --g.delay; continue; }
            const float x = (float) g.t / (float) g.len;
            const float env = window (g.win, x);
            // wow / flutter of old memories
            g.wowPh += g.wowInc; if (g.wowPh >= 1) g.wowPh -= 1;
            g.flutPh += 9.0f / (float) sr; if (g.flutPh >= 1) g.flutPh -= 1;
            const double wob = g.wowDepth > 0 ? 1.0 + g.wowDepth * sin01 (g.wowPh) + g.wowDepth * 0.25f * sin01 (g.flutPh) : 1.0;
            float sl = 0, sr_ = 0;
            if (g.ghost)
            {
                const float v = songRead (g.src + dir * g.acc);
                sl = sr_ = v;
                g.acc += g.rate * wob;
            }
            else if (g.psola)
            {
                const float outPeriod = g.period / (float) (g.rate * wob);
                if ((float) g.t >= g.nextMicro)
                {
                    g.nextMicro += std::max (8.0f, outPeriod);
                    if (g.numMicro < 8)
                    {
                        auto& m = g.micro[g.numMicro++];
                        const double q = (double) floorToInt ((double) g.t / g.period + 0.5) * g.period;
                        m.on = true; m.t = 0; m.len = 2.0f * g.period / g.fr;
                        m.pos = g.src + dir * q - dir * g.period;
                        if (g.fr == 1.0f) m.pos = (double) floorToInt (m.pos + 0.5);
                    }
                }
                const double step = dir * g.fr;
                const bool unity = g.fr == 1.0f;
                const float* m0 = mem[0].data(); const float* m1 = mem[1].data();
                for (int k = 0; k < g.numMicro;)
                {
                    auto& m = g.micro[k];
                    const float w = window (1, m.t / m.len);
                    if (unity)
                    {
                        // formant unchanged: micro-grains read whole samples (pitch marks rounded)
                        const int64_t ix = (int64_t) m.pos + (int64_t) dir * (int64_t) m.t;
                        sl += w * m0[ix & memMask]; sr_ += w * m1[ix & memMask];
                    }
                    else
                    {
                        float a, b; memRead2 (m.pos + step * (double) m.t, a, b);
                        sl += w * a; sr_ += w * b;
                    }
                    m.t += 1.0f;
                    if (m.t >= m.len) { m = g.micro[--g.numMicro]; continue; }
                    ++k;
                }
                const float k = clampf (g.fr / (float) (g.rate * wob), 0.3f, 2.5f);
                sl *= k; sr_ *= k;
                g.rate *= g.rateMul;
            }
            else
            {
                memRead2 (g.src + dir * g.acc, sl, sr_);
                g.acc += g.rate * wob;
                g.rate *= g.rateMul;
            }
            // wear: dusty low pass, bit rot, crackle, dropouts
            if (g.lpA > 0) { g.lpL = sl + g.lpA * (g.lpL - sl); g.lpR = sr_ + g.lpA * (g.lpR - sr_); sl = g.lpL; sr_ = g.lpR; }
            if (g.bitStep > 0) { const float ib = 1.0f / g.bitStep; sl = roundFast (sl * ib) * g.bitStep; sr_ = roundFast (sr_ * ib) * g.bitStep; }
            if (g.crackleProb > 0)
            {
                if (r.uni() < g.crackleProb) g.crackle = (r.bi() > 0 ? 1.0f : -1.0f) * (0.05f + 0.15f * g.wear);
                g.crackle *= 0.75f;
                // dust only shows on top of something: silent memory stays silent
                g.srcEnv = std::max (std::fabs (sl), g.srcEnv * 0.9995f);
                const float ck = g.crackle * std::min (1.0f, g.srcEnv * 30.0f);
                sl += ck; sr_ += ck * 0.9f;
            }
            const float dTarget = (x >= g.dropFrom && x < g.dropTo) ? 0.12f : 1.0f;
            g.dropG += (dTarget - g.dropG) * 0.004f;
            const float a = env * g.gain * g.dropG;
            outL[i] += sl * a * g.gl;
            outR[i] += sr_ * a * g.gr;
            if (++g.t >= g.len) { g.on = false; break; }
        }
        g.rs = r.s;
    }
    activeGrains.store (active, std::memory_order_relaxed);
    activeAvg += ((float) active - activeAvg) * 0.05f;
}

// ----------------------------------------------------------------------------- looper
void Engine::runLooper (const HostInfo& host, int n)
{
    static const int barsFor[] = { 1, 2, 4, 8 };
    const double loopBeats = barsFor[std::max (0, std::min (3, (int) values[loopLen]))] * host.barBeats;
    const double bpm = std::max (20.0, host.bpm);
    intBeat += n / sr * bpm / 60.0;
    const double beat = (host.playing && host.ppqValid) ? host.ppq : intBeat;
    const int framesUsed = std::max (1, std::min ((int) frames.size(), (int) (loopBeats * 96.0)));
    const double ph = std::fmod (std::fmod (beat, loopBeats) + loopBeats, loopBeats) / loopBeats;
    const int idx = std::min (framesUsed - 1, (int) (ph * framesUsed));

    const HandFrame live { values[handX], values[handY], values[handPull], values[handGust] };
    const bool liveDown = std::fabs (live.pull) > 0.001f || live.gust > 0.001f;

    const int cmd = looperCmd.exchange (0);
    if (cmd == 1) { loopState = 1; recStartBeat = beat; lastFrame = -1; }
    if (cmd == 2) loopState = 0;

    HandFrame out = live;
    if (loopState == 1)
    {
        // record: fill every frame passed since the last block
        if (lastFrame < 0) frames[(size_t) idx] = live;
        else for (int f = lastFrame; f != idx; f = (f + 1) % framesUsed) frames[(size_t) ((f + 1) % framesUsed)] = live;
        if (beat - recStartBeat >= loopBeats - 1e-6 || beat < recStartBeat) loopState = 2;
    }
    else if (loopState == 2)
    {
        if (liveDown)
        {
            if (lastFrame >= 0) for (int f = lastFrame; f != idx; f = (f + 1) % framesUsed) frames[(size_t) ((f + 1) % framesUsed)] = live;
            frames[(size_t) idx] = live;
        }
        else out = frames[(size_t) idx];
    }
    lastFrame = idx;
    looperState.store (loopState);
    looperPhase.store ((float) ph);
    effHandX = out.x; effHandY = out.y; effHandPull = out.pull; effHandGust = out.gust;
}

// ----------------------------------------------------------------------------- song memory
void Engine::runSongMemory (const float* mono, int n, const HostInfo& host)
{
    if (! (host.playing && host.ppqValid)) { lastBar = -1; curBar = -1; return; }
    const double samplesPerBeat = sr * 60.0 / std::max (20.0, host.bpm);
    for (int i = 0; i < n; ++i)
    {
        const double ppq = host.ppq + i / samplesPerBeat;
        const int bar = (int) std::floor (ppq / host.barBeats + 1e-9);
        if (bar != lastBar)
        {
            // finish the previous bar: auto bookmarks on loud moments (chorus / drop)
            if (lastBar >= 0 && bar == lastBar + 1 && barSamples > 0)
            {
                const double e = barEnergy / barSamples;
                if (barMean < 0) barMean = e;
                else
                {
                    if (values[ghostAuto] > 0.5f && e > 1e-4 && e > 2.0 * barMean)
                    {
                        const juce::SpinLock::ScopedTryLockType sl (bookmarkLock);
                        if (sl.isLocked())
                        {
                            bool near = false;
                            for (int k = 0; k < maxBookmarks; ++k) if (bookmarks[k] >= 0 && std::abs (bookmarks[k] - lastBar) < 8) near = true;
                            if (! near)
                            {
                                int slot = -1;
                                for (int k = 0; k < maxBookmarks && slot < 0; ++k) if (bookmarks[k] < 0) slot = k;
                                for (int k = 0; k < maxBookmarks && slot < 0; ++k) if (bookmarkAuto[k]) slot = k;
                                if (slot >= 0)
                                {
                                    bookmarks[slot] = lastBar; bookmarkAuto[slot] = true;
                                    uiBookmarks[slot] = lastBar; uiBookmarkAuto[slot] = true;
                                }
                            }
                        }
                    }
                    barMean += (e - barMean) * 0.15;
                }
            }
            barEnergy = 0; barSamples = 0;
            if (bar >= 0 && bar < maxBars) barStart[(size_t) bar].store (songPos + (decimCount > 0 ? 1 : 0));
            lastBar = bar;
            curBar = bar;
        }
        const float v = mono[i];
        barEnergy += (double) v * v; ++barSamples;
        songLp1 = v + songLpA * (songLp1 - v);
        songLp2 = songLp1 + songLpA * (songLp2 - songLp1);
        if (++decimCount >= songDecim)
        {
            decimCount = 0;
            song[(size_t) (songPos % songCap)] = (int16_t) std::lround (clampf (songLp2, -1.0f, 1.0f) * 32767.0f);
            ++songPos;
        }
    }
}

// ----------------------------------------------------------------------------- main
void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const juce::MidiBuffer& midi, const HostInfo& host)
{
    HostInfo h = host;
    int done = 0;
    while (done < n)
    {
        const int c = std::min (maxChunk, n - done);
        processChunk (inL + done, inR + done, outL + done, outR + done, c, midi, done, h);
        const double beats = c / sr * std::max (20.0, h.bpm) / 60.0;
        h.ppq += beats;
        done += c;
    }
}

void Engine::processChunk (const float* inL, const float* inR, float* outL, float* outR, int n,
                           const juce::MidiBuffer& midi, int midiStart, const HostInfo& host)
{
    const float fsr = (float) sr;
    for (int i = 0; i < n; ++i)
    {
        dryL[(size_t) i] = inL[i]; dryR[(size_t) i] = inR[i];
        monoIn[(size_t) i] = 0.5f * (inL[i] + inR[i]);
    }

    // ---- transport start: everything random restarts the same way
    if (host.playing && ! wasPlaying)
    {
        weather.reset (1); gravity.reset();
        rng.setSeed (99); smog.reseed (7);
        lifePitchN.seed (11); lifeSizeN.seed (12); lifeDensN.seed (13);
        intBeat = host.ppq;
    }
    wasPlaying = host.playing;

    const Stop fc = forecastStop (values[forecast]);
    const float transientMix = clamp01 ((values[forecast] - 0.8f) * 5.0f);

    // ---- MIDI
    const int midiMode_ = (int) values[midiMode];
    for (const auto meta : midi)
    {
        if (meta.samplePosition < midiStart || meta.samplePosition >= midiStart + n) continue;
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            heldNotes[m.getNoteNumber()] = 1;
            if (midiMode_ == 2) weather.spawnNote ((float) (m.getNoteNumber() - 60) + values[pitch], 2 + (int) (m.getFloatVelocity() * 6.0f));
        }
        else if (m.isNoteOff()) heldNotes[m.getNoteNumber()] = 0;
        else if (m.isAllNotesOff() || m.isAllSoundOff()) for (auto& v : heldNotes) v = 0;
    }
    heldMask = 0; int lowest = -1;
    for (int k = 0; k < 128; ++k) if (heldNotes[k]) { heldMask |= 1 << (k % 12); if (lowest < 0) lowest = k; }

    // ---- effective key + scale
    const int keyParam = (int) values[key];
    const int scaleParam = (int) values[scale];
    const bool midiKey = midiMode_ == 1 && heldMask != 0;
    if (midiKey)
    {
        effRoot = lowest % 12;
        int m = 0; for (int k = 0; k < 12; ++k) if (heldMask & (1 << ((k + effRoot) % 12))) m |= 1 << k;
        effMask = m;
        effScale = scaleParam == scOctaves || scaleParam == scFifths || scaleParam == scFree ? scaleParam : scMinor;
        if (keyParam > 0) effRoot = keyParam - 1;
    }
    else
    {
        effRoot = keyParam > 0 ? keyParam - 1 : listener.keyRoot.load();
        const bool minor = listener.keyMinor.load();
        effScale = scaleParam == scAuto ? (minor ? scMinor : scMajor) : scaleParam;
        effMask = scaleMaskFor (effScale, minor);
    }
    uiRoot = effRoot; uiScale = effScale; uiKeyFromMidi = midiKey;

    // ---- memory length
    static const int memBarsFor[] = { 1, 2, 4, 8, 16 };
    float memSec = values[memLen];
    if (values[memSync] > 0.5f)
        memSec = clampf ((float) (memBarsFor[std::max (0, std::min (4, (int) values[memBars]))] * host.barBeats * 60.0 / std::max (20.0, host.bpm)), 0.5f, 30.0f);
    uiMemSeconds = memSec;

    // ---- life
    const float lifeAmt = clamp01 (values[life]);
    const float dtc = (float) n / fsr;
    const float lifeP = lifePitchN.next (dtc * 0.07f) * lifeAmt * 0.35f;
    const float lifeS = 1.0f + lifeSizeN.next (dtc * 0.05f) * lifeAmt * 0.35f;
    const float lifeD = lifeDensN.next (dtc * 0.04f) * lifeAmt * 0.2f;
    srcLife = lifeDensN.next (0.0f);

    // ---- hand + gesture looper
    runLooper (host, n);

    // ---- gravity bodies (springs get kicked by input transients)
    for (int b = 0; b < numBodies; ++b)
        gravity.setParams (b, (int) values[g1Type + b * bodyStride], values[g1Speed + b * bodyStride], values[g1Energy + b * bodyStride]);
    const int onsets = listener.onsetsSinceLastCall();
    if (onsets > 0)
        for (auto& b : gravity.bodies)
            if (b.type == bodySpring || b.type == bodyFirefly)
                b.kick (clampf (listener.lastOnset * 0.12f, 0.2f, 1.0f) * (0.4f + 0.6f * b.energy), (uint32_t) (writeAbs & 0xffffff));
    gravity.process (n, sr);

    // ---- weather inputs from the UI
    for (int e = 0; e < 4; ++e) weather.setEmitter (e, emOn[e].load(), emX[e].load(), emY[e].load());
    {
        const juce::SpinLock::ScopedTryLockType sl (strokeLock);
        if (sl.isLocked())
        {
            for (int k = 0; k < numPendingStrokes; ++k) weather.addStroke (pendingStrokes[k]);
            numPendingStrokes = 0;
        }
    }
    const int bs = burstSeq.load();
    if (bs != lastBurstSeq) { lastBurstSeq = bs; weather.burst (burstX.load(), burstY.load(), burstS.load()); }

    const float sizeSec = values[gsize] * 0.001f * fc.size * lifeS;
    WeatherParams wp;
    wp.humidity = clampf ((values[humidity] + lifeD) * fc.dens, 0.0f, 1.6f);
    wp.sizeSec = sizeSec;
    wp.reach = values[reach];
    wp.wind = clamp01 (values[wind] + fc.wind);
    wp.windDir = values[windDir];
    wp.turb = clamp01 (values[turb] + fc.turb);
    wp.gravity = clamp01 (values[ml::gravity] + fc.grav);
    wp.temp = values[temp];
    wp.bodyForce = values[bodyForce];
    wp.handX = effHandX.load(); wp.handY = effHandY.load() * 2.0f - 1.0f;
    wp.handPull = effHandPull.load(); wp.handGust = effHandGust.load();
    wp.lifeAmt = lifeAmt;
    wp.overlap = 1.0f;
    const int numEv = weather.process (n, sr, wp, gravity);

    // ---- record into memory (freeze = 50 ms crossfade, head stops)
    const bool frz = values[freeze] > 0.5f || holdFreeze.load();
    const float recStep = 1.0f / (0.05f * fsr);
    const bool recordingAtStart = recGain > 0.5f;
    const int64_t writeAtStart = writeAbs;
    float inPk = 0;
    for (int i = 0; i < n; ++i)
    {
        recGain = frz ? std::max (0.0f, recGain - recStep) : std::min (1.0f, recGain + recStep);
        inPk = std::max (inPk, std::max (std::fabs (inL[i]), std::fabs (inR[i])));
        if (recGain <= 0.0f) continue;
        const int64_t w = writeAbs & memMask;
        const float vl = dryL[(size_t) i] + fbL[(size_t) i], vr = dryR[(size_t) i] + fbR[(size_t) i];
        if (recGain >= 1.0f) { mem[0][(size_t) w] = vl; mem[1][(size_t) w] = vr; }
        else
        {
            mem[0][(size_t) w] = lerp (mem[0][(size_t) w], vl, recGain);
            mem[1][(size_t) w] = lerp (mem[1][(size_t) w], vr, recGain);
        }
        const int64_t ci = (writeAbs / ovChunk) & ovMask;
        if (writeAbs % ovChunk == 0) ov[(size_t) ci] = 0;
        ov[(size_t) ci] = std::max (ov[(size_t) ci], std::max (std::fabs (vl), std::fabs (vr)));
        ++writeAbs;
        if (recordedCount < memSize - 16) ++recordedCount;
    }
    freezeAmt = 1.0f - recGain;
    uiWrite = writeAbs;
    inLevel = inPk;
    listener.push (monoIn.data(), n, writeAtStart, recordingAtStart);
    runSongMemory (monoIn.data(), n, host);

    // ---- spawn grains
    const float memSecUsed = std::min (memSec, (float) ((memSize - 0.5 * sr) / sr));
    for (int e = 0; e < numEv; ++e) startGrain (weather.events[e], memSecUsed, sizeSec, lifeP, transientMix);
    if (values[ghosts] > 0.001f && host.playing && host.ppqValid)
    {
        ghostAcc += dtc * (0.3f + 5.0f * values[ghostRate]) * values[ghosts];
        while (ghostAcc >= 1.0f) { ghostAcc -= 1.0f; startGhost (host, sizeSec); }
    }

    // ---- render the cloud
    float* L = cl.data(); float* R = cr.data();
    std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
    renderGrains (L, R, n);
    const float normTarget = 1.3f / std::sqrt (std::max (1.0f, activeAvg));
    for (int i = 0; i < n; ++i)
    {
        cloudNorm += (normTarget - cloudNorm) * 0.002f;
        L[i] *= cloudNorm; R[i] *= cloudNorm;
    }
    lastCloudGain = cloudNorm;

    // ---- character: tone tilt -> filter -> drive -> Smog -> shimmer space -> width
    const float tl = clampf (values[tone] + fc.tone, -1.0f, 1.0f);
    const float gLo = std::pow (10.0f, -tl * 9.0f / 20.0f), gHi = std::pow (10.0f, tl * 9.0f / 20.0f);
    tilt[0].setCutoff (650.0f, fsr); tilt[1].setCutoff (650.0f, fsr);
    const int ft = (int) values[fltType];
    cutR.set (values[fltCut], n);
    const float res = values[fltRes];
    driveR.set (values[drive], n);
    const int dType = (int) values[driveType];
    for (int i = 0; i < n; ++i)
    {
        float s[2] = { L[i], R[i] };
        const float cut = cutR.next();
        const float dr = driveR.next();
        if (ft > 0 && (i & 7) == 0) { svf[0].set (cut, res, fsr); svf[1].set (cut, res, fsr); }
        for (int c = 0; c < 2; ++c)
        {
            float x = s[c];
            const float lo = tilt[c].lp (x);
            x = lo * gLo + (x - lo) * gHi;
            if (ft > 0)
            {
                float lp, bp, hp; svf[c].tick (x, lp, bp, hp);
                x = ft == 1 ? lp : (ft == 2 ? bp * (1.0f + res) : hp);
            }
            if (dr > 0.0005f)
            {
                const float gdr = 1.0f + dr * 9.0f;
                const float y = dType == 0 ? fastTanh (x * gdr)
                                           : fastTanh (x * gdr + 0.35f * dr) - fastTanh (0.35f * dr);
                x = lerp (x, y / std::sqrt (gdr) * 1.3f, std::min (1.0f, dr * 4.0f));
            }
            s[c] = x;
        }
        L[i] = s[0]; R[i] = s[1];
    }
    const float smogAmt = clamp01 (values[ml::smog] + fc.smog);
    // hiss/crackle only while the cloud is sounding
    float pre = 0; for (int i = 0; i < n; ++i) pre = std::max (pre, std::max (std::fabs (L[i]), std::fabs (R[i])));
    cloudActivity = std::max (pre, cloudActivity * std::exp (-dtc / 0.4f));
    smog.process (L, R, n, smogAmt, 1.0f, 0.45f, 0.7f * std::min (1.0f, cloudActivity * 40.0f), 0.5f);
    reverb.process (L, R, n, values[revSize], values[revDecay], -0.15f, clamp01 (values[shimmer] + fc.shim), 0.5f, 18.0f,
                    clamp01 (values[space] + fc.space));
    const float wd = values[width];
    float cPk = 0;
    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]) * std::min (wd, 1.5f);
        L[i] = softClip (m + sd); R[i] = softClip (m - sd);
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) { L[i] = R[i] = 0; }
        cPk = std::max (cPk, std::max (std::fabs (L[i]), std::fabs (R[i])));
    }
    cloudLevel = cPk;

    // ---- feedback (written into the memory with the next block)
    const float fbAmt = std::min (0.92f, values[feedback] + fc.fb);
    fbLp[0].setCutoff (6000.0f, fsr); fbLp[1].setCutoff (6000.0f, fsr);
    for (int i = 0; i < n; ++i)
    {
        fbL[(size_t) i] = fbAmt > 0.0005f ? fastTanh (fbLp[0].lp (L[i])) * fbAmt : 0.0f;
        fbR[(size_t) i] = fbAmt > 0.0005f ? fastTanh (fbLp[1].lp (R[i])) * fbAmt : 0.0f;
    }
    if (n < maxChunk) { std::fill (fbL.begin() + n, fbL.end(), 0.0f); std::fill (fbR.begin() + n, fbR.end(), 0.0f); }

    // ---- ducking, mix, output
    const float duck_ = clamp01 (values[duck]);
    const float atk = std::exp (-1.0f / (0.004f * fsr)), rel = std::exp (-1.0f / (0.25f * fsr));
    mixR.set (clamp01 (values[mix]), n);
    outRamp.set (values[output], n);
    for (int i = 0; i < n; ++i)
    {
        const float a = std::max (std::fabs (dryL[(size_t) i]), std::fabs (dryR[(size_t) i]));
        duckEnv = a > duckEnv ? a + atk * (duckEnv - a) : a + rel * (duckEnv - a);
        const float dg = 1.0f - duck_ * std::min (1.0f, duckEnv * 5.0f);
        const float m = mixR.next();
        const float od = outRamp.next();
        const float og = od == 0.0f ? 1.0f : std::pow (10.0f, od / 20.0f);
        const float dryG = std::cos (m * kPi * 0.5f), wetG = m <= 0.0f ? 0.0f : std::sin (m * kPi * 0.5f);
        outL[i] = (dryL[(size_t) i] * dryG + L[i] * dg * wetG) * og;
        outR[i] = (dryR[(size_t) i] * dryG + R[i] * dg * wetG) * og;
    }
}
} // namespace ml
