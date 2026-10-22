#!/usr/bin/env python3
# Builds docs/TRoss110-History.pdf, the update history (needs: pip install reportlab).
#   python3 tools/make_history.py
# Add new versions at the top of HISTORY.
import os, re
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, KeepTogether

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = re.search(r"project\(TRoss110 VERSION ([0-9.]+)\)", open(os.path.join(ROOT, "CMakeLists.txt")).read()).group(1)
OUT = os.path.join(ROOT, "docs", "TRoss110-History.pdf")

HISTORY = [
    ("0.3.3", "22 October 2026", [
        "Click the top strip of the panel to switch the window between 100 % and a smaller 75 % size.",
        "HELP is now off by default.",
        "New defaults: Bass Drum Decay 27.8 %; Hand Clap Decay 30 %, Tail 15 %.",
        "New: this update history.",
    ]),
    ("0.3.2", "22 October 2026", [
        "Hand clap: new tail. It now starts together with the burst of hits, stops short instead of fading into hiss, "
        "and is darker than the hits, closer to the original machine.",
    ]),
    ("0.3.1", "22 October 2026", [
        "New: click the TRoss 110 logo for the About window, with the main shortcuts and the contact address.",
        "Hand Clap Tail default 25.2 %.",
    ]),
    ("0.3.0", "17 October 2026", [
        "New: parameter locks. Any step can play a drum with its own Tune, Decay, Level or extra-knob value. "
        "Press P-LOCK to switch the screen to the lock view, pick a drum and a knob, and drag each step's bar.",
        "Steps with locks show a small corner mark on the step grid.",
        "Locks are copied, cleared and saved with their pattern.",
        "HELP moved to the top of the panel, next to the version number.",
    ]),
    ("0.2.0", "16 October 2026", [
        "New: right-click a pattern key to copy it to another slot or clear it.",
        "New: pattern chains. Shift-click pattern keys to chain up to 32 patterns, one per bar, following the song position. "
        "The chain is saved with the project and can be exported as MIDI.",
        "Fixed: the help windows were empty. They now show readable text, and every control has one.",
        "New: HELP key to switch the help windows on and off (knob values always show).",
        "Empty pattern slots show a small dash on their key.",
        "New defaults: Bass Drum Decay 24.1 %, Level -8.4 dB; Snare Decay 43.9 %, Snappy 60 %; "
        "Hand Clap Tune +2.6 st, Decay 20.3 %, Level -3.6 dB.",
    ]),
    ("0.1.0", "15 October 2026", [
        "First release: six analog-style voices (bass drum, snare, hand clap, closed hat, open hat, cymbal) with a global accent.",
        "16-step sequencer with 12 pattern slots, recording from the keyboard and the name strips, LENGTH 1-16.",
        "MIDI export, including drag-to-track.",
        "Separate output for every drum.",
        "AU, VST3, LV2 and standalone app for macOS, Windows and Linux.",
    ]),
]

INK = colors.HexColor("#1e1f21"); BLUE = colors.HexColor("#3f7f9e"); GREY = colors.HexColor("#6b6e6a")
title = ParagraphStyle("t", fontName="Helvetica-Bold", fontSize=28, leading=32, textColor=INK)
sub = ParagraphStyle("s", fontName="Helvetica", fontSize=11, leading=15, textColor=GREY, spaceAfter=16)
ver = ParagraphStyle("v", fontName="Helvetica-Bold", fontSize=13, leading=17, textColor=INK, spaceBefore=12, spaceAfter=3)
item = ParagraphStyle("i", fontName="Helvetica", fontSize=9.8, leading=13.8, textColor=INK, leftIndent=12, bulletIndent=2, spaceAfter=3)


def footer(c, doc):
    c.saveState(); c.setFont("Helvetica", 8); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, f"TRoss 110 update history  ·  Ticco Ross 2026  ·  ticcoross@gmail.com")
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.setStrokeColor(BLUE); c.setLineWidth(0.6); c.line(18 * mm, 13.5 * mm, A4[0] - 18 * mm, 13.5 * mm)
    c.restoreState()


s = [Paragraph('TRoss <font color="#3f7f9e">110</font>  update history', title),
     Paragraph(f"What changed in each version, newest first. Current version: {VERSION}", sub)]
for v, date, items in HISTORY:
    block = [Paragraph(f'Version {v} <font size="9.5" color="#6b6e6a">&nbsp;&nbsp;{date}</font>', ver)]
    block += [Paragraph(t, item, bulletText="•") for t in items]
    s.append(KeepTogether(block))

SimpleDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=18 * mm, bottomMargin=20 * mm,
                  title=f"TRoss 110 Update History {VERSION}", author="Ticco Ross").build(s, onFirstPage=footer, onLaterPages=footer)
print("wrote", OUT)
