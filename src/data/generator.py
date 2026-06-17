"""
ZDC graph data generator.
Loads ROOT files from GEANT4/DD4HEP simulation, applies hit selection,
builds kNN graphs, and yields batches for TF training.

Reference: arXiv:2406.12877v2, Sec. 3
Reference code: github.com/eiccodesign/regressiononly (zdc_classification branch)
"""

import os
import glob
import queue
import multiprocessing as mp
from typing import Optional

import numpy as np
import uproot
import awkward as ak
from sklearn.neighbors import KDTree
import compress_pickle as cpickle


# ── Branch name helpers ────────────────────────────────────────────────────────

def _momentum_to_E_theta(px, py, pz):
    """Convert MC truth momentum to total momentum magnitude and polar angle."""
    p = np.sqrt(px**2 + py**2 + pz**2)
    # theta in mrad; pz is along beam axis
    theta = np.arctan2(np.sqrt(px**2 + py**2), np.abs(pz)) * 1000.0
    return p, theta


def _load_root_event(tree, entry, det_branch, mc_branch, cfg):
    """
    Extract one event from a ROOT tree entry.

    Returns:
        nodes  : (N, 4) float32  — [log10(E), x, y, z]
        global : (1,)   float32  — calibrated cluster energy
        target : dict           — truth values
        valid  : bool           — False if event should be skipped
    """
    # ── Hit data ──────────────────────────────────────────────────────────────
    energies = ak.to_numpy(tree[f"{det_branch}.energy"].array(entry_start=entry, entry_stop=entry+1)[0])
    times    = ak.to_numpy(tree[f"{det_branch}.time"].array(entry_start=entry, entry_stop=entry+1)[0])
    xs       = ak.to_numpy(tree[f"{det_branch}.position.x"].array(entry_start=entry, entry_stop=entry+1)[0])
    ys       = ak.to_numpy(tree[f"{det_branch}.position.y"].array(entry_start=entry, entry_stop=entry+1)[0])
    zs       = ak.to_numpy(tree[f"{det_branch}.position.z"].array(entry_start=entry, entry_stop=entry+1)[0])

    # ── Hit selection (paper Sec. 3) ──────────────────────────────────────────
    mask = (energies > cfg["energy_threshold"]) & (times < cfg["time_cut"])
    energies, times, xs, ys, zs = energies[mask], times[mask], xs[mask], ys[mask], zs[mask]

    if len(energies) < 2:
        return None, None, None, False

    # ── Node features ─────────────────────────────────────────────────────────
    log_e = np.log10(energies).astype(np.float32)
    nodes = np.stack([log_e, xs.astype(np.float32),
                      ys.astype(np.float32), zs.astype(np.float32)], axis=1)

    # ── Global feature: calibrated cluster energy ─────────────────────────────
    e_calib = np.sum(energies) / cfg["sampling_fraction"]
    global_feat = np.array([e_calib], dtype=np.float32)

    # ── MC truth ──────────────────────────────────────────────────────────────
    mc_px = ak.to_numpy(tree[f"{mc_branch}.momentum.x"].array(entry_start=entry, entry_stop=entry+1)[0])
    mc_py = ak.to_numpy(tree[f"{mc_branch}.momentum.y"].array(entry_start=entry, entry_stop=entry+1)[0])
    mc_pz = ak.to_numpy(tree[f"{mc_branch}.momentum.z"].array(entry_start=entry, entry_stop=entry+1)[0])

    # primary particle is index 0
    p_truth, theta_truth = _momentum_to_E_theta(mc_px[0], mc_py[0], mc_pz[0])

    # PDG id for pi0/gamma classification (optional)
    pdg = None
    if f"{mc_branch}.PDG" in tree:
        pdg = int(ak.to_numpy(tree[f"{mc_branch}.PDG"].array(entry_start=entry, entry_stop=entry+1)[0])[0])

    if theta_truth > cfg["theta_max"]:
        return None, None, None, False

    target = {
        "energy": np.float32(p_truth),
        "theta":  np.float32(theta_truth),
        "pdg":    pdg,
    }

    return nodes, global_feat, target, True


def _build_knn_edges(positions, k):
    """Build k-nearest-neighbor edge index from hit (x,y,z) positions."""
    tree = KDTree(positions)
    # k+1 because query includes self
    _, idx = tree.query(positions, k=min(k + 1, len(positions)))
    senders, receivers = [], []
    for i, neighbors in enumerate(idx):
        for j in neighbors:
            if j != i:
                senders.append(i)
                receivers.append(j)
    return np.array(senders, dtype=np.int32), np.array(receivers, dtype=np.int32)


def _preprocess_file(args):
    """Worker: preprocess one ROOT file → compressed pickle."""
    root_path, out_path, cfg = args

    graphs = []
    with uproot.open(root_path) as f:
        # TODO: update tree name to match your simulation output
        tree_name = cfg.get("tree_name", "events")
        tree = f[tree_name]
        n_events = tree.num_entries

        for entry in range(n_events):
            nodes, global_feat, target, valid = _load_root_event(
                tree, entry, cfg["detector_branch"], cfg["mc_branch"], cfg
            )
            if not valid:
                continue

            positions = nodes[:, 1:]  # x, y, z
            senders, receivers = _build_knn_edges(positions, cfg["k_neighbors"])

            graphs.append({
                "nodes":     nodes,
                "globals":   global_feat,
                "senders":   senders,
                "receivers": receivers,
                "target":    target,
            })

    cpickle.dump(graphs, out_path, compression="gzip")
    return len(graphs)


class ZDCGraphDataGenerator:
    """
    Multi-process data generator for ZDC GNN training.

    Usage:
        gen = ZDCGraphDataGenerator(file_list, cfg)
        for batch in gen.generator():
            train_step(batch)
    """

    def __init__(self,
                 file_list: list,
                 cfg: dict,
                 preprocessed_dir: Optional[str] = None,
                 shuffle: bool = True):
        self.file_list = file_list
        self.cfg = cfg
        self.preprocessed_dir = preprocessed_dir or os.path.join(cfg["output_dir"], "preprocessed")
        self.shuffle = shuffle
        self.batch_size = cfg["batch_size"]
        self.num_procs = cfg.get("num_procs", 8)

        os.makedirs(self.preprocessed_dir, exist_ok=True)

        # compute normalization stats from first 8 files
        self._means_path = os.path.join(self.preprocessed_dir, "means.pkl")
        self._stds_path  = os.path.join(self.preprocessed_dir, "stds.pkl")

    # ── Preprocessing ─────────────────────────────────────────────────────────

    def preprocess(self):
        """Serialize all ROOT files to compressed pickle. Run once."""
        args = []
        for path in self.file_list:
            name = os.path.splitext(os.path.basename(path))[0]
            out = os.path.join(self.preprocessed_dir, f"{name}.pkl.gz")
            if not os.path.exists(out):
                args.append((path, out, self.cfg))

        if not args:
            print("All files already preprocessed.")
            return

        print(f"Preprocessing {len(args)} files with {self.num_procs} workers...")
        with mp.Pool(self.num_procs) as pool:
            counts = pool.map(_preprocess_file, args)
        print(f"Preprocessed {sum(counts)} events total.")

    def _compute_normalization(self):
        """Compute per-feature mean/std from first 8 preprocessed files."""
        pkl_files = sorted(glob.glob(os.path.join(self.preprocessed_dir, "*.pkl.gz")))[:8]
        all_nodes, all_globals = [], []
        for path in pkl_files:
            graphs = cpickle.load(path)
            for g in graphs:
                all_nodes.append(g["nodes"])
                all_globals.append(g["globals"])

        nodes_arr   = np.concatenate(all_nodes, axis=0)
        globals_arr = np.stack(all_globals, axis=0)

        self._node_mean = nodes_arr.mean(axis=0).astype(np.float32)
        self._node_std  = nodes_arr.std(axis=0).astype(np.float32) + 1e-8
        self._glob_mean = globals_arr.mean(axis=0).astype(np.float32)
        self._glob_std  = globals_arr.std(axis=0).astype(np.float32) + 1e-8

        cpickle.dump((self._node_mean, self._node_std), self._means_path)
        cpickle.dump((self._glob_mean, self._glob_std), self._stds_path)

    def load_normalization(self):
        if os.path.exists(self._means_path):
            self._node_mean, self._node_std = cpickle.load(self._means_path)
            self._glob_mean, self._glob_std = cpickle.load(self._stds_path)
        else:
            self._compute_normalization()

    # ── Batch generation ──────────────────────────────────────────────────────

    def _normalize(self, graph):
        graph["nodes"]   = (graph["nodes"]   - self._node_mean) / self._node_std
        graph["globals"] = (graph["globals"] - self._glob_mean) / self._glob_std
        return graph

    def generator(self):
        """Yield normalized graph batches indefinitely."""
        pkl_files = sorted(glob.glob(os.path.join(self.preprocessed_dir, "*.pkl.gz")))
        if self.shuffle:
            np.random.shuffle(pkl_files)

        batch = []
        for path in pkl_files:
            graphs = cpickle.load(path)
            if self.shuffle:
                np.random.shuffle(graphs)
            for g in graphs:
                batch.append(self._normalize(g))
                if len(batch) == self.batch_size:
                    yield self._collate(batch)
                    batch = []

    @staticmethod
    def _collate(graphs):
        """
        Merge a list of graphs into a single batched graph dict
        compatible with graph_nets GraphsTuple.
        """
        node_counts = [g["nodes"].shape[0] for g in graphs]
        offsets = np.cumsum([0] + node_counts[:-1])

        nodes     = np.concatenate([g["nodes"]   for g in graphs], axis=0)
        globals_  = np.stack([g["globals"] for g in graphs], axis=0)
        senders   = np.concatenate([g["senders"]   + off for g, off in zip(graphs, offsets)])
        receivers = np.concatenate([g["receivers"] + off for g, off in zip(graphs, offsets)])
        n_node    = np.array(node_counts, dtype=np.int32)
        n_edge    = np.array([len(g["senders"]) for g in graphs], dtype=np.int32)

        # edge features: Euclidean distance between sender and receiver
        sender_pos   = nodes[senders,   1:]
        receiver_pos = nodes[receivers, 1:]
        edges = np.linalg.norm(sender_pos - receiver_pos, axis=1, keepdims=True).astype(np.float32)

        targets = {
            "energy": np.array([g["target"]["energy"] for g in graphs], dtype=np.float32),
            "theta":  np.array([g["target"]["theta"]  for g in graphs], dtype=np.float32),
        }
        if graphs[0]["target"]["pdg"] is not None:
            # 22 = photon (gamma), 111 = pi0 -> label 0/1
            pdg_to_label = {22: 0, 111: 1}
            targets["label"] = np.array(
                [pdg_to_label.get(g["target"]["pdg"], -1) for g in graphs], dtype=np.float32
            )

        return {
            "nodes":     nodes,
            "edges":     edges,
            "globals":   globals_,
            "senders":   senders,
            "receivers": receivers,
            "n_node":    n_node,
            "n_edge":    n_edge,
            "targets":   targets,
        }
