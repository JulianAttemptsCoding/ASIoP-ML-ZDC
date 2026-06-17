"""
Map MC sample files to their true beam energy.

Each ROOT file is a single fixed beam energy, 1000 events.
Gamma energy is in the filename; neutron energy is indexed (steering files).
"""

import os
import re

# Neutron file index -> beam energy [GeV] (from steering_neutron*.py).
# neutron4 (100 GeV) is absent from the provided sample set.
NEUTRON_ENERGY_GEV = {
    1: 10.0,
    2: 20.0,
    3: 50.0,
    4: 100.0,
    5: 150.0,
    6: 200.0,
    7: 300.0,
}


def gamma_energy_from_name(filename: str) -> float:
    """outfile_gamma1GeV.root -> 1.0 ; outfile_gamma300MeV.root -> 0.3 (GeV)."""
    base = os.path.basename(filename)
    m = re.search(r"gamma(\d+)(MeV|GeV)", base)
    if not m:
        raise ValueError(f"Cannot parse gamma energy from {base}")
    val, unit = float(m.group(1)), m.group(2)
    return val / 1000.0 if unit == "MeV" else val


def neutron_energy_from_name(filename: str) -> float:
    """outfile_neutron7.root -> 300.0 (GeV)."""
    base = os.path.basename(filename)
    m = re.search(r"neutron(\d+)", base)
    if not m:
        raise ValueError(f"Cannot parse neutron index from {base}")
    idx = int(m.group(1))
    return NEUTRON_ENERGY_GEV[idx]


def beam_energy(filename: str, particle: str) -> float:
    if particle == "gamma":
        return gamma_energy_from_name(filename)
    if particle == "neutron":
        return neutron_energy_from_name(filename)
    raise ValueError(f"Unknown particle: {particle}")
