"""
ZDC GNN training script.

Usage:
    python train.py                          # uses configs/default.yaml
    python train.py --config configs/a100_neutron.yaml
    python train.py --config configs/a100_pi0_gamma.yaml

Paper: arXiv:2406.12877v2
Reference code: github.com/eiccodesign/regressiononly (zdc_classification branch)
"""

import argparse
import glob
import os
import time

import numpy as np
import tensorflow as tf
import yaml

from src.data.generator import ZDCGraphDataGenerator
from src.models.gnn import ZDCModel, batch_to_graphs_tuple
from src.models.losses import get_loss_fn


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--config", default="configs/default.yaml")
    return p.parse_args()


def load_config(path):
    with open(path) as f:
        return yaml.safe_load(f)


def build_file_lists(cfg):
    pattern = os.path.join(cfg["data_dir"], "*.root")
    all_files = sorted(glob.glob(pattern))
    if not all_files:
        raise FileNotFoundError(f"No ROOT files found in {cfg['data_dir']}")

    # 75% train, 25% val (mirrors paper: 750k/250k split)
    split = int(0.75 * len(all_files))
    return all_files[:split], all_files[split:]


def make_lr_schedule(cfg):
    """Halve LR every lr_halve_every epochs, floor at lr_min."""
    initial_lr = cfg["learning_rate"]
    halve_every = cfg["lr_halve_every"]
    lr_min = cfg["lr_min"]

    def schedule(epoch):
        factor = 0.5 ** (epoch // halve_every)
        return max(initial_lr * factor, lr_min)

    return schedule


@tf.function
def train_step(model, batch, loss_fn, optimizer):
    graphs = batch_to_graphs_tuple(batch)
    with tf.GradientTape() as tape:
        preds = model(graphs, training=True)
        total_loss, loss_dict = loss_fn(preds, batch["targets"])
    grads = tape.gradient(total_loss, model.trainable_variables)
    optimizer.apply_gradients(zip(grads, model.trainable_variables))
    return loss_dict


@tf.function
def val_step(model, batch, loss_fn):
    graphs = batch_to_graphs_tuple(batch)
    preds = model(graphs, training=False)
    _, loss_dict = loss_fn(preds, batch["targets"])
    return loss_dict


def train(cfg):
    os.makedirs(cfg["output_dir"], exist_ok=True)

    # save config copy
    with open(os.path.join(cfg["output_dir"], "config.yaml"), "w") as f:
        yaml.dump(cfg, f)

    # ── Data ──────────────────────────────────────────────────────────────────
    train_files, val_files = build_file_lists(cfg)
    print(f"Train files: {len(train_files)}, Val files: {len(val_files)}")

    train_gen = ZDCGraphDataGenerator(train_files, cfg, shuffle=True)
    val_gen   = ZDCGraphDataGenerator(val_files,   cfg, shuffle=False)

    if not cfg.get("already_preprocessed", False):
        train_gen.preprocess()
        val_gen.preprocess()

    train_gen.load_normalization()
    val_gen.load_normalization()

    # ── Model + optimizer ─────────────────────────────────────────────────────
    model     = ZDCModel(cfg)
    loss_fn   = get_loss_fn(cfg)
    optimizer = tf.keras.optimizers.Adam(learning_rate=cfg["learning_rate"])
    lr_schedule = make_lr_schedule(cfg)

    best_val_loss = np.inf
    train_history, val_history = [], []

    # ── Training loop ─────────────────────────────────────────────────────────
    for epoch in range(cfg["num_epochs"]):
        lr = lr_schedule(epoch)
        optimizer.learning_rate.assign(lr)

        # --- train ---
        t0 = time.time()
        train_losses = []
        for batch in train_gen.generator():
            d = train_step(model, batch, loss_fn, optimizer)
            train_losses.append({k: float(v) for k, v in d.items()})

        train_mean = {k: np.mean([x[k] for x in train_losses]) for k in train_losses[0]}

        # --- val ---
        val_losses = []
        for batch in val_gen.generator():
            d = val_step(model, batch, loss_fn)
            val_losses.append({k: float(v) for k, v in d.items()})

        val_mean = {k: np.mean([x[k] for x in val_losses]) for k in val_losses[0]}

        elapsed = time.time() - t0
        print(f"Epoch {epoch+1:3d}/{cfg['num_epochs']} | lr={lr:.2e} | "
              f"train_loss={train_mean['loss']:.4f} | val_loss={val_mean['loss']:.4f} | "
              f"{elapsed:.0f}s")

        train_history.append(train_mean)
        val_history.append(val_mean)

        # --- checkpoint ---
        if val_mean["loss"] < best_val_loss:
            best_val_loss = val_mean["loss"]
            model.save_weights(os.path.join(cfg["output_dir"], "best_model"))
            print(f"  -> New best val loss: {best_val_loss:.4f}")

        # save latest
        model.save_weights(os.path.join(cfg["output_dir"], "last_model"))

    # ── Save loss history ─────────────────────────────────────────────────────
    np.savez(
        os.path.join(cfg["output_dir"], "losses.npz"),
        train=train_history,
        val=val_history,
    )
    print("Training complete.")


if __name__ == "__main__":
    args = parse_args()
    cfg  = load_config(args.config)

    # GPU memory growth
    gpus = tf.config.list_physical_devices("GPU")
    for gpu in gpus:
        tf.config.experimental.set_memory_growth(gpu, True)

    train(cfg)
