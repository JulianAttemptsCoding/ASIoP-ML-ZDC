"""
Traditional energy reconstruction (slide pp.6-8).

E_rec is a linear combination of the ECAL and HCAL deposited energies, fit
globally across all beam energies. Three function forms x two weighting schemes.

  Linear :    E_rec = p0 + p1*E_ECAL + p2*E_HCAL
  Ratio  :    E_rec = p0 + p1*E_ECAL + p2*E_HCAL + p3*E_HCAL/(E_ECAL+E_HCAL)
  Quad   :    E_rec = p0 + p1*E_ECAL^2 + p2*E_HCAL^2

Weighting (per event entering the least-squares fit):
  equal  :    w = 1
  sqrt   :    w = sqrt(E_beam)
"""

import numpy as np


FUNCTIONS = ("linear", "ratio", "quadratic")
WEIGHTINGS = ("equal", "sqrt")


def _design_matrix(e_ecal, e_hcal, func):
    """Columns of the linear model for the chosen functional form."""
    e_ecal = np.asarray(e_ecal, dtype=np.float64)
    e_hcal = np.asarray(e_hcal, dtype=np.float64)
    ones = np.ones_like(e_ecal)
    if func == "linear":
        return np.column_stack([ones, e_ecal, e_hcal])
    if func == "ratio":
        denom = e_ecal + e_hcal
        ratio = np.divide(e_hcal, denom, out=np.zeros_like(denom), where=denom > 0)
        return np.column_stack([ones, e_ecal, e_hcal, ratio])
    if func == "quadratic":
        return np.column_stack([ones, e_ecal ** 2, e_hcal ** 2])
    raise ValueError(f"Unknown function: {func}")


def fit_energy(e_ecal, e_hcal, e_beam, func="ratio", weighting="sqrt"):
    """
    Weighted least-squares fit of E_rec parameters.

    Returns the parameter vector (length depends on func).
    """
    X = _design_matrix(e_ecal, e_hcal, func)
    y = np.asarray(e_beam, dtype=np.float64)

    if weighting == "equal":
        w = np.ones_like(y)
    elif weighting == "sqrt":
        w = np.sqrt(y)
    else:
        raise ValueError(f"Unknown weighting: {weighting}")

    sw = np.sqrt(w)
    Xw = X * sw[:, None]
    yw = y * sw
    params, *_ = np.linalg.lstsq(Xw, yw, rcond=None)
    return params


def predict_energy(e_ecal, e_hcal, params, func="ratio"):
    """Apply fitted parameters to get E_rec."""
    X = _design_matrix(e_ecal, e_hcal, func)
    return X @ params
