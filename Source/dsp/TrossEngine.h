// TRoss 110 engine: six analog-style drum voices, the global accent, the pattern memory and the
// sequencer hooks. No JUCE in here, so it can be tested on its own (tools/render_test.cpp).
#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace tross110
{
enum Voice { BD, SD, CP, CH, OH, CY, kNumVoices };
// sequencer rows: 0 = accent, 1..6 = the voices above (row = voice + 1)
constexpr int kNumRows = 7, kNumSteps = 16, kNumSlots = 12;

// keyboard note -> voice (General MIDI drum layout), -1 = not mapped
inline int noteToVoice (int n)
{
    switch (n)
    {
        case 36: case 35:                                     return BD;
        case 38: case 40: case 37:                            return SD;
        case 39:                                              return CP;
        case 42: case 44:                                     return CH;
        case 46:                                              return OH;
        case 49: case 51: case 52: case 53: case 55: case 57: return CY;
        default:                                              return -1;
    }
}

// Sound parameters (units as shown on the panel)
struct SoundParams
{
    float master = -6, accent = 50;
    float lvl[kNumVoices]   { 0, 0, 0, 0, 0, 0 };        // dB, -70..6
    float tune[kNumVoices]  { 0, 0, 0, 0, 0, 0 };        // semitones
    float decay[kNumVoices] { 50, 50, 50, 50, 50, 50 };  // %, 50 = the original timing
    float bdAttack = 50, sdSnappy = 60, cpTail = 50, cyTone = 50;
    bool mute[kNumVoices] {}, solo[kNumVoices] {};
    bool ownOut[kNumVoices] {};   // true = the drum leaves the main mix and plays on its own output
};

// 12 slots x 7 rows x 16 steps, one 16-bit mask per row. Edited from the UI thread,
// read and recorded into from the audio thread, so every row is atomic.
class PatternMemory
{
public:
    uint16_t get (int slot, int row) const          { return masks[slot][row].load (std::memory_order_relaxed); }
    void set (int slot, int row, uint16_t m)       { masks[slot][row].store (m, std::memory_order_relaxed); ++version; }
    bool cell (int slot, int row, int step) const  { return (get (slot, row) >> step) & 1; }
    void setCell (int slot, int row, int step, bool on)
    {
        auto m = get (slot, row);
        m = on ? (uint16_t) (m | (1u << step)) : (uint16_t) (m & ~(1u << step));
        set (slot, row, m);
    }
    void clear (int slot) { for (int r = 0; r < kNumRows; ++r) set (slot, r, 0); }
    int getVersion() const { return version.load(); }

private:
    std::atomic<uint16_t> masks[kNumSlots][kNumRows] {};
    std::atomic<int> version { 0 };
};

// ================================================================================================
// building blocks
namespace dsp
{
constexpr double kPi = 3.141592653589793, kTwoPi = 6.283185307179586;

inline double decayFactor (double pct) { return std::pow (4.0, (pct - 50.0) / 50.0); }   // 0 % = x0.25, 100 % = x4
inline double tauCoef (double fs, double tau) { return std::exp (-1.0 / (fs * (tau > 1e-5 ? tau : 1e-5))); }
inline double semis (double st) { return std::pow (2.0, st / 12.0); }

// 18-stage shift register clocked at 80 kHz (taps 13 and 18), averaged over each output sample
struct ShiftNoise
{
    uint32_t reg = 0x2a5f3;
    double phase = 0.0, inc = 1.0;
    void prepare (double fs) { inc = 80000.0 / fs; }
    inline double tick()
    {
        double acc = 0.0, w = 0.0, left = 1.0, pos = phase;
        // walk through this sample's span of register clocks, weighting each bit by how long it is held
        while (left > 0.0)
        {
            const double toEdge = (1.0 - pos) / inc;   // fraction of the output sample until the next clock
            const double seg = toEdge < left ? toEdge : left;
            acc += ((reg & 1u) ? 1.0 : -1.0) * seg; w += seg;
            left -= seg; pos += seg * inc;
            if (pos >= 1.0 - 1e-12)
            {
                pos -= 1.0;
                const uint32_t b = ((reg >> 12) ^ (reg >> 17)) & 1u;
                reg = ((reg << 1) | b) & 0x3ffffu;
            }
        }
        phase = pos;
        return acc / w;
    }
};

inline double blep (double t, double dt)
{
    if (t < dt)       { const double x = t / dt;         return x + x - x * x - 1.0; }
    if (t > 1.0 - dt) { const double x = (t - 1.0) / dt; return x * x + x + x + 1.0; }
    return 0.0;
}

struct Square
{
    double ph = 0.0, dt = 0.01;
    void setFreq (double f, double fs) { dt = f / fs; if (dt > 0.45) dt = 0.45; }
    inline double tick()
    {
        double y = ph < 0.5 ? 1.0 : -1.0;
        y += blep (ph, dt);
        double t2 = ph + 0.5; if (t2 >= 1.0) t2 -= 1.0;
        y -= blep (t2, dt);
        ph += dt; if (ph >= 1.0) ph -= 1.0;
        return y;
    }
};

// zero-delay-feedback state variable filter
struct Svf
{
    double a1 = 0, a2 = 0, a3 = 0, k = 1, ic1 = 0, ic2 = 0;
    void set (double f, double q, double fs)
    {
        if (f > fs * 0.45) f = fs * 0.45;
        const double g = std::tan (kPi * f / fs);
        k = 1.0 / q; a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void tick (double v0, double& lp, double& bp)
    {
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
        lp = v2; bp = v1;
    }
    void reset() { ic1 = ic2 = 0; }
};

struct OnePole
{
    double a = 0, z = 0, xPrev = 0;
    void set (double f, double fs) { a = std::exp (-kTwoPi * f / fs); }
    inline double lp (double x) { z = x + a * (z - x); return z; }
    inline double hp (double x) { z = a * (z + x - xPrev); xPrev = x; return z; }
};

// transistor "swing" VCA envelope: the trigger charges the capacitor almost at once, then it discharges
struct Env
{
    double e = 0, target = 0, attInc = 0, coef = 0.999;
    int attLeft = 0, attSamples = 20;
    void prepare (double fs) { attSamples = (int) (fs * 0.0004) + 1; }
    void trigger (double level)
    {
        target = level > e ? level : e;
        attLeft = attSamples; attInc = (target - e) / attSamples;
    }
    void choke (double c) { coef = c; }
    inline double tick()
    {
        if (attLeft > 0) { e += attInc; --attLeft; }
        else              e *= coef;
        if (e < 1e-9) e = 0;
        return e;
    }
};

// damped oscillator (bridged-T ringing), as a decaying rotating phasor so the pitch can move smoothly
struct Ringer
{
    double re = 0, im = 0;
    inline double tick (double x, double r, double w)
    {
        const double c = std::cos (w), s = std::sin (w);
        const double nr = r * (re * c - im * s) + x;
        im = r * (re * s + im * c);
        re = nr;
        return im;
    }
};

// four detuned squares + noise through the "metal" filter (6 dB/oct below, resonant 12 dB/oct above)
struct Metal
{
    static constexpr double kFreqs[4] = { 305.0, 444.0, 794.0, 1136.0 };
    Square sq[4];
    void setTune (double ratio, double fs) { for (int i = 0; i < 4; ++i) sq[i].setFreq (kFreqs[i] * ratio, fs); }
    inline double tick (double noise)
    {
        return (sq[0].tick() + sq[1].tick() + sq[2].tick() + sq[3].tick()) * 0.25 + noise * 0.4;
    }
};

struct MetalFilter
{
    OnePole hp; Svf lp; double q = 6.05;
    void set (double f, double fs) { hp.set (f, fs); lp.set (f, q, fs); }
    inline double tick (double x) { double l, b; lp.tick (hp.hp (x), l, b); return l / q; }
};

inline double vel (int v) { const double m = v < 100 ? v : 100; return 0.3 + 0.7 * m / 100.0; }
inline double softClip (double x) { return x / std::sqrt (1.0 + x * x); }
} // namespace dsp

// ================================================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        noise = {}; noise.prepare (fs);
        for (auto* e : { &bdEnvPitch, &sdSnap, &chEnv, &ohEnv, &cyTing, &cyBody, &cyLow, &cpHit, &cpTail, &accEnv })
        { *e = {}; e->prepare (fs); }
        bdRing = {}; sdRing = {};
        for (auto& m : metalCh.sq) m = {};
        for (auto& m : metalOh.sq) m = {};
        for (auto& m : metalCy.sq) m = {};
        chF = {}; ohF = {}; cyHi = {}; cyLo = {}; cpBp.reset();
        for (auto& h : dcBlock) { h = {}; h.set (18.0, fs); }
        bdPulseLp = {}; sdSnapHp = {}; sdSnapLp = {}; metalHp = {};
        bdPulseLeft = sdPulseLeft = 0;
        cpClock = -1;
        for (auto& g : gainNow) g = 0.0;
        smooth = dsp::tauCoef (fs, 0.01);
        dirty = true;
        setParams (params);
    }

    void setParams (const SoundParams& p)
    {
        const bool changed = dirty
            || p.tune[BD] != params.tune[BD] || p.tune[SD] != params.tune[SD] || p.tune[CP] != params.tune[CP]
            || p.tune[CH] != params.tune[CH] || p.tune[OH] != params.tune[OH] || p.tune[CY] != params.tune[CY]
            || p.cyTone != params.cyTone;
        params = p;
        bool anySolo = false;
        for (int i = 0; i < kNumVoices; ++i) anySolo |= p.solo[i];
        for (int i = 0; i < kNumVoices; ++i)
        {
            const bool on = ! p.mute[i] && (! anySolo || p.solo[i]);
            gainTarget[i] = on && p.lvl[i] > -69.9f ? std::pow (10.0, p.lvl[i] / 20.0) : 0.0;
            ownOut[i] = p.ownOut[i];
        }
        masterTarget = p.master > -69.9f ? std::pow (10.0, p.master / 20.0) : 0.0;
        accentAmt = p.accent / 100.0;

        // decay coefficients (cheap, so always)
        const auto df = [&] (int v) { return dsp::decayFactor (p.decay[v]); };
        bdFreq = 58.0 * dsp::semis (p.tune[BD]);
        // the bass drum's decay follows its pitch, like the original: higher = shorter
        bdR = dsp::tauCoef (fs, 0.075 * df (BD) * std::pow (58.0 / bdFreq, 0.8));
        sdFreq = 205.0 * dsp::semis (p.tune[SD]);
        sdR = dsp::tauCoef (fs, 0.022 * df (SD));
        sdSnap.coef  = dsp::tauCoef (fs, 0.033 * df (SD));
        chCoef = dsp::tauCoef (fs, 0.027 * df (CH));
        ohCoef = dsp::tauCoef (fs, 0.233 * df (OH));
        if (! ohChoked) ohEnv.coef = ohCoef;
        chEnv.coef  = chCoef;
        cyTing.coef = dsp::tauCoef (fs, 0.020 * df (CY));
        cyBody.coef = dsp::tauCoef (fs, 0.300 * df (CY));
        cyLow.coef  = dsp::tauCoef (fs, 0.467 * df (CY));
        cpShortCoef = dsp::tauCoef (fs, 0.0035);
        cpLastCoef  = dsp::tauCoef (fs, 0.047 * df (CP));
        cpTail.coef = dsp::tauCoef (fs, 0.233 * df (CP));
        accEnv.coef = dsp::tauCoef (fs, 0.040);
        bdWhack = p.bdAttack / 100.0 * 1.6;
        sdSnappy = p.sdSnappy / 100.0 * 1.6;
        cpTailAmt = p.cpTail / 100.0 * 0.9;
        const double tone = p.cyTone / 100.0;
        cyTingAmt = 0.3 + 1.4 * tone;  cyBodyAmt = 1.7 - 1.4 * tone;

        if (changed)
        {
            dirty = false;
            const double fsr = fs;
            bdPulseLp.set (260.0, fsr);
            metalCh.setTune (dsp::semis (p.tune[CH]), fsr);
            metalOh.setTune (dsp::semis (p.tune[OH]), fsr);
            metalCy.setTune (dsp::semis (p.tune[CY]), fsr);
            chF.set (7120.0 * dsp::semis (p.tune[CH] * 0.5), fsr);
            ohF.set (7120.0 * dsp::semis (p.tune[OH] * 0.5), fsr);
            cyHi.set (7120.0 * dsp::semis (p.tune[CY] * 0.5), fsr);
            cyLo.set (3450.0 * dsp::semis (p.tune[CY] * 0.5), fsr);
            sdSnapHp.set (1800.0 * dsp::semis (p.tune[SD] * 0.5), fsr);
            sdSnapLp.set (11000.0, fsr);
            cpBp.set (910.0 * dsp::semis (p.tune[CP]), 3.2, fsr);
            metalHp.set (120.0, fsr);
        }
    }

    // a hit from the keyboard or a pad (velocity 1..127; above 100 also fires the accent)
    void trigger (int voice, int velocity)
    {
        if (velocity > 100) fireAccent();
        hit (voice, dsp::vel (velocity));
    }

    // the sequencer reached `step`; rows are this pattern's masks (row 0 = accent). The hardware
    // has no velocity: every step plays at the same level, accented steps go through the accent.
    void sequencerStep (int step, const uint16_t* rows, bool seqOn)
    {
        if (! seqOn) return;
        if ((rows[0] >> step) & 1) fireAccent();
        for (int v = 0; v < kNumVoices; ++v)
            if ((rows[v + 1] >> step) & 1) hit (v, 1.0);
    }

    void setOwnOut (int voice, bool on) { ownOut[voice] = on; }

    // one sample of the full mix (every drum, times Volume)
    inline float tick()
    {
        float outs[kNumVoices];
        const bool keep[kNumVoices] = { ownOut[0], ownOut[1], ownOut[2], ownOut[3], ownOut[4], ownOut[5] };
        for (auto& o : ownOut) o = false;
        const float s = tick (outs);
        for (int i = 0; i < kNumVoices; ++i) ownOut[i] = keep[i];
        return s;
    }

    // one sample with routing: returns the main mix (drums on MAIN, times Volume);
    // outs[i] = drum i when it is on its own output (not affected by Volume), else 0
    inline float tick (float* outs)
    {
        double v[kNumVoices];
        render (v);

        // global accent: a short swell in level that also drives the output stage into distortion
        const double a = accEnv.tick() * accentAmt;
        const double gain = 1.0 + 1.6 * a;
        masterNow = masterTarget + smooth * (masterNow - masterTarget);

        double mainMix = 0.0;
        for (int i = 0; i < kNumVoices; ++i)
        {
            gainNow[i] = gainTarget[i] + smooth * (gainNow[i] - gainTarget[i]);
            double x = dcBlock[i].hp (v[i]) * gain;
            x = dsp::softClip (x * kHeadroom) / kHeadroom * kOutScale;   // the output VCA: near-clean normally, gritty when accented
            x *= gainNow[i];
            if (ownOut[i]) outs[i] = (float) x;
            else         { outs[i] = 0.0f; mainMix += x; }
        }
        return (float) (mainMix * masterNow);
    }

private:
    double fs = 48000.0;
    SoundParams params;
    bool dirty = true;

    dsp::ShiftNoise noise;
    // bass drum
    dsp::Ringer bdRing; dsp::Env bdEnvPitch; dsp::OnePole bdPulseLp;
    int bdPulseLeft = 0; double bdPulseLevel = 0, bdFreq = 58, bdR = 0.99, bdWhack = 0.8;
    // snare
    dsp::Ringer sdRing; dsp::Env sdSnap; dsp::OnePole sdSnapHp, sdSnapLp;
    int sdPulseLeft = 0; double sdPulseLevel = 0, sdFreq = 205, sdR = 0.99, sdSnappy = 1.0;
    // hats and cymbal
    dsp::Metal metalCh, metalOh, metalCy;
    dsp::MetalFilter chF, ohF, cyHi, cyLo;
    dsp::OnePole metalHp;
    dsp::Env chEnv, ohEnv, cyTing, cyBody, cyLow;
    double chCoef = 0.99, ohCoef = 0.999, cyTingAmt = 1, cyBodyAmt = 1;
    bool ohChoked = false;
    // clap
    dsp::Svf cpBp; dsp::Env cpHit, cpTail;
    int cpClock = -1; double cpLevel = 1, cpShortCoef = 0.99, cpLastCoef = 0.999, cpTailAmt = 0.45;
    // accent + output
    dsp::Env accEnv; double accentAmt = 0.5;
    static constexpr double kHeadroom = 0.7, kOutScale = 0.6;
    dsp::OnePole dcBlock[kNumVoices];
    double gainTarget[kNumVoices] {}, gainNow[kNumVoices] {}, masterTarget = 0.5, masterNow = 0.5, smooth = 0.999;
    bool ownOut[kNumVoices] {};

    void fireAccent() { accEnv.trigger (1.0); }

    void hit (int voice, double level)
    {
        switch (voice)
        {
            case BD: bdPulseLeft = (int) (fs * 0.001); bdPulseLevel = level; bdEnvPitch.coef = dsp::tauCoef (fs, 0.007); bdEnvPitch.trigger (1.0); break;
            case SD: sdPulseLeft = (int) (fs * 0.001); sdPulseLevel = level; sdSnap.trigger (level); break;
            case CP: cpClock = 0; cpLevel = level; break;
            case CH: chEnv.trigger (level); ohChoked = true; ohEnv.coef = dsp::tauCoef (fs, 0.004); break;   // closed hat cuts the open hat
            case OH: ohChoked = false; ohEnv.coef = ohCoef; ohEnv.trigger (level); break;
            case CY: cyTing.trigger (level); cyBody.trigger (level); cyLow.trigger (level * 0.45); break;
            default: break;
        }
    }

    inline void render (double* v)
    {
        const double n = noise.tick();

        // ---- bass drum: lowpassed trigger pulse rings the damped oscillator, and is heard itself as the "whack"
        double pulse = 0.0;
        if (bdPulseLeft > 0) { pulse = bdPulseLevel; --bdPulseLeft; }
        const double p = bdPulseLp.lp (pulse);
        const double bend = 1.0 + 0.12 * bdEnvPitch.tick();
        const double w = dsp::kTwoPi * bdFreq * bend / fs;
        const double body = bdRing.tick (p * (6.0 / (fs * 0.001)), bdR, w);
        v[BD] = dsp::softClip (body * 0.8) * 1.2 + p * bdWhack * 0.55;

        // ---- snare: the unfiltered trigger rings a higher damped oscillator; gated noise is the snap
        double sp = 0.0;
        if (sdPulseLeft > 0) { sp = sdPulseLevel; --sdPulseLeft; }
        const double sbody = sdRing.tick (sp * (1.6 / (fs * 0.001)), sdR, dsp::kTwoPi * sdFreq / fs);
        const double snap = sdSnapLp.lp (sdSnapHp.hp (n * sdSnap.tick()));
        v[SD] = sbody * 0.75 + sp * 0.12 + snap * sdSnappy * 0.9;

        // ---- hats: high metal through their VCAs
        const double chM = chF.tick (metalCh.tick (n));
        const double ohM = ohF.tick (metalOh.tick (n));
        const double che = chEnv.tick(), ohe = ohEnv.tick();
        // the swing VCA lets a little of the envelope itself through: a soft thump before the high-pass
        v[CH] = (chM * 10.0 + che * 0.04) * che;
        v[OH] = (ohM * 6.5 + ohe * 0.04) * ohe;

        // ---- cymbal: high metal "ting" + low metal body on two envelopes
        const double cm = metalCy.tick (n);
        const double hi = cyHi.tick (cm), lo = cyLo.tick (cm);
        v[CY] = hi * cyTing.tick() * cyTingAmt * 3.2 + lo * (cyBody.tick() + cyLow.tick()) * cyBodyAmt * 2.8;

        // ---- clap: three quick hits, a last longer one, and a "reverb" tail, through a 910 Hz band-pass
        if (cpClock >= 0)
        {
            const int ms10 = (int) (fs * 0.010);
            if (cpClock == 0 || cpClock == ms10 || cpClock == 2 * ms10) { cpHit.coef = cpShortCoef; cpHit.e = 0; cpHit.trigger (cpLevel); }
            if (cpClock == 3 * ms10) { cpHit.coef = cpLastCoef; cpHit.e = 0; cpHit.trigger (cpLevel); cpTail.trigger (cpLevel * cpTailAmt); cpClock = -2; }
            if (cpClock >= 0) ++cpClock;
        }
        double cl, cb;
        cpBp.tick (n * (cpHit.tick() + cpTail.tick()), cl, cb);
        v[CP] = cb * 1.5;
    }
};
} // namespace tross110
