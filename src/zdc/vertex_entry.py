"""
Vertex AI custom-job entry point (phase 2).

Pattern (per project Vertex notes):
  1. pip-install only light deps in-container (numpy<2 to respect the prebuilt ABI)
  2. gsutil-pull the dataset from --data_uri
  3. train (zdc.ml.train)
  4. gsutil-push artifacts to --out_uri

Invoked as:  python -m zdc.vertex_entry --data_uri=gs://... --out_uri=gs://... --epochs=50
gcloud runs through the shell so .cmd shims resolve on Windows submitters.
"""

import argparse
import os
import subprocess
import sys


def sh(cmd):
    print(f"$ {cmd}", flush=True)
    subprocess.run(cmd, shell=True, check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data_uri", required=True, help="gs:// dir holding ml_<particle>.npz")
    ap.add_argument("--out_uri", required=True, help="gs:// dir for model + metrics")
    ap.add_argument("--particle", default="neutron")
    ap.add_argument("--epochs", type=int, default=50)
    ap.add_argument("--batch_size", type=int, default=128)
    ap.add_argument("--lr", type=float, default=1e-3)
    args = ap.parse_args()

    # 1. light deps only (image already has torch/numpy/pyarrow prebuilt)
    sh(f'{sys.executable} -m pip install -q "numpy<2" pyyaml matplotlib')

    # 2. pull data
    local_data = "/tmp/data"
    os.makedirs(local_data, exist_ok=True)
    sh(f'gsutil -q -m cp -r "{args.data_uri}/*" "{local_data}/"')

    # 3. train
    local_out = "/tmp/out"
    os.makedirs(local_out, exist_ok=True)
    npz = os.path.join(local_data, f"ml_{args.particle}.npz")
    sh(f'{sys.executable} -m zdc.ml.train --data "{npz}" --out_dir "{local_out}" '
       f'--epochs {args.epochs} --batch_size {args.batch_size} --lr {args.lr}')

    # 4. push artifacts
    sh(f'gsutil -q -m cp -r "{local_out}/*" "{args.out_uri}/"')
    print("entry: done", flush=True)


if __name__ == "__main__":
    main()
