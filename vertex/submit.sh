#!/bin/bash
# Submit a ZDC ML training job to Vertex AI (phase 2).
#
# Prereqs (DONE 2026-06-17): SDK installed, juliansjuan08@gmail.com authed,
# project asiop-zdc created, billing linked, APIs enabled, bucket exists.
#   Build sdist + upload data happen below.
#
# gcloud crashes if the OpenAI/Codex bin is on PATH ("untrusted mount point"):
export PATH="$(echo "$PATH" | tr ':' '\n' | grep -vi Codex | paste -sd:)"
# gcloud not on PATH on this Windows box — add the winget install location:
export PATH="$PATH:/c/Users/Julia/AppData/Local/Google/Cloud SDK/google-cloud-sdk/bin"

set -euo pipefail

# ---- constants (project: ASIoP ZDC) ---------------------------------------
PROJECT="asiop-zdc"
REGION="us-central1"
BUCKET="gs://asiop-zdc-uscentral1"
IMAGE="us-docker.pkg.dev/vertex-ai/training/pytorch-xla.2-4.py310:latest"
MACHINE="${MACHINE:-n1-standard-16}"          # n1-standard-4 for smoke
PARTICLE="${PARTICLE:-neutron}"
EPOCHS="${EPOCHS:-50}"
RID="$(date +%Y%m%d_%H%M%S)_${PARTICLE}"

# ---- 1. build + upload package --------------------------------------------
python -m build --sdist --outdir vertex/dist .
gsutil -q -m cp vertex/dist/*.tar.gz "${BUCKET}/packages/"
PKG="$(ls -t vertex/dist/*.tar.gz | head -1 | xargs basename)"

# ---- 2. upload dataset (built locally by zdc.ml.dataset) ------------------
gsutil -q -m cp "results/ml/ml_${PARTICLE}.npz" "${BUCKET}/repo_inputs/zdc/"

# ---- 3. submit -------------------------------------------------------------
gcloud ai custom-jobs create \
  --region="${REGION}" --project="${PROJECT}" \
  --display-name="zdc_${RID}" \
  --python-package-uris="${BUCKET}/packages/${PKG}" \
  --worker-pool-spec="machine-type=${MACHINE},replica-count=1,executor-image-uri=${IMAGE},python-module=zdc.vertex_entry" \
  --args="--data_uri=${BUCKET}/repo_inputs/zdc,--out_uri=${BUCKET}/runs/${RID},--particle=${PARTICLE},--epochs=${EPOCHS}"

echo "submitted zdc_${RID}; poll with:"
echo "  gcloud ai custom-jobs list --region=${REGION} --project=${PROJECT} --filter='displayName:zdc_*' --format='table(displayName,state)'"
echo "fetch results with:"
echo "  gsutil -q -m cp -r '${BUCKET}/runs/${RID}/*' results/ml/${RID}/"
