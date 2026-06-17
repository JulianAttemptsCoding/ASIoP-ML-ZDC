"""
GNN model for ZDC energy/angle regression and pi0/gamma classification.

Architecture: DeepSets or GraphNetwork blocks (graph_nets + sonnet),
followed by MLP head for output.

Paper: arXiv:2406.12877v2, Sec. 3.1
  - 4 dense layers x 64 nodes, ReLU, He-normal init
  - Adam optimizer, LR 1e-3, halved every 5 epochs, min 1e-6
"""

import tensorflow as tf
import sonnet as snt
import graph_nets as gn
from graph_nets import graphs as gn_graphs
from graph_nets import modules as gn_modules
from graph_nets import utils_tf


# ── MLP factory ───────────────────────────────────────────────────────────────

def make_mlp(latent_dim, num_layers, activation="relu", use_layer_norm=True, name=None):
    """Standard MLP block used throughout the network."""
    act_fn = tf.keras.activations.get(activation)
    layers = []
    for _ in range(num_layers):
        layers.append(snt.Linear(latent_dim,
                                 w_init=tf.initializers.he_normal(),
                                 b_init=tf.initializers.zeros()))
    if use_layer_norm:
        layers.append(snt.LayerNorm(axis=-1, create_scale=True, create_offset=True))

    def forward(x):
        for i, layer in enumerate(layers):
            if isinstance(layer, snt.Linear):
                x = act_fn(layer(x))
            else:
                x = layer(x)
        return x

    return forward


# ── DeepSets block ────────────────────────────────────────────────────────────

class DeepSetsBlock(snt.Module):
    """
    Permutation-invariant set aggregation block.
    Nodes processed independently, then aggregated into global.
    """

    def __init__(self, latent_dim, num_layers, activation, use_layer_norm,
                 concat_input=True, name="deepsets_block"):
        super().__init__(name=name)
        self.concat_input = concat_input
        self._node_mlp   = make_mlp(latent_dim, num_layers, activation, use_layer_norm)
        self._global_mlp = make_mlp(latent_dim, num_layers, activation, use_layer_norm)
        self._node_proj  = snt.Linear(latent_dim, w_init=tf.initializers.he_normal())

    def __call__(self, graphs, training=False):
        nodes   = graphs.nodes
        globals_ = graphs.globals

        # node update
        node_out = self._node_mlp(nodes)
        if self.concat_input:
            node_out = tf.concat([nodes, node_out], axis=-1)
        node_out = self._node_proj(node_out)

        # aggregate nodes -> global (mean)
        agg = utils_tf.segment_mean(node_out, graphs.graph_of_node_indices(),
                                    num_segments=graphs.n_node.shape[0])
        if globals_ is not None:
            global_in = tf.concat([globals_, agg], axis=-1)
        else:
            global_in = agg

        global_out = self._global_mlp(global_in)
        if self.concat_input and globals_ is not None:
            global_out = tf.concat([globals_, global_out], axis=-1)

        return graphs.replace(nodes=node_out, globals=global_out)


# ── Full GNN model ────────────────────────────────────────────────────────────

class ZDCModel(snt.Module):
    """
    Stacked DeepSets or GraphNetwork blocks + output MLP head.

    Outputs depend on task:
      'neutron'   : [E_pred, theta_pred]
      'pi0_gamma' : [E_pred, theta_pred, class_logit]
    """

    def __init__(self, cfg, name="zdc_model"):
        super().__init__(name=name)
        self.task         = cfg["task"]
        self.num_blocks   = cfg["num_blocks"]
        self.latent_dim   = cfg["latent_dim"]
        self.layers       = cfg["layers_per_block"]
        self.activation   = cfg["activation"]
        self.layer_norm   = cfg["use_layer_norm"]
        self.skip         = cfg["skip_connections"]
        self.model_type   = cfg["model_type"]

        self._blocks = [
            DeepSetsBlock(self.latent_dim, self.layers,
                          self.activation, self.layer_norm,
                          concat_input=self.skip,
                          name=f"block_{i}")
            for i in range(self.num_blocks)
        ]

        # output dimension: 2 for neutron (E, theta), 3 for pi0/gamma (E, theta, class)
        out_dim = 3 if self.task == "pi0_gamma" else 2
        self._output_mlp = snt.Sequential([
            snt.Linear(64, w_init=tf.initializers.he_normal()),
            tf.keras.layers.Activation(self.activation),
            snt.Linear(out_dim, w_init=tf.initializers.he_normal()),
        ])

    def __call__(self, graphs, training=False):
        x = graphs
        for block in self._blocks:
            x = block(x, training=training)

        out = self._output_mlp(x.globals)  # (batch, out_dim)

        result = {
            "energy": out[:, 0],
            "theta":  out[:, 1],
        }
        if self.task == "pi0_gamma":
            result["class_logit"] = out[:, 2]
            result["class_prob"]  = tf.sigmoid(out[:, 2])
        return result


# ── Graph tuple builder ───────────────────────────────────────────────────────

def batch_to_graphs_tuple(batch):
    """Convert collated batch dict to graph_nets GraphsTuple."""
    return gn_graphs.GraphsTuple(
        nodes=tf.constant(batch["nodes"],     dtype=tf.float32),
        edges=tf.constant(batch["edges"],     dtype=tf.float32),
        globals=tf.constant(batch["globals"], dtype=tf.float32),
        senders=tf.constant(batch["senders"], dtype=tf.int32),
        receivers=tf.constant(batch["receivers"], dtype=tf.int32),
        n_node=tf.constant(batch["n_node"],   dtype=tf.int32),
        n_edge=tf.constant(batch["n_edge"],   dtype=tf.int32),
    )
