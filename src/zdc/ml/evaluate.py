"""
Evaluate the ZDC particle-finder classifier on the held-out validation split.

Reports accuracy + confusion matrix + per-class precision/recall.
Needs torch (Vertex image has it; locally `pip install torch`).

Run:  python -m zdc.ml.evaluate --data results/ml/particles.npz \
          --model results/ml/run/model.pt
"""

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--out_dir", default="results/ml/eval")
    ap.add_argument("--val_frac", type=float, default=0.2)
    args = ap.parse_args()

    import torch
    from zdc.ml.model import DeepSetsClassifier
    from zdc.ml.train import load_npz, normalize

    clouds, masks, labels, class_names = load_npz(args.data)
    clouds_n, _, _ = normalize(clouds, masks)

    # replicate the train/val split exactly (train.py: default_rng(0))
    n = len(clouds_n)
    idx = np.random.default_rng(0).permutation(n)
    sel = idx[:int(args.val_frac * n)]
    clouds_n, masks, labels = clouds_n[sel], masks[sel], labels[sel]
    print(f"evaluating on {len(sel)} held-out validation events")

    ckpt = torch.load(args.model, map_location="cpu", weights_only=False)
    model = DeepSetsClassifier(num_classes=len(class_names))
    model.load_state_dict(ckpt["state_dict"])
    model.eval()

    with torch.no_grad():
        logits = model(torch.tensor(clouds_n), torch.tensor(masks))
    pred = logits.argmax(1).numpy()

    k = len(class_names)
    cm = np.zeros((k, k), dtype=int)
    for t, p in zip(labels, pred):
        cm[t, p] += 1

    acc = (pred == labels).mean()
    print(f"\noverall accuracy: {acc:.4f}\n")
    print("confusion matrix (rows=truth, cols=pred):")
    print("            " + "  ".join(f"{c:>8s}" for c in class_names))
    for i, c in enumerate(class_names):
        print(f"  {c:>8s}  " + "  ".join(f"{v:8d}" for v in cm[i]))
    print()
    for i, c in enumerate(class_names):
        tp = cm[i, i]
        prec = tp / cm[:, i].sum() if cm[:, i].sum() else 0
        rec = tp / cm[i, :].sum() if cm[i, :].sum() else 0
        print(f"  {c:>8s}  precision={prec:.3f}  recall={rec:.3f}")

    os.makedirs(args.out_dir, exist_ok=True)
    np.savez(os.path.join(args.out_dir, "eval.npz"),
             confusion=cm, accuracy=acc, class_names=np.asarray(class_names))
    print(f"\n-> {args.out_dir}/eval.npz")


if __name__ == "__main__":
    main()
