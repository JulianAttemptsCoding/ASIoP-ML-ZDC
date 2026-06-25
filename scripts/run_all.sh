#!/usr/bin/env bash
# v5 regression-fix runner. Run from repo root:
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
root -l -q 'scripts/zdc_make_graphs_v5.C("data","plots","qa")'
echo ""
echo "Done. Open with:"
echo "  root -l plots/energy_reconstruction_graphs_v5.root"
echo "  new TBrowser"
echo ""
echo "Main comparison canvases:"
echo "  01_main_graphs/c_gamma_resolution_bias_p0free"
echo "  01_main_graphs/c_gamma_resolution_bias_p0zero"
echo "  01_main_graphs/c_gamma_resolution_bias_p0ridge"
echo "  01_main_graphs/c_neutron_resolution_bias_p0free"
echo "  01_main_graphs/c_neutron_resolution_bias_p0zero"
echo "  01_main_graphs/c_neutron_resolution_bias_p0ridge"
