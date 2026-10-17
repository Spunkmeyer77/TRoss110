// Engine test without JUCE: renders every drum on its own plus a pattern, prints levels and
// lengths, and writes WAV files.
//   g++ -O2 -std=c++17 -ISource tools/render_test.cpp -o render_test && ./render_test [outdir]
#include "dsp/TrossEngine.h"
#include <cstdio>
#include <string>
#include <vector>
#include <cmath>
#include <ctime>

using namespace tross110;

static void writeWav (const std::string& path, const std::vector<float>& x, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + (uint32_t) x.size() * 2); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (1); u32 (sr); u32 (sr * 2); u16 (2); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 ((uint32_t) x.size() * 2);
    for (float v : x) { int16_t s = (int16_t) std::lrint (std::fmax (-1.f, std::fmin (1.f, v)) * 32767); std::fwrite (&s, 2, 1, f); }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    const char* names[] = { "bd", "sd", "cp", "ch", "oh", "cy" };
    int fails = 0;

    for (int sr : { 44100, 48000, 96000 })
    {
        for (int v = 0; v < kNumVoices; ++v)
        {
            for (int vel : { 100, 127 })
            {
                Engine e; e.prepare (sr);
                SoundParams p; e.setParams (p);
                std::vector<float> out (sr * 2);
                e.trigger (v, vel);
                double peak = 0, energy = 0; int last = 0, bad = 0;
                for (size_t i = 0; i < out.size(); ++i)
                {
                    out[i] = e.tick();
                    if (! std::isfinite (out[i])) ++bad;
                    peak = std::fmax (peak, std::fabs (out[i]));
                    energy += out[i] * out[i];
                    if (std::fabs (out[i]) > 0.001) last = (int) i;
                }
                // crude brightness: zero crossings per second in the first 100 ms
                int zc = 0;
                for (int i = 1; i < sr / 10; ++i) zc += (out[i - 1] < 0) != (out[i] < 0);
                std::printf ("%5d Hz %s vel %3d  peak %.3f  rms %.4f  length %4d ms  zc/s %6d  nonfinite %d\n",
                             sr, names[v], vel, peak, std::sqrt (energy / out.size()), last * 1000 / sr, zc * 10, bad);
                if (bad || peak < 0.01 || peak > 1.5) ++fails;
                if (sr == 48000 && vel == 100) writeWav (dir + "/voice_" + names[v] + ".wav", out, sr);
            }
        }
    }

    // a pattern at 120 bpm for 2 bars, through the sequencer path
    {
        const int sr = 48000;
        Engine e; e.prepare (sr);
        SoundParams p; e.setParams (p);
        uint16_t rows[kNumRows] {};
        auto put = [&] (int row, const char* s) { for (int i = 0; i < 16; ++i) if (s[i] == 'x') rows[row] |= (uint16_t) (1u << i); };
        put (0, "x.......x.......");
        put (1, "x...x...x...x...");
        put (2, "....x.......x...");
        put (4, "x.x.x.x.x.x.x.x.");
        put (5, "..............x.");
        put (3, "............x...");
        put (6, "x...............");
        const int stepLen = sr * 60 / 120 / 4;
        std::vector<float> out (stepLen * 32 + sr);
        for (size_t i = 0; i < out.size(); ++i)
        {
            if (i % stepLen == 0 && i / stepLen < 32) e.sequencerStep ((int) (i / stepLen) % 16, rows, true);
            out[i] = e.tick();
        }
        double peak = 0; for (float v : out) peak = std::fmax (peak, std::fabs (v));
        std::printf ("pattern peak %.3f\n", peak);
        writeWav (dir + "/pattern.wav", out, sr);
    }

    // parameter locks: a step with BD Tune locked to +12 st plays higher; the next plain step is back to the knob
    {
        const int sr = 48000;
        Engine e; e.prepare (sr); SoundParams p; e.setParams (p);
        uint16_t rows[kNumRows] {}; rows[1] = 0b11;   // BD on steps 1 and 2
        float locks[kNumSteps][kNumVoices][kNumLockParams];
        for (auto& a : locks) for (auto& b : a) for (auto& c : b) c = kNoLock;
        locks[0][BD][LTune] = 12.0f;
        auto zc = [&] (int step) {
            e.sequencerStep (step, rows, true, locks[step]);
            int z = 0; float prev = 0;
            for (int i = 0; i < sr / 4; ++i) { const float v = e.tick(); if (i > 200 && (prev < 0) != (v < 0)) ++z; prev = v; }
            return z;
        };
        const int locked = zc (0), plain = zc (1);
        std::printf ("p-lock: BD zero crossings, locked +12 st %d, unlocked %d\n", locked, plain);
        if (! (locked > plain * 1.6)) { std::printf ("P-LOCK CHECK FAILED\n"); ++fails; }
    }

    // CPU: 10 s of every voice hitting on every 16th at 48 kHz
    {
        Engine e; e.prepare (48000); SoundParams p; e.setParams (p);
        uint16_t rows[kNumRows]; for (auto& r : rows) r = 0xffff;
        const int stepLen = 6000; volatile float sink = 0;
        auto t0 = std::clock();
        for (int i = 0; i < 480000; ++i) { if (i % stepLen == 0) e.sequencerStep ((i / stepLen) % 16, rows, true); sink = sink + e.tick(); }
        const double secs = double (std::clock() - t0) / CLOCKS_PER_SEC;
        std::printf ("CPU: 10 s of audio, all voices on every step, rendered in %.2f s (%.1f %% of one core)\n", secs, secs * 10);
    }
    std::printf ("%s\n", fails ? "SOME CHECKS FAILED" : "all voices OK");
    return fails ? 1 : 0;
}
