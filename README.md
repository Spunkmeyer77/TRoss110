# TRoss 110

110-style analog drum machine with a 16-step sequencer (AU / VST3 / LV2 / Standalone) by Ticco Ross, 2026.

Six voices: bass drum, snare, hand clap, closed hat, open hat and cymbal, plus a global accent.
Defaults: BD Decay 24.1 %, Level -8.4 dB · SD Decay 43.9 %, Snappy 60 % · Clap Tune +2.6 st, Decay 20.3 %, Level -3.6 dB.

The full manual is in [`docs/TRoss110-Manual.pdf`](docs/TRoss110-Manual.pdf).

## Using it

| Control | What it does |
|---|---|
| 6 drum channels | Tune / Decay / Level, plus Attack (BD), Snappy (SD), Tail (clap reverb tail), Tone (cymbal: shimmer vs body). **M** mutes, **S** solos, **OWN OUT** sends the drum to its own output. Click a drum's name strip to play it like a pad (Shift-click = accented hit). Hover or drag a knob to see its value; double-click resets it |
| Decay | 50 % is the classic timing. The bass drum's decay also follows its tuning: tune it higher and it gets shorter |
| Hats | The closed hat cuts off a ringing open hat |
| Master | Accent amount and Volume. Accent is a short swell on the whole output that also pushes it into grit, so every drum on an accented step hits harder. The display shows the playing pattern (and the queued one) |
| LCD step grid | 7 rows (AC + 6 drums) × 16 steps. Click to toggle, or drag along a row to paint. The arrow at the top follows the playing step |
| RUN | Sequencer on/off |
| LENGTH | Pattern length, 1–16 steps: drag the read-out up/down (use 12 for shuffle and triplet feels) |
| CLEAR | Clears the pattern that's playing |
| REC | While the host is playing, keyboard hits and name-strip clicks are written into the nearest step. Velocity above 100 also sets the accent. On by default |
| PATTERN 1–12 | 12 pattern slots. While playing, a new slot waits until the current pattern reaches step 1. While stopped, it switches immediately. The LED shows which slot is playing; a blinking LED shows the slot that's queued |
| Pattern keys, right-click | **Copy to** another slot, **Clear**, or edit the chain. A dash on a key means that slot is empty |
| Pattern chain | **Shift-click** pattern keys to chain them (up to 32 steps, one pattern per bar). The chain follows the song position, so jumps and loops land on the right pattern. Dim LEDs = in the chain; the display shows `CHN step/length`. A plain click ends the chain. Saved with the project |
| P-LOCK | Switches the screen between the step grid and the **parameter-lock** screen: pick a drum (left) and a knob (Tune, Decay, Level or the drum's extra knob, below), then drag each step's bar to lock that value for that step. Right-click or double-click removes a lock; Shift-right-click clears a lane, a drum or the whole pattern. A lock lasts for that hit's sound; live notes use the knobs. Locked steps show a corner mark on the step grid. Copied, cleared and saved with the pattern |
| HELP (top right) | Help windows on/off (the knob value read-outs always show). Saved with the project |
| EXPORT MIDI | Click for a menu: export the playing pattern, all patterns in a row, all patterns as separate `.mid` files, or the chain. Drag the key onto a MIDI track to drop the playing pattern in as a clip. Steps are 16th notes, accented steps velocity 127, channel 10 |

**Keyboard map (Ableton note names, C1 = MIDI 36):** BD C1/B0 · SD D1/E1/C#1 · HC D#1 · CH F#1/G#1 · OH A#1 ·
CY C#2/D#2/E2/F2/G2/A2.

Velocity: velocities up to 100 scale the level, and anything above 100 also fires the Accent.

All 12 patterns are saved with your Live set, along with every knob. The window can be resized from its corner.

## Outputs

| Output | Carries |
|---|---|
| Main (stereo) | Every drum whose OWN OUT is off, after Volume (the same mono mix on both channels) |
| BD, SD, CP, CH, OH, CY (mono) | That drum alone while its OWN OUT is on: after its Level, Mute/Solo and accent, **not** affected by Volume |

All drums start on the main output. Turn on a drum's **OWN OUT** and it leaves the main mix and plays on its own
output instead. If the host has that output switched off, the drum stays in the main mix.

**Ableton Live:** make an audio track for each separate drum, set **Audio From** to the TRoss 110 track,
pick the output (for example `BD`) in the second menu, and set Monitor to **In**.

The **standalone app** has only the main output, so it doesn't show the OWN OUT switches. It has no host
transport, so it shows its own **PLAY** key and BPM read-out in the master section.

The build's version number is shown at the top right of the panel.

## Build on your Mac (macOS 12+)

1. **Compiler:** run `xcode-select --install` in Terminal.
2. **CMake 3.22 or newer:** download it from https://cmake.org/download/, then run
   `sudo "/Applications/CMake.app/Contents/bin/cmake-gui" --install`
3. **Build** from inside this folder:

   ```sh
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
   cmake --build build --config Release -j8
   ```

   The plug-ins are copied to `~/Library/Audio/Plug-Ins/Components/TRoss 110.component` and
   `~/Library/Audio/Plug-Ins/VST3/TRoss 110.vst3`.
4. **Validate the AU (optional):** `auval -v aumu T110 Tcro`
5. **Ableton Live:** go to Settings → Plug-Ins, turn on "Use Audio Units v2", and rescan.
   The plug-in is under Plug-Ins → Audio Units → Ticco Ross → TRoss 110.

To build the installer, run `./tools/make_pkg.sh` after the build. It creates `dist/TRoss110-<version>.pkg`.
Signing and notarizing work the same way as for TRoss 606 (`DEV_ID_APP`, `DEV_ID_INSTALLER`, `NOTARY_PROFILE`).

## Automatic builds (GitHub Actions)

Each push to `main` builds everything on GitHub's macOS, Windows and Linux machines and runs the
tests on Linux. The files appear under the run's **Artifacts** on the **Actions** tab:

| Platform | Files |
|---|---|
| macOS | `TRoss110-<v>-macOS.pkg` (AU + VST3 + app) with `TRoss110-<v>-Manual.pdf`, plus AU and VST3 zips |
| Windows | `TRoss110-<v>-Windows-Setup.exe` (VST3 + app), plus a VST3 zip |
| Linux | `TRoss110-<v>-Linux-x86_64.tar.gz` (VST3 + LV2 + app, with `install.sh`) |

**Publishing a release:** raise the version in `CMakeLists.txt`, commit, then
`git tag v0.2.0 && git push --tags`. The installers and the manual get attached to a new GitHub release.

The installers aren't signed. The first time you open one, macOS shows a Gatekeeper warning
(Privacy & Security → Open Anyway) and Windows shows SmartScreen (More info → Run anyway).

## Tests

```sh
g++ -O2 -std=c++17 -ISource tools/render_test.cpp -o render_test && ./render_test   # engine only, no JUCE
cmake -B build -DT110_BUILD_TESTS=ON && cmake --build build --target TRoss110Test     # full plug-in
```

The plug-in test covers host sync, pattern queueing, LENGTH, keyboard recording, own outputs, MIDI export and
state save/restore. It also takes a snapshot of the editor.
