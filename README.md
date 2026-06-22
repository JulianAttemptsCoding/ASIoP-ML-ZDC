# ASIoP ZDC — ML (clean slate)

ML work for the **Taiwan-group ZDC** (Academia Sinica, EIC-Asia): a Zero Degree
Calorimeter = **LYSO-crystal ECAL + SiPM-on-tile HCAL**.

`main` is intentionally a **clean slate**. The exact ML task is still to be defined.
All prior work is preserved on side branches (below).

## Branch index

| branch | what's there |
|---|---|
| `main` | clean slate (this README) |
| [`v1-recon-reproduction`](https://github.com/JulianAttemptsCoding/ASIoP-ML-ZDC/tree/v1-recon-reproduction) | reproduced the supervisor's **traditional** energy + position reconstruction (crystal-ball energy fit, HCAL pol1 track fit) **and** a first **regression ML** pass (DeepSets, energy+position) trained on Vertex AI |
| [`particle-finder-scaffold`](https://github.com/JulianAttemptsCoding/ASIoP-ML-ZDC/tree/particle-finder-scaffold) | scaffold for a **particle-ID classifier** (DeepSets, gamma vs neutron) on the reusable ROOT IO + Vertex infra |

Pull anything forward when the task is set, e.g.:
`git checkout v1-recon-reproduction -- src/zdc/io.py vertex/submit.sh`

## Detector & data (reference)

| | |
|---|---|
| ECAL | LYSO crystal, 20×20 cells, 3×3×7 cm³ |
| HCAL | sampling (steel + scint + SiPM), 64 layers, ~24.9 mm pitch, 163 cm z |
| Beam | spread beam 25°; gamma 0.1–40 GeV, neutron 10–300 GeV |
| Format | EDM4hep/podio ROOT: `EcalFarForwardZDCHits`, `HcalFarForwardZDCHits`, `MCParticles` |

MC samples (1.6 GB) live in `to2026Summer/` (gitignored). `reference paper.pdf` =
arXiv:2406.12877 (GNN reconstruction for a related EIC ZDC).

## Compute — Vertex AI (already provisioned)

- project **asiop-zdc** (us-central1), account juliansjuan08@gmail.com
- bucket `gs://asiop-zdc-uscentral1`, billing linked, Vertex AI + Storage APIs enabled
- image `us-docker.pkg.dev/vertex-ai/training/pytorch-xla.2-4.py310:latest`
- gcloud installed (winget) at `~/AppData/Local/Google/Cloud SDK/...` — not on PATH
- gotcha: first job in the project sits PENDING ~14 min (image pull) before RUNNING

## Next

Define the exact ZDC ML task, then branch from `main` and pull the needed pieces from
the scaffolds above.
