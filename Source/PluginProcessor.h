#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/TrossEngine.h"

class Tross110Processor : public juce::AudioProcessor
{
public:
    Tross110Processor();
    ~Tross110Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties makeBuses();
    juce::AudioProcessorValueTreeState apvts;

    // ---- pattern memory / sequencer, shared with the editor
    tross110::PatternMemory patterns;
    int  getCurrentSlot() const { return currentSlot.load(); }   // the slot that plays and is edited
    int  getPendingSlot() const { return pendingSlot.load(); }   // queued slot, -1 = none
    int  getPlayStep() const    { return playStep.load(); }      // step being played, -1 = stopped
    void clearCurrentPattern()  { patterns.clear (currentSlot.load()); }

    // a plain click on a pattern button: that slot (queued until step 1 while playing)
    void selectSlot (int slot);

    // ---- pattern chain (message thread edits, audio thread plays). Locked to the song position:
    // chain step = (song position in steps / LENGTH) mod chain length, so jumps and loops land right.
    static constexpr int kMaxChain = 32;
    void chainAppend (int slot);                 // Shift-click a pattern button
    void chainClear();
    void chainRemoveLast();
    int  getChainLength() const          { return chainLen.load(); }
    int  getChainPos() const             { return chainPos.load(); }   // chain step that's playing
    bool isInChain (int slot) const      { for (int i = 0; i < chainLen.load(); ++i) if (chain[i].load() == slot) return true; return false; }
    std::vector<int> getChain() const    { std::vector<int> v; for (int i = 0; i < chainLen.load(); ++i) v.push_back (chain[i].load()); return v; }
    int  getChainVersion() const         { return chainVersion.load(); }

    // a click on a drum's name strip: plays it like a keyboard hit (accent = Shift-click, velocity 127)
    // and, with REC on while playing, records it. Called from the UI thread; handled in the next block.
    void panelHit (int voice, bool accent)
    {
        const int w = hitWrite.load (std::memory_order_relaxed);
        const int next = (w + 1) % kHitQueue;
        if (next == hitRead.load (std::memory_order_acquire)) return;   // queue full: drop the hit
        hitQueue[w] = { voice, accent ? 127 : 100 };
        hitWrite.store (next, std::memory_order_release);
    }

    // Standalone app only: there is no host transport, so it gets its own play button and tempo
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }
    std::atomic<bool>  internalPlay { false };
    std::atomic<float> internalBpm { 120.0f };

    // HELP key: help windows on/off (saved with the project; knob value read-outs always show)
    std::atomic<bool> showHelp { true };

    // the song tempo seen in the last audio block (used for MIDI export)
    std::atomic<double> currentBpm { 120.0 };

private:
    tross110::Engine engine;
    tross110::SoundParams sound;
    double sampleRate = 44100.0;

    std::atomic<int> currentSlot { 0 }, pendingSlot { -1 }, playStep { -1 };

    void setChain (const std::vector<int>&);
    int chainSlotFor (long long absStep, int len) const;   // -1 = no chain
    std::atomic<int> chain[kMaxChain] {};
    std::atomic<int> chainLen { 0 }, chainPos { -1 }, chainVersion { 0 };

    // single-producer / single-consumer queue of name-strip clicks (UI thread -> audio thread)
    static constexpr int kHitQueue = 64;
    struct PanelHit { int voice = 0, velocity = 100; };
    PanelHit hitQueue[kHitQueue];
    std::atomic<int> hitWrite { 0 }, hitRead { 0 };
    int lastPatternParam = 0;

    // transport following
    bool wasPlaying = false;
    juce::int64 lastSixteenth = 0;
    double internalPpq = 0.0;

    struct Raw
    {
        std::atomic<float>* lvl[tross110::kNumVoices];
        std::atomic<float>* tune[tross110::kNumVoices];
        std::atomic<float>* decay[tross110::kNumVoices];
        std::atomic<float>* mute[tross110::kNumVoices];
        std::atomic<float>* solo[tross110::kNumVoices];
        std::atomic<float>* ownOut[tross110::kNumVoices];
        std::atomic<float> *bdAttack, *sdSnappy, *cpTail, *cyTone, *accent, *master,
                           *seqOn, *seqLen, *pattern, *rec;
    } raw {};

    void pullParams();
    void doStep (int step, long long absStep, int len, bool seqOn);
    void recordHit (int voice, int velocity, double sixteenthPos, int len);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tross110Processor)
};
