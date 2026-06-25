#!/usr/bin/env bash
# Fast COMPILED run -- use this when the per-event cache must be rebuilt (e.g. the raw
# outfile_*.root changed). It works around two WSL quirks seen on this machine:
#   1. Reading ~2 GB of raw data over the /mnt/c (9p) mount is slow and can stall.
#   2. root.exe crashes when it dlopen's an ACLiC-built .so that lives on /mnt/c.
# Fix: stage the raw data and the macro on the native ext4 home, build the shared
# library there, but keep the project dir as CWD so plots/ and qa/cache_*.txt land in
# the repo (and the existing caches are reused).
#
# Normal day-to-day runs should just use scripts/run_all.sh -- once the cache exists it
# is fast and needs none of this.
set -euo pipefail

PROJ="$(cd "$(dirname "$0")/.." && pwd)"
EXT="$HOME/zdc_fast"
mkdir -p "$EXT/data"

echo "[run_fast] staging raw data on ext4 ($EXT/data) ..."
cp -ru "$PROJ/data/." "$EXT/data/"
cp "$PROJ/scripts/zdc_reco_browser.C" "$EXT/zdcmacro.C"
rm -f "$EXT"/zdcmacro_C*.so "$EXT"/zdcmacro_C*.d "$EXT"/zdcmacro_C*.pcm 2>/dev/null || true

if ! command -v root >/dev/null 2>&1; then
  [ -f "$HOME/root/bin/thisroot.sh" ] && source "$HOME/root/bin/thisroot.sh"
fi

cd "$PROJ"   # plots/ and qa/cache_*.txt are written relative to here
echo "[run_fast] compiling on ext4 and running ..."
root -l -b -q -e ".L $EXT/zdcmacro.C+" -e "zdc_reco_browser(\"$EXT/data\",\"plots\")"

echo "[run_fast] done. PNGs in plots/, cache refreshed in qa/cache_*.txt"
