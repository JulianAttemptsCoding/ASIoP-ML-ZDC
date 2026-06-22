"""
Train the ZDC particle-finder classifier.

Runs locally (CPU/GPU) and inside the Vertex container. Reads the padded .npz
from zdc.ml.dataset, normalizes features, trains a DeepSets classifier with
cross-entropy, writes model + metrics to --out_dir.
"""

import argparse
import json
import os

import numpy as np
import torch
from torch.utils.data import DataLoader, TensorDataset


def load_npz(path):
    d = np.load(path, allow_pickle=True)
    return d["clouds"], d["masks"], d["labels"], list(d["class_names"])


def normalize(clouds, masks):
    """Z-score node features over real hits only; return normalized clouds + stats."""
    flat = clouds[masks.astype(bool)]
    mean = flat.mean(axis=0)
    std = flat.std(axis=0) + 1e-6
    norm = ((clouds - mean) / std) * masks[..., None]
    return norm.astype(np.float32), mean, std


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", required=True, help="path to particles.npz")
    ap.add_argument("--out_dir", default="results/ml/run")
    ap.add_argument("--epochs", type=int, default=40)
    ap.add_argument("--batch_size", type=int, default=128)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--val_frac", type=float, default=0.2)
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    from zdc.ml.model import DeepSetsClassifier

    clouds, masks, labels, class_names = load_npz(args.data)
    clouds, feat_mean, feat_std = normalize(clouds, masks)
    num_classes = len(class_names)

    n = len(clouds)
    idx = np.random.default_rng(0).permutation(n)
    n_val = int(args.val_frac * n)
    val_idx, tr_idx = idx[:n_val], idx[n_val:]

    def loader(ids, shuffle):
        ds = TensorDataset(torch.tensor(clouds[ids]), torch.tensor(masks[ids]),
                           torch.tensor(labels[ids]))
        return DataLoader(ds, batch_size=args.batch_size, shuffle=shuffle)

    tr, val = loader(tr_idx, True), loader(val_idx, False)

    device = "cuda" if torch.cuda.is_available() else "cpu"
    model = DeepSetsClassifier(num_classes=num_classes).to(device)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr)
    sched = torch.optim.lr_scheduler.StepLR(opt, step_size=10, gamma=0.5)
    ce = torch.nn.CrossEntropyLoss()

    history = []
    for epoch in range(args.epochs):
        model.train()
        for xb, mb, yb in tr:
            xb, mb, yb = xb.to(device), mb.to(device), yb.to(device)
            loss = ce(model(xb, mb), yb)
            opt.zero_grad(); loss.backward(); opt.step()
        sched.step()

        model.eval()
        correct, total, vloss, nb = 0, 0, 0.0, 0
        with torch.no_grad():
            for xb, mb, yb in val:
                xb, mb, yb = xb.to(device), mb.to(device), yb.to(device)
                logits = model(xb, mb)
                vloss += ce(logits, yb).item(); nb += 1
                correct += (logits.argmax(1) == yb).sum().item(); total += len(yb)
        acc = correct / total
        history.append({"epoch": epoch, "val_loss": vloss / nb, "val_acc": acc})
        print(f"epoch {epoch:3d} | val loss={vloss/nb:.4f} | val acc={acc:.4f}")

    torch.save({"state_dict": model.state_dict(),
                "feat_mean": feat_mean, "feat_std": feat_std,
                "class_names": class_names},
               os.path.join(args.out_dir, "model.pt"))
    with open(os.path.join(args.out_dir, "history.json"), "w") as f:
        json.dump(history, f, indent=2)
    print(f"-> saved model + history to {args.out_dir}")


if __name__ == "__main__":
    import sys
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
    main()
