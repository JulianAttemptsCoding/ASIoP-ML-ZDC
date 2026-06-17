"""
Read EDM4hep/podio ROOT files from the ZDC DD4hep simulation.

Collections of interest (per event):
  EcalFarForwardZDCHits   - LYSO ECAL hits (energy [GeV], position x/y/z [mm])
  HcalFarForwardZDCHits   - SiPM-on-tile HCAL hits (energy [GeV], position x/y/z [mm])
  MCParticles             - truth; primary = first entry with generatorStatus == 1
"""

from dataclasses import dataclass

import numpy as np
import uproot
import awkward as ak

ECAL = "EcalFarForwardZDCHits"
HCAL = "HcalFarForwardZDCHits"

# HCAL sampling-layer pitch along z [mm], from sample inspection (~24.9 mm, 64 layers).
HCAL_LAYER_PITCH_MM = 24.9


@dataclass
class EventScalars:
    """Per-event quantities for energy regression."""
    e_ecal: np.ndarray        # summed ECAL deposited energy [GeV]
    e_hcal: np.ndarray        # summed HCAL deposited energy [GeV]
    n_ecal_hits: np.ndarray
    n_hcal_hits: np.ndarray
    # truth (primary particle)
    vertex: np.ndarray        # (N,3) [mm]
    momentum: np.ndarray      # (N,3) [GeV]
    pdg: np.ndarray


def _primary_index(gen_status):
    """Index of the first generatorStatus==1 particle in an event (else 0)."""
    gs = np.asarray(ak.to_list(gen_status))
    idx = np.where(gs == 1)[0]
    return int(idx[0]) if len(idx) else 0


def read_event_scalars(path: str, tree: str = "events") -> EventScalars:
    """Load per-event ECAL/HCAL summed energies and primary truth."""
    t = uproot.open(path)[tree]
    a = t.arrays([
        f"{ECAL}.energy",
        f"{HCAL}.energy",
        "MCParticles.PDG",
        "MCParticles.generatorStatus",
        "MCParticles.vertex.x", "MCParticles.vertex.y", "MCParticles.vertex.z",
        "MCParticles.momentum.x", "MCParticles.momentum.y", "MCParticles.momentum.z",
    ])

    e_ecal = ak.to_numpy(ak.sum(a[f"{ECAL}.energy"], axis=1))
    e_hcal = ak.to_numpy(ak.sum(a[f"{HCAL}.energy"], axis=1))
    n_ecal = ak.to_numpy(ak.num(a[f"{ECAL}.energy"], axis=1))
    n_hcal = ak.to_numpy(ak.num(a[f"{HCAL}.energy"], axis=1))

    n = len(e_ecal)
    vertex = np.zeros((n, 3), dtype=np.float64)
    momentum = np.zeros((n, 3), dtype=np.float64)
    pdg = np.zeros(n, dtype=np.int64)

    for i in range(n):
        j = _primary_index(a["MCParticles.generatorStatus"][i])
        vertex[i] = (a["MCParticles.vertex.x"][i][j],
                     a["MCParticles.vertex.y"][i][j],
                     a["MCParticles.vertex.z"][i][j])
        momentum[i] = (a["MCParticles.momentum.x"][i][j],
                       a["MCParticles.momentum.y"][i][j],
                       a["MCParticles.momentum.z"][i][j])
        pdg[i] = a["MCParticles.PDG"][i][j]

    return EventScalars(e_ecal, e_hcal, n_ecal, n_hcal, vertex, momentum, pdg)


def read_hcal_hits(path: str, tree: str = "events"):
    """
    Load HCAL hits per event for track reconstruction.

    Returns a list (length n_events) of dicts with arrays:
        x, y, z [mm], e [GeV]
    plus the per-event primary truth (vertex, momentum) as numpy arrays.
    """
    t = uproot.open(path)[tree]
    a = t.arrays([
        f"{HCAL}.energy",
        f"{HCAL}.position.x", f"{HCAL}.position.y", f"{HCAL}.position.z",
        "MCParticles.generatorStatus",
        "MCParticles.vertex.x", "MCParticles.vertex.y", "MCParticles.vertex.z",
        "MCParticles.momentum.x", "MCParticles.momentum.y", "MCParticles.momentum.z",
    ])

    n = len(a[f"{HCAL}.energy"])
    events = []
    vertex = np.zeros((n, 3))
    momentum = np.zeros((n, 3))

    for i in range(n):
        events.append({
            "x": ak.to_numpy(a[f"{HCAL}.position.x"][i]).astype(np.float64),
            "y": ak.to_numpy(a[f"{HCAL}.position.y"][i]).astype(np.float64),
            "z": ak.to_numpy(a[f"{HCAL}.position.z"][i]).astype(np.float64),
            "e": ak.to_numpy(a[f"{HCAL}.energy"][i]).astype(np.float64),
        })
        j = _primary_index(a["MCParticles.generatorStatus"][i])
        vertex[i] = (a["MCParticles.vertex.x"][i][j],
                     a["MCParticles.vertex.y"][i][j],
                     a["MCParticles.vertex.z"][i][j])
        momentum[i] = (a["MCParticles.momentum.x"][i][j],
                       a["MCParticles.momentum.y"][i][j],
                       a["MCParticles.momentum.z"][i][j])

    return events, vertex, momentum


def layer_average_positions(hit: dict, pitch_mm: float = HCAL_LAYER_PITCH_MM):
    """
    Group HCAL hits into z-layers and compute energy-weighted mean x,y per layer.

    Returns arrays (z_layer, xbar, ybar, e_layer, n_layer), sorted by z.
    Implements  xbar = sum(E*x)/sum(E)  (slide p.10).
    """
    z, x, y, e = hit["z"], hit["x"], hit["y"], hit["e"]
    if len(z) == 0:
        empty = np.array([])
        return empty, empty, empty, empty, empty

    layer_id = np.round(z / pitch_mm).astype(np.int64)
    uniq = np.unique(layer_id)

    zl = np.empty(len(uniq)); xb = np.empty(len(uniq)); yb = np.empty(len(uniq))
    el = np.empty(len(uniq)); nl = np.empty(len(uniq))
    for k, lid in enumerate(uniq):
        m = layer_id == lid
        esum = e[m].sum()
        zl[k] = np.average(z[m], weights=e[m]) if esum > 0 else z[m].mean()
        xb[k] = np.average(x[m], weights=e[m]) if esum > 0 else x[m].mean()
        yb[k] = np.average(y[m], weights=e[m]) if esum > 0 else y[m].mean()
        el[k] = esum
        nl[k] = m.sum()

    order = np.argsort(zl)
    return zl[order], xb[order], yb[order], el[order], nl[order]
