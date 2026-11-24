#include "Presets.h"
#include "DspUtil.h"
#include "Gravity.h"
#include <cmath>

namespace ml
{
namespace
{
int dstIndex (int param)
{
    int n = 0; const int* d = dstParams (n);
    for (int i = 1; i < n; ++i) if (d[i] == param) return i;
    return 0;
}
} // namespace

const char* const* defaultMacroNames()
{
    static const char* n[] = { "Fog", "Drift", "Pitch", "Age", "Space", "Glow" };
    return n;
}

void defaultValues (float* v)
{
    const auto* t = paramTable();
    for (int i = 0; i < numParams; ++i) v[i] = t[i].def;
    struct R { int src, dst; float amt; };
    const R routes[] = {
        { sM1, humidity, 0.5f }, { sM2, turb, 0.6f }, { sM2, wind, 0.4f }, { sM3, pspread, 0.45f },
        { sM4, nostalgia, 0.6f }, { sM5, space, 0.55f }, { sM6, shimmer, 0.55f }, { sM6, tone, 0.25f },
    };
    int k = 0;
    for (auto& r : routes)
    {
        v[s1Src + k * slotStride] = (float) r.src;
        v[s1Dst + k * slotStride] = (float) dstIndex (r.dst);
        v[s1Amt + k * slotStride] = r.amt;
        ++k;
    }
}

const std::vector<FactoryPreset>& factoryPresets()
{
    using V = std::vector<std::pair<int, float>>;
    static const std::vector<FactoryPreset> p = {
        { "Marine Layer", "Fog", V { { forecast, 0.5f }, { humidity, 0.45f }, { gsize, 180 }, { reach, 0.6f }, { pspread, 5 }, { reverse, 0.1f },
                                     { nostalgia, 0.4f }, { space, 0.4f }, { shimmer, 0.1f }, { life, 0.45f }, { feedback, 0.12f } }, {} },
        { "June Gloom", "Fog", V { { forecast, 0.55f }, { fltType, 1 }, { fltCut, 2200 }, { fltRes, 0.25f }, { gsize, 400 }, { nostalgia, 0.5f },
                                   { tone, -0.35f }, { space, 0.5f }, { mix, 0.45f }, { humidity, 0.5f } }, {} },
        { "Frozen Pier", "Fog", V { { feedback, 0.6f }, { gsize, 600 }, { humidity, 0.6f }, { turb, 0.15f }, { space, 0.7f }, { revDecay, 12 },
                                    { shimmer, 0.2f }, { life, 0.6f }, { pspread, 3 } }, {} },
        { "Cathedral Smear", "Fog", V { { gsize, 900 }, { humidity, 0.7f }, { space, 0.85f }, { revSize, 1 }, { revDecay, 15 }, { shimmer, 0.3f },
                                        { reverse, 0.2f }, { forecast, 0.45f }, { pspread, 7 } }, {} },
        { "Morning Burn-off", "Fog", V { { forecast, 0.15f }, { humidity, 0.3f }, { gsize, 300 }, { space, 0.3f }, { nostalgia, 0.1f }, { tone, 0.15f } }, {} },
        { "Ghost Choir", "Voices", V { { listen, 1 }, { snap, 1 }, { pitch, 12 }, { pspread, 7 }, { shimmer, 0.5f }, { space, 0.6f }, { gsize, 350 },
                                       { humidity, 0.5f }, { fsafe, 1 }, { forecast, 0.3f }, { mix, 0.55f }, { window, 0 } }, {} },
        { "Seagull Choir", "Voices", V { { listen, 1 }, { snap, 1 }, { pitch, 19 }, { pspread, 5 }, { fsafe, 1 }, { formant, 4 }, { glide, -5 },
                                         { gsize, 250 }, { space, 0.5f } }, {} },
        { "Fog Horn", "Voices", V { { pitch, -12 }, { pspread, 0 }, { scale, scFifths }, { gsize, 800 }, { fsafe, 1 }, { formant, -3 },
                                    { humidity, 0.35f }, { space, 0.6f }, { drive, 0.25f } }, {} },
        { "Siren Bloom", "Voices", V { { glide, 12 }, { gsize, 600 }, { pspread, 7 }, { temp, 0.5f }, { shimmer, 0.4f }, { space, 0.5f }, { snap, 0.6f } }, {} },
        { "Glass Rain", "Rain", V { { gsize, 30 }, { humidity, 0.8f }, { pitch, 12 }, { pspread, 12 }, { gravity, 0.5f }, { tone, 0.5f },
                                    { window, 3 }, { space, 0.3f }, { shimmer, 0.2f }, { forecast, 0.7f } }, {} },
        { "Sea Glass", "Rain", V { { pitch, 12 }, { pspread, 12 }, { gsize, 60 }, { window, 2 }, { tone, 0.4f }, { shimmer, 0.3f }, { humidity, 0.45f } }, {} },
        { "Low Tide", "Rain", V { { listen, 3 }, { gsize, 250 }, { humidity, 0.5f }, { gravity, 0.3f }, { tone, 0.2f }, { space, 0.4f } }, {} },
        { "Concrete Stutter", "Glitch", V { { listen, 2 }, { gsize, 90 }, { window, 3 }, { humidity, 0.6f }, { reverse, 0 }, { memSync, 1 }, { memBars, 1 },
                                            { wind, 0.3f }, { turb, 0.4f }, { drive, 0.4f }, { forecast, 0.85f }, { space, 0.15f } }, {} },
        { "Breakwater", "Glitch", V { { listen, 2 }, { duck, 0.8f }, { gsize, 140 }, { humidity, 0.55f }, { reach, 0.25f }, { drive, 0.3f }, { window, 3 } }, {} },
        { "Broken VHS", "Glitch", V { { nostalgia, 0.9f }, { smog, 0.7f }, { drive, 0.3f }, { driveType, 1 }, { gsize, 200 }, { reverse, 0.3f }, { forecast, 0.6f } }, {} },
        { "Storm Front", "Glitch", V { { forecast, 1.0f }, { humidity, 0.6f }, { drive, 0.3f }, { space, 0.3f } }, {} },
        { "Santa Ana Winds", "Weather", V { { wind, 0.9f }, { windDir, 20 }, { turb, 0.6f }, { gsize, 100 }, { humidity, 0.7f }, { drive, 0.2f }, { forecast, 0.7f } }, {} },
        { "Heat Haze", "Weather", V { { temp, 0.7f }, { turb, 0.5f }, { pspread, 2 }, { gsize, 220 }, { tone, 0.2f }, { forecast, 0.25f }, { glide, 1 } }, {} },
        { "Rip Current", "Weather", V { { wind, 0.7f }, { windDir, 180 }, { reverse, 0.5f }, { gsize, 160 }, { turb, 0.2f }, { feedback, 0.3f } }, {} },
        { "Reverse Tide", "Weather", V { { reverse, 0.9f }, { gsize, 450 }, { wind, 0.5f }, { windDir, 180 }, { humidity, 0.4f }, { space, 0.5f }, { forecast, 0.4f } }, {} },
        { "Night Swim", "Bodies", V { { fltType, 1 }, { fltCut, 1500 }, { fltRes, 0.5f }, { gsize, 300 }, { wind, 0.2f }, { turb, 0.5f }, { life, 0.8f },
                                      { space, 0.55f }, { tone, -0.3f }, { g1Type, bodyFirefly } }, {} },
        { "Lantern Swarm", "Bodies", V { { g1Type, bodyFirefly }, { g2Type, bodyFirefly }, { g3Type, bodyFirefly }, { g1Energy, 0.7f }, { g2Energy, 0.7f },
                                         { g3Energy, 0.7f }, { bodyForce, 0.8f }, { humidity, 0.6f }, { gsize, 120 }, { pspread, 12 }, { snap, 1 }, { shimmer, 0.3f } }, {} },
        { "Pendulum Delay", "Bodies", V { { g1Type, bodyPendulum }, { g1Energy, 0.7f }, { bodyForce, 0.9f }, { humidity, 0.3f }, { gsize, 120 },
                                          { reach, 0.3f }, { window, 3 }, { space, 0.3f } }, {} },
        { "Kelp Forest", "Bodies", V { { g1Type, bodySpring }, { g2Type, bodySpring }, { bodyForce, 0.7f }, { turb, 0.4f }, { gsize, 200 },
                                       { fltType, 2 }, { fltCut, 900 }, { fltRes, 0.6f } }, {} },
        { "Pier Lights", "Bodies", V { { g1Type, bodyOrbit }, { g1Energy, 0.7f }, { g2Type, bodyFirefly }, { humidity, 0.5f }, { pitch, 12 },
                                       { shimmer, 0.35f }, { gsize, 140 } }, {} },
        { "Tape Ghosts", "Memory", V { { ghosts, 0.7f }, { ghostAge, 0.4f }, { ghostRate, 0.4f }, { nostalgia, 0.7f }, { smog, 0.3f }, { humidity, 0.35f } }, {} },
        { "Lost Postcards", "Memory", V { { ghosts, 0.9f }, { ghostAge, 0.9f }, { nostalgia, 0.8f }, { smog, 0.4f }, { reach, 1.0f }, { humidity, 0.3f } }, {} },
        { "Dust Memory", "Memory", V { { nostalgia, 0.95f }, { reach, 1.0f }, { gsize, 250 }, { humidity, 0.45f }, { smog, 0.35f }, { tone, -0.3f } }, {} },
        { "Underpass Drone", "Drones", V { { pitch, -12 }, { pspread, 0 }, { scale, scOctaves }, { feedback, 0.75f }, { gsize, 700 }, { humidity, 0.55f },
                                           { fltType, 1 }, { fltCut, 900 }, { fltRes, 0.4f }, { space, 0.5f }, { drive, 0.3f }, { forecast, 0.55f } }, {} },
        { "Ocean Floor", "Drones", V { { pitch, -24 }, { pspread, 0 }, { scale, scOctaves }, { gsize, 500 }, { fltType, 1 }, { fltCut, 600 },
                                       { feedback, 0.4f }, { gravity, 0.6f } }, {} },
    };
    return p;
}

void rollDice (float* v, bool wild, uint32_t seed)
{
    Rng r (seed);
    auto u = [&] (float lo, float hi) { return lo + (hi - lo) * r.uni(); };
    auto chance = [&] (float p) { return r.uni() < p; };
    const float w = wild ? 1.0f : 0.0f;

    v[forecast] = wild ? u (0, 1) : u (0.2f, 0.75f);
    v[humidity] = u (0.25f, 0.75f + 0.2f * w);
    v[gsize] = wild ? std::exp (u (std::log (12.0f), std::log (1000.0f))) : std::exp (u (std::log (60.0f), std::log (700.0f)));
    v[reach] = u (0.2f, 1.0f);
    v[pitch] = chance (0.6f - 0.3f * w) ? 0.0f : (float) ((int) u (-2, 3) * 12);
    if (wild && chance (0.3f)) v[pitch] = (float) (int) u (-24, 25);
    v[pspread] = chance (0.3f) ? 0.0f : u (2, 12 + 12 * w);
    v[glide] = chance (0.75f - 0.3f * w) ? 0.0f : u (-12, 12);
    v[reverse] = chance (0.5f) ? u (0, 0.2f) : u (0, 1);
    v[window] = (float) (int) u (0, 4.99f);
    v[formant] = chance (0.7f) ? 0.0f : u (-6 - 6 * w, 6 + 6 * w);
    v[width] = u (0.6f, 1.6f);
    v[wind] = u (0, 0.5f + 0.5f * w);
    v[windDir] = chance (0.5f) ? 0.0f : u (-180, 180);
    v[turb] = u (0, 0.6f + 0.4f * w);
    v[gravity] = chance (0.6f) ? u (0, 0.2f) : u (0, 0.8f);
    v[temp] = chance (0.6f) ? u (0, 0.2f) : u (0, 0.8f);
    v[life] = u (0.1f, 0.9f);
    v[listen] = chance (0.5f) ? 0.0f : (float) (int) u (0, 3.99f);
    v[snap] = chance (0.7f) ? u (0.6f, 1) : u (0, 1);
    v[scale] = chance (0.6f) ? (float) scAuto : (float) (int) u (1, 9.99f);
    v[nostalgia] = u (0, 0.8f + 0.2f * w);
    v[feedback] = chance (0.5f) ? u (0, 0.2f) : u (0, 0.6f + 0.3f * w);
    v[tone] = u (-0.5f, 0.4f);
    v[fltType] = chance (0.6f) ? 0.0f : (float) (int) u (1, 3.99f);
    v[fltCut] = std::exp (u (std::log (300.0f), std::log (8000.0f)));
    v[fltRes] = u (0, 0.6f + 0.3f * w);
    v[drive] = chance (0.5f) ? u (0, 0.15f) : u (0, 0.5f + 0.4f * w);
    v[driveType] = chance (0.5f) ? 0.0f : 1.0f;
    v[smog] = chance (0.5f) ? u (0, 0.15f) : u (0, 0.5f + 0.4f * w);
    v[space] = u (0.15f, 0.75f);
    v[revSize] = u (0.3f, 1);
    v[revDecay] = u (2, 10 + 8 * w);
    v[shimmer] = chance (0.5f) ? u (0, 0.15f) : u (0, 0.6f);
    v[duck] = chance (0.7f) ? 0.0f : u (0, 0.8f);
    for (int b = 0; b < numBodies; ++b)
    {
        v[g1Type + b * bodyStride] = chance (b == 0 ? 0.45f : 0.2f + 0.2f * w) ? (float) (int) u (1, 5.99f) : 0.0f;
        v[g1Speed + b * bodyStride] = u (0.4f, 1.8f + 1.5f * w);
        v[g1Energy + b * bodyStride] = u (0.2f, 0.9f);
    }
    v[bodyForce] = u (0.3f, 0.9f);
    // keep ghosts if the user has them; roll them only when wild
    if (wild) { v[ghosts] = chance (0.5f) ? 0.0f : u (0.2f, 0.9f); v[ghostAge] = u (0, 1); }
}
} // namespace ml
