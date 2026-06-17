# ASIoP ZDC — Reconstruction (Traditional + ML)

Reconstruction software for the **Taiwan-group ZDC** (Academia Sinica, EIC-Asia):
a Zero Degree Calorimeter = **LYSO-crystal ECAL + SiPM-on-tile HCAL**.

Goal (from lab supervisor Chia-Yu Hsieh): reproduce the **traditional** energy &
position reconstruction from the 2026/05/01 status talk, then develop the **ML**
method together. This repo does both.

- Task spec: `to2026Summer/20260501_ZDC_MC.pdf`
- MC samples: `to2026Summer/20260421_gamma_LYSO_diffAngle/`, `…/20260324_neutron_LYSO_diffAngle/`
- Background paper (different EIC ZDC, GNN reference): `reference paper.pdf` (arXiv:2406.12877v2)

## Detector & samples

| | |
|---|---|
| ECAL | LYSO crystal, 20×20 cells, 3×3×7 cm³, 60×60 cm² |
| HCAL | sampling (steel + scint tile + SiPM), 64 layers, ~24.9 mm pitch, 163 cm in z |
| Beam | spread beam, 25° opening; gamma 0.1–40 GeV, neutron 10–300 GeV |
| Format | EDM4hep/podio ROOT: `EcalFarForwardZDCHits`, `HcalFarForwardZDCHits`, `MCParticles` |
| Files | one fixed beam energy per file, 1000 events; primary = first `generatorStatus==1` |

ROOT files are **not** committed (1.6 GB). Unzip the supervisor's archive into
`to2026Summer/` to reproduce.

## Layout

```
src/zdc/
  io.py           EDM4hep reader: per-event E_ECAL/E_HCAL, HCAL hits, truth; layer grouping
  beam.py         filename -> beam energy (gamma in name; neutron index -> steering)
  energy.py       energy regression: linear/ratio/quadratic x equal/sqrt weighting
  crystalball.py  double-sided crystal ball fit -> resolution (sigma) & bias (mu)
  position.py     HCAL track recon: layer-avg pos, pol1 fit, residual vs truth
  ml/             phase 2: dataset.py (point clouds), model.py (DeepSets), train.py
  vertex_entry.py Vertex AI custom-job entry (pip light -> gsutil pull -> train -> push)
scripts/
  build_features.py        ROOT -> results/features_<p>.npz  (energy scalars)
  energy_resolution.py     reproduce energy resolution+bias -> results/energy_*.png/csv
  position_resolution.py   reproduce position resolution+bias -> results/position_*.png/csv
vertex/submit.sh           gcloud custom-job submission (phase 2)
results/                   committed reproduction plots + CSV summaries
```

## Run the traditional reproduction

```bash
pip install -r requirements.txt
export PYTHONPATH=src

python scripts/build_features.py --particle both
python scripts/energy_resolution.py --particle both
python scripts/position_resolution.py --particle both          # all 64 layers, energy-and-hit weighting
```

### Reproduced results (matches the status talk)

**Energy** (E_rec = f(E_ECAL, E_HCAL), √E_beam weighting + ratio fn; DSCB fit of E_rec/E_beam):

| beam | gamma σ/E | neutron σ/E |
|---|---|---|
| 1 GeV | 14.6 % | — |
| 50 GeV | — | 7.6 % |
| 300 GeV | — | 4.2 % (bias 1.03) |
| 40 GeV | 3.2 % | — |

→ gamma satisfies 20%/√E ⊕ 5%; neutron **fails < 50 GeV**, ~satisfies above — same
conclusion as the talk.

**Position** (HCAL only, energy-weighted layer x̄/ȳ, pol1, energy-and-hit weighting):

| beam | neutron σ_x | gamma σ_x |
|---|---|---|
| 1 GeV | — | 13.3 mm |
| 300 GeV | 8.7 mm | — |
| 40 GeV | — | 2.3 mm |

→ resolution improves with energy, ~mm-scale bias — matches the talk (neutron peak
~6 mm, gamma peak ~5 mm; small offsets from reference-z and fit-window choices).

## ML phase 2 (DeepSets, on Vertex AI)

Each event → point cloud of hits `[log10 E, x, y, z, is_ecal]`; permutation-invariant
DeepSets predicts E_beam and (x,y). Pure PyTorch (no graph lib) to match the Vertex
`pytorch-xla` image.

```bash
export PYTHONPATH=src
python -m zdc.ml.dataset --particle both --max_hits 512     # ROOT -> results/ml/ml_<p>.npz
# local smoke (needs torch):   python -m zdc.ml.train --data results/ml/ml_neutron.npz --epochs 5
# cloud:                       bash vertex/submit.sh        # see "Vertex AI status"
```

### Vertex AI status — **live, models trained**

Project **ASIoP ZDC** (`asiop-zdc`, us-central1), bucket `gs://asiop-zdc-uscentral1`,
billing linked, APIs enabled. First neutron + gamma runs **SUCCEEDED** (n1-standard-16,
50 epochs). Convergence + comparison in [`results/ML_RESULTS.md`](results/ML_RESULTS.md);
models at `gs://asiop-zdc-uscentral1/runs/full_20260617_213548_<particle>/model.pt`.

Per-energy ML resolution (apples-to-apples vs the traditional CSVs):
`python -m zdc.ml.evaluate --data results/ml/ml_<p>.npz --model <model.pt> --particle <p>`
(needs torch locally, or run as a Vertex job).

gcloud was installed via winget to `~/AppData/Local/Google/Cloud SDK/...` (not on PATH;
`vertex/submit.sh` adds it).

## To-do (from the talk, p.18)

- HCAL geometry hexagonal → rectangular scintillator tile
- evaluate with neutron/gamma/**lambda** gun across varying z-positions (Sullivan process, Λ→nπ⁰→n2γ)
- WSi ECAL variant
- GNN energy + track reconstruction (this repo's phase 2)
