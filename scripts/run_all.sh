#!/bin/bash
# run_all.sh — regenerate all energy reconstruction plots
# Run from repo root in WSL: bash scripts/run_all.sh
# Requires ROOT via: source /home/ulia/root/bin/thisroot.sh

set -e
source /home/ulia/root/bin/thisroot.sh

mkdir -p plots

echo "=== Slide 5: Energy Dump ==="
root -b -q 'scripts/plot_slide5_energy_dump.C'

echo "=== Slide 6: E_rec/E_beam distribution (1 GeV gamma) ==="
root -b -q 'scripts/plot_slide6_erec_dist.C'

echo "=== Slides 7-8: Resolution + Bias vs Energy ==="
root -b -q 'scripts/plot_slide7_8_resolution.C'

echo ""
echo "All plots saved to plots/"
echo "Open plots/energy_reconstruction.root in ROOT TBrowser to browse all canvases."
