"""
Overlay ML (held-out val) vs traditional reconstruction, per beam energy.

Reads:
  results/energy_resolution_<p>.csv    (traditional; uses ratio fn + sqrt weighting)
  results/position_resolution_<p>.csv  (traditional)
  results/ml/eval/ml_eval_<p>.csv      (ML, zdc.ml.evaluate)

Writes results/compare_<p>.png (energy res + position res panels).
"""

import argparse
import csv
import os

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def read_csv(path):
    with open(path) as f:
        return list(csv.DictReader(f))


def trad_energy(particle, func="ratio", weight="sqrt"):
    rows = read_csv(f"results/energy_resolution_{particle}.csv")
    E, res = [], []
    for r in rows:
        if r["function"] == func and r["weighting"] == weight:
            E.append(float(r["E_beam_GeV"])); res.append(float(r["resolution_pct"]))
    o = np.argsort(E); return np.array(E)[o], np.array(res)[o]


def trad_position(particle):
    rows = read_csv(f"results/position_resolution_{particle}.csv")
    E = np.array([float(r["E_beam_GeV"]) for r in rows])
    sx = np.array([float(r["sigma_x_mm"]) for r in rows])
    o = np.argsort(E); return E[o], sx[o]


def ml_eval(particle):
    rows = read_csv(f"results/ml/eval/ml_eval_{particle}.csv")
    E = np.array([float(r["E_beam_GeV"]) for r in rows])
    eres = np.array([float(r["energy_res_pct"]) for r in rows])
    px = np.array([float(r["pos_res_x_mm"]) for r in rows])
    o = np.argsort(E); return E[o], eres[o], px[o]


def run(particle):
    tE, tEres = trad_energy(particle)
    tpE, tpx = trad_position(particle)
    mE, mEres, mpx = ml_eval(particle)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5))
    ax1.plot(tE, tEres, "ks-", label="traditional (ratio, √E)")
    ax1.plot(mE, mEres, "ro-", label="ML DeepSets (val)")
    ax1.set_xlabel("Beam Energy [GeV]"); ax1.set_ylabel("Energy Resolution [%]")
    ax1.set_title(f"{particle}: Energy Resolution"); ax1.legend(); ax1.grid(alpha=0.3)
    ax1.set_ylim(0, 30)

    ax2.plot(tpE, tpx, "ks-", label="traditional (HCAL track)")
    ax2.plot(mE, mpx, "ro-", label="ML DeepSets (val)")
    ax2.set_xlabel("Beam Energy [GeV]"); ax2.set_ylabel("Position Resolution σ_x [mm]")
    ax2.set_title(f"{particle}: Position Resolution"); ax2.legend(); ax2.grid(alpha=0.3)
    ax2.set_ylim(0, None)

    plt.tight_layout()
    out = f"results/compare_{particle}.png"
    plt.savefig(out, dpi=130); plt.close()
    print(f"-> {out}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--particle", choices=["gamma", "neutron", "both"], default="both")
    args = ap.parse_args()
    for p in (["gamma", "neutron"] if args.particle == "both" else [args.particle]):
        run(p)
