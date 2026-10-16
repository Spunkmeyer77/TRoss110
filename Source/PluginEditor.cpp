#include "PluginEditor.h"
#include "MidiExport.h"

using namespace tross110;

namespace
{
namespace col
{
const juce::Colour caseTop     { 0xff313234 }, caseBottom { 0xff222325 };
const juce::Colour lowerTop    { 0xff26272a }, lowerBottom { 0xff1a1b1d };
const juce::Colour print       { 0xffcfd1cc };   // printed labels
const juce::Colour softPrint   { 0xff8d908b };
const juce::Colour blue        { 0xff6fb3d2 };   // the panel's accent colour
const juce::Colour blueDark    { 0xff3f7f9e };
const juce::Colour red         { 0xffff3b2f };
const juce::Colour ledOff      { 0xff3a1512 };
const juce::Colour stripBg     { 0xff17181a };
const juce::Colour stripText   { 0xffe8eae4 };
const juce::Colour key1        { 0xffb9bcb7 }, key2 { 0xff969994 };   // grey rubber keys
const juce::Colour keyHi1      { 0xffe4e6e1 }, keyHi2 { 0xffc4c7c1 };
const juce::Colour blueKey1    { 0xff7fbcd9 }, blueKey2 { 0xff5a98b8 };
const juce::Colour lcdBg1      { 0xffbcc5a8 }, lcdBg2 { 0xffa6b092 };
const juce::Colour lcdInk      { 0xff1c2318 };
const juce::Colour muteOn      { 0xffffcf4a };
}

juce::Font font (float size, bool bold = false, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
}

void drawLed (juce::Graphics& g, juce::Point<float> c, float d, bool lit, juce::Colour on = col::red)
{
    if (lit)
    {
        g.setGradientFill (juce::ColourGradient (on.withAlpha (0.45f), c, on.withAlpha (0.0f), c.translated (d * 1.0f, 0), true));
        g.fillEllipse (juce::Rectangle<float> (d * 2.0f, d * 2.0f).withCentre (c));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffe9d6), c, on.darker (0.2f), c.translated (d * 0.5f, 0), true));
    }
    else
        g.setColour (col::ledOff);
    g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (c));
}

// rubber key: soft vertical gradient on a dark well
void drawKey (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour top, juce::Colour bottom, float radius, float shadow)
{
    g.setColour (juce::Colour (0xff0c0c0d));
    g.fillRoundedRectangle (r.expanded (1.0f).translated (0, shadow * 0.5f), radius + 1.0f);
    g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
}

// LCD glass with a slight vertical sheen
void drawLcd (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff0d0e0f));
    g.fillRoundedRectangle (r.expanded (3.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient::vertical (col::lcdBg1, r.getY(), col::lcdBg2, r.getBottom()));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);
}

juce::Font lcdFont (float size) { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold)); }

const char* kVoiceIds[kNumVoices]    = { "bd", "sd", "cp", "ch", "oh", "cy" };
const char* kVoiceTitles[kNumVoices] = { "BASS DRUM", "SNARE", "HAND CLAP", "CLOSED HAT", "OPEN HAT", "CYMBAL" };
const char* kVoiceNotes[kNumVoices]  = { "C1", "D1", "D#1", "F#1", "A#1", "C#2" };
const char* kExtraIds[kNumVoices]    = { "bd_attack", "sd_snappy", "cp_tail", nullptr, nullptr, "cy_tone" };
const char* kExtraNames[kNumVoices]  = { "ATTACK", "SNAPPY", "TAIL", nullptr, nullptr, "TONE" };

const char* kTuneTips[kNumVoices] = {
    "Bass drum pitch, +/-12 semitones. Higher also makes the kick shorter, like the original circuit.",
    "Snare drum pitch, +/-12 semitones (moves the snap's brightness with it).",
    "Hand clap pitch: moves the clap's band-pass filter, +/-12 semitones.",
    "Closed hat pitch: moves the whole metallic cluster, +/-12 semitones.",
    "Open hat pitch: moves the whole metallic cluster, +/-12 semitones.",
    "Cymbal pitch: moves the whole metallic cluster, +/-12 semitones." };
const char* kExtraTips[kNumVoices] = {
    "The click at the front of the kick.",
    "How much noise snap against the drum body.",
    "Level of the clap's reverb-like tail after the burst of hits.",
    nullptr, nullptr,
    "Right: bright attack shimmer. Left: darker ringing body." };

// ---- layout (1000 x 500)
constexpr int kMargin = 18, kChanW = 128, kTopH = 262, kSeqY = 266;
constexpr int kKnobRow1 = 98, kKnobRow2 = 152, kKnobD = 34;
constexpr int kMasterX = 802, kMasterW = 180;
int chanX (int v) { return kMargin + v * kChanW; }

// lower section, in StepGrid coordinates (the grid sits at 0, kSeqY, 760 wide)
constexpr float kGridX = 74.0f, kColPitch = 41.5f, kCellW = 33.0f, kCellH = 15.0f, kRowPitch = 22.0f;
constexpr float kLcdTop = 30.0f, kRowsTop = kLcdTop + 16.0f;
} // namespace

// ================================================================================================
TrossLookAndFeel::TrossLookAndFeel()
{
    setColour (juce::BubbleComponent::backgroundColourId, col::lcdBg1);
    setColour (juce::BubbleComponent::outlineColourId, juce::Colours::black);
    setColour (juce::TooltipWindow::textColourId, col::lcdInk);
}

void TrossLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                         juce::Slider& s)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();

    if (s.getProperties().contains ("seg"))   // LCD number read-out (LENGTH, BPM)
    {
        drawLcd (g, area.reduced (2.0f));
        const auto txt = s.getTextFromValue (s.getValue());
        const float size = (float) s.getProperties().getWithDefault ("segSize", 18.0f);
        g.setFont (lcdFont (size));
        g.setColour (col::lcdInk.withAlpha (0.25f));
        g.drawText (txt, area.translated (0.8f, 0.8f), juce::Justification::centred);
        g.setColour (col::lcdInk);
        g.drawText (txt, area, juce::Justification::centred);
        return;
    }

    // matte grey cap on a dark collar, with a printed scale and a white line
    const float d = juce::jmin (area.getWidth(), area.getHeight()) - 6.0f;
    const auto c = area.getCentre();
    auto body = juce::Rectangle<float> (d, d).withCentre (c);

    // scale ticks
    g.setColour (col::softPrint);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (float) i / 10.0f * (end - start);
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (d * 0.5f + 2.0f, a), c.getPointOnCircumference (d * 0.5f + (i % 5 == 0 ? 5.0f : 3.5f), a)), 1.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (body.expanded (1.5f).translated (0, 1.5f));
    g.setColour (juce::Colour (0xff121314));
    g.fillEllipse (body.expanded (1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6c6e), c.x - d * 0.2f, c.y - d * 0.3f,
                                             juce::Colour (0xff2a2b2d), c.x + d * 0.4f, c.y + d * 0.5f, true));
    g.fillEllipse (body.reduced (1.0f));
    g.setColour (juce::Colour (0xff4b4d4f));
    g.fillEllipse (body.reduced (d * 0.2f));

    const float angle = start + pos * (end - start);
    juce::Path p;
    p.startNewSubPath (c.getPointOnCircumference (d * 0.5f - 2.0f, angle));
    p.lineTo (c.getPointOnCircumference (d * 0.12f, angle));
    g.setColour (juce::Colour (0xfff4f5f0));
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void TrossLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);

    if (style == "small")   // M / S / OWN OUT on the upper panel
    {
        const auto onColour = juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["onColour"]);
        r = r.translated (0, down ? 1.0f : 0.0f);
        drawKey (g, r, on ? onColour.brighter (0.15f) : (highlighted ? col::key1.brighter (0.08f) : col::key1),
                 on ? onColour.darker (0.15f) : col::key2, 3.0f, down ? 0.5f : 2.0f);
        return;
    }

    // keys on the lower section (RUN, CLEAR, REC, PLAY)
    r = r.withTrimmedBottom (1.0f).translated (0, down ? 1.0f : 0.0f);
    const bool blue = style == "blue";
    drawKey (g, r, blue ? (highlighted ? col::blueKey1.brighter (0.08f) : col::blueKey1) : (highlighted ? col::key1.brighter (0.08f) : col::key1),
             blue ? col::blueKey2 : col::key2, 4.0f, down ? 0.5f : 2.0f);

    if (b.getProperties().contains ("led"))
        drawLed (g, { r.getX() + 11.0f, r.getCentreY() }, 6.0f, on);
}

void TrossLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    auto r = b.getLocalBounds().translated (0, down ? 1 : 0);
    if (style == "small")
    {
        g.setColour (juce::Colour (0xff222324));
        g.setFont (font (8.0f, true, 0.06f));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
        return;
    }
    if (b.getProperties().contains ("led")) r = r.withTrimmedLeft (14);
    g.setColour (juce::Colour (0xff1e1f20));
    g.setFont (font (9.0f, true, 0.08f));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void TrossLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    drawLcd (g, body.reduced (2.0f));
}

juce::Font TrossLookAndFeel::getSliderPopupFont (juce::Slider&) { return lcdFont (12.0f); }

static juce::TextLayout tipLayout (const juce::String& text, float maxW)
{
    juce::AttributedString a;
    a.setJustification (juce::Justification::topLeft);
    a.append (text, font (12.0f), col::lcdInk);
    juce::TextLayout l;
    l.createLayoutWithBalancedLineLengths (a, maxW);
    return l;
}

juce::Rectangle<int> TrossLookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> area)
{
    const auto l = tipLayout (tip, 300.0f);
    const int w = (int) l.getWidth() + 22, h = (int) l.getHeight() + 16;
    return juce::Rectangle<int> (pos.x > area.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > area.getCentreY() ? pos.y - (h + 6)  : pos.y + 8, w, h).constrainedWithin (area);
}

void TrossLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const juce::Rectangle<float> r (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (juce::Colour (0xff0d0e0f));
    g.fillRect (r);
    g.setGradientFill (juce::ColourGradient::vertical (col::lcdBg1, 0.0f, col::lcdBg2, (float) height));
    g.fillRect (r.reduced (2.0f));
    tipLayout (text, 300.0f).draw (g, r.reduced (11.0f, 8.0f));
}

// ================================================================================================
juce::Rectangle<float> StepGrid::padRect (int row, int step)
{
    return { kGridX + (float) step * kColPitch, kRowsTop + (float) row * kRowPitch, kCellW, kCellH };
}

void StepGrid::paint (juce::Graphics& g)
{
    static const char* rowNames[kNumRows] = { "AC", "BD", "SD", "HC", "CH", "OH", "CY" };

    g.setGradientFill (juce::ColourGradient::vertical (col::lowerTop, 0.0f, col::lowerBottom, (float) getHeight()));
    g.fillAll();

    const int slot = proc.getCurrentSlot();
    const int playing = proc.getPlayStep();
    const int len = (int) proc.apvts.getRawParameterValue ("seqlen")->load();

    // printed step numbers above the glass
    for (int s = 0; s < kNumSteps; ++s)
    {
        const auto x = kGridX + (float) s * kColPitch;
        g.setColour (s % 4 == 0 ? col::blue : col::softPrint);
        g.setFont (font (9.5f, s % 4 == 0));
        g.drawText (juce::String (s + 1), juce::Rectangle<float> (x, 10.0f, kCellW, 12.0f), juce::Justification::centred);
    }

    const juce::Rectangle<float> glass (kGridX - 8.0f, kLcdTop, 16 * kColPitch - (kColPitch - kCellW) + 16.0f,
                                        (float) kNumRows * kRowPitch + 22.0f);
    drawLcd (g, glass);

    // segment "ghosts", group lines and the playing-step arrow
    for (int s = 0; s < kNumSteps; ++s)
    {
        const auto x = kGridX + (float) s * kColPitch;
        if (s > 0 && s % 4 == 0)
        {
            g.setColour (col::lcdInk.withAlpha (0.35f));
            g.fillRect (x - (kColPitch - kCellW) * 0.5f - 0.5f, glass.getY() + 4.0f, 1.0f, glass.getHeight() - 8.0f);
        }
        juce::Path tri;
        const float cx = x + kCellW * 0.5f, ty = kLcdTop + 4.0f;
        tri.addTriangle (cx - 5.0f, ty, cx + 5.0f, ty, cx, ty + 7.0f);
        g.setColour (col::lcdInk.withAlpha (s == playing ? 0.95f : 0.07f));
        g.fillPath (tri);
    }

    for (int row = 0; row < kNumRows; ++row)
    {
        g.setColour (row == 0 ? col::blue : col::print);
        g.setFont (font (10.0f, true, 0.08f));
        g.drawText (rowNames[row], juce::Rectangle<float> ((float) kMargin, kRowsTop + (float) row * kRowPitch, 44.0f, kCellH),
                    juce::Justification::centredLeft);

        for (int s = 0; s < kNumSteps; ++s)
        {
            auto r = padRect (row, s);
            const bool on = proc.patterns.cell (slot, row, s);
            const bool active = s < len;
            if (on)
            {
                g.setColour (col::lcdInk.withAlpha (active ? 0.92f : 0.35f));
                g.fillRect (r.reduced (1.0f, 2.0f));
                g.setColour (col::lcdInk.withAlpha (0.18f));
                g.fillRect (r.reduced (1.0f, 2.0f).translated (1.2f, 1.2f));
            }
            else
            {
                g.setColour (col::lcdInk.withAlpha (active ? 0.07f : 0.03f));
                g.fillRect (r.reduced (1.0f, 2.0f));
                g.setColour (col::lcdInk.withAlpha (active ? 0.35f : 0.12f));
                g.fillRect (juce::Rectangle<float> (4.0f, 2.0f).withCentre (r.getCentre()));
            }
            if (s == playing && active)
            {
                g.setColour (col::lcdInk.withAlpha (0.55f));
                g.drawRect (r.expanded (1.0f, 0.0f), 1.0f);
            }
        }
    }
}

bool StepGrid::hit (juce::Point<float> p, int& row, int& step) const
{
    for (row = 0; row < kNumRows; ++row)
        for (step = 0; step < kNumSteps; ++step)
            if (padRect (row, step).expanded ((kColPitch - kCellW) * 0.5f, (kRowPitch - kCellH) * 0.5f).contains (p)) return true;
    return false;
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    int row, step;
    if (! hit (e.position, row, step)) return;
    const int slot = proc.getCurrentSlot();
    dragValue = ! proc.patterns.cell (slot, row, step);
    dragRow = row; lastDragStep = step;
    proc.patterns.setCell (slot, row, step, dragValue);
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0) return;
    int row, step;
    if (! hit (e.position, row, step) || row != dragRow || step == lastDragStep) return;
    lastDragStep = step;
    proc.patterns.setCell (proc.getCurrentSlot(), row, step, dragValue);
    repaint();
}

// ================================================================================================
void NameStrip::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float t = juce::jlimit (0.0f, 1.0f, (float) (juce::Time::getMillisecondCounter() - lastHit) / 180.0f);
    const auto flash = lastAccent ? juce::Colour (0xffffffff) : col::blue;
    const auto bg = lastHit != 0 && t < 1.0f ? flash.interpolatedWith (col::stripBg, t) : col::stripBg;
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (bg);
    g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);
    g.setColour (bg.getPerceivedBrightness() > 0.5f ? col::stripBg : col::stripText);
    g.setFont (font (10.0f, true, 0.15f));
    g.drawText (title, getLocalBounds(), juce::Justification::centred);
}

void NameStrip::mouseDown (const juce::MouseEvent& e)
{
    lastAccent = e.mods.isShiftDown();
    proc.panelHit (voice, lastAccent);
    lastHit = juce::Time::getMillisecondCounter();
    repaint();
}

// ================================================================================================
void ExportKey::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (1.0f).translated (0, pressed ? 1.0f : 0.0f);
    drawKey (g, r, isMouseOver() ? col::key1.brighter (0.08f) : col::key1, col::key2, 4.0f, pressed ? 0.5f : 2.0f);
    g.setColour (juce::Colour (0xff1e1f20));
    g.setFont (font (8.5f, true, 0.04f));
    g.drawText ("EXPORT MIDI", r, juce::Justification::centred);
}

int ExportKey::length() const { return juce::jlimit (1, kNumSteps, (int) proc.apvts.getRawParameterValue ("seqlen")->load()); }

std::vector<int> ExportKey::nonEmptySlots() const
{
    std::vector<int> slots;
    for (int s = 0; s < kNumSlots; ++s)
        if (! MidiExport::isEmpty (proc.patterns, s)) slots.push_back (s);
    return slots;
}

void ExportKey::mouseDrag (const juce::MouseEvent& e)
{
    if (dragged || e.getDistanceFromDragStart() < 6) return;
    dragged = true; pressed = false; repaint();
    const int slot = proc.getCurrentSlot();
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("TRoss110");
    dir.createDirectory();
    const auto file = dir.getChildFile (MidiExport::fileName (slot));
    if (MidiExport::write (MidiExport::make (proc.patterns, { slot }, length(), proc.currentBpm.load(),
                                             "TRoss 110 Pattern " + juce::String (slot + 1)), file))
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void ExportKey::mouseUp (const juce::MouseEvent& e)
{
    const bool wasClick = pressed && ! dragged && getLocalBounds().contains (e.getPosition());
    pressed = false; repaint();
    if (wasClick) showMenu();
}

void ExportKey::showMenu()
{
    const int slot = proc.getCurrentSlot();
    const bool playingEmpty = MidiExport::isEmpty (proc.patterns, slot);
    const auto filled = nonEmptySlots();
    juce::PopupMenu m;
    m.addSectionHeader ("Export as MIDI (" + juce::String (length()) + " steps, "
                        + juce::String (juce::roundToInt (proc.currentBpm.load())) + " BPM)");
    m.addItem (1, "Pattern " + juce::String (slot + 1) + " (playing)" + (playingEmpty ? " - empty" : ""), ! playingEmpty);
    m.addItem (2, "All patterns, in a row (" + juce::String ((int) filled.size()) + " with notes)", ! filled.empty());
    m.addItem (3, "All patterns as separate files", ! filled.empty());
    const auto chain = proc.getChain();
    m.addItem (5, "The chain, in a row (" + juce::String ((int) chain.size()) + " steps)", ! chain.empty());
    m.addSeparator();
    m.addItem (4, "Tip: drag this key onto a MIDI track to drop the playing pattern as a clip", false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [safe = juce::Component::SafePointer<ExportKey> (this), chain] (int id)
                     {
                         if (safe == nullptr) return;
                         if (id == 5) safe->exportChain (chain);
                         if (id == 1) safe->exportPlaying();
                         if (id == 2) safe->exportAllInARow();
                         if (id == 3) safe->exportAllSeparate();
                     });
}

static juce::File exportStartFolder()
{
    auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    return docs.isDirectory() ? docs : juce::File::getSpecialLocation (juce::File::userHomeDirectory);
}

void ExportKey::exportPlaying()
{
    const int slot = proc.getCurrentSlot();
    auto midi = MidiExport::make (proc.patterns, { slot }, length(), proc.currentBpm.load(),
                                  "TRoss 110 Pattern " + juce::String (slot + 1));
    chooser = std::make_unique<juce::FileChooser> ("Save pattern as MIDI", exportStartFolder().getChildFile (MidiExport::fileName (slot)), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
                          [midi] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f != juce::File()) MidiExport::write (midi, f.withFileExtension ("mid"));
                          });
}

void ExportKey::exportAllInARow()
{
    auto midi = MidiExport::make (proc.patterns, nonEmptySlots(), length(), proc.currentBpm.load(), "TRoss 110 Patterns");
    chooser = std::make_unique<juce::FileChooser> ("Save all patterns as one MIDI file", exportStartFolder().getChildFile ("TRoss110 Patterns.mid"), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
                          [midi] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f != juce::File()) MidiExport::write (midi, f.withFileExtension ("mid"));
                          });
}

void ExportKey::exportChain (const std::vector<int>& chain)
{
    auto midi = MidiExport::make (proc.patterns, chain, length(), proc.currentBpm.load(), "TRoss 110 Chain");
    chooser = std::make_unique<juce::FileChooser> ("Save the chain as one MIDI file", exportStartFolder().getChildFile ("TRoss110 Chain.mid"), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
                          [midi] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f != juce::File()) MidiExport::write (midi, f.withFileExtension ("mid"));
                          });
}

void ExportKey::exportAllSeparate()
{
    std::vector<std::pair<juce::String, juce::MidiFile>> files;
    for (int s : nonEmptySlots())
        files.push_back ({ MidiExport::fileName (s),
                           MidiExport::make (proc.patterns, { s }, length(), proc.currentBpm.load(), "TRoss 110 Pattern " + juce::String (s + 1)) });
    chooser = std::make_unique<juce::FileChooser> ("Choose a folder for the MIDI files", exportStartFolder());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [files] (const juce::FileChooser& fc)
                          {
                              const auto dir = fc.getResult();
                              if (dir.isDirectory())
                                  for (auto& [name, midi] : files) MidiExport::write (midi, dir.getChildFile (name));
                          });
}

// ================================================================================================
// PatternPad sits at (760, kSeqY), 240 x 108
juce::Rectangle<float> PatternPad::buttonRect (int slot)
{
    return { 12.0f + (float) (slot % 4) * 54.0f, 28.0f + (float) (slot / 4) * 26.0f, 48.0f, 20.0f };
}

void PatternPad::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient::vertical (col::lowerTop, 0.0f, col::lowerBottom.interpolatedWith (col::lowerTop, 0.55f), (float) getHeight()));
    g.fillAll();

    g.setColour (col::softPrint);
    g.setFont (font (8.5f, false, 0.15f));
    g.drawText ("PATTERN", 12, 10, 210, 12, juce::Justification::centredLeft);

    const int sel = (int) proc.apvts.getRawParameterValue ("pattern")->load();
    const int cur = proc.getCurrentSlot(), pending = proc.getPendingSlot();
    const bool blink = (juce::Time::getMillisecondCounter() / 200) % 2 == 0;

    for (int i = 0; i < kNumSlots; ++i)
    {
        const auto r = buttonRect (i);
        if (i == sel) drawKey (g, r, col::keyHi1, col::keyHi2, 4.0f, 2.0f);
        else          drawKey (g, r, col::key1.darker (0.15f), col::key2.darker (0.15f), 4.0f, 2.0f);
        const juce::Point<float> ledC { r.getX() + 8.0f, r.getCentreY() };
        if (proc.getChainLength() > 0 && proc.isInChain (i) && i != cur)   // in the chain: dim LED; the playing one is bright
        {
            g.setColour (col::red.withAlpha (0.45f));
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (ledC));
        }
        else
            drawLed (g, ledC, 5.0f, i == cur || (i == pending && blink));
        if (proc.patterns.isEmpty (i))   // small dash for empty slots
        {
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.fillRect (r.getRight() - 9.0f, r.getBottom() - 5.0f, 5.0f, 1.5f);
        }
        g.setColour (juce::Colour (0xff1e1f20));
        g.setFont (font (10.0f, true));
        g.drawText (juce::String (i + 1), r.withTrimmedLeft (8.0f), juce::Justification::centred);
    }
}

void PatternPad::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kNumSlots; ++i)
        if (buttonRect (i).expanded (2.0f).contains (e.position))
        {
            if (e.mods.isPopupMenu())   // right-click: copy, clear, chain
            {
                juce::PopupMenu copyTo;
                for (int s = 0; s < kNumSlots; ++s) copyTo.addItem (100 + s, "Pattern " + juce::String (s + 1), s != i);
                juce::PopupMenu m;
                m.addSectionHeader ("Pattern " + juce::String (i + 1));
                m.addSubMenu ("Copy to", copyTo);
                m.addItem (1, "Clear", ! proc.patterns.isEmpty (i));
                m.addSeparator();
                m.addSectionHeader ("Chain  (Shift-click pattern keys to build it)");
                m.addItem (2, "Add pattern " + juce::String (i + 1) + " to the chain", proc.getChainLength() < Tross110Processor::kMaxChain);
                m.addItem (3, "Remove the last chain step", proc.getChainLength() > 0);
                m.addItem (4, "Clear the chain", proc.getChainLength() > 0);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                 [safe = juce::Component::SafePointer<PatternPad> (this), i] (int id)
                                 {
                                     if (safe == nullptr) return;
                                     if (id == 1) safe->proc.patterns.clear (i);
                                     if (id == 2) safe->proc.chainAppend (i);
                                     if (id == 3) safe->proc.chainRemoveLast();
                                     if (id == 4) safe->proc.chainClear();
                                     if (id >= 100) safe->proc.patterns.copySlot (i, id - 100);
                                     safe->repaint();
                                 });
                return;
            }
            if (e.mods.isShiftDown()) { proc.chainAppend (i); repaint(); return; }   // build a chain
            proc.chainClear();   // a plain click goes back to a single pattern
            proc.selectSlot (i);
            if (auto* p = proc.apvts.getParameter ("pattern"))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) i));
                p->endChangeGesture();
            }
            repaint();
            return;
        }
}

// ================================================================================================
void StatusLcd::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    drawLcd (g, r);
    const int cur = proc.getCurrentSlot(), pending = proc.getPendingSlot();
    const int chainN = proc.getChainLength();
    const juce::String txt = chainN > 0 ? "CHN " + juce::String (juce::jmax (0, proc.getChainPos()) + 1) + "/" + juce::String (chainN)
                                            + " P" + juce::String (cur + 1).paddedLeft ('0', 2)
                           : pending >= 0 ? "PAT " + juce::String (cur + 1).paddedLeft ('0', 2) + " > " + juce::String (pending + 1).paddedLeft ('0', 2)
                                          : "PATTERN " + juce::String (cur + 1).paddedLeft ('0', 2);
    g.setFont (lcdFont (12.5f));
    g.setColour (col::lcdInk.withAlpha (0.22f));
    g.drawText (txt, r.translated (0.8f, 0.8f), juce::Justification::centred);
    g.setColour (col::lcdInk);
    g.drawText (txt, r, juce::Justification::centred);
}

// ================================================================================================
Tross110Editor::Tross110Editor (Tross110Processor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setBounds (0, 0, kWidth, kHeight);

    for (int v = 0; v < kNumVoices; ++v)
    {
        const int x = chanX (v);
        const juce::String id = kVoiceIds[v];
        auto strip = std::make_unique<NameStrip> (proc, v, kVoiceTitles[v]);
        strip->setBounds (x + 8, 186, kChanW - 16, 20);
        strip->setTooltip ("Click to play. Shift-click for an accented hit. Records into the pattern while REC is on and the song plays.");
        content.addAndMakeVisible (*strip);
        strips.push_back (std::move (strip));
        addKnob (id + "_tune",  { x + 36, kKnobRow1 }, kKnobD).setTooltip (kTuneTips[v]);
        addKnob (id + "_decay", { x + 92, kKnobRow1 }, kKnobD).setTooltip (v == BD
            ? "How long the kick rings. 50 % is the classic timing. Tuning higher also shortens it."
            : "How long the sound lasts. 50 % is the classic timing; 0 % is a quarter as long, 100 % four times as long.");
        addKnob (id + "_lvl",   { x + 36, kKnobRow2 }, kKnobD).setTooltip ("Channel level, -inf to +6 dB.");
        if (kExtraIds[v] != nullptr) addKnob (kExtraIds[v], { x + 92, kKnobRow2 }, kKnobD).setTooltip (kExtraTips[v]);
        auto& mute = addButton (id + "_mute", "M", { x + 8, 213, 26, 17 }, "small");
        mute.getProperties().set ("onColour", (juce::int64) col::muteOn.getARGB());
        mute.setTooltip ("Mute this drum.");
        auto& solo = addButton (id + "_solo", "S", { x + kChanW - 34, 213, 26, 17 }, "small");
        solo.getProperties().set ("onColour", (juce::int64) col::blue.getARGB());
        solo.setTooltip ("Solo: only soloed drums play.");
        if (! proc.isStandalone())   // the standalone app has only the main output
        {
            auto& own = addButton (id + "_out", "OWN OUT", { x + 34, 236, 60, 17 }, "small");
            own.getProperties().set ("onColour", (juce::int64) col::blue.getARGB());
            own.setTooltip ("On: this drum leaves the main mix and plays on its own mono output");
        }
    }
    addKnob ("accent", { kMasterX + 44, 106 }, 46).setTooltip ("How hard accented steps hit: a short swell on the whole machine that also adds grit. 0 % turns accents off.");
    addKnob ("master", { kMasterX + 136, 106 }, 46).setTooltip ("Main output level. Drums on their own outputs are not affected.");
    content.addAndMakeVisible (lcd);
    lcd.setTooltip ("Playing pattern (and the queued one), or the chain step while a chain runs.");

    helpBtn.getProperties().set ("style", "small");
    helpBtn.getProperties().set ("onColour", (juce::int64) col::blue.getARGB());
    helpBtn.setClickingTogglesState (true);
    helpBtn.setToggleState (proc.showHelp.load(), juce::dontSendNotification);
    helpBtn.onClick = [this] { proc.showHelp = helpBtn.getToggleState(); tooltips.hideTip(); };
    helpBtn.setTooltip ("Help windows on/off. The knob value read-outs always show.");
    helpBtn.setBounds (kMasterX + kMasterW - 62, 230, 54, 18);
    content.addAndMakeVisible (helpBtn);
    lcd.setBounds (kMasterX + 6, 150, kMasterW - 12, 30);

    // sequencer
    content.addAndMakeVisible (grid);
    grid.setBounds (0, kSeqY, 760, kHeight - kSeqY);
    grid.setTooltip ("Click a cell to set or clear a step; drag along a row to paint. AC = accent for every drum on that step.");
    content.addAndMakeVisible (patternPad);
    patternPad.setBounds (760, kSeqY, 240, 108);
    patternPad.setTooltip ("Click: play that pattern (waits for step 1 while playing). Shift-click: add it to a chain. "
                           "Right-click: copy to another slot, clear, or edit the chain.");

    const int ry = kSeqY + 112;
    auto len = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
    len->getProperties().set ("seg", true);
    len->setMouseDragSensitivity (120);
    len->setBounds (772, ry + 16, 58, 32);
    content.addAndMakeVisible (*len);
    sAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "seqlen", *len));
    len->setDoubleClickReturnValue (true, 16.0);
    len->setTooltip ("Drag up/down to set the pattern length (12 for triplet feels)");
    sliders.push_back (std::move (len));

    auto& run = addButton ("seqon", "RUN", { 842, ry + 2, 70, 25 }, "blue");
    run.getProperties().set ("led", true);
    run.setTooltip ("Sequencer on/off. Off: TRoss 110 only plays from MIDI.");
    clearBtn.setTooltip ("Clears the pattern that's playing.");
    clearBtn.getProperties().set ("style", "key");
    clearBtn.setBounds (918, ry + 2, 66, 25);
    clearBtn.onClick = [this] { proc.clearCurrentPattern(); grid.repaint(); };
    content.addAndMakeVisible (clearBtn);
    auto& rec = addButton ("rec", "REC", { 842, ry + 34, 70, 25 }, "key");
    rec.getProperties().set ("led", true);
    rec.setTooltip ("While the song plays, keyboard notes and name-strip clicks are written into the nearest step. Velocity above 100 adds the accent.");
    exportKey.setBounds (918, ry + 34, 66, 25);
    exportKey.setTooltip ("Click: export patterns as MIDI files. Drag onto a MIDI track: drop the playing pattern as a clip.");
    content.addAndMakeVisible (exportKey);

    if (proc.isStandalone())   // no host transport: own play button + tempo
    {
        playBtn.getProperties().set ("style", "blue");
        playBtn.getProperties().set ("led", true);
        playBtn.setClickingTogglesState (true);
        playBtn.setToggleState (proc.internalPlay.load(), juce::dontSendNotification);
        playBtn.onClick = [this] { proc.internalPlay = playBtn.getToggleState(); };
        playBtn.setBounds (kMasterX + 6, 196, 70, 25);
        playBtn.setTooltip ("Start / stop (standalone app).");
        content.addAndMakeVisible (playBtn);

        bpmBox.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        bpmBox.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        bpmBox.getProperties().set ("seg", true);
        bpmBox.getProperties().set ("segSize", 12.0f);
        bpmBox.setRange (40.0, 300.0, 1.0);
        bpmBox.setValue (proc.internalBpm.load(), juce::dontSendNotification);
        bpmBox.setMouseDragSensitivity (300);
        bpmBox.textFromValueFunction = [] (double v) { return juce::String ((int) v) + " BPM"; };
        bpmBox.onValueChange = [this] { proc.internalBpm = (float) bpmBox.getValue(); };
        bpmBox.setBounds (kMasterX + 84, 193, 90, 30);
        bpmBox.setTooltip ("Tempo: drag up or down.");
        content.addAndMakeVisible (bpmBox);
    }

    setResizable (true, true);
    setResizeLimits (kWidth * 3 / 4, kHeight * 3 / 4, kWidth * 2, kHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) kWidth / kHeight);
    setSize (kWidth, kHeight);
    startTimerHz (30);
}

Tross110Editor::~Tross110Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Slider& Tross110Editor::addKnob (const juce::String& paramId, juce::Point<int> centre, int diameter)
{
    auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
    s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    const int box = diameter + 14;
    s->setBounds (juce::Rectangle<int> (box, box).withCentre (centre));
    s->setPopupDisplayEnabled (true, true, this, 1200);
    content.addAndMakeVisible (*s);
    sAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, *s));
    if (auto* p = proc.apvts.getParameter (paramId))
        s->setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    sliders.push_back (std::move (s));
    return *sliders.back();
}

juce::Button& Tross110Editor::addButton (const juce::String& paramId, const juce::String& text, juce::Rectangle<int> r, const char* style)
{
    auto b = std::make_unique<juce::TextButton> (text);
    b->setClickingTogglesState (true);
    b->getProperties().set ("style", style);
    b->setBounds (r);
    content.addAndMakeVisible (*b);
    bAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, *b));
    buttons.push_back (std::move (b));
    return *buttons.back();
}

void Tross110Editor::paint (juce::Graphics& g) { g.fillAll (col::lowerBottom); }

void Tross110Editor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / kWidth));
}

void Tross110Editor::paintPanel (juce::Graphics& g)
{
    // ---- charcoal upper case with a fine horizontal grain
    g.setGradientFill (juce::ColourGradient::vertical (col::caseTop, 0.0f, col::caseBottom, (float) kTopH));
    g.fillRect (0, 0, kWidth, kTopH);
    for (int y = 0; y < kTopH; y += 2)
    {
        g.setColour (juce::Colours::white.withAlpha (y % 4 == 0 ? 0.012f : 0.0f));
        g.fillRect (0, y, kWidth, 1);
    }

    // header: name, subtitle, build version
    auto logo = font (30.0f, true, -0.02f);
    g.setFont (logo);
    const float troW = juce::GlyphArrangement::getStringWidth (logo, "TRoss ");
    g.setColour (col::print);
    g.drawText ("TRoss ", juce::Rectangle<float> ((float) kMargin, 10.0f, troW + 4.0f, 34.0f), juce::Justification::bottomLeft);
    const float numW = juce::GlyphArrangement::getStringWidth (logo, "110");
    g.setColour (col::blue);
    g.drawText ("110", juce::Rectangle<float> ((float) kMargin + troW, 10.0f, numW + 4.0f, 34.0f), juce::Justification::bottomLeft);
    g.setColour (col::softPrint);
    g.setFont (font (10.5f, false, 0.3f));
    g.drawText ("ANALOG-STYLE RHYTHM SYNTHESIZER", juce::Rectangle<float> ((float) kMargin + troW + numW + 14.0f, 24.0f, 400.0f, 16.0f),
                juce::Justification::bottomLeft);
    g.setFont (font (10.0f, false, 0.2f));
    g.drawText ("v" JucePlugin_VersionString, kWidth - kMargin - 200, 24, 200, 16, juce::Justification::bottomRight);
    // thin blue rule
    g.setColour (col::blueDark);
    g.fillRect (kMargin, 50, kWidth - 2 * kMargin, 2);

    auto knobLabel = [&] (const juce::String& text, int cx, int y)
    {
        g.setColour (col::print);
        g.setFont (font (8.5f, false, 0.12f));
        g.drawText (text, cx - 32, y, 64, 11, juce::Justification::centred);
    };

    for (int v = 0; v < kNumVoices; ++v)
    {
        const int x = chanX (v);
        knobLabel ("TUNE", x + 36, 64);
        knobLabel ("DECAY", x + 92, 64);
        knobLabel ("LEVEL", x + 36, 118);
        if (kExtraNames[v] != nullptr) knobLabel (kExtraNames[v], x + 92, 118);
        g.setColour (col::softPrint);
        g.setFont (font (9.0f, false, 0.1f));
        g.drawText (kVoiceNotes[v], x + 34, 213, kChanW - 68, 17, juce::Justification::centred);

        if (v < kNumVoices - 1)   // engraved divider
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (x + kChanW - 1, 62, 1, 192);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRect (x + kChanW, 62, 1, 192);
        }
    }

    // master
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRoundedRectangle ((float) kMasterX, 60.0f, (float) kMasterW, 194.0f, 6.0f);
    knobLabel ("ACCENT", kMasterX + 44, 66);
    knobLabel ("VOLUME", kMasterX + 136, 66);
    if (! proc.isStandalone())
    {
        g.setColour (col::softPrint);
        g.setFont (font (8.5f, false, 0.15f));
        g.drawText ("MASTER", kMasterX, 200, kMasterW, 14, juce::Justification::centred);
    }

    // ---- seam and the lower section
    g.setColour (juce::Colour (0xff0b0b0c));
    g.fillRect (0, kTopH, kWidth, kSeqY - kTopH);
    g.setColour (col::blueDark.withAlpha (0.6f));
    g.fillRect (0, kTopH + 1, kWidth, 1);
    g.setGradientFill (juce::ColourGradient::vertical (col::lowerTop, (float) kSeqY, col::lowerBottom, (float) kHeight));
    g.fillRect (0, kSeqY, kWidth, kHeight - kSeqY);

    g.setColour (col::softPrint);
    g.setFont (font (8.5f, false, 0.15f));
    g.drawText ("LENGTH", 772, kSeqY + 112, 58, 14, juce::Justification::centred);
}

void Tross110Editor::timerCallback()
{
    const int v = proc.patterns.getVersion(), st = proc.getPlayStep();
    const int slot = proc.getCurrentSlot(), pend = proc.getPendingSlot();
    const int len = (int) proc.apvts.getRawParameterValue ("seqlen")->load();
    const int sel = (int) proc.apvts.getRawParameterValue ("pattern")->load();

    if (v != lastVersion || st != lastStep || slot != lastSlot || len != lastLen)
        grid.repaint();
    if (v != lastVersion) patternPad.repaint();   // empty-slot dashes follow the pads
    if (slot != lastSlot || pend != lastPending || pend >= 0 || sel != lastSel)
    {
        patternPad.repaint();
        lcd.repaint();
    }
    for (auto& s : strips)
        if (s->lastHit != 0 && juce::Time::getMillisecondCounter() - s->lastHit < 260) s->repaint();
    if (const int cv = proc.getChainVersion() * 64 + proc.getChainPos(); cv != lastChain) { patternPad.repaint(); lcd.repaint(); lastChain = cv; }
    lastVersion = v; lastStep = st; lastSlot = slot; lastPending = pend; lastLen = len; lastSel = sel;
}
