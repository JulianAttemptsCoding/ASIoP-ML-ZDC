"""
Reproduce position resolution & bias (slide pp.10-16).

HCAL-only track reconstruction: energy-weighted layer positions, pol1 fit with
hit-and-energy layer weighting, residual at energy-weighted reference z.

    resolution = sigma of residual distribution [mm]
    bias       = mean of residual distribution  [mm]

Outputs CSV + 2-panel plot (sigma_x/sigma_y vs E ; bias_x/bias_y vs E).
"""

import argparse
import glob
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy.stats import norm

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))
from zdc.io import read_hcal_hits
from zdc.position import reconstruct_file
from zdc.beam import beam_energy

DATA = {
    "gamma":   "20260421_gamma_LYSO_diffAngle",
    "neutron": "20260324_neutron_LYSO_diffAngle",
}


def gaussian_sigma(residuals, n_sigma=3.0):
    """Iterative Gaussian fit (mean, sigma) with simple outlier trimming."""
    r = residuals[np.isfinite(residuals)]
    if len(r) < 20:
        return np.nan, np.nan
    mu, sigma = np.median(r), r.std()
    for _ in range(3):
        m = np.abs(r - mu) < n_sigma * sigma
        if m.sum() < 20:
            break
        mu, sigma = norm.fit(r[m])
    return mu, sigma


def run(particle, data_root, scheme, n_layer_cut):
    folder = os.path.join(data_root, DATA[particle])
    files = sorted(glob.glob(os.path.join(folder, "outfile_*.root")))

    rows = []
    E, sx, sy, bx, by = [], [], [], [], []
    for path in files:
        ebeam = beam_energy(path, particle)
        events, vertex, momentum = read_hcal_hits(path)
        rx, ry = reconstruct_file(events, vertex, momentum, scheme, n_layer_cut)
        mux, sigx = gaussian_sigma(rx)
        muy, sigy = gaussian_sigma(ry)
        E.append(ebeam); sx.append(sigx); sy.append(sigy); bx.append(mux); by.append(muy)
        rows.append([particle, ebeam, sigx, sigy, mux, muy, len(rx)])
        print(f"  E={ebeam:7.2f} GeV  sigma_x={sigx:6.2f}  sigma_y={sigy:6.2f}  "
              f"bias_x={mux:6.2f}  bias_y={muy:6.2f}  (n={len(rx)})")

    order = np.argsort(E)
    E = np.array(E)[order]
    sx, sy = np.array(sx)[order], np.array(sy)[order]
    bx, by = np.array(bx)[order], np.array(by)[order]

    _plot(particle, E, sx, sy, bx, by)
    _save_csv(particle, rows)
    return rows


def _plot(particle, E, sx, sy, bx, by):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5))
    ax1.plot(E, sx, "ro-", label=r"$\sigma_x$ (xz)")
    ax1.plot(E, sy, "bs-", label=r"$\sigma_y$ (yz)")
    ax1.set_xlabel("Beam Energy [GeV]"); ax1.set_ylabel("Position Resolution [mm]")
    ax1.set_title(f"{particle}: Position Resolution"); ax1.legend(); ax1.grid(alpha=0.3)
    ax1.set_ylim(0, None)

    ax2.plot(E, bx, "ro-", label=r"$\mu_x$ (xz)")
    ax2.plot(E, by, "bs-", label=r"$\mu_y$ (yz)")
    ax2.axhline(0, color="k", ls=":")
    ax2.set_xlabel("Beam Energy [GeV]"); ax2.set_ylabel("Position Bias [mm]")
    ax2.set_title(f"{particle}: Position Bias"); ax2.legend(); ax2.grid(alpha=0.3)

    plt.tight_layout()
    out = f"results/position_resolution_{particle}.png"
    plt.savefig(out, dpi=130); plt.close()
    print(f"-> {out}")


def _save_csv(particle, rows):
    out = f"results/position_resolution_{particle}.csv"
    with open(out, "w") as f:
        f.write("particle,E_beam_GeV,sigma_x_mm,sigma_y_mm,bias_x_mm,bias_y_mm,n_events\n")
        for r in rows:
            f.write(f"{r[0]},{r[1]:.3f},{r[2]:.4f},{r[3]:.4f},{r[4]:.4f},{r[5]:.4f},{r[6]}\n")
    print(f"-> {out}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--data_root", default="to2026Summer")
    ap.add_argument("--particle", choices=["gamma", "neutron", "both"], default="both")
    ap.add_argument("--scheme", default="energy-and-hit",
                    help="layer weighting: linear-energy|log-energy|hit-count|energy-and-hit")
    ap.add_argument("--n_layer_cut", type=int, default=None,
                    help="keep layers within +/- N of most energetic (default: all 64)")
    args = ap.parse_args()

    targets = ["gamma", "neutron"] if args.particle == "both" else [args.particle]
    for p in targets:
        print(f"=== {p} position resolution (scheme={args.scheme}) ===")
        run(p, args.data_root, args.scheme, args.n_layer_cut)
