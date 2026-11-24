# Working on this plug-in

This repo was created from the user's **plugin-template**. The user
makes music in **Ableton Live on an Apple Silicon Mac** and installs plug-ins
from the one-click `.pkg` that GitHub Actions publishes on every push to `main`.

## Starting a new plug-in from the template
1. `scripts/new-plugin.sh <Name> <Code> "<tagline>" <Category>`: sets the
   name, 4-char code, bundle id and category in `plugin.cmake`. The code must be
   unique per plug-in and start with an uppercase letter.
2. Replace the example parts:
   - `Source/dsp/Engine.*`: the DSP (plain C++ + juce_dsp, no UI code)
   - `Source/Params.h`: one row per parameter. The editor builds knobs automatically.
   - `Source/PluginProcessor.cpp`: map parameters to `engine::Params` in `process()`
   - `Source/ui/Display.*`: the big centre panel (curve, spectrum, graph...)
   - `Source/ui/Theme.h`: `col::accent` gives each plug-in its own colour
3. Update `README.md` (controls, install, credits) and `Tests/EngineTests.cpp`.

## Rules
- Clones of commercial plug-ins: same concept/controls/behaviour, built from
  scratch, with an original name and UI. No copied code, names, logos or look.
- GPL ports: keep the licence (add `COPYING`; `make-pkg.sh` shows it in the
  installer), keep the copyright headers, credit the authors, mark changes `2026 port`.
- Never rename parameter ids after a release (they are stored in Ableton sets).
- Keep the user updated in short, plain-language messages.

## Verify before every push
`scripts/build-local.sh` (Linux; needs JUCE at `~/JUCE` or pass a path):
builds the VST3, runs `EngineTests`, renders the UI to `snapshot.png`. Look at
the snapshot. Also check with `clang++ -stdlib=libc++ -fsyntax-only` when
porting old code, because Apple's toolchain is stricter than gcc.

Linux packages needed: `libasound2-dev libxrandr-dev libxinerama-dev
libxcursor-dev libgl-dev libfontconfig1-dev libxext-dev libxcomposite-dev xvfb
ninja-build clang libc++-dev`.

## GitHub build
Push to `main` → `.github/workflows/build-mac.yml` builds a universal VST3 on
`macos-15-intel`, runs the tests, ad-hoc signs, makes the `.pkg` with
`scripts/make-pkg.sh` and publishes to the `latest` release:
`https://github.com/<owner>/<repo>/releases/download/latest/<Name>-Installer.pkg`

Monitor with `gh run list -R <owner>/<repo>` and
`gh api repos/<owner>/<repo>/actions/runs/<id>/jobs` (+ `/actions/jobs/<id>/logs`).

## Known gotchas
- `juce::dsp::FFT::perform` must not run in place; use a separate output
  buffer. Check the inverse scaling at prepare time.
- `AudioParameterBool/Float::getValue()` is private: call it via
  `juce::AudioProcessorParameter*`.
- Parameter attachments fire `onClick` on automation changes, so gate UI
  navigation on `isMouseOverOrDragging()`.
- Old code on libc++: `std::auto_ptr` needs `_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR=1`;
  `long` is 8 bytes on 64-bit; MSVC-era templates need `this->`/`template`.
- FFTW via FetchContent needs `set(CMAKE_POLICY_VERSION_MINIMUM 3.5)`.
- Quote YAML step names that contain a colon.
- macOS blocks writing into `~/Library/Audio/Plug-Ins` through the device
  bridge; deliver the `.pkg` link instead.

## Delivering
Give the user: the `.pkg` link, install steps (double-click; Privacy & Security
→ Open Anyway if blocked; Ableton → Settings → Plug-Ins → Rescan), what was
verified and what wasn't, and an offer to tune by ear.

## Privacy
This repo is public. Never put the owner's name in code, docs, plug-in metadata or commit
author fields (commit as `Marine Layer <noreply@example.com>`).
