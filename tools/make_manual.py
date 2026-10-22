#!/usr/bin/env python3
# Builds docs/TRoss110-Manual.pdf (needs: pip install reportlab).
#   python3 tools/make_manual.py
# The panel picture is docs/panel.png (the editor snapshot written by the plug-in test).
import os, re
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image, PageBreak, KeepTogether)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = re.search(r"project\(TRoss110 VERSION ([0-9.]+)\)", open(os.path.join(ROOT, "CMakeLists.txt")).read()).group(1)
OUT = os.path.join(ROOT, "docs", "TRoss110-Manual.pdf")

INK = colors.HexColor("#1e1f21")
BLUE = colors.HexColor("#3f7f9e")
GREY = colors.HexColor("#6b6e6a")
LIGHT = colors.HexColor("#eef1ea")

body = ParagraphStyle("body", fontName="Helvetica", fontSize=9.6, leading=13.6, textColor=INK, spaceAfter=6)
small = ParagraphStyle("small", parent=body, fontSize=8.6, leading=11.6, spaceAfter=0)
h1 = ParagraphStyle("h1", fontName="Helvetica-Bold", fontSize=15, leading=19, textColor=INK, spaceBefore=10, spaceAfter=7, keepWithNext=1)
h2 = ParagraphStyle("h2", fontName="Helvetica-Bold", fontSize=11, leading=14, textColor=BLUE, spaceBefore=9, spaceAfter=4, keepWithNext=1)
title = ParagraphStyle("title", fontName="Helvetica-Bold", fontSize=34, leading=38, textColor=INK)
sub = ParagraphStyle("sub", fontName="Helvetica", fontSize=11, leading=15, textColor=GREY, spaceAfter=14)
cell = ParagraphStyle("cell", parent=small)
cellb = ParagraphStyle("cellb", parent=small, fontName="Helvetica-Bold")


def table(rows, widths):
    data = [[Paragraph(str(c), cellb if i == 0 or j == 0 else cell) for j, c in enumerate(r)] for i, r in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), LIGHT),
        ("LINEBELOW", (0, 0), (-1, 0), 0.8, BLUE),
        ("LINEBELOW", (0, 1), (-1, -1), 0.3, colors.HexColor("#cfd3cb")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5),
    ]))
    return t


def P(t): return Paragraph(t, body)


def footer(canv, doc):
    canv.saveState()
    canv.setFont("Helvetica", 8)
    canv.setFillColor(GREY)
    canv.drawString(18 * mm, 10 * mm, f"TRoss 110 {VERSION}  ·  Ticco Ross 2026")
    canv.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    canv.setStrokeColor(BLUE); canv.setLineWidth(0.6)
    canv.line(18 * mm, 13.5 * mm, A4[0] - 18 * mm, 13.5 * mm)
    canv.restoreState()


W = A4[0] - 36 * mm
s = []
s += [Spacer(1, 18 * mm), Paragraph('TRoss <font color="#3f7f9e">110</font>', title),
      Paragraph(f"Analog-style rhythm synthesizer with step sequencer  ·  Owner's manual  ·  version {VERSION}", sub)]
img = os.path.join(ROOT, "docs", "panel.png")
if os.path.exists(img):
    s += [Image(img, width=W, height=W * 0.5), Spacer(1, 8)]
s += [P("TRoss 110 is a compact drum machine with six analog-style voices - bass drum, snare, hand clap, closed hat, "
        "open hat and cymbal - a global accent, and a 16-step sequencer with 12 pattern slots. It runs as an Audio Unit, "
        "VST3 or LV2 plug-in and as a standalone app on macOS, Windows and Linux."),
      P("Its character is small, punchy and a little gritty: a short, thudding kick with a click on the front, a noisy snare, "
        "a clap with a built-in room tail, metallic hats and a shimmering cymbal. Hit the accent and the whole machine swells "
        "and bites.")]

s += [PageBreak(), Paragraph("1  Installing", h1)]
s += [table([
    ["System", "File", "What it installs"],
    ["macOS 10.13+", f"TRoss110-{VERSION}-macOS.pkg", "Audio Unit, VST3 and the standalone app. Click Customize to skip any of them."],
    ["Windows 10+", f"TRoss110-{VERSION}-Windows-Setup.exe", "VST3 (Common Files\\VST3) and the standalone app."],
    ["Linux x86_64", f"TRoss110-{VERSION}-Linux-x86_64.tar.gz", "Unpack and run ./install.sh: VST3 to ~/.vst3, LV2 to ~/.lv2, app to ~/.local/bin."],
], [28 * mm, 62 * mm, W - 90 * mm])]
s += [Spacer(1, 6),
      P("The installers are not signed. The first time you open one, macOS shows a Gatekeeper warning (System Settings > "
        "Privacy &amp; Security > Open Anyway) and Windows shows SmartScreen (More info > Run anyway)."),
      P("<b>Ableton Live:</b> go to Settings > Plug-Ins, turn on \"Use Audio Units v2\" (Mac) and rescan. TRoss 110 is under "
        "Plug-Ins > Audio Units (or VST3) > Ticco Ross. Drop it on a MIDI track.")]

s += [Paragraph("2  The panel", h1),
      P("The upper half holds the six drum channels and the master section. The lower half is the sequencer: the LCD step "
        "grid on the left, the pattern keys and transport keys on the right. The window can be resized from its bottom-right "
        "corner. Hover over a knob or drag it to see its value; double-click a knob to reset it.")]

s += [Paragraph("2.1  Drum channels", h2)]
s += [table([
    ["Control", "What it does"],
    ["TUNE", "Pitch, ±12 semitones. On the hats and cymbal it moves the whole metallic cluster."],
    ["DECAY", "Length of the sound. 50 % is the classic timing; 0 % is a quarter as long, 100 % four times as long. "
              "The bass drum's decay also follows its tuning: tune it up and it gets shorter, as on the original circuit."],
    ["LEVEL", "Channel level, -inf to +6 dB."],
    ["ATTACK (BD)", "The click at the front of the kick."],
    ["SNAPPY (SD)", "Amount of noise snap against the drum body."],
    ["TAIL (HC)", "Level of the clap's reverb-like tail after the burst of hits."],
    ["TONE (CY)", "Balance between the cymbal's bright attack shimmer (right) and its darker ringing body (left)."],
    ["Name strip", "Click to play the drum. Shift-click plays an accented hit. While the song plays with REC on, the hit is recorded."],
    ["M / S", "Mute and solo."],
    ["OWN OUT", "Sends the drum to its own mono output instead of the main mix (see section 5). Not shown in the standalone app."],
], [30 * mm, W - 30 * mm])]

s += [Paragraph("2.2  Master section", h2)]
s += [table([
    ["Control", "What it does"],
    ["ACCENT", "How hard accented steps hit. The accent is a short swell on the whole machine - every drum on that step, and the "
               "tails ringing under it - which also drives the output into grit. 0 % turns accents off."],
    ["VOLUME", "Main output level. Drums on their own outputs are not affected."],
    ["Display", "Shows the playing pattern, and the queued one (for example PAT 01 > 04)."],
    ["Logo", "Click the TRoss 110 logo (top left) for the About window: the version, the main shortcuts and the contact address. Click anywhere to close it."],
    ["P-LOCK", "Switches the screen between the step grid and the parameter locks (section 3.1)."],
    ["HELP (top right)", "Help windows on/off. With HELP on, resting the mouse on any control shows what it does. The knob value read-outs "
             "show either way. The setting is saved with your song."],
    ["PLAY / BPM", "Standalone app only: start/stop and tempo (drag the read-out up or down)."],
], [30 * mm, W - 30 * mm])]

s += [Paragraph("3  The sequencer", h1),
      P("The sequencer follows your DAW: it starts, stops and keeps time with the host's transport. Each pattern has 16 steps "
        "of 16th notes and seven rows: <b>AC</b> (accent) and the six drums <b>BD, SD, HC, CH, OH, CY</b>.")]
s += [table([
    ["Control", "What it does"],
    ["LCD grid", "Click a cell to set or clear it. Drag along a row to paint several steps. The arrow along the top of the "
                 "display follows the playing step. Steps past the pattern length are dimmed."],
    ["AC row", "Every drum on an accented step plays with the accent."],
    ["RUN", "Sequencer on/off. With RUN off, TRoss 110 is a pure sound module played from MIDI."],
    ["LENGTH", "1 to 16 steps; drag the read-out up or down, double-click for 16. Use 12 for shuffles and triplet grooves."],
    ["CLEAR", "Clears the pattern that is playing."],
    ["REC", "On by default. While the song plays, notes from your keyboard and clicks on the name strips are written into the "
            "nearest step of the playing pattern. Velocity above 100 (or Shift-click) also sets the accent."],
    ["PATTERN 1-12", "Twelve pattern slots. While playing, a new slot waits until the current pattern returns to step 1, so "
                     "changes always land on the bar; its LED blinks while it waits. While stopped, it switches at once. "
                     "A small dash on a key means that slot is empty. The selected slot can also be automated from the host "
                     "(the \"Pattern\" parameter)."],
    ["Right-click a pattern key", "Copy to: copies that pattern into another slot. Clear: empties it. Also adds to, shortens or clears the chain."],
    ["Shift-click pattern keys", "Builds a chain: each Shift-click adds that pattern as the next step (up to 32). The chain plays one "
                                 "pattern per bar and follows the song position, so jumping or looping in the DAW lands on the right "
                                 "pattern. Patterns in the chain show a dim LED, the playing one a bright LED, and the display shows "
                                 "CHN step/length and the pattern. A plain click on a pattern key ends the chain."],
    ["EXPORT MIDI", "Click for a menu: export the playing pattern, all patterns one after another, or every pattern as its "
                    "own .mid file, or the chain as one file. Or drag the key straight onto a MIDI track to drop the playing pattern in as a clip."],
], [30 * mm, W - 30 * mm])]
s += [Spacer(1, 6), P("All twelve patterns and the chain are saved with your song, together with every knob.")]

s += [Paragraph("3.1  Parameter locks", h2),
      P("Any step can play a drum with different knob settings: a higher snare, a shorter hat, a quieter kick. Press <b>P-LOCK</b> "
        "(master section) to swap the step grid for the lock screen; press it again to go back."),
      table([
          ["Control", "What it does"],
          ["Drum (left)", "Choose which drum's locks you see: BD, SD, HC, CH, OH, CY. A blue dot means that drum has locks."],
          ["Knob tabs (below)", "TUNE, DECAY, LEVEL, and the drum's extra knob (ATTACK, SNAPPY, TAIL or TONE)."],
          ["Bars", "Drag a step's bar up or down to lock that value; drag across steps to draw several. The dotted line is "
                   "the knob's current setting. Small squares under the bars show the steps where the drum plays."],
          ["Right-click / double-click", "Remove the lock on a step (right-drag removes across steps)."],
          ["Shift-right-click", "Clear this knob's locks, all of this drum's locks, or every lock in the pattern."],
      ], [36 * mm, W - 36 * mm]),
      Spacer(1, 6),
      P("A lock holds for that hit's whole sound; the next hit without a lock goes back to the knobs. Notes you play live always use "
        "the knobs. On the step grid, a small corner mark shows steps that carry locks. Locks are copied with Copy to, removed by "
        "Clear, and saved with your song. MIDI export writes the notes only.")]

s += [Paragraph("4  Playing from MIDI", h1),
      P("TRoss 110 listens on all MIDI channels. Velocities up to 100 set the level of the hit; anything above 100 also fires "
        "the accent. Note names are Ableton's (C1 = MIDI note 36).")]
s += [table([
    ["Drum", "Notes", "MIDI numbers"],
    ["Bass drum (BD)", "C1, B0", "36, 35"],
    ["Snare (SD)", "D1, E1, C#1", "38, 40, 37"],
    ["Hand clap (HC)", "D#1", "39"],
    ["Closed hat (CH)", "F#1, G#1", "42, 44"],
    ["Open hat (OH)", "A#1", "46"],
    ["Cymbal (CY)", "C#2, D#2, E2, F2, G2, A2", "49, 51, 52, 53, 55, 57"],
], [42 * mm, 60 * mm, W - 102 * mm])]
s += [Spacer(1, 6), P("The closed hat cuts off a ringing open hat, so alternating the two gives the classic choked hat. "
                      "Exported MIDI uses the first note of each drum, channel 10, velocity 100 (127 on accented steps).")]

s += [KeepTogether([Paragraph("5  Outputs", h1),
      P("All drums start on the main stereo output. Turn on a drum's OWN OUT and it leaves the main mix and plays on its own mono "
        "output (BD, SD, CP, CH, OH, CY) after its level, mute/solo and accent, but not Volume. If the host has that output "
        "switched off, the drum stays in the main mix."),
      P("<b>Ableton Live:</b> make an audio track for each separate drum, set Audio From to the TRoss 110 track, choose the "
        "output (for example BD) in the second menu, and set Monitor to In.")])]

s += [Paragraph("6  Tips", h1)]
for t in [
    "For the classic dry box sound, keep the decays at 50 % and push ACCENT to 70-100 %: the accent is where this machine gets its punch.",
    "Tune the bass drum down a few semitones for a longer, rounder kick; tune it up for a short tom-like thud.",
    "Turn TAIL down for a tight, dry clap, or up and add decay for a big roomy one.",
    "Pull the cymbal's TONE left and its decay up for a dark, washy ride.",
    "LENGTH 12 against a 16-step bassline makes instant polyrhythms; queue patterns ahead while playing - they always change on the bar.",
]:
    s.append(Paragraph("•  " + t, body))

s += [P("Questions or feedback: <b>ticcoross@gmail.com</b>")]
s += [Paragraph("7  Specifications", h1)]
s += [table([
    ["", ""],
    ["Voices", "6: bass drum, snare, hand clap, closed hat, open hat, cymbal; plus global accent"],
    ["Sequencer", "16 steps (length 1-16), 7 rows, 12 pattern slots, host sync, real-time recording"],
    ["Outputs", "Main stereo + 6 mono drum outputs (plug-in)"],
    ["Formats", "AU, VST3 (macOS); VST3 (Windows); VST3, LV2 (Linux); standalone app on all three"],
    ["Sample rates", "Any host rate (44.1-192 kHz)"],
], [30 * mm, W - 30 * mm])]

doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=18 * mm, bottomMargin=20 * mm,
                        title=f"TRoss 110 Manual {VERSION}", author="Ticco Ross", subject="TRoss 110 owner's manual")
doc.build(s, onFirstPage=footer, onLaterPages=footer)
print("wrote", OUT)
