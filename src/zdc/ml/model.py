"""
DeepSets regressor for ZDC reconstruction (phase 2), pure PyTorch.

Permutation-invariant over hits, so no graph library needed (works on the
Vertex `pytorch-xla` image without torch-geometric). Two heads:
    energy  -> E_beam [GeV]
    position-> (x, y) at HCAL reference z [mm]

Reference approach: eiccodesign/regressiononly (DeepSets), adapted to the
LYSO-ECAL + HCAL point cloud of this detector.
"""

import torch
import torch.nn as nn


class DeepSetsZDC(nn.Module):
    def __init__(self, in_dim=5, hidden=128, latent=256,
                 predict_energy=True, predict_position=True):
        super().__init__()
        self.predict_energy = predict_energy
        self.predict_position = predict_position

        self.phi = nn.Sequential(
            nn.Linear(in_dim, hidden), nn.ReLU(),
            nn.Linear(hidden, hidden), nn.ReLU(),
            nn.Linear(hidden, latent), nn.ReLU(),
        )
        self.rho = nn.Sequential(
            nn.Linear(latent, hidden), nn.ReLU(),
            nn.Linear(hidden, hidden), nn.ReLU(),
        )
        if predict_energy:
            self.energy_head = nn.Linear(hidden, 1)
        if predict_position:
            self.position_head = nn.Linear(hidden, 2)

    def forward(self, x, mask):
        """
        x    : (B, N, in_dim) padded hit features
        mask : (B, N) 1 for real hits, 0 for padding
        """
        h = self.phi(x)                          # (B, N, latent)
        h = h * mask.unsqueeze(-1)               # zero padded hits
        pooled = h.sum(dim=1)                    # permutation-invariant sum
        g = self.rho(pooled)                     # (B, hidden)

        out = {}
        if self.predict_energy:
            out["energy"] = self.energy_head(g).squeeze(-1)
        if self.predict_position:
            out["position"] = self.position_head(g)
        return out
