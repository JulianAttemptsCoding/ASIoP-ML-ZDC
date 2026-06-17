# ML ZDC — GNN Reconstruction for Zero Degree Calorimeter

Remake of arXiv:2406.12877v2 ("Design and Simulation of a SiPM-on-Tile ZDC for the future EIC, and its Performance with Graph Neural Networks") for an HEP lab experiment.

Reference code: [eiccodesign/regressiononly @ zdc_classification](https://github.com/eiccodesign/regressiononly/tree/zdc_classification)  
Reference ONNX models: [zenodo.11187659](https://doi.org/10.5281/zenodo.11187659)

## Tasks

| Task | Output | Config |
|------|--------|--------|
| Single-neutron regression | E, θ | `a100_neutron.yaml` |
| π⁰/γ classification + regression | E, θ, class | `a100_pi0_gamma.yaml` |
| Multi-neutron energy | E_total | *(extend neutron config)* |

## Setup

```bash
conda env create -f environment.yml
conda activate ml-zdc
```

## Quick start

1. Set `data_dir` in your chosen config to point at your ROOT files.
2. Set `detector_branch` to match your simulation output branch name.
3. First run preprocesses ROOT → pickles (slow once, fast after):

```bash
python train.py --config configs/a100_neutron.yaml
```

4. On A100 cluster:

```bash
sbatch submit_a100.sh configs/a100_neutron.yaml
```

5. After training, evaluate:

```bash
python inference.py \
  --config configs/a100_neutron.yaml \
  --checkpoint outputs/a100_neutron/best_model
```

## Key parameters (from paper)

| Parameter | Value |
|-----------|-------|
| Sampling fraction | 2.1% |
| Hit energy threshold | 0.5 MIP = 0.25 MeV |
| Hit time cut | < 275 ns |
| kNN neighbors | 10 |
| Node features | log₁₀(E), x, y, z |
| Architecture | 4 × Dense(64), ReLU, He-normal |
| Batch size | 256 (paper) / 512 (A100 config) |
| Optimizer | Adam, LR=1e-3, halved every 5 epochs |
| Classification α | 0.75 |

## TODO before first run

- [ ] Set `data_dir` in configs
- [ ] Confirm ROOT branch names from your simulation (`detector_branch`, `mc_branch`)
- [ ] Confirm `tree_name` in `src/data/generator.py` matches your ROOT file structure
- [ ] Set SLURM `--partition` and `--account` in `submit_a100.sh`
- [ ] Install environment: `conda env create -f environment.yml`
