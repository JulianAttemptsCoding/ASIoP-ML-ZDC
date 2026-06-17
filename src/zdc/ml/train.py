"""
Train the DeepSets ZDC regressor (phase 2).

Runs locally (CPU/GPU) for smoke tests and inside the Vertex container.
Reads the padded .npz produced by zdc.ml.dataset, normalizes features,
trains energy + position heads, writes model + metrics to --out_dir.
"""

import argparse
import json
import os

import numpy as np
import torch
from torch.utils.data import DataLoader, TensorDataset


def load_npz(path):
    d = np.load(path, allow_pickle=True)
    return d["clouds"], d["masks"], d["e_beam"], d["pos"]


def normalize(clouds, masks):
    """Z-score node features over real hits only; return normalized clouds + stats."""
    flat = clouds[masks.astype(bool)]
    mean = flat.mean(axis=0)
    std = flat.std(axis=0) + 1e-6
    norm = (clouds - mean) / std
    norm = norm * masks[..., None]
    return norm.astype(np.float32), mean, std


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", required=True, help="path to ml_<particle>.npz")
    ap.add_argument("--out_dir", default="results/ml/run")
    ap.add_argument("--epochs", type=int, default=50)
    ap.add_argument("--batch_size", type=int, default=128)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--pos_weight", type=float, default=0.01,
                    help="weight of position loss (mm scale) vs energy loss (GeV)")
    ap.add_argument("--val_frac", type=float, default=0.2)
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    from zdc.ml.model import DeepSetsZDC

    clouds, masks, e_beam, pos = load_npz(args.data)
    clouds, feat_mean, feat_std = normalize(clouds, masks)

    # log-energy target keeps the wide GeV dynamic range trainable
    e_target = np.log10(np.clip(e_beam, 1e-3, None)).astype(np.float32)

    n = len(clouds)
    rng = np.random.default_rng(0)
    idx = rng.permutation(n)
    n_val = int(args.val_frac * n)
    val_idx, tr_idx = idx[:n_val], idx[n_val:]

    def make_loader(ids, shuffle):
        ds = TensorDataset(
            torch.tensor(clouds[ids]), torch.tensor(masks[ids]),
            torch.tensor(e_target[ids]), torch.tensor(pos[ids]),
        )
        return DataLoader(ds, batch_size=args.batch_size, shuffle=shuffle)

    tr, val = make_loader(tr_idx, True), make_loader(val_idx, False)

    device = "cuda" if torch.cuda.is_available() else "cpu"
    model = DeepSetsZDC().to(device)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr)
    sched = torch.optim.lr_scheduler.StepLR(opt, step_size=10, gamma=0.5)
    mae = torch.nn.L1Loss()

    history = []
    for epoch in range(args.epochs):
        model.train()
        for xb, mb, eb, pb in tr:
            xb, mb, eb, pb = xb.to(device), mb.to(device), eb.to(device), pb.to(device)
            out = model(xb, mb)
            loss = mae(out["energy"], eb) + args.pos_weight * mae(out["position"], pb)
            opt.zero_grad(); loss.backward(); opt.step()
        sched.step()

        model.eval()
        ve, vp, nb = 0.0, 0.0, 0
        with torch.no_grad():
            for xb, mb, eb, pb in val:
                xb, mb, eb, pb = xb.to(device), mb.to(device), eb.to(device), pb.to(device)
                out = model(xb, mb)
                ve += mae(out["energy"], eb).item()
                vp += mae(out["position"], pb).item()
                nb += 1
        rec = {"epoch": epoch, "val_e_mae_log10": ve / nb, "val_pos_mae_mm": vp / nb}
        history.append(rec)
        print(f"epoch {epoch:3d} | val E MAE(log10)={rec['val_e_mae_log10']:.4f} | "
              f"val pos MAE={rec['val_pos_mae_mm']:.2f} mm")

    torch.save({"state_dict": model.state_dict(),
                "feat_mean": feat_mean, "feat_std": feat_std},
               os.path.join(args.out_dir, "model.pt"))
    with open(os.path.join(args.out_dir, "history.json"), "w") as f:
        json.dump(history, f, indent=2)
    print(f"-> saved model + history to {args.out_dir}")


if __name__ == "__main__":
    import sys
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
    main()
