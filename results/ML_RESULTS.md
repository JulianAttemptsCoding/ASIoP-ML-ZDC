# ML Results — DeepSets on Vertex AI (first run)

Trained on Vertex AI, project **asiop-zdc**, `n1-standard-16`, image
`pytorch-xla.2-4.py310`. DeepSets over per-event hit point clouds
`[log10 E, x, y, z, is_ecal]`, two heads (energy, position). 50 epochs each.
Models stored at `gs://asiop-zdc-uscentral1/runs/full_20260617_213548_<particle>/model.pt`.

## Convergence (validation, 20% holdout)

| particle | energy MAE(log10) start→best | position MAE start→best |
|---|---|---|
| neutron | 0.242 → **0.097** | 47.8 → **8.9 mm** |
| gamma   | 0.174 → **0.044** | 47.3 → **8.2 mm** |

- Energy head learns strongly (gamma 0.044 in log10 ≈ ~11% multiplicative spread aggregated over 0.1–40 GeV; neutron harder, as in the traditional study).
- Position head reaches ~8 mm MAE across **all** energies combined — same order as the
  traditional baseline (neutron 300 GeV ≈ 8.7 mm). Untuned first pass.

## vs traditional (reproduced earlier)

| metric | traditional | ML (this run) |
|---|---|---|
| gamma energy res @ high E | ~3 % | aggregate ~few-% (needs per-E DSCB eval) |
| neutron energy res @ 300 GeV | 4.2 % | per-E eval pending |
| neutron position @ 300 GeV | 8.7 mm | ~8.9 mm MAE (all-E) |
| gamma position @ 40 GeV | 2.3 mm | per-E eval pending |

Per-energy ML resolution/bias (apples-to-apples with the traditional CSVs) comes from
`python -m zdc.ml.evaluate` — needs torch locally, or run as a Vertex eval job.

## Tuning levers for the "ML method together" phase

- `--pos_weight` (default 0.01) balances position-mm vs energy-log10 loss
- separate energy vs position models, or per-particle position-only model
- larger `--max_hits`, more epochs, GPU accelerator for bigger sweeps
- add edges (kNN) → message-passing GNN if DeepSets plateaus
