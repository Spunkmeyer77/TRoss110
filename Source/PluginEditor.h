#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Slim dark drum-machine look: matte knobs on a charcoal case, grey rubber keys, an LCD step grid
class TrossLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TrossLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
};

// Help windows: shown only while HELP is on (the knob value read-outs are separate and always work)
class HelpTooltips : public juce::TooltipWindow
{
public:
    HelpTooltips (juce::Component* parent, Tross110Processor& p) : juce::TooltipWindow (parent, 600), proc (p) {}
    juce::String getTipFor (juce::Component& c) override { return proc.showHelp.load() ? juce::TooltipWindow::getTipFor (c) : juce::String(); }
private:
    Tross110Processor& proc;
};

// Lower section: step numbers and the 7 x 16 LCD grid. Click a cell to toggle, drag to paint.
class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (Tross110Processor& p) : proc (p) { setOpaque (true); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragRow = -1; }

    static juce::Rectangle<float> padRect (int row, int step);

private:
    Tross110Processor& proc;
    int dragRow = -1, lastDragStep = -1;
    bool dragValue = false;
    bool hit (juce::Point<float>, int& row, int& step) const;
};

// Parameter-lock screen, drawn over the step grid: choose a drum (left) and a knob (tabs below), then
// drag each step's bar to lock that knob's value for that step. Right-click / double-click removes a lock.
class LockGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LockGrid (Tross110Processor& p) : proc (p) { setOpaque (true); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; lastCol = -1; }
    void mouseDoubleClick (const juce::MouseEvent&) override;
    int voice = 0, param = 0;
private:
    Tross110Processor& proc;
    bool dragging = false, erasing = false;
    int lastCol = -1;
    int columnAt (float x) const;
    float valueAt (float y) const;
    void apply (int col, float y);
    static juce::Rectangle<float> tabRect (int i);
    static juce::Rectangle<float> voiceRect (int v);
};

// 12 pattern keys, 4 across x 3 down. LED = slot playing (blinks when queued), light cap = selected.
// A drum's black name strip, played like a pad: click = hit, Shift-click = accented hit
class NameStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    NameStrip (Tross110Processor& p, int v, const juce::String& t) : proc (p), voice (v), title (t)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isFlashing() const { return juce::Time::getMillisecondCounter() - lastHit < 180; }
    juce::uint32 lastHit = 0;
    bool lastAccent = false;
private:
    Tross110Processor& proc;
    int voice;
    juce::String title;
};

// EXPORT MIDI key: click = menu (this pattern / all in a row / all as separate files), drag = drop the
// playing pattern onto a DAW track as a MIDI clip
class ExportKey : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ExportKey (Tross110Processor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { pressed = true; dragged = false; repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    Tross110Processor& proc;
    bool pressed = false, dragged = false;
    std::unique_ptr<juce::FileChooser> chooser;
    void showMenu();
    void exportPlaying();
    void exportAllInARow();
    void exportAllSeparate();
    void exportChain (const std::vector<int>& chain);
    std::vector<int> nonEmptySlots() const;
    int length() const;
};

class PatternPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PatternPad (Tross110Processor& p) : proc (p) { setOpaque (true); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    static juce::Rectangle<float> buttonRect (int slot);
private:
    Tross110Processor& proc;
};

// LCD read-out in the master section: playing pattern, and the queued one
// About window: opens from the logo, closes on any click
class AboutBox : public juce::Component
{
public:
    AboutBox() { setInterceptsMouseClicks (true, true); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    static constexpr const char* kEmail = "ticcoross@gmail.com";
private:
    juce::Rectangle<float> panel() const;
    juce::Rectangle<float> emailRect;
};

class StatusLcd : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StatusLcd (Tross110Processor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    Tross110Processor& proc;
};

class Tross110Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Tross110Editor (Tross110Processor&);
    ~Tross110Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 1000, kHeight = 500;

private:
    // everything is laid out at 1000 x 500 here; the editor scales it
    struct Content : public juce::Component
    {
        explicit Content (Tross110Editor& e) : ed (e) { setOpaque (true); }
        void paint (juce::Graphics& g) override { ed.paintPanel (g); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (logoArea().contains (e.getPosition())) ed.showAbout();
            else if (e.getPosition().y < 50) ed.toggleSize();   // click the top strip: 100 % <-> smaller
        }
        void mouseMove (const juce::MouseEvent& e) override
        {
            setMouseCursor (logoArea().contains (e.getPosition()) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        }
        static juce::Rectangle<int> logoArea() { return { 14, 8, 130, 40 }; }
        Tross110Editor& ed;
    };

    Tross110Processor& proc;
    TrossLookAndFeel lnf;
    Content content { *this };
    StepGrid grid { proc };
    LockGrid lockGrid { proc };
    juce::TextButton lockBtn { "P-LOCK" };
    PatternPad patternPad { proc };
    StatusLcd lcd { proc };
    std::vector<std::unique_ptr<NameStrip>> strips;
    ExportKey exportKey { proc };

    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::Button>> buttons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtts;
    juce::TextButton clearBtn { "CLEAR" };
    // standalone app only
    juce::TextButton playBtn { "PLAY" };
    juce::Slider bpmBox;
    HelpTooltips tooltips { this, proc };
    juce::TextButton helpBtn { "HELP" };
    AboutBox about;
public:
    void showAbout() { about.setVisible (true); about.toFront (false); }
    void toggleSize()
    {
        if (getWidth() != kWidth) setSize (kWidth, kHeight);              // back to 100 %
        else                      setSize (kWidth * 3 / 4, kHeight * 3 / 4);   // smaller (75 %)
    }
private:

    int lastVersion = -1, lastStep = -2, lastSlot = -1, lastPending = -2, lastLen = -1, lastSel = -1, lastChain = -1;

    juce::Slider& addKnob (const juce::String& paramId, juce::Point<int> centre, int diameter);
    juce::Button& addButton (const juce::String& paramId, const juce::String& text, juce::Rectangle<int> r, const char* style);
    void paintPanel (juce::Graphics&);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tross110Editor)
};
