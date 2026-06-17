"""
Build ML datasets from the ZDC ROOT files (phase 2).

Each event becomes a variable-length point cloud of calorimeter hits:
    node features = [log10(E), x, y, z, is_ecal]
    targets       = E_beam [GeV], (x_true, y_true) at HCAL reference z

Saved as a padded .npz (point clouds + mask + targets) so the Vertex container
needs no ROOT/uproot — only numpy. Heavy ROOT reading happens here, locally.
"""

import argparse
import glob
import os
import sys

import numpy as np
import uproot
import awkward as ak

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from zdc.io import ECAL, HCAL, _primary_index
from zdc.beam import beam_energy

DATA = {
    "gamma":   "20260421_gamma_LYSO_diffAngle",
    "neutron": "20260324_neutron_LYSO_diffAngle",
}

# reference plane for the position target: front face of HCAL region [mm]
Z_REF_MM = 35822.0


def _event_pointcloud(a, i):
    """Stack ECAL+HCAL hits of event i into (n_hits, 5) feature array."""
    feats = []
    for tag, is_ecal in ((ECAL, 1.0), (HCAL, 0.0)):
        e = ak.to_numpy(a[f"{tag}.energy"][i]).astype(np.float64)
        if len(e) == 0:
            continue
        x = ak.to_numpy(a[f"{tag}.position.x"][i]).astype(np.float64)
        y = ak.to_numpy(a[f"{tag}.position.y"][i]).astype(np.float64)
        z = ak.to_numpy(a[f"{tag}.position.z"][i]).astype(np.float64)
        loge = np.log10(np.clip(e, 1e-9, None))
        feats.append(np.column_stack([loge, x, y, z, np.full_like(e, is_ecal)]))
    if not feats:
        return np.zeros((0, 5))
    return np.concatenate(feats, axis=0)


def build(particle, data_root, max_hits, out_dir):
    folder = os.path.join(data_root, DATA[particle])
    files = sorted(glob.glob(os.path.join(folder, "outfile_*.root")))

    clouds, masks, e_targets, pos_targets = [], [], [], []
    for path in files:
        ebeam = beam_energy(path, particle)
        t = uproot.open(path)["events"]
        a = t.arrays([
            f"{ECAL}.energy", f"{ECAL}.position.x", f"{ECAL}.position.y", f"{ECAL}.position.z",
            f"{HCAL}.energy", f"{HCAL}.position.x", f"{HCAL}.position.y", f"{HCAL}.position.z",
            "MCParticles.generatorStatus",
            "MCParticles.vertex.x", "MCParticles.vertex.y", "MCParticles.vertex.z",
            "MCParticles.momentum.x", "MCParticles.momentum.y", "MCParticles.momentum.z",
        ])
        n = len(a[f"{ECAL}.energy"])
        for i in range(n):
            pc = _event_pointcloud(a, i)
            if len(pc) == 0:
                continue
            # keep highest-energy hits if over budget
            if len(pc) > max_hits:
                keep = np.argsort(pc[:, 0])[-max_hits:]
                pc = pc[keep]
            padded = np.zeros((max_hits, 5), dtype=np.float32)
            mask = np.zeros(max_hits, dtype=np.float32)
            padded[:len(pc)] = pc
            mask[:len(pc)] = 1.0

            j = _primary_index(a["MCParticles.generatorStatus"][i])
            vx, vy, vz = (a["MCParticles.vertex.x"][i][j], a["MCParticles.vertex.y"][i][j], a["MCParticles.vertex.z"][i][j])
            px, py, pz = (a["MCParticles.momentum.x"][i][j], a["MCParticles.momentum.y"][i][j], a["MCParticles.momentum.z"][i][j])
            xt = vx + (px / pz) * (Z_REF_MM - vz) if pz else vx
            yt = vy + (py / pz) * (Z_REF_MM - vz) if pz else vy

            clouds.append(padded); masks.append(mask)
            e_targets.append(ebeam); pos_targets.append([xt, yt])

        print(f"  {os.path.basename(path):35s} E={ebeam:7.2f}  cum events={len(clouds)}")

    out = os.path.join(out_dir, f"ml_{particle}.npz")
    os.makedirs(out_dir, exist_ok=True)
    np.savez_compressed(
        out,
        clouds=np.asarray(clouds, dtype=np.float32),
        masks=np.asarray(masks, dtype=np.float32),
        e_beam=np.asarray(e_targets, dtype=np.float32),
        pos=np.asarray(pos_targets, dtype=np.float32),
        particle=particle,
    )
    print(f"-> saved {out}  ({len(clouds)} events, max_hits={max_hits})")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--data_root", default="to2026Summer")
    ap.add_argument("--particle", choices=["gamma", "neutron", "both"], default="both")
    ap.add_argument("--max_hits", type=int, default=512)
    ap.add_argument("--out_dir", default="results/ml")
    args = ap.parse_args()

    targets = ["gamma", "neutron"] if args.particle == "both" else [args.particle]
    for p in targets:
        print(f"=== build ML dataset: {p} ===")
        build(p, args.data_root, args.max_hits, args.out_dir)
