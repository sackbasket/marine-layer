# Marine Layer

A granular **memory** effect you play with the mouse (VST3, macOS universal).
Loop your song, and Marine Layer remembers what it hears. Its grains are particles
drifting in weather you control, turning the music into clouds, choirs, ghosts and
glitches that stay in the song's key.

## Install (macOS)

Download **[MarineLayer-Installer.pkg](https://github.com/sackbasket/marine-layer/releases/download/latest/MarineLayer-Installer.pkg)** and double-click it.
If macOS says it can't verify the developer: **System Settings → Privacy & Security → Open Anyway**.
Then in Ableton: **Settings → Plug-Ins → Rescan** (with "Use VST3 Plug-In System Folders" on).
Marine Layer is under Audio Effects (VST3). Latency 0.

## Playing the fog

| Gesture | What it does |
|---|---|
| Drag in the fog | your hand pulls the grains in (drag speed = gust) |
| Right-drag / Alt-drag | blows them away |
| Quick flick | scatter burst |
| Shift-drag | paints a wind current the grains follow for a few seconds |
| Double-click | places an emitter (a fountain of grains from that memory spot and pitch); drag to move, drag off the scene or double-click to delete (up to 4) |
| Scroll / Shift-scroll / pinch | Humidity (density) / grain size |
| Space (scene focused) or FREEZE | hold = momentary freeze, click FREEZE = latch |
| Drag a body | fling it |
| Click the timeline | bookmark that bar for Ghosts |

In the scene, **x is time** (left = now, right = oldest) and **y is pitch**. Each particle
"rains" grains from where it floats; old memories are worn (darker, wobblier, dustier,
bit-rotted) depending on **Nostalgia**.

**REC HAND** records your mouse performance for 1/2/4/8 bars (tempo-synced) and loops it;
touch the fog while it loops to overdub. Hand X/Y/Pull/Gust are also automatable, so
Ableton can record them too. **Snapshots**: 6 pads; click an empty pad to save, click a saved
pad to morph to it (Instant to 4 bars). Shift-click = save over, Alt-click = clear.

**Forecast** (big slider) morphs many settings at once: Clear → Haze → Fog → Drizzle
(grains fall like rain) → Storm (dense, chaotic, picks transients). Automate it to tell a story.

## Controls (Advanced view)

- **Grains**: Humidity, Size, Reach, Pitch, Pitch Spread, Glide, Reverse, Window (Gaussian, Hann, Tukey, Perc, Reverse Perc), Formant Safe (PSOLA), Formant, Width.
- **Weather**: Forecast, Wind + Direction, Turbulence, Gravity, Warmth (temperature), Life.
- **Listen + Key**: Listen (All / Tonal / Transient / Air), Key (Auto or fixed), Scale (Auto, Minor, Major, Phrygian, Harmonic Minor, Dorian, Pentatonic Minor, Octaves, Fifths + Octaves, Free), Key Follow, MIDI (Sets Key / Plays Cloud).
- **Memory + Ghosts**: Memory 1–30 s (or synced 1–16 bars), Nostalgia, Feedback, Ghosts, Ghost Age, Ghost Rate, Auto Bookmarks.
- **Character** (cloud only, dry stays untouched): Tone, Filter (LP/BP/HP) + Cutoff + Reso, Drive (Tape/Tube), Smog, Space (reverb) + Room + Decay, Shimmer, Ducking.
- **Gravity**: 4 bodies (Ball, Pendulum, Spring, Orbit, Firefly) with Speed and Energy, plus Push.
- **Matrix**: 12 slots; sources Macro 1–6, Life, Input Level, Forecast, Hand X/Y, Body X/Y.

30 factory presets, Dice (Alt = wild), 40-step Undo, user presets (`.mlpreset` in
`~/Library/Application Support/Marine Layer/Presets`). Window sizes 80–130 %.

## Build

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPLUGIN_BUILD_TESTS=ON
cmake --build build --config Release
./build/EngineTests_artefacts/Release/EngineTests
```

Every push to `main` builds a universal binary on GitHub Actions, runs the tests and
publishes the installer to the `latest` release.

## Credits

Gravity bodies, the shimmer reverb, Smog and the dusk UI theme come from LA (same author,
[sackbasket/LA](https://github.com/sackbasket/LA)). Built with [JUCE](https://juce.com) 8.
