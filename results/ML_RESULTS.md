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

## Testing results — per beam energy (held-out 20% validation)

`zdc.ml.evaluate --split val` (same seed as training, no train leakage); DSCB fit of
E_pred/E_beam for energy, Gaussian σ of (pred−true) for position. Overlay plots:
`results/compare_<particle>.png`; full tables: `results/ml_eval_<particle>.csv`.

**gamma** (selected):

| E [GeV] | ML E-res | trad E-res | ML σx | trad σx |
|---|---|---|---|---|
| 1  | 9.7 % | 14.6 % | 12.7 mm | 13.3 mm |
| 10 | 10.1 % | 6.3 % | 7.1 mm | 3.6 mm |
| 40 | 9.2 % | 3.2 % | 5.3 mm | 2.3 mm |

**neutron** (selected):

| E [GeV] | ML E-res | trad E-res | ML σx | trad σx |
|---|---|---|---|---|
| 20  | 9.5 % | 37.8 % | 20.8 mm | 18.4 mm |
| 50  | 16.6 % | 7.6 % | 10.3 mm | 13.5 mm |
| 300 | 14.4 % | 4.2 % | 11.3 mm | 8.7 mm |

**Read:** ML (first, untuned pass) is competitive with traditional at low energy and on
neutrons <50 GeV (where traditional struggles), but traditional — a tuned analytic fit —
wins at high energy. Per-energy ML numbers are noisy (~200 val events/energy → unstable
DSCB fits, e.g. the 9 GeV gamma / 200 GeV neutron outliers); the aggregate training MAEs
above are the more stable summary.

## Tuning levers for the "ML method together" phase

- `--pos_weight` (default 0.01) balances position-mm vs energy-log10 loss
- separate energy vs position models, or per-particle position-only model
- larger `--max_hits`, more epochs, GPU accelerator for bigger sweeps
- add edges (kNN) → message-passing GNN if DeepSets plateaus
