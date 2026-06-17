"""
Double-sided Crystal Ball fit.

Used to extract resolution (sigma) and bias (mu) from the E_rec/E_beam
distribution (slide p.6: "Fit E_rec with double-sided crystal ball function").
"""

import numpy as np
from scipy.optimize import curve_fit


def double_sided_crystal_ball(x, mu, sigma, alpha_l, n_l, alpha_r, n_r, norm):
    """
    Gaussian core with power-law tails on both sides.

    alpha_* : transition point (in units of sigma) for left/right tail
    n_*     : power-law exponent for left/right tail
    """
    t = (x - mu) / sigma
    out = np.empty_like(t)

    core = (t > -alpha_l) & (t < alpha_r)
    out[core] = np.exp(-0.5 * t[core] ** 2)

    left = t <= -alpha_l
    a_l = (n_l / alpha_l) ** n_l * np.exp(-0.5 * alpha_l ** 2)
    b_l = n_l / alpha_l - alpha_l
    out[left] = a_l * (b_l - t[left]) ** (-n_l)

    right = t >= alpha_r
    a_r = (n_r / alpha_r) ** n_r * np.exp(-0.5 * alpha_r ** 2)
    b_r = n_r / alpha_r - alpha_r
    out[right] = a_r * (b_r + t[right]) ** (-n_r)

    return norm * out


def fit_resolution(values, bins=60, range_sigma=4.0):
    """
    Fit the distribution of `values` (e.g. E_rec/E_beam) with a double-sided
    crystal ball and return (mu, sigma, mu_err, sigma_err, hist_info).

    Falls back to a plain Gaussian if the DSCB fit fails to converge.
    """
    values = np.asarray(values)
    values = values[np.isfinite(values)]
    if len(values) < 20:
        return np.nan, np.nan, np.nan, np.nan, None

    mu0, sig0 = np.median(values), values.std()
    lo, hi = mu0 - range_sigma * sig0, mu0 + range_sigma * sig0
    counts, edges = np.histogram(values, bins=bins, range=(lo, hi))
    centers = 0.5 * (edges[:-1] + edges[1:])

    p0 = [mu0, sig0, 1.5, 2.0, 1.5, 2.0, counts.max()]
    bounds = (
        [lo, 1e-6, 0.1, 0.5, 0.1, 0.5, 0.0],
        [hi, 5 * sig0, 10.0, 50.0, 10.0, 50.0, 5 * counts.max()],
    )
    try:
        popt, pcov = curve_fit(
            double_sided_crystal_ball, centers, counts,
            p0=p0, bounds=bounds, maxfev=20000,
        )
        perr = np.sqrt(np.diag(pcov))
        mu, sigma = popt[0], popt[1]
        mu_err, sigma_err = perr[0], perr[1]
    except Exception:
        # Gaussian fallback
        def gauss(x, m, s, a):
            return a * np.exp(-0.5 * ((x - m) / s) ** 2)
        try:
            popt, pcov = curve_fit(gauss, centers, counts,
                                   p0=[mu0, sig0, counts.max()], maxfev=10000)
            perr = np.sqrt(np.diag(pcov))
            mu, sigma, mu_err, sigma_err = popt[0], popt[1], perr[0], perr[1]
        except Exception:
            mu, sigma, mu_err, sigma_err = mu0, sig0, np.nan, np.nan

    hist_info = {"centers": centers, "counts": counts}
    return mu, abs(sigma), mu_err, sigma_err, hist_info
