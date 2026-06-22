# ASIoP ZDC — Particle Finder (ML)

ML particle finder for the **Taiwan-group ZDC** (Academia Sinica, EIC-Asia):
a Zero Degree Calorimeter = **LYSO-crystal ECAL + SiPM-on-tile HCAL**.

Goal: identify the particle that hit the ZDC from its calorimeter hit pattern,
using a learned model over the raw hit cloud (not hand-tuned formulas).

> **Project restarted.** The earlier work — reproducing the supervisor's traditional
> energy/position reconstruction *and* a first regression-ML pass — is preserved on
> branch [`v1-recon-reproduction`](https://github.com/JulianAttemptsCoding/ASIoP-ML-ZDC/tree/v1-recon-reproduction).
> `main` is now a clean scaffold for the particle-finder task.

## Open task definition (fill in)

"Particle finder" is not yet pinned down. Current scaffold = **per-event particle-ID
classifier** (gamma vs neutron, from the two MC sample sets). Decide and adjust:

- [ ] which classes? gamma / neutron now; add **pi0** (→2γ), **lambda decay** (n+2γ), background/noise
- [ ] single-label ID, or multi-object **finding / counting** (how many particles, where)?
- [ ] inputs: ECAL+HCAL hits (current), or HCAL-only, or add timing?
- [ ] metric: accuracy / ROC for ID, vs efficiency-purity for finding

## Detector & data

| | |
|---|---|
| ECAL | LYSO crystal, 20×20 cells, 3×3×7 cm³ |
| HCAL | sampling (steel + scint + SiPM), 64 layers, ~24.9 mm pitch, 163 cm z |
| Beam | spread beam 25°; gamma 0.1–40 GeV, neutron 10–300 GeV |
| Format | EDM4hep/podio ROOT: `EcalFarForwardZDCHits`, `HcalFarForwardZDCHits`, `MCParticles` |

ROOT files (1.6 GB) are gitignored. Unzip the supervisor's archive into `to2026Summer/`.

## Layout

```
src/zdc/
  io.py            EDM4hep reader (hits, truth, layer grouping) — reusable
  beam.py          filename -> beam energy / particle
  ml/
    dataset.py     ROOT -> point clouds + particle labels  (results/ml/particles.npz)
    model.py       DeepSets classifier (pure PyTorch; GNN-upgrade path noted)
    train.py       cross-entropy training, val accuracy
    evaluate.py    confusion matrix + per-class precision/recall on held-out val
  vertex_entry.py  Vertex custom-job entry (pip light -> gsutil pull -> train -> push)
vertex/submit.sh   gcloud submission (project asiop-zdc, bucket asiop-zdc-uscentral1)
pyproject.toml     sdist packaging for Vertex
```

## Workflow

```bash
pip install -r requirements.txt          # + torch for local training/eval
export PYTHONPATH=src

# 1. build the labeled dataset from ROOT
python -m zdc.ml.dataset --max_hits 512                 # -> results/ml/particles.npz

# 2a. train locally (smoke)
python -m zdc.ml.train --data results/ml/particles.npz --epochs 10

# 2b. or train on Vertex AI (heavy)
bash vertex/submit.sh                                   # n1-standard-16, 40 epochs

# 3. evaluate on held-out validation
python -m zdc.ml.evaluate --data results/ml/particles.npz --model results/ml/run/model.pt
```

## Vertex AI (already live)

Project **asiop-zdc** (us-central1), bucket `gs://asiop-zdc-uscentral1`, billing linked,
APIs enabled, `pytorch-xla.2-4.py310` image. gcloud installed at
`~/AppData/Local/Google/Cloud SDK/...` (not on PATH; `submit.sh` adds it).
First job in a fresh project sits PENDING ~14 min (image pull) before RUNNING — normal.

## Model upgrade path

DeepSets (sum-pool, no edges) is the baseline — runs on the Vertex image without
torch-geometric. To go to a true message-passing GNN: add kNN edges in `dataset.py`
and replace the pooling in `model.py` with edge-conditioned aggregation. The
background paper (`reference paper.pdf`, arXiv:2406.12877) used GNNs for the same ZDC
energy/angle/classification problem.
```
