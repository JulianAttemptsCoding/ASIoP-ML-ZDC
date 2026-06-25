#!/usr/bin/env bash
# Main runner: generates all slide PNGs from raw outfile_*.root data.
# Run from repo root:
#   bash scripts/run_all.sh
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p plots qa
if ! command -v root >/dev/null 2>&1; then
  if [ -f /home/ulia/root/bin/thisroot.sh ]; then
    source /home/ulia/root/bin/thisroot.sh
  fi
fi
if ! command -v root >/dev/null 2>&1; then
  echo "[ERROR] ROOT is not on PATH. Run: source /path/to/root/bin/thisroot.sh" >&2
  exit 1
fi

# Main slide generator (slides 5-8 PNGs + TBrowser ROOT file)
root -l -q 'scripts/zdc_reco_browser.C("data","plots")'

echo ""
echo "Done. Slide PNGs:"
echo "  plots/slide5_energy_dump.png"
echo "  plots/slide6_1GeV_gamma_regression.png"
echo "  plots/slide7_gamma_resolution_bias.png"
echo "  plots/slide8_neutron_resolution_bias.png"
echo ""
echo "TBrowser (inspect all canvases + histograms):"
echo "  root -l plots/energy_reconstruction_browser.root"
echo "  new TBrowser"
