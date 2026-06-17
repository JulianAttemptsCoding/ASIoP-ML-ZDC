"""
Reproduce energy resolution & bias (slide pp.6-8).

For each (function, weighting): fit E_rec params globally, then for each beam
energy fit E_rec/E_beam with a double-sided crystal ball:
    resolution = sigma / E_beam * 100  [%]     (slide: sigma/E_beam)
    bias       = mu / E_beam                    (slide: mu/E_beam)

Outputs a CSV table and a 2-panel plot per particle.
"""

import argparse
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))
from zdc.energy import fit_energy, predict_energy, FUNCTIONS, WEIGHTINGS
from zdc.crystalball import fit_resolution


def requirement_curve(E, a, b=5.0):
    """a%/sqrt(E) (+) b%, added in quadrature."""
    return np.sqrt((a / np.sqrt(E)) ** 2 + b ** 2)


def run(particle, req_a):
    d = np.load(f"results/features_{particle}.npz")
    e_ecal, e_hcal, e_beam = d["e_ecal"], d["e_hcal"], d["e_beam"]
    energies = np.unique(e_beam)

    rows = []
    curves = {}  # (func,weight) -> (E, res, bias)
    for func in FUNCTIONS:
        for weight in WEIGHTINGS:
            params = fit_energy(e_ecal, e_hcal, e_beam, func, weight)
            e_rec = predict_energy(e_ecal, e_hcal, params, func)

            E_list, res_list, bias_list = [], [], []
            for E in energies:
                m = e_beam == E
                ratio = e_rec[m] / E
                mu, sigma, mu_e, sig_e, _ = fit_resolution(ratio)
                res = sigma * 100.0     # sigma/E_beam in %  (e_rec already /E)
                bias = mu                # mu/E_beam
                E_list.append(E); res_list.append(res); bias_list.append(bias)
                rows.append([particle, func, weight, E, res, bias])
            curves[(func, weight)] = (np.array(E_list), np.array(res_list), np.array(bias_list))

    _plot(particle, curves, energies, req_a)
    _save_csv(particle, rows)
    return rows


def _plot(particle, curves, energies, req_a):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5))
    for (func, weight), (E, res, bias) in curves.items():
        ls = "-" if weight == "sqrt" else "--"
        ax1.plot(E, res, ls, marker="o", ms=3, label=f"{weight}, {func}")
        ax2.plot(E, bias, ls, marker="o", ms=3, label=f"{weight}, {func}")

    Egrid = np.linspace(energies.min(), energies.max(), 200)
    ax1.plot(Egrid, requirement_curve(Egrid, req_a), "k-", lw=2,
             label=f"Req: {req_a:.0f}%/sqrt(E)+5%")
    ax1.set_xlabel("Beam Energy [GeV]"); ax1.set_ylabel("Energy Resolution [%]")
    ax1.set_title(f"{particle}: Energy Resolution"); ax1.legend(fontsize=7); ax1.grid(alpha=0.3)
    ax1.set_ylim(0, min(40, np.nanmax([c[1].max() for c in curves.values()]) * 1.2))

    ax2.axhline(1.0, color="k", ls=":")
    ax2.set_xlabel("Beam Energy [GeV]"); ax2.set_ylabel(r"$E_{rec}/E_{beam}$ (bias)")
    ax2.set_title(f"{particle}: Energy Bias"); ax2.legend(fontsize=7); ax2.grid(alpha=0.3)
    ax2.set_ylim(0.8, 1.5)  # focus on physics range; sub-GeV outliers run off-scale

    plt.tight_layout()
    out = f"results/energy_resolution_{particle}.png"
    plt.savefig(out, dpi=130); plt.close()
    print(f"-> {out}")


def _save_csv(particle, rows):
    out = f"results/energy_resolution_{particle}.csv"
    with open(out, "w") as f:
        f.write("particle,function,weighting,E_beam_GeV,resolution_pct,bias\n")
        for r in rows:
            f.write(f"{r[0]},{r[1]},{r[2]},{r[3]:.3f},{r[4]:.4f},{r[5]:.4f}\n")
    print(f"-> {out}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--particle", choices=["gamma", "neutron", "both"], default="both")
    args = ap.parse_args()

    # Requirement constants from slides: gamma 20%/sqrt(E)+5%, neutron 35%/sqrt(E)+5%
    req = {"gamma": 20.0, "neutron": 35.0}
    targets = ["gamma", "neutron"] if args.particle == "both" else [args.particle]
    for p in targets:
        print(f"=== {p} energy resolution ===")
        run(p, req[p])
