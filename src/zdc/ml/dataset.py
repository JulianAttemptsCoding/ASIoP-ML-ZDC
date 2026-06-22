"""
Build the ML dataset for the ZDC particle finder.

Each event -> a point cloud of calorimeter hits with a particle-type LABEL.
    node features = [log10(E), x, y, z, is_ecal]
    label         = particle class (0=gamma, 1=neutron, extensible)

Heavy ROOT reading happens here (local); the padded .npz that lands in the
Vertex container needs only numpy.

TODO (define the "particle finder" task):
  - which classes? gamma vs neutron now; add pi0, lambda-decay (n+2gamma), noise...
  - per-event single label (ID) vs multi-object finding/counting (later)
"""

import argparse
import glob
import os
import sys

import numpy as np
import uproot
import awkward as ak

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from zdc.io import ECAL, HCAL

# particle -> (sample folder, class label)
SAMPLES = {
    "gamma":   ("20260421_gamma_LYSO_diffAngle", 0),
    "neutron": ("20260324_neutron_LYSO_diffAngle", 1),
}
CLASS_NAMES = ["gamma", "neutron"]


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


def build(data_root, max_hits, out_path):
    clouds, masks, labels = [], [], []

    for particle, (folder, label) in SAMPLES.items():
        files = sorted(glob.glob(os.path.join(data_root, folder, "outfile_*.root")))
        if not files:
            print(f"  WARNING: no files for {particle} in {folder}")
            continue
        for path in files:
            t = uproot.open(path)["events"]
            a = t.arrays([
                f"{ECAL}.energy", f"{ECAL}.position.x", f"{ECAL}.position.y", f"{ECAL}.position.z",
                f"{HCAL}.energy", f"{HCAL}.position.x", f"{HCAL}.position.y", f"{HCAL}.position.z",
            ])
            for i in range(len(a[f"{ECAL}.energy"])):
                pc = _event_pointcloud(a, i)
                if len(pc) == 0:
                    continue
                if len(pc) > max_hits:                       # keep highest-energy hits
                    pc = pc[np.argsort(pc[:, 0])[-max_hits:]]
                padded = np.zeros((max_hits, 5), dtype=np.float32)
                mask = np.zeros(max_hits, dtype=np.float32)
                padded[:len(pc)] = pc
                mask[:len(pc)] = 1.0
                clouds.append(padded); masks.append(mask); labels.append(label)
        print(f"  {particle:8s} (label {label}): cum events={len(clouds)}")

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    np.savez_compressed(
        out_path,
        clouds=np.asarray(clouds, dtype=np.float32),
        masks=np.asarray(masks, dtype=np.float32),
        labels=np.asarray(labels, dtype=np.int64),
        class_names=np.asarray(CLASS_NAMES),
    )
    lab = np.asarray(labels)
    print(f"-> saved {out_path}  ({len(clouds)} events, "
          f"classes={ {n: int((lab==i).sum()) for i,n in enumerate(CLASS_NAMES)} })")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--data_root", default="to2026Summer")
    ap.add_argument("--max_hits", type=int, default=512)
    ap.add_argument("--out", default="results/ml/particles.npz")
    args = ap.parse_args()
    print("=== build ZDC particle-finder dataset ===")
    build(args.data_root, args.max_hits, args.out)
