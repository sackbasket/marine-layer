// Marine Layer: every automatable parameter in one table (X-macro), shared by
// the engine, the processor (APVTS), presets, dice and the UI.
// Parameter ids are stored in Ableton sets: never rename one after release.
#pragma once
#include <cstddef>

namespace ml
{
// ---- choice lists ----------------------------------------------------------
inline const char* const kWindow[] = { "Gaussian", "Hann", "Tukey", "Perc", "Reverse Perc", nullptr };
inline const char* const kListen[] = { "All", "Tonal", "Transient", "Air", nullptr };
inline const char* const kKey[]    = { "Auto", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", nullptr };
inline const char* const kScale[]  = { "Auto", "Minor", "Major", "Phrygian", "Harmonic Minor", "Dorian",
                                       "Pentatonic Minor", "Octaves", "Fifths + Octaves", "Free", nullptr };
enum Scale { scAuto, scMinor, scMajor, scPhrygian, scHarmMinor, scDorian, scPentaMinor, scOctaves, scFifths, scFree };
inline const char* const kMidiMode[] = { "Off", "Sets Key", "Plays Cloud", nullptr };
inline const char* const kMemBars[]  = { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", nullptr };
inline const char* const kFilter[]   = { "Off", "Low Pass", "Band Pass", "High Pass", nullptr };
inline const char* const kDrive[]    = { "Tape", "Tube", nullptr };
inline const char* const kBody[]     = { "Off", "Ball", "Pendulum", "Spring", "Orbit", "Firefly", nullptr };
inline const char* const kLoopLen[]  = { "1 bar", "2 bars", "4 bars", "8 bars", nullptr };
inline const char* const kMorph[]    = { "Instant", "1/4", "1/2", "1 bar", "2 bars", "4 bars", nullptr };

// modulation matrix sources
inline const char* const kSrc[] = { "None", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6",
    "Life", "Input Level", "Forecast", "Hand X", "Hand Y",
    "Body 1 X", "Body 1 Y", "Body 2 X", "Body 2 Y", "Body 3 X", "Body 3 Y", "Body 4 X", "Body 4 Y", nullptr };
enum Src { sNone, sM1, sM2, sM3, sM4, sM5, sM6, sLife, sInput, sForecast, sHandX, sHandY,
           sB1x, sB1y, sB2x, sB2y, sB3x, sB3y, sB4x, sB4y, sCount };

enum class PT { Float, Int, Bool, Choice };

// X (enum, id, name, type, min, max, default, skewCentre, unit, decimals, choices, bipolar)
#define ML_BODY(X, P, p, N, TYPE) \
    X(P##Type,   p "_type",   N " Type",   PT::Choice, 0, 0, TYPE, 0, "", 0, kBody, false) \
    X(P##Speed,  p "_speed",  N " Speed",  PT::Float, 0.05f, 4, 1, 1, "x", 2, nullptr, false) \
    X(P##Energy, p "_energy", N " Energy", PT::Float, 0, 1, 0.5f, 0, "%", 0, nullptr, false)

#define ML_SLOT(X, P, p, N, SRC, DST, AMT) \
    X(P##Src, p "_src", N " Source", PT::Choice, 0, 0, SRC, 0, "", 0, kSrc, false) \
    X(P##Dst, p "_dst", N " Target", PT::Choice, 0, 0, DST, 0, "", 0, kDst(), false) \
    X(P##Amt, p "_amt", N " Amount", PT::Float, -1, 1, AMT, 0, "%", 0, nullptr, true)

inline const char* const* kDst();

#define ML_PARAMS(X) \
    /* ---- forecast + main ---- */ \
    X(forecast, "forecast",  "Forecast",       PT::Float, 0, 1, 0.5f, 0, "wx", 0, nullptr, false) \
    X(mix,      "mix",       "Mix",            PT::Float, 0, 1, 0.5f, 0, "%", 0, nullptr, false) \
    X(output,   "output",    "Output",         PT::Float, -24, 12, 0, 0, "dB", 1, nullptr, true) \
    X(freeze,   "freeze",    "Freeze",         PT::Bool, 0, 1, 0, 0, "", 0, nullptr, false) \
    /* ---- grains ---- */ \
    X(humidity, "humidity",  "Humidity",       PT::Float, 0, 1, 0.45f, 0, "%", 0, nullptr, false) \
    X(gsize,    "size",      "Grain Size",     PT::Float, 10, 1000, 160, 150, "ms", 0, nullptr, false) \
    X(reach,    "reach",     "Reach",          PT::Float, 0.02f, 1, 0.6f, 0, "%", 0, nullptr, false) \
    X(pitch,    "pitch",     "Pitch",          PT::Float, -24, 24, 0, 0, "st", 1, nullptr, true) \
    X(pspread,  "pspread",   "Pitch Spread",   PT::Float, 0, 24, 5, 0, "st", 1, nullptr, false) \
    X(glide,    "glide",     "Glide",          PT::Float, -12, 12, 0, 0, "st", 1, nullptr, true) \
    X(reverse,  "reverse",   "Reverse",        PT::Float, 0, 1, 0.15f, 0, "%", 0, nullptr, false) \
    X(window,   "window",    "Window",         PT::Choice, 0, 0, 0, 0, "", 0, kWindow, false) \
    X(fsafe,    "fsafe",     "Formant Safe",   PT::Bool, 0, 1, 1, 0, "", 0, nullptr, false) \
    X(formant,  "formant",   "Formant",        PT::Float, -12, 12, 0, 0, "st", 1, nullptr, true) \
    X(width,    "width",     "Width",          PT::Float, 0, 2, 1, 0, "%", 0, nullptr, false) \
    /* ---- weather ---- */ \
    X(wind,     "wind",      "Wind",           PT::Float, 0, 1, 0.15f, 0, "%", 0, nullptr, false) \
    X(windDir,  "wind_dir",  "Wind Direction", PT::Float, -180, 180, 0, 0, "deg", 0, nullptr, true) \
    X(turb,     "turbulence","Turbulence",     PT::Float, 0, 1, 0.3f, 0, "%", 0, nullptr, false) \
    X(gravity,  "gravity",   "Gravity",        PT::Float, 0, 1, 0.1f, 0, "%", 0, nullptr, false) \
    X(temp,     "temperature","Temperature",   PT::Float, 0, 1, 0.1f, 0, "%", 0, nullptr, false) \
    X(life,     "life",      "Life",           PT::Float, 0, 1, 0.35f, 0, "%", 0, nullptr, false) \
    /* ---- listen / key ---- */ \
    X(listen,   "listen",    "Listen",         PT::Choice, 0, 0, 0, 0, "", 0, kListen, false) \
    X(key,      "key",       "Key",            PT::Choice, 0, 0, 0, 0, "", 0, kKey, false) \
    X(scale,    "scale",     "Scale",          PT::Choice, 0, 0, 0, 0, "", 0, kScale, false) \
    X(snap,     "keyfollow", "Key Follow",     PT::Float, 0, 1, 0.8f, 0, "%", 0, nullptr, false) \
    X(midiMode, "midi_mode", "MIDI",           PT::Choice, 0, 0, 1, 0, "", 0, kMidiMode, false) \
    /* ---- memory / ghosts ---- */ \
    X(memLen,   "memory",    "Memory",         PT::Float, 1, 30, 12, 8, "s", 1, nullptr, false) \
    X(memSync,  "mem_sync",  "Memory Sync",    PT::Bool, 0, 1, 0, 0, "", 0, nullptr, false) \
    X(memBars,  "mem_bars",  "Memory Bars",    PT::Choice, 0, 0, 2, 0, "", 0, kMemBars, false) \
    X(nostalgia,"nostalgia", "Nostalgia",      PT::Float, 0, 1, 0.4f, 0, "%", 0, nullptr, false) \
    X(feedback, "feedback",  "Feedback",       PT::Float, 0, 1, 0.1f, 0, "%", 0, nullptr, false) \
    X(ghosts,   "ghosts",    "Ghosts",         PT::Float, 0, 1, 0, 0, "%", 0, nullptr, false) \
    X(ghostAge, "ghost_age", "Ghost Age",      PT::Float, 0, 1, 0.5f, 0, "%", 0, nullptr, false) \
    X(ghostRate,"ghost_rate","Ghost Rate",     PT::Float, 0, 1, 0.4f, 0, "%", 0, nullptr, false) \
    X(ghostAuto,"ghost_auto","Auto Bookmarks", PT::Bool, 0, 1, 1, 0, "", 0, nullptr, false) \
    /* ---- character ---- */ \
    X(tone,     "tone",      "Tone",           PT::Float, -1, 1, -0.1f, 0, "", 2, nullptr, true) \
    X(fltType,  "flt_type",  "Filter",         PT::Choice, 0, 0, 0, 0, "", 0, kFilter, false) \
    X(fltCut,   "flt_cut",   "Cutoff",         PT::Float, 40, 18000, 2500, 1000, "Hz", 0, nullptr, false) \
    X(fltRes,   "flt_res",   "Resonance",      PT::Float, 0, 1, 0.3f, 0, "%", 0, nullptr, false) \
    X(drive,    "drive",     "Drive",          PT::Float, 0, 1, 0.1f, 0, "%", 0, nullptr, false) \
    X(driveType,"drive_type","Drive Type",     PT::Choice, 0, 0, 0, 0, "", 0, kDrive, false) \
    X(smog,     "smog",      "Smog",           PT::Float, 0, 1, 0.1f, 0, "%", 0, nullptr, false) \
    X(space,    "space",     "Space",          PT::Float, 0, 1, 0.35f, 0, "%", 0, nullptr, false) \
    X(revSize,  "rev_size",  "Space Size",     PT::Float, 0, 1, 0.7f, 0, "%", 0, nullptr, false) \
    X(revDecay, "rev_decay", "Space Decay",    PT::Float, 0.5f, 20, 5, 4, "s", 1, nullptr, false) \
    X(shimmer,  "shimmer",   "Shimmer",        PT::Float, 0, 1, 0.1f, 0, "%", 0, nullptr, false) \
    X(duck,     "duck",      "Ducking",        PT::Float, 0, 1, 0, 0, "%", 0, nullptr, false) \
    /* ---- gravity bodies ---- */ \
    ML_BODY(X, g1, "g1", "Body 1", 0) \
    ML_BODY(X, g2, "g2", "Body 2", 0) \
    ML_BODY(X, g3, "g3", "Body 3", 0) \
    ML_BODY(X, g4, "g4", "Body 4", 0) \
    X(bodyForce,"body_force","Body Force",     PT::Float, 0, 1, 0.5f, 0, "%", 0, nullptr, false) \
    /* ---- hand (mouse performance, automatable) ---- */ \
    X(handX,    "hand_x",    "Hand X",         PT::Float, 0, 1, 0.3f, 0, "%", 0, nullptr, false) \
    X(handY,    "hand_y",    "Hand Y",         PT::Float, 0, 1, 0.5f, 0, "%", 0, nullptr, false) \
    X(handPull, "hand_pull", "Hand Pull",      PT::Float, -1, 1, 0, 0, "%", 0, nullptr, true) \
    X(handGust, "hand_gust", "Hand Gust",      PT::Float, 0, 1, 0, 0, "%", 0, nullptr, false) \
    X(loopLen,  "hand_loop", "Hand Loop",      PT::Choice, 0, 0, 1, 0, "", 0, kLoopLen, false) \
    X(morph,    "morph",     "Snapshot Morph", PT::Choice, 0, 0, 3, 0, "", 0, kMorph, false) \
    /* ---- macros + matrix ---- */ \
    X(m1, "macro1", "Macro 1", PT::Float, 0, 1, 0, 0, "%", 0, nullptr, false) \
    X(m2, "macro2", "Macro 2", PT::Float, 0, 1, 0, 0, "%", 0, nullptr, false) \
    X(m3, "macro3", "Macro 3", PT::Float, 0, 1, 0, 0, "%", 0, nullptr, false) \
    X(m4, "macro4", "Macro 4", PT::Float, 0, 1, 0, 0, "%", 0, nullptr, false) \
    X(m5, "macro5", "Macro 5", PT::Float, 0, 1, 0, 0, "%", 0, nullptr, false) \
    X(m6, "macro6", "Macro 6", PT::Float, 0, 1, 0, 0, "%", 0, nullptr, false) \
    ML_SLOT(X, s1, "mx1", "Slot 1", 0, 0, 0) \
    ML_SLOT(X, s2, "mx2", "Slot 2", 0, 0, 0) \
    ML_SLOT(X, s3, "mx3", "Slot 3", 0, 0, 0) \
    ML_SLOT(X, s4, "mx4", "Slot 4", 0, 0, 0) \
    ML_SLOT(X, s5, "mx5", "Slot 5", 0, 0, 0) \
    ML_SLOT(X, s6, "mx6", "Slot 6", 0, 0, 0) \
    ML_SLOT(X, s7, "mx7", "Slot 7", 0, 0, 0) \
    ML_SLOT(X, s8, "mx8", "Slot 8", 0, 0, 0) \
    ML_SLOT(X, s9, "mx9", "Slot 9", 0, 0, 0) \
    ML_SLOT(X, s10, "mx10", "Slot 10", 0, 0, 0) \
    ML_SLOT(X, s11, "mx11", "Slot 11", 0, 0, 0) \
    ML_SLOT(X, s12, "mx12", "Slot 12", 0, 0, 0)

enum P : int {
#define ML_ENUM(e, ...) e,
    ML_PARAMS(ML_ENUM)
#undef ML_ENUM
    numParams
};

struct ParamInfo
{
    const char* id; const char* name; PT type;
    float min, max, def, skewCentre; const char* unit; int decimals;
    const char* const* choices; bool bipolar;
};

inline int countChoices (const char* const* c) { int n = 0; while (c && c[n]) ++n; return n; }

// matrix destinations: continuous parameters only
inline const int* dstParams (int& count)
{
    static const int d[] = { -1, forecast, mix, humidity, gsize, reach, pitch, pspread, glide, reverse, formant, width,
        wind, windDir, turb, gravity, temp, life, snap, nostalgia, feedback, ghosts,
        tone, fltCut, fltRes, drive, smog, space, revSize, shimmer, duck, bodyForce };
    count = (int) (sizeof (d) / sizeof (d[0]));
    return d;
}

inline const ParamInfo* paramTable();

inline const char* const* kDst()
{
    static const char* names[64] = {};
    static bool built = false;
    if (! built)
    {
        int n = 0; const int* d = dstParams (n);
        names[0] = "None";
        for (int i = 1; i < n; ++i) names[i] = paramTable()[d[i]].name;
        names[n] = nullptr;
        built = true;
    }
    return names;
}

inline const ParamInfo* paramTable()
{
    static ParamInfo t[numParams];
    static bool built = false;
    if (! built)
    {
        built = true;   // set first: kDst() below reads names back from this table
        int i = 0;
#define ML_ROW(e, id_, name_, type_, mn, mx, df, sk, unit_, dec, ch, bp) \
        t[i++] = ParamInfo { id_, name_, type_, (float) (mn), (float) (mx), (float) (df), (float) (sk), unit_, dec, nullptr, bp };
        ML_PARAMS(ML_ROW)
#undef ML_ROW
        // choices filled in a second pass (kDst() needs the names above)
        i = 0;
#define ML_CH(e, id_, name_, type_, mn, mx, df, sk, unit_, dec, ch, bp) t[i++].choices = ch;
        ML_PARAMS(ML_CH)
#undef ML_CH
        for (auto& p : t) if (p.type == PT::Choice) { p.min = 0; p.max = (float) (countChoices (p.choices) - 1); }
    }
    return t;
}

constexpr int numMacros = 6, numSlots = 12, numBodies = 4;
constexpr int bodyStride = g2Type - g1Type;
constexpr int slotStride = s2Src - s1Src;

inline const char* forecastName (float w)
{
    static const char* n[] = { "Clear", "Haze", "Fog", "Drizzle", "Storm" };
    int i = (int) (w * 4.0f + 0.5f); i = i < 0 ? 0 : (i > 4 ? 4 : i);
    return n[i];
}
} // namespace ml
