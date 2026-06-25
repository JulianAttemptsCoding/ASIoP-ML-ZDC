#!/bin/bash
# run_all.sh — regenerate all energy reconstruction plots from raw data
# Run from repo root in WSL: bash scripts/run_all.sh
# Requires ROOT: source /home/ulia/root/bin/thisroot.sh

set -e
source /home/ulia/root/bin/thisroot.sh

mkdir -p plots

echo "=== Full pipeline: raw outfile_*.root -> all 4 plots ==="
root -b -q 'scripts/plot_from_raw.C'

echo ""
echo "All plots saved to plots/"
echo "Open plots/energy_reconstruction.root in ROOT TBrowser to browse all canvases."
