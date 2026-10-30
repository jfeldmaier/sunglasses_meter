# SPDX-FileCopyrightText: 2026 J. Feldmaier
# SPDX-License-Identifier: Apache-2.0

"""
Generates a PDF one-pager (A4) for the IR Sunglasses Meter.
Run with the project venv:
    .venv/Scripts/python.exe make_onepager.py
Output: IR_Sunglasses_Meter_Anleitung.pdf
"""

from fpdf import FPDF

# ── colour palette ──────────────────────────────────────────────────────────
BRAND   = (10,  109, 157)   # blue
OK      = (0,   122, 100)   # green
WARN    = (173,  62,  46)   # red
BG      = (243, 245, 247)   # light grey
PANEL   = (255, 255, 255)
LINE    = (219, 226, 232)
TXT     = (23,   35,  47)
MUTED   = (91,  107, 121)
YELLOW  = (255, 243, 205)
YELLB   = (255, 228, 132)

# ── helpers ──────────────────────────────────────────────────────────────────

class PDF(FPDF):

    def card(self, x, y, w, h, fill=PANEL, border=LINE):
        self.set_fill_color(*fill)
        self.set_draw_color(*border)
        self.set_line_width(0.3)
        self.rect(x, y, w, h, "FD")

    def badge(self, x, y, w, h, label, color, text_color=(255,255,255)):
        self.set_fill_color(*color)
        self.set_draw_color(*color)
        self.set_line_width(0)
        self.rect(x, y, w, h, "F")
        self.set_font("Helvetica", "B", 7)
        self.set_text_color(*text_color)
        self.set_xy(x, y + (h - 3) / 2)
        self.cell(w, 3, label, align="C")

    def step_circle(self, cx, cy, r, num, done=False):
        fill = OK if done else BRAND
        self.set_fill_color(*fill)
        self.set_draw_color(*fill)
        self.ellipse(cx - r, cy - r, r * 2, r * 2, "F")
        self.set_font("Helvetica", "B", 7)
        self.set_text_color(255, 255, 255)
        self.set_xy(cx - r, cy - r)
        self.cell(r * 2, r * 2, str(num), align="C")

    def tile(self, x, y, w, h, label, value, unit="", val_color=TXT):
        self.card(x, y, w, h, fill=(251, 253, 255))
        self.set_font("Helvetica", "", 6.5)
        self.set_text_color(*MUTED)
        self.set_xy(x + 1.5, y + 1.5)
        self.cell(w - 3, 4, label)
        self.set_font("Helvetica", "B", 9.5)
        self.set_text_color(*val_color)
        self.set_xy(x + 1.5, y + 6)
        self.cell(w - 3, 5, value)
        if unit:
            self.set_font("Helvetica", "", 7)
            self.set_text_color(*MUTED)
            self.cell(0, 5, unit)

    def section_title(self, x, y, w, title, color=BRAND):
        self.set_fill_color(*color)
        self.set_draw_color(*color)
        self.set_line_width(0)
        self.rect(x, y, 2.5, 5.5, "F")
        self.set_font("Helvetica", "B", 9)
        self.set_text_color(*color)
        self.set_xy(x + 4, y)
        self.cell(w - 4, 5.5, title.upper())

    def bullet(self, x, y, text, indent=0, font_size=7.5, color=TXT):
        self.set_fill_color(*BRAND)
        self.rect(x + indent, y + 2.2, 1.2, 1.2, "F")
        self.set_font("Helvetica", "", font_size)
        self.set_text_color(*color)
        self.set_xy(x + indent + 2.5, y)
        self.cell(0, 5, text)

    def step_row(self, x, y, num, text, col_w=55, done=False):
        self.step_circle(x + 3, y + 3, 3, num, done)
        self.set_font("Helvetica", "", 7.5)
        self.set_text_color(*TXT)
        self.set_xy(x + 7.5, y + 0.5)
        self.multi_cell(col_w - 8, 4.2, text)


def make_pdf():
    pdf = PDF(orientation="P", unit="mm", format="A4")
    pdf.set_auto_page_break(False)
    pdf.add_page()
    pdf.set_margins(0, 0, 0)

    W, H = 210, 297
    M = 10          # outer margin
    COL = (W - 2*M - 6) / 3   # 3-column grid gap=3

    # ── HEADER BANNER ────────────────────────────────────────────────────────
    pdf.set_fill_color(*BRAND)
    pdf.rect(0, 0, W, 26, "F")

    # title
    pdf.set_font("Helvetica", "B", 16)
    pdf.set_text_color(255, 255, 255)
    pdf.set_xy(M, 5)
    pdf.cell(0, 8, "IR Sunglasses Meter")

    pdf.set_font("Helvetica", "", 9)
    pdf.set_text_color(180, 220, 245)
    pdf.set_xy(M, 14)
    pdf.cell(0, 5, "Kurzanleitung zum Messgeraet")

    # version badge
    pdf.badge(W - M - 22, 8, 22, 8, "ESP8266 / Web-UI", (255,255,255), BRAND)

    # ── BODY ─────────────────────────────────────────────────────────────────
    Y0 = 30   # top of body

    # ─────────────────────────────────────────────────────────────────────────
    # ROW 1: How it works  |  Quick Start  |  Web UI mockup
    # ─────────────────────────────────────────────────────────────────────────
    C1 = M
    C2 = M + COL + 3
    C3 = M + 2*(COL + 3)
    ROW1_H = 68

    # ── Col 1 - Was misst das Geraet? ─────────────────────────────────────────
    pdf.card(C1, Y0, COL, ROW1_H)
    pdf.section_title(C1 + 3, Y0 + 4, COL - 6, "Funktionsweise")

    entries = [
        "IR-LED sendet 850-940 nm Licht",
        "Photodiode misst durchgelassenes Licht",
        "Berechnung: Transmission in Prozent",
        "Kalibrierung korrigiert Sensor-Fehler",
        "Ergebnis: z.B. 15% = 85% Lichtblock",
    ]
    for i, e in enumerate(entries):
        pdf.bullet(C1 + 3, Y0 + 12 + i * 9.5, e, font_size=7.2)

    # mini diagram
    dx, dy = C1 + 4, Y0 + 61
    pdf.set_fill_color(*BRAND)
    pdf.ellipse(dx, dy - 2.5, 5, 5, "F")
    pdf.set_font("Helvetica", "B", 5.5)
    pdf.set_text_color(255,255,255)
    pdf.set_xy(dx, dy - 2.5)
    pdf.cell(5, 5, "IR", align="C")
    pdf.set_draw_color(*BRAND)
    pdf.set_line_width(0.5)
    pdf.line(dx + 5.5, dy, dx + 11, dy)
    pdf.line(dx + 9.5, dy - 1, dx + 11, dy)
    pdf.line(dx + 9.5, dy + 1, dx + 11, dy)
    pdf.set_fill_color(*LINE)
    pdf.set_draw_color(*MUTED)
    pdf.rect(dx + 11.5, dy - 4, 4, 8, "FD")
    pdf.set_font("Helvetica", "B", 5)
    pdf.set_text_color(*MUTED)
    pdf.set_xy(dx + 11.5, dy - 1.5)
    pdf.cell(4, 3, "GL", align="C")
    pdf.set_draw_color(*OK)
    pdf.line(dx + 16, dy, dx + 21, dy)
    pdf.line(dx + 19.5, dy - 1, dx + 21, dy)
    pdf.line(dx + 19.5, dy + 1, dx + 21, dy)
    pdf.set_fill_color(*OK)
    pdf.ellipse(dx + 21.5, dy - 2.5, 5, 5, "F")
    pdf.set_font("Helvetica", "B", 5)
    pdf.set_text_color(255,255,255)
    pdf.set_xy(dx + 21.5, dy - 2.5)
    pdf.cell(5, 5, "PD", align="C")
    pdf.set_font("Helvetica", "", 5.5)
    pdf.set_text_color(*MUTED)
    pdf.set_xy(dx + 28, dy - 1)
    pdf.cell(0, 3, "-> A0 -> ESP")

    # ── Col 2 - Schnellstart ──────────────────────────────────────────────────
    pdf.card(C2, Y0, COL, ROW1_H)
    pdf.badge(C2 + 3, Y0 + 4, 22, 6, "SCHNELLSTART", BRAND)
    pdf.section_title(C2 + 27, Y0 + 4, COL - 30, "Starten")

    steps_qs = [
        "Geraet mit USB verbinden",
        "WLAN waehlen: IR-Sunglasses-Meter",
        "Browser: http://sunglasses.local",
        "Messmodus ist aktiv (Standard)",
        "Brille vor Sensor halten",
        "Auf 'Messen' klicken",
    ]
    for i, s in enumerate(steps_qs):
        pdf.step_row(C2 + 3, Y0 + 12 + i * 9.3, i + 1, s, COL - 3)

    # ── Col 3 - Web-UI Mockup ─────────────────────────────────────────────────
    pdf.card(C3, Y0, COL, ROW1_H)
    pdf.section_title(C3 + 3, Y0 + 4, COL - 6, "Web-Interface")

    bx, by, bw, bh = C3 + 3, Y0 + 12, COL - 6, ROW1_H - 15
    pdf.set_fill_color(50, 60, 70)
    pdf.rect(bx, by, bw, 6, "F")
    pdf.set_fill_color(255,255,255)
    pdf.rect(bx + 1, by + 1, bw - 2, 4, "F")
    pdf.set_font("Helvetica", "", 5)
    pdf.set_text_color(*MUTED)
    pdf.set_xy(bx + 2, by + 1.5)
    pdf.cell(bw - 4, 3, "http://sunglasses.local")
    pdf.set_fill_color(237, 243, 248)
    pdf.rect(bx, by + 6, bw, bh - 6, "F")
    pdf.set_fill_color(*BRAND)
    pdf.rect(bx, by + 6, bw, 7, "F")
    pdf.set_font("Helvetica", "B", 7)
    pdf.set_text_color(255,255,255)
    pdf.set_xy(bx + 2, by + 7)
    pdf.cell(0, 5, "IR Sunglasses Meter")
    pdf.set_fill_color(*BRAND)
    pdf.rect(bx + 1.5, by + 14.5, bw/2 - 2.5, 5, "F")
    pdf.set_fill_color(240, 244, 248)
    pdf.set_draw_color(*LINE)
    pdf.set_line_width(0.2)
    pdf.rect(bx + bw/2 + 1, by + 14.5, bw/2 - 2.5, 5, "FD")
    pdf.set_font("Helvetica", "B", 5.5)
    pdf.set_text_color(255,255,255)
    pdf.set_xy(bx + 1.5, by + 15.5)
    pdf.cell(bw/2 - 2.5, 3, "Messmodus", align="C")
    pdf.set_text_color(*MUTED)
    pdf.set_xy(bx + bw/2 + 1, by + 15.5)
    pdf.cell(bw/2 - 2.5, 3, "Kalibriermodus", align="C")
    tw = (bw - 3) / 3
    for ti, (lbl, val) in enumerate([("Kalibriert", "42.3%"), ("Rohwert", "40.1%"), ("Ref.", "850")]):
        tx = bx + 1.5 + ti * (tw + 0.5)
        pdf.tile(tx, by + 21, tw, 12, lbl, val)
    pdf.set_fill_color(*BRAND)
    pdf.rect(bx + 1.5, by + 35, bw - 3, 6, "F")
    pdf.set_font("Helvetica", "B", 6.5)
    pdf.set_text_color(255,255,255)
    pdf.set_xy(bx + 1.5, by + 36.5)
    pdf.cell(bw - 3, 3, "Messen", align="C")

    # ─────────────────────────────────────────────────────────────────────────
    # ROW 2: Messmodus  |  Kalibriermodus  |  Quick-Delta
    # ─────────────────────────────────────────────────────────────────────────
    Y1 = Y0 + ROW1_H + 4
    ROW2_H = 80

    # ── Col 1 - Messmodus ────────────────────────────────────────────────────
    pdf.card(C1, Y1, COL, ROW2_H)
    pdf.badge(C1 + 3, Y1 + 4, 22, 6, "MESSMODUS", BRAND)
    pdf.section_title(C1 + 27, Y1 + 4, COL - 30, "Messen")

    msteps = [
        ("Kalibrierung laden", "Im Abschnitt 'Kalibrierdatensaetze' waehlen."),
        ("Brille positionieren", "Glas ueber IR-Sensor halten."),
        ("Messen klicken", "Button - Transmission wird berechnet."),
        ("Ablesen", "Wert in Prozent (15% = 85% Lichtblock)."),
        ("Wiederholung", "Beliebig oft messen, sofort aktualisiert."),
    ]

    yy = Y1 + 14
    for i, (title, body) in enumerate(msteps):
        pdf.step_circle(C1 + 6, yy + 3, 3, i + 1)
        pdf.set_font("Helvetica", "B", 7.5)
        pdf.set_text_color(*TXT)
        pdf.set_xy(C1 + 11, yy)
        pdf.cell(0, 4.5, title)
        pdf.set_font("Helvetica", "", 6.8)
        pdf.set_text_color(*MUTED)
        pdf.set_xy(C1 + 11, yy + 4.5)
        pdf.multi_cell(COL - 14, 3.8, body)
        yy += 14.5

    # tip box
    tip_y = Y1 + ROW2_H - 19
    pdf.set_fill_color(*YELLOW)
    pdf.set_draw_color(*YELLB)
    pdf.set_line_width(0.3)
    pdf.rect(C1 + 3, tip_y, COL - 6, 16, "FD")
    pdf.set_font("Helvetica", "B", 7)
    pdf.set_text_color(100, 70, 0)
    pdf.set_xy(C1 + 5, tip_y + 2)
    pdf.cell(0, 4, "Tipp")
    pdf.set_font("Helvetica", "", 6.8)
    pdf.set_xy(C1 + 5, tip_y + 6)
    pdf.multi_cell(COL - 10, 3.8, "Ohne Kalibrierung: Rohwert ist unkalibriert.")

    # ── Col 2 - Kalibriermodus ───────────────────────────────────────────────
    pdf.card(C2, Y1, COL, ROW2_H)
    pdf.badge(C2 + 3, Y1 + 4, 26, 6, "KALIBRIERMODUS", OK)
    pdf.section_title(C2 + 31, Y1 + 4, COL - 34, "Kalibrieren")

    csteps = [
        "Kalibriermodus aktivieren",
        "Brille entfernen -> Referenz setzen",
        "Known % eingeben (echte Transmission)",
        "Brille -> Messen -> Punkt aufnehmen",
        "Schritte 3-4 mit 2-3 Werten wiederholen",
        "Kalibrierung speichern",
        "Messmodus: fertig",
    ]
    yy = Y1 + 14
    for i, s in enumerate(csteps):
        pdf.step_row(C2 + 3, yy, i + 1, s, COL - 6)
        yy += 9

    # formula
    fy = Y1 + ROW2_H - 22
    pdf.set_fill_color(245, 250, 255)
    pdf.set_draw_color(*LINE)
    pdf.set_line_width(0.3)
    pdf.rect(C2 + 3, fy, COL - 6, 19, "FD")
    pdf.set_font("Helvetica", "B", 6.5)
    pdf.set_text_color(*BRAND)
    pdf.set_xy(C2 + 5, fy + 2)
    pdf.cell(0, 4, "Kalibrierformel (Linear)")
    pdf.set_font("Courier", "B", 8)
    pdf.set_text_color(*TXT)
    pdf.set_xy(C2 + 5, fy + 7)
    pdf.cell(0, 5, "T_cal = m * T_raw + b")
    pdf.set_font("Helvetica", "", 6.5)
    pdf.set_text_color(*MUTED)
    pdf.set_xy(C2 + 5, fy + 13)
    pdf.cell(0, 4, "m=Steigung  b=Achsenabschnitt")

    # ── Col 3 - Quick-Delta ──────────────────────────────────────────────────
    pdf.card(C3, Y1, COL, ROW2_H)
    pdf.badge(C3 + 3, Y1 + 4, 28, 6, "QUICK-DELTA", (120, 60, 160))
    pdf.section_title(C3 + 33, Y1 + 4, COL - 36, "Schnellvergleich")

    pdf.set_font("Helvetica", "", 7)
    pdf.set_text_color(*MUTED)
    pdf.set_xy(C3 + 3, Y1 + 13)
    pdf.multi_cell(COL - 6, 4, "Kein Referenzglas? Quick-Delta misst relativ zur Umgebung.")

    qsteps = [
        ("Referenz aufnehmen", "Brille entfernen -> Referenz klicken."),
        ("Mit Glas messen", "Brille -> Mit Glas messen."),
        ("Quick % ablesen", "Verhaeltnis zur Referenz in %%."),
        ("Reset", "Quick Reset loescht Referenz."),
    ]
    yy = Y1 + 26
    for i, (title, body) in enumerate(qsteps):
        pdf.step_circle(C3 + 6, yy + 3, 3, i + 1, done=False)
        pdf.set_font("Helvetica", "B", 7.5)
        pdf.set_text_color(*TXT)
        pdf.set_xy(C3 + 11, yy)
        pdf.cell(0, 4.5, title)
        pdf.set_font("Helvetica", "", 6.8)
        pdf.set_text_color(*MUTED)
        pdf.set_xy(C3 + 11, yy + 4.5)
        pdf.multi_cell(COL - 14, 3.8, body)
        yy += 15.5

    # ─────────────────────────────────────────────────────────────────────────
    # ROW 3: Pinbelegung  |  Spezifikationen  |  Kalibrierung verwalten
    # ─────────────────────────────────────────────────────────────────────────
    Y2 = Y1 + ROW2_H + 4
    ROW3_H = 76

    # ── Col 1 - Pinbelegung ──────────────────────────────────────────────────
    pdf.card(C1, Y2, COL, ROW3_H)
    pdf.section_title(C1 + 3, Y2 + 4, COL - 6, "Pinbelegung (NodeMCU)")

    rows = [
        ("GPIO5 / D1", "IR-LED MOSFET Gate", BRAND),
        ("A0",         "Photodiode Analog", OK),
        ("3.3V",       "Sensor VCC", (180, 90, 0)),
        ("GND",        "Gemeinsame Masse", MUTED),
        ("D5 / GPIO14","Button UP (opt.)", MUTED),
        ("D6 / GPIO12","Button OK (opt.)", MUTED),
        ("D7 / GPIO13","Button DN (opt.)", MUTED),
    ]
    for i, (pin, desc, col) in enumerate(rows):
        ry = Y2 + 13 + i * 7.8
        bg = (248, 252, 255) if i % 2 == 0 else (255, 255, 255)
        pdf.set_fill_color(*bg)
        pdf.rect(C1 + 3, ry, COL - 6, 7.8, "F")
        pdf.set_fill_color(*col)
        pdf.set_draw_color(*col)
        pdf.rect(C1 + 4, ry + 1.5, 22, 4.5, "F")
        pdf.set_font("Helvetica", "B", 5.5)
        pdf.set_text_color(255, 255, 255)
        pdf.set_xy(C1 + 4, ry + 2)
        pdf.cell(22, 3.5, pin, align="C")
        pdf.set_font("Helvetica", "", 6.5)
        pdf.set_text_color(*TXT)
        pdf.set_xy(C1 + 28, ry + 2)
        pdf.cell(0, 3.5, desc)

    # schematic note
    sn_y = Y2 + ROW3_H - 12
    pdf.set_font("Helvetica", "I", 6.2)
    pdf.set_text_color(*MUTED)
    pdf.set_xy(C1 + 3, sn_y)
    pdf.multi_cell(COL - 6, 3.5, "IR-LED nie direkt an GPIO! MOSFET als Schalter nutzen.")

    # ── Col 2 - Spezifikationen ─────────────────────────────────────────────
    pdf.card(C2, Y2, COL, ROW3_H)
    pdf.section_title(C2 + 3, Y2 + 4, COL - 6, "Spezifikationen")

    specs = [
        ("Wellenlaenge", "850-940 nm (IR)"),
        ("Samples/Messung", "40 (gemittelt)"),
        ("WLAN", "Access Point"),
        ("IP-Adresse", "http://sunglasses.local"),
        ("Port", "HTTP 80"),
        ("Kalibrierung", "Linear, 16 Punkte max"),
        ("Speicher", "EEPROM autosave"),
        ("Versorgung", "USB 5V"),
    ]
    for i, (k, v) in enumerate(specs):
        ry = Y2 + 13 + i * 7
        pdf.set_font("Helvetica", "B", 6.8)
        pdf.set_text_color(*MUTED)
        pdf.set_xy(C2 + 3, ry)
        pdf.cell(28, 4.5, k + ":")
        pdf.set_font("Helvetica", "", 6.8)
        pdf.set_text_color(*TXT)
        pdf.set_xy(C2 + 31, ry)
        pdf.cell(COL - 34, 4.5, v)

    # warning box
    wy = Y2 + ROW3_H - 28
    pdf.set_fill_color(255, 235, 235)
    pdf.set_draw_color(220, 100, 80)
    pdf.set_line_width(0.3)
    pdf.rect(C2 + 3, wy, COL - 6, 25, "FD")
    pdf.set_font("Helvetica", "B", 7)
    pdf.set_text_color(*WARN)
    pdf.set_xy(C2 + 5, wy + 2)
    pdf.cell(0, 4, "Achtung - Spannungsgrenze")
    pdf.set_font("Helvetica", "", 6.5)
    pdf.set_text_color(120, 40, 30)
    pdf.set_xy(C2 + 5, wy + 7)
    pdf.multi_cell(COL - 10, 3.8, "A0 begrenzt auf 3.3V. Sensor vor Anschluss pruefen!")

    # ── Col 3 - Kalibrierung verwalten ─────────────────────────────────────────────────
    pdf.card(C3, Y2, COL, ROW3_H)
    pdf.section_title(C3 + 3, Y2 + 4, COL - 6, "Kalibrierung verwalten")

    mgmt = [
        ("Laden", "Datensatz waehlen und Laden klicken."),
        ("Speichern", "Speichern - Namen eingeben."),
        ("Export", "Export -> JSON herunterladen."),
        ("Werkseinstellung", "Zurueck auf Standard (Slope=1, Int=0)."),
    ]
    yy = Y2 + 14
    for i, (title, body) in enumerate(mgmt):
        pdf.set_fill_color(*BRAND)
        pdf.rect(C3 + 3, yy, 4, 4, "F")
        pdf.set_font("Helvetica", "B", 6)
        pdf.set_text_color(255,255,255)
        pdf.set_xy(C3 + 3, yy + 0.5)
        pdf.cell(4, 3, str(i+1), align="C")
        pdf.set_font("Helvetica", "B", 7.5)
        pdf.set_text_color(*TXT)
        pdf.set_xy(C3 + 9, yy)
        pdf.cell(0, 4.5, title)
        pdf.set_font("Helvetica", "", 6.8)
        pdf.set_text_color(*MUTED)
        pdf.set_xy(C3 + 9, yy + 4.5)
        pdf.multi_cell(COL - 12, 3.8, body)
        yy += 16

    # ─────────────────────────────────────────────────────────────────────────
    # FOOTER
    # ─────────────────────────────────────────────────────────────────────────
    fy2 = H - 10
    pdf.set_fill_color(*BG)
    pdf.rect(0, fy2 - 2, W, 12, "F")
    pdf.set_draw_color(*LINE)
    pdf.set_line_width(0.3)
    pdf.line(M, fy2 - 2, W - M, fy2 - 2)

    pdf.set_font("Helvetica", "", 6.5)
    pdf.set_text_color(*MUTED)
    pdf.set_xy(M, fy2 + 0.5)
    pdf.cell(0, 3.5, "IR Sunglasses Meter - ESP8266 NodeMCU v2 - WLAN: IR-Sunglasses-Meter")

    pdf.set_xy(W - M - 50, fy2 + 0.5)
    pdf.cell(50, 3.5, "http://sunglasses.local", align="R")

    # ── save ─────────────────────────────────────────────────────────────────
    out = "IR_Sunglasses_Meter_Anleitung.pdf"
    pdf.output(out)
    print("PDF erstellt: " + out)


if __name__ == "__main__":
    make_pdf()
