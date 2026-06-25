"""
Optional helper only. The main requested workflow is ROOT TBrowser:

    bash scripts/run_all.sh
    root -l plots/energy_reconstruction_browser.root
    new TBrowser

This file is retained so older repo references do not break. It does not generate
or modify the TBrowser output.
"""
from pathlib import Path

plots = Path("plots")
expected = [
    "slide5_energy_dump.png",
    "slide6_1GeV_gamma_regression.png",
    "slide7_gamma_resolution_bias.png",
    "slide8_neutron_resolution_bias.png",
]

if __name__ == "__main__":
    print("Main workflow: bash scripts/run_all.sh, then open plots/energy_reconstruction_browser.root in TBrowser.")
    for name in expected:
        p = plots / name
        print(f"{'OK' if p.exists() else 'MISSING'}  {p}")
