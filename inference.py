"""
ZDC GNN inference and evaluation.

Loads trained model, runs on test data, saves predictions,
computes energy/angle resolution and classification efficiency.

Usage:
    python inference.py --config configs/a100_neutron.yaml --checkpoint outputs/a100_neutron/best_model
    python inference.py --config configs/a100_pi0_gamma.yaml --checkpoint outputs/a100_pi0_gamma/best_model
"""

import argparse
import glob
import os

import numpy as np
import tensorflow as tf
import yaml
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from src.data.generator import ZDCGraphDataGenerator
from src.models.gnn import ZDCModel, batch_to_graphs_tuple
from src.utils.metrics import energy_resolution, position_resolution, classification_efficiency


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--config",     required=True)
    p.add_argument("--checkpoint", required=True)
    p.add_argument("--test_dir",   default=None,
                   help="Directory with test ROOT files. Defaults to data_dir.")
    return p.parse_args()


def run_inference(model, data_gen):
    all_preds  = {"energy": [], "theta": [], "class_prob": []}
    all_truth  = {"energy": [], "theta": [], "label": []}

    for batch in data_gen.generator():
        graphs = batch_to_graphs_tuple(batch)
        preds  = model(graphs, training=False)

        all_preds["energy"].append(preds["energy"].numpy())
        all_preds["theta"].append(preds["theta"].numpy())
        if "class_prob" in preds:
            all_preds["class_prob"].append(preds["class_prob"].numpy())

        all_truth["energy"].append(batch["targets"]["energy"])
        all_truth["theta"].append(batch["targets"]["theta"])
        if "label" in batch["targets"]:
            all_truth["label"].append(batch["targets"]["label"])

    return (
        {k: np.concatenate(v) for k, v in all_preds.items() if v},
        {k: np.concatenate(v) for k, v in all_truth.items() if v},
    )


def plot_energy_resolution(e_truth, e_reco, out_path):
    centers, res, scale = energy_resolution(e_truth, e_reco)
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7, 8), sharex=True)

    ax1.plot(centers, res, "o-", label="GNN")
    ax1.axhline(50, color="k", linestyle="--", label="YR 50%/√E⊕5%")
    ax1.set_ylabel("σ(E_reco)/μ(E_reco) [%]")
    ax1.legend()
    ax1.set_title("ZDC Energy Resolution — GNN")

    ax2.plot(centers, scale, "s-", label="GNN")
    ax2.axhline(0, color="k", linestyle="--")
    ax2.set_xlabel("E_truth [GeV]")
    ax2.set_ylabel("μ(E_reco)/E_truth - 1 [%]")
    ax2.legend()

    plt.xscale("log")
    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    plt.close()
    print(f"Saved: {out_path}")


def plot_position_resolution(theta_truth, theta_reco, out_path):
    centers, sigma = position_resolution(theta_truth, theta_reco)
    fig, ax = plt.subplots(figsize=(7, 5))
    ax.plot(centers, sigma * 1000, "o-", label="GNN")  # mrad -> urad
    ax.set_xlabel("θ_truth [mrad]")
    ax.set_ylabel("σ_θ [mrad]")
    ax.set_title("ZDC Angular Resolution — GNN")
    ax.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    plt.close()
    print(f"Saved: {out_path}")


def main():
    args = parse_args()
    with open(args.config) as f:
        cfg = yaml.safe_load(f)

    out_dir = cfg["output_dir"]
    os.makedirs(out_dir, exist_ok=True)

    test_dir = args.test_dir or cfg["data_dir"]
    test_files = sorted(glob.glob(os.path.join(test_dir, "*.root")))
    if not test_files:
        raise FileNotFoundError(f"No ROOT files in {test_dir}")

    # ── Load model ────────────────────────────────────────────────────────────
    model = ZDCModel(cfg)
    # build model with dummy input to initialize weights
    dummy_batch = next(iter(ZDCGraphDataGenerator(test_files[:1], cfg).generator()))
    model(batch_to_graphs_tuple(dummy_batch), training=False)
    model.load_weights(args.checkpoint)
    print(f"Loaded weights from {args.checkpoint}")

    # ── Run inference ─────────────────────────────────────────────────────────
    data_gen = ZDCGraphDataGenerator(test_files, cfg, shuffle=False)
    data_gen.load_normalization()

    preds, truth = run_inference(model, data_gen)

    np.savez(os.path.join(out_dir, "predictions.npz"),
             pred_energy=preds["energy"],
             pred_theta=preds["theta"],
             truth_energy=truth["energy"],
             truth_theta=truth["theta"],
             **({} if "class_prob" not in preds else {"class_prob": preds["class_prob"]}),
             **({} if "label"      not in truth else {"label":      truth["label"]}),
             )

    # ── Plots ─────────────────────────────────────────────────────────────────
    plot_energy_resolution(truth["energy"], preds["energy"],
                           os.path.join(out_dir, "energy_resolution.png"))
    plot_position_resolution(truth["theta"], preds["theta"],
                             os.path.join(out_dir, "position_resolution.png"))

    if "class_prob" in preds and "label" in truth:
        cut = cfg.get("classification_cut", 0.3)
        eff_g, eff_p = classification_efficiency(truth["label"], preds["class_prob"], cut=cut)
        print(f"gamma efficiency: {eff_g:.3f}  |  pi0 pass-rate: {eff_p:.3f}")

    print("Inference complete.")


if __name__ == "__main__":
    gpus = tf.config.list_physical_devices("GPU")
    for gpu in gpus:
        tf.config.experimental.set_memory_growth(gpu, True)
    main()
