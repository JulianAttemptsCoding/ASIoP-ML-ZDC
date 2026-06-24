"""
make_presentation.py
Generates ZDC Energy Reconstruction presentation (.pptx)
Run: python scripts/make_presentation.py
"""

from pptx import Presentation
from pptx.util import Inches, Pt, Emu
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN
from pptx.util import Inches, Pt
import os

BASE = r"C:\Users\Julia\OneDrive\Desktop\coding\ASIoP\Explore MC-sim data for ZDC"
OUT  = os.path.join(BASE, "plots", "ZDC_Energy_Reconstruction.pptx")

PLOT = {
    "slide5": os.path.join(BASE, "plots", "slide5_energy_dump.png"),
    "slide6": os.path.join(BASE, "plots", "slide6_erec_dist.png"),
    "slide7": os.path.join(BASE, "plots", "slide7_gamma_resolution.png"),
    "slide8": os.path.join(BASE, "plots", "slide8_neutron_resolution.png"),
}

# ── colours ──────────────────────────────────────────────────────────────────
C_DARK   = RGBColor(0x1A, 0x1A, 0x2E)   # near-black navy
C_MID    = RGBColor(0x16, 0x21, 0x3E)   # dark blue
C_ACCENT = RGBColor(0x0F, 0x89, 0xCA)   # bright blue
C_WHITE  = RGBColor(0xFF, 0xFF, 0xFF)
C_LIGHT  = RGBColor(0xD0, 0xE8, 0xF5)   # pale blue text
C_YELLOW = RGBColor(0xFF, 0xD7, 0x00)
C_GREEN  = RGBColor(0x5C, 0xD6, 0x5C)
C_GRAY   = RGBColor(0xCC, 0xCC, 0xCC)

W = Inches(13.33)   # widescreen 16:9
H = Inches(7.5)

prs = Presentation()
prs.slide_width  = W
prs.slide_height = H

BLANK = prs.slide_layouts[6]   # completely blank


# ── helpers ──────────────────────────────────────────────────────────────────

def slide():
    s = prs.slides.add_slide(BLANK)
    bg = s.background.fill
    bg.solid()
    bg.fore_color.rgb = C_DARK
    return s

def box(s, x, y, w, h, text="", size=18, bold=False, color=C_WHITE,
        align=PP_ALIGN.LEFT, bg=None, border=None, wrap=True):
    tf = s.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    if bg:
        tf.fill.solid(); tf.fill.fore_color.rgb = bg
    if border:
        tf.line.color.rgb = border; tf.line.width = Pt(1)
    frame = tf.text_frame
    frame.word_wrap = wrap
    frame.auto_size = None
    p = frame.paragraphs[0]
    p.alignment = align
    run = p.add_run()
    run.text = text
    run.font.size  = Pt(size)
    run.font.bold  = bold
    run.font.color.rgb = color
    return tf

def mbox(s, x, y, w, h, lines, size=16, color=C_WHITE,
         align=PP_ALIGN.LEFT, bg=None, border=None, leading_pt=None):
    """Multi-paragraph textbox. lines = list of (text, bold, color_override_or_None)"""
    tf = s.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    if bg:
        tf.fill.solid(); tf.fill.fore_color.rgb = bg
    if border:
        tf.line.color.rgb = border; tf.line.width = Pt(1)
    frame = tf.text_frame
    frame.word_wrap = True
    for i, line in enumerate(lines):
        text, bold, col = line
        p = frame.paragraphs[0] if i == 0 else frame.add_paragraph()
        p.alignment = align
        if leading_pt:
            p.space_before = Pt(leading_pt)
        run = p.add_run()
        run.text = text
        run.font.size  = Pt(size)
        run.font.bold  = bold
        run.font.color.rgb = col if col else color
    return tf

def hbar(s, y, color=C_ACCENT, thickness=0.04):
    bar = s.shapes.add_shape(1, Inches(0), Inches(y), W, Inches(thickness))
    bar.fill.solid(); bar.fill.fore_color.rgb = color
    bar.line.fill.background()

def img(s, path, x, y, w):
    s.shapes.add_picture(path, Inches(x), Inches(y), width=Inches(w))

def section_header(s, title, subtitle=""):
    hbar(s, 0, C_MID, 7.5)
    hbar(s, 0, C_ACCENT, 0.08)
    box(s, 0.3, 0.12, 12.5, 0.6, title, size=32, bold=True, color=C_WHITE)
    if subtitle:
        box(s, 0.3, 0.75, 12.5, 0.4, subtitle, size=17, color=C_LIGHT)
    hbar(s, 1.15, C_ACCENT, 0.03)


# ═══════════════════════════════════════════════════════════════════════════
# SLIDE 1 — TITLE
# ═══════════════════════════════════════════════════════════════════════════
s1 = slide()
# full-width accent bar at top
hbar(s1, 0, C_ACCENT, 0.12)
# institution / project line
box(s1, 0.4, 0.18, 12, 0.4,
    "EIC  ·  Zero Degree Calorimeter  ·  Monte Carlo Study",
    size=14, color=C_LIGHT)
# main title
mbox(s1, 0.4, 0.7, 12, 1.5, [
    ("ZDC Energy Reconstruction", True, C_WHITE),
], size=42)
mbox(s1, 0.4, 1.75, 12, 0.6, [
    ("Gamma (0.7 – 40 GeV)   |   Neutron (20 – 300 GeV)", False, C_LIGHT),
], size=20)

hbar(s1, 2.55, C_ACCENT, 0.04)

mbox(s1, 0.4, 2.8, 8, 3.5, [
    ("Background", True,  C_ACCENT),
    ("The Electron-Ion Collider (EIC) will collide electrons with protons and nuclei. "
     "The Zero Degree Calorimeter (ZDC) sits at 0° and catches neutral particles "
     "(neutrons, photons) that travel straight through the bending magnets.", False, C_WHITE),
    ("", False, None),
    ("Goal", True, C_ACCENT),
    ("Determine how well the ZDC can measure the energy of these particles using "
     "Monte Carlo simulations, and check if performance meets physics requirements.", False, C_WHITE),
], size=16, leading_pt=4)

# right side mini-diagram
mbox(s1, 9.2, 2.8, 3.8, 3.5, [
    ("Detector layers", True,  C_YELLOW),
    ("", False, None),
    ("ECAL  —  LYSO crystals + SiPM", False, C_LIGHT),
    ("Detects: photons, electrons", False, C_GRAY),
    ("", False, None),
    ("HCAL  —  Steel + scintillator", False, C_LIGHT),
    ("Detects: neutrons, hadrons",    False, C_GRAY),
    ("", False, None),
    ("Both layers needed to fully", False, C_WHITE),
    ("reconstruct particle energy.",  False, C_WHITE),
], size=14, bg=C_MID, border=C_ACCENT, leading_pt=2)

hbar(s1, 7.3, C_ACCENT, 0.12)
box(s1, 0.3, 7.1, 12, 0.35,
    "ASIoP  ·  Explore MC-sim data for ZDC",
    size=12, color=C_GRAY, align=PP_ALIGN.CENTER)


# ═══════════════════════════════════════════════════════════════════════════
# SLIDE 2 — RAW DATA & VARIABLE DEFINITIONS
# ═══════════════════════════════════════════════════════════════════════════
s2 = slide()
section_header(s2, "From Raw Data to Graph Axes",
               "Every variable defined — step by step")

mbox(s2, 0.3, 1.3, 5.9, 5.8, [
    ("Step 1 — Simulation output (raw data)", True, C_YELLOW),
    ("Each simulated particle event gives:", False, C_LIGHT),
    ("", False, None),
    ("  E_beam   True beam energy (GeV). Set by simulation. This is what we want to reconstruct.", False, C_WHITE),
    ("  E_ECAL   Energy deposited in the ECAL layer (GeV). Measured signal.", False, C_WHITE),
    ("  E_HCAL   Energy deposited in the HCAL layer (GeV). Measured signal.", False, C_WHITE),
    ("", False, None),
    ("These three numbers exist for every event. All other quantities are derived.", False, C_GRAY),
    ("", False, None),
    ("Step 2 — Energy reconstruction", True, C_YELLOW),
    ("Combine the two detector signals into one energy estimate:", False, C_LIGHT),
    ("", False, None),
    ("  E_rec  =  p0 + p1·E_ECAL + p2·E_HCAL", False, C_GREEN),
    ("", False, None),
    ("p0, p1, p2 are fit parameters found by least-squares regression "
     "across all simulated events. Three regression methods are used:", False, C_WHITE),
    ("  m0 — Linear    m1 — Ratio    m2 — Quadratic", False, C_LIGHT),
    ("Two weighting schemes:", False, C_WHITE),
    ("  w0 — all energies equal weight", False, C_LIGHT),
    ("  w1 — weight each event by √E_beam  (emphasises low-energy accuracy)", False, C_LIGHT),
], size=13, leading_pt=1)

mbox(s2, 6.5, 1.3, 6.5, 5.8, [
    ("Step 3 — Per-event ratio", True, C_YELLOW),
    ("", False, None),
    ("  r  =  E_rec / E_beam", False, C_GREEN),
    ("", False, None),
    ("r = 1.0  →  perfect reconstruction", False, C_WHITE),
    ("r > 1.0  →  overestimated", False, C_WHITE),
    ("r < 1.0  →  underestimated", False, C_WHITE),
    ("", False, None),
    ("Step 4 — Histogram & DCB fit", True, C_YELLOW),
    ("Collect r for all events at one beam energy.", False, C_WHITE),
    ("Fit with Double Crystal Ball (bell curve with power-law tails).", False, C_WHITE),
    ("Extract two numbers:", False, C_LIGHT),
    ("", False, None),
    ("  μ  (mu)    =  mean of fit  →  bias", False, C_GREEN),
    ("  σ  (sigma) =  width of fit →  resolution", False, C_GREEN),
    ("", False, None),
    ("Step 5 — Graph axes", True, C_YELLOW),
    ("", False, None),
    ("Slide 5 x-axis:  E_beam (GeV)", False, C_WHITE),
    ("Slide 5 y-axis:  mean(E_sub / E_beam)  per beam energy", False, C_WHITE),
    ("Slide 6 x-axis:  r = E_rec / E_beam  (single energy)", False, C_WHITE),
    ("Slide 6 y-axis:  event count", False, C_WHITE),
    ("Slides 7-8 x:    E_beam (GeV)", False, C_WHITE),
    ("Slides 7-8 y:    σ (resolution)  or  μ (bias)  from DCB fit", False, C_WHITE),
], size=13, bg=C_MID, border=C_ACCENT, leading_pt=1)


# ═══════════════════════════════════════════════════════════════════════════
# SLIDE 3 — METHOD & THINKING
# ═══════════════════════════════════════════════════════════════════════════
s3 = slide()
section_header(s3, "Method & Thinking",
               "How we go from signals to performance numbers")

mbox(s3, 0.3, 1.3, 4.0, 5.8, [
    ("Why two detectors?", True, C_YELLOW),
    ("Gammas deposit ~30–50% of energy in ECAL, almost none in HCAL. "
     "Neutrons are the opposite. Using both signals together gives a more complete picture.", False, C_WHITE),
    ("", False, None),
    ("Why regression?", True, C_YELLOW),
    ("Raw signals are not equal to energy — there is leakage, dead material, "
     "and non-linearity. Regression learns the correction from simulation.", False, C_WHITE),
    ("", False, None),
    ("Why √E weighting?", True, C_YELLOW),
    ("Equal weighting lets high-energy events dominate the fit "
     "(they have bigger absolute residuals). "
     "√E weighting balances accuracy across the full energy range.", False, C_WHITE),
], size=14, leading_pt=3)

mbox(s3, 4.5, 1.3, 4.4, 5.8, [
    ("Why DCB fit?", True, C_YELLOW),
    ("Real detector distributions have heavier tails than a pure Gaussian — "
     "energy leaking out the sides creates a low-side tail, "
     "shower fluctuations create a high-side tail. "
     "The Double Crystal Ball handles both.", False, C_WHITE),
    ("", False, None),
    ("The requirement curve", True, C_YELLOW),
    ("σ(E)  =  p0 / √E  +  0.05", False, C_GREEN),
    ("Physics demands resolution better than this threshold.", False, C_WHITE),
    ("  Gamma:   p0 = 0.20  (new target)", False, C_LIGHT),
    ("  Neutron: p0 = 0.35  (new target)", False, C_LIGHT),
    ("The 0.05 floor is irreducible — calibration and geometry limits.", False, C_GRAY),
], size=14, leading_pt=3)

mbox(s3, 9.1, 1.3, 4.0, 5.8, [
    ("6 method combinations", True, C_YELLOW),
    ("", False, None),
    ("m0 w0 — Linear, Equal",        False, C_WHITE),
    ("m1 w0 — Ratio,  Equal",        False, C_WHITE),
    ("m2 w0 — Quadratic, Equal",     False, C_WHITE),
    ("m0 w1 — Linear, √E-weighted",  False, C_WHITE),
    ("m1 w1 — Ratio,  √E-weighted",  False, C_WHITE),
    ("m2 w1 — Quadratic, √E-weighted", False, C_WHITE),
    ("", False, None),
    ("All six appear on resolution", False, C_LIGHT),
    ("and bias plots. Compare them", False, C_LIGHT),
    ("to find best-performing method.", False, C_LIGHT),
    ("", False, None),
    ("Output files:", True, C_YELLOW),
    ("res2_1_m{0-2}_w{0-1}_{particle}.root", False, C_GREEN),
    ("Each stores gReso_dcb and", False, C_WHITE),
    ("gBias_dcb — one point per", False, C_WHITE),
    ("beam energy tested.", False, C_WHITE),
], size=13, bg=C_MID, border=C_ACCENT, leading_pt=2)


# ═══════════════════════════════════════════════════════════════════════════
# SLIDE 4 — GRAPH: ENERGY DUMP (slide 5)
# ═══════════════════════════════════════════════════════════════════════════
s4 = slide()
section_header(s4, "Energy Deposition vs Beam Energy",
               "How much of the particle's energy does each detector layer actually see?")

img(s4, PLOT["slide5"], 0.2, 1.25, 9.0)

mbox(s4, 9.4, 1.3, 3.7, 5.8, [
    ("What the axes mean", True, C_YELLOW),
    ("", False, None),
    ("x-axis:  E_beam (GeV)", False, C_LIGHT),
    ("True simulated beam energy.", False, C_WHITE),
    ("", False, None),
    ("y-axis:  E_sub / E_beam", False, C_LIGHT),
    ("Fraction of beam energy seen", False, C_WHITE),
    ("by ECAL, HCAL, or both.", False, C_WHITE),
    ("Averaged over all events at", False, C_WHITE),
    ("that beam energy.", False, C_WHITE),
    ("", False, None),
    ("Observations", True, C_YELLOW),
    ("", False, None),
    ("Gamma ECAL: 0.5 → 0.15", False, C_WHITE),
    ("Falls with energy (saturation).", False, C_GRAY),
    ("HCAL: tiny (10⁻³ scale).", False, C_GRAY),
    ("", False, None),
    ("Neutron ECAL: near zero.", False, C_WHITE),
    ("HCAL: 0.01–0.02, rises slowly.", False, C_GRAY),
    ("Neither detector alone is enough.", False, C_GRAY),
], size=13, bg=C_MID, border=C_ACCENT, leading_pt=2)


# ═══════════════════════════════════════════════════════════════════════════
# SLIDE 5 — GRAPH: E_rec DISTRIBUTION (slide 6)
# ═══════════════════════════════════════════════════════════════════════════
s5 = slide()
section_header(s5, "E_rec / E_beam Distribution  —  1 GeV Gamma",
               "The shape of reconstructed energy tells us resolution and bias")

img(s5, PLOT["slide6"], 1.5, 1.2, 7.0)

mbox(s5, 8.8, 1.3, 4.3, 5.8, [
    ("What the axes mean", True, C_YELLOW),
    ("", False, None),
    ("x-axis:  r = E_rec / E_beam", False, C_LIGHT),
    ("Reconstructed energy divided by", False, C_WHITE),
    ("true beam energy, per event.", False, C_WHITE),
    ("r = 1.0 is perfect.", False, C_WHITE),
    ("", False, None),
    ("y-axis:  Counts", False, C_LIGHT),
    ("How many events fell in each r bin.", False, C_WHITE),
    ("", False, None),
    ("Red curve:  DCB fit", False, C_LIGHT),
    ("Bell curve with heavier tails.", False, C_WHITE),
    ("Fit extracts μ and σ.", False, C_WHITE),
    ("", False, None),
    ("Stats box", True, C_YELLOW),
    ("Mean  = 1.01  →  bias near 0%", False, C_GREEN),
    ("Sigma = 0.15  →  15% resolution", False, C_WHITE),
    ("                at 1 GeV (expected)", False, C_GRAY),
    ("Chi2/NDF = 0.78  →  good fit", False, C_WHITE),
    ("", False, None),
    ("Note: left tail = energy leakage.", False, C_GRAY),
    ("Right tail = shower fluctuations.", False, C_GRAY),
], size=13, bg=C_MID, border=C_ACCENT, leading_pt=2)


# ═══════════════════════════════════════════════════════════════════════════
# SLIDE 6 — GRAPH: GAMMA RESOLUTION & BIAS (slide 7)
# ═══════════════════════════════════════════════════════════════════════════
s6 = slide()
section_header(s6, "Gamma:  Resolution & Bias vs Beam Energy  (0.7 – 40 GeV)",
               "Does the detector meet requirements? How accurate is reconstruction?")

img(s6, PLOT["slide7"], 0.1, 1.2, 13.0)

mbox(s6, 0.3, 5.8, 6.2, 1.5, [
    ("Resolution (left panel):", True, C_YELLOW),
    ("y = σ from DCB fit.  x = E_beam.  "
     "Dashed lines = requirements  σ = p0/√E + 0.05.  "
     "All methods fall below new requirement (p0=0.20) → detector passes.", False, C_WHITE),
], size=13, bg=C_MID, border=C_ACCENT)

mbox(s6, 6.7, 5.8, 6.3, 1.5, [
    ("Bias (centre panel):", True, C_YELLOW),
    ("y = μ from DCB fit.  Ideal = 1.0.  "
     "Gamma bias is well-behaved across all methods (0.90–1.06).  "
     "Small dip at 0.7 GeV for quadratic equal-weighted (m2 w0).", False, C_WHITE),
], size=13, bg=C_MID, border=C_ACCENT)


# ═══════════════════════════════════════════════════════════════════════════
# SLIDE 7 — GRAPH: NEUTRON RESOLUTION & BIAS (slide 8)
# ═══════════════════════════════════════════════════════════════════════════
s7 = slide()
section_header(s7, "Neutron:  Resolution & Bias vs Beam Energy  (20 – 300 GeV)",
               "Equal-weighting struggles at low energy — sqrt-E weighting fixes it")

img(s7, PLOT["slide8"], 0.1, 1.2, 13.0)

mbox(s7, 0.3, 5.8, 6.2, 1.5, [
    ("Resolution (left panel):", True, C_YELLOW),
    ("y = σ from DCB fit.  x = E_beam.  "
     "√E-weighted methods (open markers) meet the new requirement (p0=0.35) at most energies.  "
     "Equal-weighted methods are worse, especially below 50 GeV.", False, C_WHITE),
], size=13, bg=C_MID, border=C_ACCENT)

mbox(s7, 6.7, 5.8, 6.3, 1.5, [
    ("Bias (centre panel):", True, C_YELLOW),
    ("y = μ from DCB fit.  Equal-weighted methods: μ up to 2–3.5 at 20 GeV  "
     "(regression dominated by high-energy events, fails to extrapolate low).  "
     "√E-weighted: μ ≈ 1.1–1.3 at 20 GeV — far more controlled.", False, C_WHITE),
], size=13, bg=C_MID, border=C_ACCENT)


# ═══════════════════════════════════════════════════════════════════════════
# SLIDE 8 — OBSERVATIONS
# ═══════════════════════════════════════════════════════════════════════════
s8 = slide()
section_header(s8, "Observations", "What the data tells us")

mbox(s8, 0.4, 1.3, 5.9, 5.8, [
    ("Energy deposition", True, C_YELLOW),
    ("Gammas are almost entirely ECAL particles — HCAL contribution is 1000× smaller. "
     "Neutrons are HCAL particles with essentially zero ECAL signal. "
     "Neither sub-detector alone is sufficient — both are required for reconstruction.", False, C_WHITE),
    ("", False, None),
    ("Reconstruction distribution", True, C_YELLOW),
    ("At 1 GeV gamma: mean = 1.01 (near-perfect accuracy), sigma = 0.15 (15% resolution). "
     "The DCB fit captures the asymmetric tails well (chi2/NDF ≈ 0.78). "
     "The left tail is caused by leakage; the right by upward shower fluctuations.", False, C_WHITE),
], size=15, leading_pt=4)

mbox(s8, 6.6, 1.3, 6.4, 5.8, [
    ("Gamma performance", True, C_YELLOW),
    ("All six reconstruction methods meet the new resolution requirement "
     "(20%/√E + 5%) across 0.7–40 GeV. Bias is controlled within ±6%. "
     "Linear and quadratic methods perform similarly — ratio method slightly worse at low energy.", False, C_WHITE),
    ("", False, None),
    ("Neutron performance", True, C_YELLOW),
    ("√E-weighted methods are clearly superior. "
     "Equal-weighted bias reaches 3.5× at 20 GeV — reconstruction is unreliable there. "
     "√E-weighted bias stays below 1.35× across all energies tested. "
     "Resolution with √E-weighting is close to the new requirement (35%/√E + 5%) "
     "but may need further tuning at low energies.", False, C_WHITE),
], size=15, bg=C_MID, border=C_ACCENT, leading_pt=4)


# ═══════════════════════════════════════════════════════════════════════════
# SLIDE 9 — OUTLOOK
# ═══════════════════════════════════════════════════════════════════════════
s9 = slide()
section_header(s9, "Outlook", "What comes next")

items = [
    ("Optimise neutron at low energy",
     "The 20–50 GeV range shows the largest bias and worst resolution. "
     "Consider energy-dependent regression coefficients or a neural-network-based approach.",
     C_ACCENT),
    ("Validate with full EIC simulation",
     "Current MC uses idealized geometry. Full GEANT4 simulation with beam backgrounds, "
     "crossing angle, and realistic hit digitization will stress-test these results.",
     C_ACCENT),
    ("Extend to ML regression",
     "Linear regression is a baseline. Gradient-boosted trees or a simple feed-forward network "
     "using ECAL+HCAL hit maps (not just sums) could reduce σ and bias further.",
     C_ACCENT),
    ("Cross-check with data",
     "Once EIC test-beam data is available, compare MC resolution to real measurements "
     "to validate simulation accuracy and calibrate systematic uncertainties.",
     C_ACCENT),
]

for i, (title, body, col) in enumerate(items):
    row = i // 2
    col_idx = i % 2
    x = 0.4 + col_idx * 6.5
    y = 1.4 + row * 2.8
    mbox(s9, x, y, 6.1, 2.5, [
        (title, True, C_YELLOW),
        (body,  False, C_WHITE),
    ], size=14, bg=C_MID, border=col, leading_pt=4)


# ── save ─────────────────────────────────────────────────────────────────────
prs.save(OUT)
print(f"[OK] Saved: {OUT}")
