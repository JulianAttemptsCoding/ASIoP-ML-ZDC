"""
Evaluate a trained DeepSets model (phase 2): turn predictions into the same
resolution/bias numbers as the traditional pipeline, per beam energy, so ML and
traditional can be compared directly.

    energy   : DSCB fit of E_pred/E_beam -> sigma (resolution %), mu (bias)
    position : Gaussian sigma of (pred - true) per coordinate [mm]

Needs torch (Vertex image has it; locally `pip install torch`).
Run:  python -m zdc.ml.evaluate --data results/ml/ml_neutron.npz \
          --model results/ml/full_.../model.pt --particle neutron
"""

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from zdc.crystalball import fit_resolution


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--particle", default="neutron")
    ap.add_argument("--out_dir", default="results/ml/eval")
    ap.add_argument("--split", choices=["val", "all"], default="val",
                    help="val = held-out 20%% (same seed as training; honest test)")
    ap.add_argument("--val_frac", type=float, default=0.2)
    args = ap.parse_args()

    import torch
    from zdc.ml.model import DeepSetsZDC
    from zdc.ml.train import load_npz, normalize

    clouds, masks, e_beam, pos = load_npz(args.data)
    clouds_n, _, _ = normalize(clouds, masks)

    # replicate the train/val split exactly (train.py: default_rng(0), val_frac)
    if args.split == "val":
        n = len(clouds_n)
        idx = np.random.default_rng(0).permutation(n)
        sel = idx[:int(args.val_frac * n)]
        clouds_n, masks, e_beam, pos = clouds_n[sel], masks[sel], e_beam[sel], pos[sel]
        print(f"evaluating on {len(sel)} held-out validation events")

    # weights_only=False: checkpoint holds numpy feat_mean/std we wrote ourselves
    ckpt = torch.load(args.model, map_location="cpu", weights_only=False)
    model = DeepSetsZDC()
    model.load_state_dict(ckpt["state_dict"])
    model.eval()

    with torch.no_grad():
        out = model(torch.tensor(clouds_n), torch.tensor(masks))
    e_pred = 10 ** out["energy"].numpy()           # undo log10 target
    pos_pred = out["position"].numpy()

    os.makedirs(args.out_dir, exist_ok=True)
    csv = os.path.join(args.out_dir, f"ml_eval_{args.particle}.csv")
    energies = np.unique(e_beam)
    with open(csv, "w") as f:
        f.write("particle,E_beam_GeV,energy_res_pct,energy_bias,pos_res_x_mm,pos_res_y_mm\n")
        for E in energies:
            m = e_beam == E
            mu, sigma, *_ = fit_resolution(e_pred[m] / E)
            rx = pos_pred[m, 0] - pos[m, 0]
            ry = pos_pred[m, 1] - pos[m, 1]
            f.write(f"{args.particle},{E:.3f},{sigma*100:.4f},{mu:.4f},"
                    f"{rx.std():.4f},{ry.std():.4f}\n")
            print(f"E={E:7.2f} GeV  E-res={sigma*100:5.1f}%  bias={mu:.3f}  "
                  f"pos_x={rx.std():5.1f}mm  pos_y={ry.std():5.1f}mm")
    print(f"-> {csv}")


if __name__ == "__main__":
    main()
