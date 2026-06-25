"""Generate QA preview plots from the two uploaded example ROOT files using uproot.
This is only for this execution environment where ROOT is unavailable; the real
workflow is scripts/zdc_reco_browser.C + ROOT TBrowser.
"""
from __future__ import annotations
import json
from pathlib import Path
import numpy as np
import awkward as ak
import uproot
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
PLOTS = ROOT / "plots"
QA = ROOT / "qa"
PLOTS.mkdir(exist_ok=True)
QA.mkdir(exist_ok=True)

BR = {
    "ecal": "EcalFarForwardZDCHits/EcalFarForwardZDCHits.energy",
    "hcal": "HcalFarForwardZDCHits/HcalFarForwardZDCHits.energy",
    "px": "MCParticles/MCParticles.momentum.x",
    "py": "MCParticles/MCParticles.momentum.y",
    "pz": "MCParticles/MCParticles.momentum.z",
    "mass": "MCParticles/MCParticles.mass",
    "pdg": "MCParticles/MCParticles.PDG",
}

def read(path: Path):
    t = uproot.open(path)["events"]
    arr = t.arrays(list(BR.values()), library="ak")
    ecal = ak.to_numpy(ak.sum(arr[BR["ecal"]], axis=1))
    hcal = ak.to_numpy(ak.sum(arr[BR["hcal"]], axis=1))
    px = ak.to_numpy(arr[BR["px"]][:, 0])
    py = ak.to_numpy(arr[BR["py"]][:, 0])
    pz = ak.to_numpy(arr[BR["pz"]][:, 0])
    mass = ak.to_numpy(arr[BR["mass"]][:, 0])
    pdg = ak.to_numpy(arr[BR["pdg"]][:, 0])
    beam = np.sqrt(px * px + py * py + pz * pz + mass * mass)
    return {"path": str(path), "entries": len(beam), "ecal": ecal, "hcal": hcal, "beam": beam, "pdg": pdg}

gamma_files = sorted((DATA / "gamma").glob("outfile_gamma*.root"))
neutron_files = sorted((DATA / "neutron").glob("outfile_neutron*.root"))
records = [("gamma", read(p)) for p in gamma_files] + [("neutron", read(p)) for p in neutron_files]

stats = []
for particle, d in records:
    ecal_over = d["ecal"] / d["beam"]
    hcal_over = d["hcal"] / d["beam"]
    all_over = (d["ecal"] + d["hcal"]) / d["beam"]
    stats.append({
        "particle": particle,
        "file": Path(d["path"]).name,
        "entries": int(d["entries"]),
        "pdg_first": int(d["pdg"][0]),
        "beam_mean_GeV": float(np.mean(d["beam"])),
        "beam_min_GeV": float(np.min(d["beam"])),
        "beam_max_GeV": float(np.max(d["beam"])),
        "ecal_over_beam_mean": float(np.mean(ecal_over)),
        "hcal_over_beam_mean": float(np.mean(hcal_over)),
        "all_over_beam_mean": float(np.mean(all_over)),
    })

(QA / "uploaded_example_stats.json").write_text(json.dumps(stats, indent=2), encoding="utf-8")

# Energy dump preview — only uploaded points; full slide appears after all files are added and ROOT macro is run.
fig, axes = plt.subplots(2, 3, figsize=(14, 8))
quantities = [
    ("ecal", "Beam VS ECAL/Beam", "ECal / Beam"),
    ("hcal", "Beam VS HCAL/Beam", "HCal / Beam"),
    ("all", "Beam VS\n(ECAL + HCAL)/Beam", "(ECal + HCal) / Beam"),
]
for row, particle in enumerate(["gamma", "neutron"]):
    subset = [(p, d) for p, d in records if p == particle]
    for col, (q, title, ylabel) in enumerate(quantities):
        ax = axes[row, col]
        xs, ys, yerrs = [], [], []
        for _, d in subset:
            if q == "ecal": vals = d["ecal"] / d["beam"]
            elif q == "hcal": vals = d["hcal"] / d["beam"]
            else: vals = (d["ecal"] + d["hcal"]) / d["beam"]
            xs.append(np.mean(d["beam"]))
            ys.append(np.mean(vals))
            yerrs.append(np.std(vals, ddof=1) / np.sqrt(len(vals)))
        ax.errorbar(xs, ys, yerr=yerrs, fmt="o", fillstyle="none", capsize=3)
        if row == 0: ax.set_title(title, fontweight="bold")
        ax.set_xlabel("Beam (GeV)")
        ax.set_ylabel(ylabel)
        ax.grid(True, linestyle=":", linewidth=0.7)
        if particle == "gamma": ax.set_xlim(0, 50)
        else: ax.set_xlim(0, 350)
        if row == 0 and col == 0:
            ax.text(0.05, 0.08, "uploaded gamma example", transform=ax.transAxes, fontweight="bold")
        if row == 1 and col == 0:
            ax.text(0.05, 0.08, "uploaded neutron example", transform=ax.transAxes, fontweight="bold")
fig.suptitle("Energy Dump — uploaded examples only", fontsize=24)
fig.tight_layout()
fig.savefig(PLOTS / "uploaded_example_energy_dump_preview.png", dpi=180)
plt.close(fig)

# Gamma raw Evis/Ebeam histogram preview.
if gamma_files:
    d = records[0][1]
    ratio = (d["ecal"] + d["hcal"]) / d["beam"]
    fig, ax = plt.subplots(figsize=(7, 5))
    ax.hist(ratio, bins=70, histtype="step", linewidth=2)
    ax.set_title("1 GeV gamma — raw visible energy ratio preview")
    ax.set_xlabel("(ECal + HCal) / Ebeam")
    ax.set_ylabel("Count")
    ax.grid(True, linestyle=":", linewidth=0.7)
    fig.tight_layout()
    fig.savefig(PLOTS / "uploaded_example_gamma1GeV_raw_visible_ratio_preview.png", dpi=180)
    plt.close(fig)

report = [
    "# QA report — uploaded example files",
    "",
    "This QA was generated with `uproot` because ROOT/PyROOT is not installed in the execution sandbox. The repo's actual plotting path is `scripts/zdc_reco_browser.C`, which writes ROOT canvases for TBrowser.",
    "",
    "## Files read",
]
for s in stats:
    report.append(f"- `{s['file']}`: entries={s['entries']}, PDG={s['pdg_first']}, mean beam={s['beam_mean_GeV']:.6g} GeV, ECal/beam={s['ecal_over_beam_mean']:.6g}, HCal/beam={s['hcal_over_beam_mean']:.6g}, total/beam={s['all_over_beam_mean']:.6g}")
report += [
    "",
    "## Limitation",
    "Only one gamma and one neutron raw file were uploaded here, so the full multi-energy slide-7/8 resolution/bias curves cannot be numerically regenerated in this sandbox. Add the remaining raw files to `data/` and run `bash scripts/run_all.sh` in a ROOT environment.",
]
(QA / "QA_REPORT.md").write_text("\n".join(report) + "\n", encoding="utf-8")
print("Wrote QA previews and stats")
