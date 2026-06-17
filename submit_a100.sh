#!/bin/bash
# SLURM job submission for A100 training
# Adjust partition, account, and paths to match your cluster

#SBATCH --job-name=zdc-gnn
#SBATCH --partition=gpu          # TODO: set your A100 partition name
#SBATCH --account=your_account   # TODO: set your allocation/account
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=16       # for preprocessing workers (num_procs)
#SBATCH --gres=gpu:a100:1
#SBATCH --mem=64G
#SBATCH --time=12:00:00
#SBATCH --output=logs/%j_train.out
#SBATCH --error=logs/%j_train.err

mkdir -p logs

# ── Environment ───────────────────────────────────────────────────────────────
# Option A: conda
# source activate ml-zdc

# Option B: module system (common on HPC clusters)
# module load cuda/11.8 cudnn/8.6 python/3.10

# ── Run ───────────────────────────────────────────────────────────────────────
CONFIG=${1:-configs/a100_neutron.yaml}

echo "Starting ZDC GNN training"
echo "Config: $CONFIG"
echo "Host: $(hostname)"
echo "GPUs: $(nvidia-smi --list-gpus)"
date

python train.py --config "$CONFIG"

echo "Done"
date
