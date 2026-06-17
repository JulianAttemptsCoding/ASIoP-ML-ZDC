"""
Evaluation metrics for ZDC reconstruction performance.

Energy resolution: sigma(E_reco) / E_truth  (paper Fig. 3)
Position resolution: sigma(delta_r)          (paper Fig. 5)
"""

import numpy as np
from scipy.stats import norm as scipy_norm


def energy_resolution(e_truth, e_reco, bins=20):
    """
    Compute energy resolution (sigma/mean) in bins of E_truth.

    Returns bin centers, resolution (%), scale (mu/E_truth - 1) (%).
    """
    log_bins = np.logspace(np.log10(e_truth.min()), np.log10(e_truth.max()), bins + 1)
    centers, resolution, scale = [], [], []

    for lo, hi in zip(log_bins[:-1], log_bins[1:]):
        mask = (e_truth >= lo) & (e_truth < hi)
        if mask.sum() < 20:
            continue
        ratio = e_reco[mask] / e_truth[mask]
        mu, sigma = _fit_gaussian(ratio)
        centers.append(np.sqrt(lo * hi))
        resolution.append(100.0 * sigma)
        scale.append(100.0 * (mu - 1.0))

    return np.array(centers), np.array(resolution), np.array(scale)


def position_resolution(theta_truth, theta_reco, bins=20):
    """
    Compute angular position resolution in bins of theta_truth.
    Returns bin centers and sigma in mrad.
    """
    bin_edges = np.linspace(theta_truth.min(), theta_truth.max(), bins + 1)
    centers, sigma_vals = [], []

    for lo, hi in zip(bin_edges[:-1], bin_edges[1:]):
        mask = (theta_truth >= lo) & (theta_truth < hi)
        if mask.sum() < 20:
            continue
        residuals = theta_reco[mask] - theta_truth[mask]
        _, sigma = _fit_gaussian(residuals)
        centers.append(0.5 * (lo + hi))
        sigma_vals.append(sigma)

    return np.array(centers), np.array(sigma_vals)


def classification_efficiency(labels, class_probs, cut=0.3, bins=20, e_truth=None):
    """
    Compute gamma selection efficiency and pi0 rejection as function of energy.
    Paper: gamma efficiency ~99% at 50-250 GeV, pi0 rejection ~97-98% above 150 GeV.

    Returns efficiency arrays for gamma and pi0.
    """
    gamma_mask = labels == 0
    pi0_mask   = labels == 1

    if e_truth is not None:
        bin_edges = np.logspace(np.log10(max(e_truth.min(), 1)), np.log10(e_truth.max()), bins + 1)
        centers, eff_gamma, eff_pi0 = [], [], []
        for lo, hi in zip(bin_edges[:-1], bin_edges[1:]):
            e_mask = (e_truth >= lo) & (e_truth < hi)
            g = (e_mask & gamma_mask)
            p = (e_mask & pi0_mask)
            if g.sum() == 0 or p.sum() == 0:
                continue
            centers.append(np.sqrt(lo * hi))
            # efficiency = fraction passing cut (below cut = gamma)
            eff_gamma.append((class_probs[g] < cut).mean())
            eff_pi0.append((class_probs[p] < cut).mean())
        return np.array(centers), np.array(eff_gamma), np.array(eff_pi0)
    else:
        eff_gamma = (class_probs[gamma_mask] < cut).mean()
        eff_pi0   = (class_probs[pi0_mask]   < cut).mean()
        return eff_gamma, eff_pi0


def _fit_gaussian(arr):
    """Fit a Gaussian and return (mean, sigma)."""
    mu, sigma = scipy_norm.fit(arr)
    return mu, sigma
