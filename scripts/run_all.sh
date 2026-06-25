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

# Generate the four clean physics graphs (no slides).
# Runs interpreted (no ACLiC '+'). After the first build, qa/cache_*.txt holds the
# per-event extract, so reruns skip the ~2 GB raw read and finish in seconds -- no
# compiled .so needed, and it avoids the dlopen-from-/mnt/c crash seen under WSL.
# To rebuild the cache fast (e.g. raw files changed) use the compiled path with the
# shared library on a native (ext4) filesystem -- see scripts/run_fast.sh.
root -l -b -q 'scripts/zdc_reco_browser.C("data","plots")'

echo ""
echo "Done. 4 graphs (match reference slides 5-8):"
echo "  plots/energy_dump.png"
echo "  plots/gamma1GeV_regression.png"
echo "  plots/gamma_resolution_bias.png"
echo "  plots/neutron_resolution_bias.png"
echo ""
echo "Analysis archive (optional QA, not a plot): qa/energy_reconstruction_browser.root"
