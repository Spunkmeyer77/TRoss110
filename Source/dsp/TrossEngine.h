// TRoss 110 engine: six analog-style drum voices, the global accent, the pattern memory and the
// sequencer hooks. No JUCE in here, so it can be tested on its own (tools/render_test.cpp).
#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>

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
    //                       BD     SD     CP    CH  OH  CY
    float lvl[kNumVoices]   { -8.4f, 0,    -3.6f, 0, 0, 0 };    // dB, -70..6
    float tune[kNumVoices]  { 0,     0,     2.6f, 0, 0, 0 };    // semitones
    float decay[kNumVoices] { 24.1f, 43.9f, 20.3f, 50, 50, 50 }; // %, 50 = the original timing
    float bdAttack = 50, sdSnappy = 60, cpTail = 25.2f, cyTone = 50;
    bool mute[kNumVoices] {}, solo[kNumVoices] {};
    bool ownOut[kNumVoices] {};   // true = the drum leaves the main mix and plays on its own output
};

// ---- parameter locks: per pattern, per drum, per step, 4 lockable knobs
enum LockParam { LTune, LDecay, LLevel, LExtra, kNumLockParams };
inline bool hasExtra (int voice) { return voice != CH && voice != OH; }
inline float lockMin (int param) { return param == LTune ? -12.0f : param == LLevel ? -70.0f : 0.0f; }
inline float lockMax (int param) { return param == LTune ? 12.0f : param == LLevel ? 6.0f : 100.0f; }
inline bool isLock (float v) { return v == v; }   // NaN = no lock
constexpr float kNoLock = std::numeric_limits<float>::quiet_NaN();

// the knob a lock replaces, in the sound parameters
inline float& knobRef (SoundParams& p, int voice, int param)
{
    switch (param)
    {
        case LTune:  return p.tune[voice];
        case LDecay: return p.decay[voice];
        case LLevel: return p.lvl[voice];
        default:     return voice == BD ? p.bdAttack : voice == SD ? p.sdSnappy : voice == CP ? p.cpTail : p.cyTone;
    }
}
inline float knobValue (const SoundParams& p, int voice, int param) { return knobRef (const_cast<SoundParams&> (p), voice, param); }

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
    PatternMemory()
    {
        for (auto& a : locks) for (auto& b : a) for (auto& c : b) for (auto& d : c) d.store (kNoLock);
    }

    // locks: NaN = none
    float getLock (int slot, int voice, int param, int step) const { return locks[slot][voice][param][step].load (std::memory_order_relaxed); }
    void setLock (int slot, int voice, int param, int step, float v) { locks[slot][voice][param][step].store (v, std::memory_order_relaxed); ++version; }
    void clearLock (int slot, int voice, int param, int step)       { setLock (slot, voice, param, step, kNoLock); }
    bool stepHasLock (int slot, int voice, int step) const
    {
        for (int p = 0; p < kNumLockParams; ++p) if (isLock (getLock (slot, voice, p, step))) return true;
        return false;
    }
    bool hasAnyLock (int slot) const
    {
        for (int v = 0; v < kNumVoices; ++v) for (int s = 0; s < kNumSteps; ++s) if (stepHasLock (slot, v, s)) return true;
        return false;
    }
    void clearLocks (int slot, int voice, int param)   // one drum's lane; voice/param -1 = all
    {
        for (int v = 0; v < kNumVoices; ++v) for (int p = 0; p < kNumLockParams; ++p)
            if ((voice < 0 || v == voice) && (param < 0 || p == param))
                for (int s = 0; s < kNumSteps; ++s) locks[slot][v][p][s].store (kNoLock, std::memory_order_relaxed);
        ++version;
    }

    void clear (int slot) { for (int r = 0; r < kNumRows; ++r) set (slot, r, 0); clearLocks (slot, -1, -1); }
    void copySlot (int from, int to)
    {
        for (int r = 0; r < kNumRows; ++r) set (to, r, get (from, r));
        for (int v = 0; v < kNumVoices; ++v) for (int p = 0; p < kNumLockParams; ++p) for (int s = 0; s < kNumSteps; ++s)
            locks[to][v][p][s].store (getLock (from, v, p, s), std::memory_order_relaxed);
        ++version;
    }
    bool isEmpty (int slot) const
    {
        for (int r = 1; r < kNumRows; ++r)   // the accent row alone plays nothing
            if (get (slot, r) != 0) return false;
        return true;
    }
    int getVersion() const { return version.load(); }

private:
    std::atomic<uint16_t> masks[kNumSlots][kNumRows] {};
    std::atomic<float> locks[kNumSlots][kNumVoices][kNumLockParams][kNumSteps];
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
        chF = {}; ohF = {}; cyHi = {}; cyLo = {}; cpBp.reset(); cpTailBp.reset(); cpTailLp = {};
        for (auto& h : dcBlock) { h = {}; h.set (18.0, fs); }
        bdPulseLp = {}; sdSnapHp = {}; sdSnapLp = {}; metalHp = {};
        bdPulseLeft = sdPulseLeft = 0;
        cpClock = -1;
        for (auto& g : gainNow) g = 0.0;
        smooth = dsp::tauCoef (fs, 0.01);
        dirty = true;
        apply();
    }

    void setParams (const SoundParams& p) { knobs = p; apply(); }

    // locks for one drum's next sound (NaN = use the knob); they hold until that drum's next hit
    void setVoiceLocks (int voice, const float* vals)
    {
        bool diff = false;
        for (int q = 0; q < kNumLockParams; ++q)
            if (isLock (vals[q]) != isLock (lockVal[voice][q]) || (isLock (vals[q]) && vals[q] != lockVal[voice][q])) diff = true;
        if (! diff) return;
        for (int q = 0; q < kNumLockParams; ++q) lockVal[voice][q] = vals[q];
        apply();
        gainNow[voice] = gainTarget[voice];   // a locked level starts with the hit, not ramped
    }
    void clearVoiceLocks (int voice) { const float none[kNumLockParams] { kNoLock, kNoLock, kNoLock, kNoLock }; setVoiceLocks (voice, none); }

private:
    // the knobs with this hit's locks on top
    void apply()
    {
        SoundParams p = knobs;
        for (int v = 0; v < kNumVoices; ++v)
            for (int q = 0; q < kNumLockParams; ++q)
                if (isLock (lockVal[v][q]) && (q != LExtra || hasExtra (v))) knobRef (p, v, q) = lockVal[v][q];
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
        cpTail.coef = dsp::tauCoef (fs, 0.30 * df (CP));   // RC discharge; the VCA cuts it off well before it ends
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
            cpTailBp.set (820.0 * dsp::semis (p.tune[CP]), 2.0, fsr);   // the tail: a little lower and wider
            cpTailLp.set (3200.0 * dsp::semis (p.tune[CP] * 0.5), fsr);  // and darker
            metalHp.set (120.0, fsr);
        }
    }

public:

    // a hit from the keyboard or a pad (velocity 1..127; above 100 also fires the accent)
    void trigger (int voice, int velocity)
    {
        clearVoiceLocks (voice);   // live hits play the knobs
        if (velocity > 100) fireAccent();
        hit (voice, dsp::vel (velocity));
    }

    // the sequencer reached `step`; rows are this pattern's masks (row 0 = accent). The hardware
    // has no velocity: every step plays at the same level, accented steps go through the accent.
    // locks (optional) = this step's values, [voice][param], NaN = none.
    void sequencerStep (int step, const uint16_t* rows, bool seqOn, const float (*locks)[kNumLockParams] = nullptr)
    {
        if (! seqOn) return;
        if ((rows[0] >> step) & 1) fireAccent();
        for (int v = 0; v < kNumVoices; ++v)
            if ((rows[v + 1] >> step) & 1)
            {
                if (locks != nullptr) setVoiceLocks (v, locks[v]);
                else                  clearVoiceLocks (v);
                hit (v, 1.0);
            }
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
    SoundParams knobs, params;   // params = knobs + locks
    float lockVal[kNumVoices][kNumLockParams] { { kNoLock, kNoLock, kNoLock, kNoLock }, { kNoLock, kNoLock, kNoLock, kNoLock },
                                                { kNoLock, kNoLock, kNoLock, kNoLock }, { kNoLock, kNoLock, kNoLock, kNoLock },
                                                { kNoLock, kNoLock, kNoLock, kNoLock }, { kNoLock, kNoLock, kNoLock, kNoLock } };
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
    dsp::Svf cpBp, cpTailBp; dsp::OnePole cpTailLp; dsp::Env cpHit, cpTail;
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
            case CP: cpClock = 0; cpLevel = level; cpTail.trigger (level); break;   // the tail envelope starts with the hits
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
            if (cpClock == 3 * ms10) { cpHit.coef = cpLastCoef; cpHit.e = 0; cpHit.trigger (cpLevel); cpClock = -2; }
            if (cpClock >= 0) ++cpClock;
        }
        double cl, cb;
        // transistor VCA: no output below its threshold, so the tail stops short ("gated") instead of fading out
        auto vca = [] (double e, double th) { const double x = e - th; return x > 0.0 ? x * x / ((1.0 - th) * (1.0 - th)) : 0.0; };
        const double hitE = cpHit.tick(), tailE = cpTail.tick();
        cpBp.tick (n * hitE, cl, cb);
        double tl, tb;
        cpTailBp.tick (n * vca (tailE / (cpLevel > 0.0 ? cpLevel : 1.0), 0.22) * cpLevel, tl, tb);
        cb += cpTailLp.lp (tb) * cpTailAmt * 3.2;
        v[CP] = cb * 1.5;
    }
};
} // namespace tross110
