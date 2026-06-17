"""
Traditional track / position reconstruction (slide pp.10-16).

HCAL-only. For each event:
  1. group hits into z-layers, energy-weighted mean x,y per layer (zdc.io)
  2. fit xbar vs z and ybar vs z with pol1 (straight line), layer-weighted
  3. evaluate reconstructed x,y at a reference z (energy-weighted z)
  4. residual = reconstructed - truth-extrapolated position at that z

Layer weighting schemes (slide p.11):
  linear-energy :  w = E_layer
  log-energy    :  w = log(E_layer)
  hit-count     :  w = N_layer / N_sum
  energy-and-hit:  w = E_layer/E_sum + N_layer/N_sum   (chosen for final recon)
"""

import numpy as np

from .io import layer_average_positions


WEIGHTINGS = ("linear-energy", "log-energy", "hit-count", "energy-and-hit")


def layer_weights(e_layer, n_layer, scheme):
    e_sum = e_layer.sum()
    n_sum = n_layer.sum()
    if scheme == "linear-energy":
        return e_layer
    if scheme == "log-energy":
        return np.log(np.clip(e_layer, 1e-12, None) / 1e-12)
    if scheme == "hit-count":
        return n_layer / n_sum if n_sum > 0 else np.ones_like(n_layer)
    if scheme == "energy-and-hit":
        ef = e_layer / e_sum if e_sum > 0 else np.zeros_like(e_layer)
        nf = n_layer / n_sum if n_sum > 0 else np.zeros_like(n_layer)
        return ef + nf
    raise ValueError(f"Unknown weighting: {scheme}")


def _pol1_fit(z, v, w):
    """Weighted linear fit v = a + b z. Returns (a, b) or None if degenerate."""
    if len(z) < 2 or np.count_nonzero(w) < 2:
        return None
    W = np.diag(w)
    A = np.column_stack([np.ones_like(z), z])
    try:
        ATA = A.T @ W @ A
        ATb = A.T @ W @ v
        a, b = np.linalg.solve(ATA, ATb)
        return a, b
    except np.linalg.LinAlgError:
        return None


def reconstruct_event(hit, vertex, momentum, scheme="energy-and-hit",
                      n_layer_cut=None):
    """
    Reconstruct (x,y) at the energy-weighted reference z and compute residuals.

    n_layer_cut: if set, keep only layers within +/- n of the most energetic
                 layer before fitting (slide p.11-13).

    Returns dict with residual_x, residual_y, z_ref (or None if not reconstructable).
    """
    zl, xb, yb, el, nl = layer_average_positions(hit)
    if len(zl) < 2:
        return None

    if n_layer_cut is not None:
        peak = np.argmax(el)
        keep = np.abs(np.arange(len(zl)) - peak) <= n_layer_cut
        zl, xb, yb, el, nl = zl[keep], xb[keep], yb[keep], el[keep], nl[keep]
        if len(zl) < 2:
            return None

    w = layer_weights(el, nl, scheme)

    fit_x = _pol1_fit(zl, xb, w)
    fit_y = _pol1_fit(zl, yb, w)
    if fit_x is None or fit_y is None:
        return None

    # reference z = energy-weighted mean layer z (near shower max, slide p.15)
    z_ref = np.average(zl, weights=el) if el.sum() > 0 else zl.mean()

    x_rec = fit_x[0] + fit_x[1] * z_ref
    y_rec = fit_y[0] + fit_y[1] * z_ref

    # truth extrapolation from vertex along momentum to z_ref
    px, py, pz = momentum
    if pz == 0:
        return None
    x_true = vertex[0] + (px / pz) * (z_ref - vertex[2])
    y_true = vertex[1] + (py / pz) * (z_ref - vertex[2])

    return {
        "residual_x": x_rec - x_true,
        "residual_y": y_rec - y_true,
        "z_ref": z_ref,
        "x_rec": x_rec, "y_rec": y_rec,
        "x_true": x_true, "y_true": y_true,
    }


def reconstruct_file(events, vertex, momentum, scheme="energy-and-hit",
                     n_layer_cut=None):
    """Run reconstruct_event over all events; return residual arrays."""
    rx, ry = [], []
    for i, hit in enumerate(events):
        r = reconstruct_event(hit, vertex[i], momentum[i], scheme, n_layer_cut)
        if r is None:
            continue
        rx.append(r["residual_x"])
        ry.append(r["residual_y"])
    return np.array(rx), np.array(ry)
