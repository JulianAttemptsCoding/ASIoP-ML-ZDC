"""
DeepSets classifier for the ZDC particle finder (pure PyTorch).

Permutation-invariant over hits -> no graph library needed (runs on the Vertex
`pytorch-xla` image without torch-geometric). One classification head over
particle classes.

Swap-in path to a real message-passing GNN later: add kNN edges in the dataset
and replace the sum-pool with edge-conditioned aggregation.
"""

import torch
import torch.nn as nn


class DeepSetsClassifier(nn.Module):
    def __init__(self, in_dim=5, hidden=128, latent=256, num_classes=2):
        super().__init__()
        self.phi = nn.Sequential(
            nn.Linear(in_dim, hidden), nn.ReLU(),
            nn.Linear(hidden, hidden), nn.ReLU(),
            nn.Linear(hidden, latent), nn.ReLU(),
        )
        self.rho = nn.Sequential(
            nn.Linear(latent, hidden), nn.ReLU(),
            nn.Linear(hidden, hidden), nn.ReLU(),
        )
        self.head = nn.Linear(hidden, num_classes)

    def forward(self, x, mask):
        """
        x    : (B, N, in_dim) padded hit features
        mask : (B, N) 1 for real hits, 0 for padding
        returns logits (B, num_classes)
        """
        h = self.phi(x)
        h = h * mask.unsqueeze(-1)        # zero padded hits
        pooled = h.sum(dim=1)             # permutation-invariant aggregation
        g = self.rho(pooled)
        return self.head(g)
