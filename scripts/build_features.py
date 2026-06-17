"""
Extract per-event scalar features from all gamma + neutron ROOT files
into a single compressed table for energy-regression studies.

Output: results/features_<particle>.npz with columns
        e_ecal, e_hcal, n_ecal, n_hcal, e_beam, vx,vy,vz, px,py,pz
"""

import argparse
import glob
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))
from zdc.io import read_event_scalars
from zdc.beam import beam_energy

DATA = {
    "gamma":   "to2026Summer/20260421_gamma_LYSO_diffAngle",
    "neutron": "to2026Summer/20260324_neutron_LYSO_diffAngle",
}


def build(particle, data_root):
    folder = os.path.join(data_root, os.path.basename(DATA[particle]))
    files = sorted(glob.glob(os.path.join(folder, "outfile_*.root")))
    if not files:
        raise FileNotFoundError(f"No outfile_*.root in {folder}")

    cols = {k: [] for k in
            ["e_ecal", "e_hcal", "n_ecal", "n_hcal", "e_beam",
             "vx", "vy", "vz", "px", "py", "pz"]}

    for path in files:
        ebeam = beam_energy(path, particle)
        s = read_event_scalars(path)
        n = len(s.e_ecal)
        cols["e_ecal"].append(s.e_ecal)
        cols["e_hcal"].append(s.e_hcal)
        cols["n_ecal"].append(s.n_ecal_hits)
        cols["n_hcal"].append(s.n_hcal_hits)
        cols["e_beam"].append(np.full(n, ebeam))
        cols["vx"].append(s.vertex[:, 0]); cols["vy"].append(s.vertex[:, 1]); cols["vz"].append(s.vertex[:, 2])
        cols["px"].append(s.momentum[:, 0]); cols["py"].append(s.momentum[:, 1]); cols["pz"].append(s.momentum[:, 2])
        print(f"  {os.path.basename(path):35s} E_beam={ebeam:7.2f} GeV  "
              f"nEvt={n}  <E_ecal>={s.e_ecal.mean():.3f}  <E_hcal>={s.e_hcal.mean():.3f}")

    data = {k: np.concatenate(v) for k, v in cols.items()}
    out = f"results/features_{particle}.npz"
    os.makedirs("results", exist_ok=True)
    np.savez_compressed(out, **data)
    print(f"-> saved {out}  ({len(data['e_beam'])} events)")
    return out


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--data_root", default="to2026Summer")
    ap.add_argument("--particle", choices=["gamma", "neutron", "both"], default="both")
    args = ap.parse_args()

    targets = ["gamma", "neutron"] if args.particle == "both" else [args.particle]
    for p in targets:
        print(f"=== {p} ===")
        build(p, args.data_root)
