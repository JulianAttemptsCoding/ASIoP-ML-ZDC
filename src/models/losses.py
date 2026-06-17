"""
Loss functions for ZDC GNN training.

Regression:     MAE on energy and theta
Classification: binary crossentropy + MAE  (Eq. 1, arXiv:2406.12877v2)
  L = (1 - alpha) * L_classification + alpha * L_regression
  alpha = 0.75 (paper default)
"""

import tensorflow as tf


def regression_loss(predictions, targets, weights=(1.0, 1.0)):
    """MAE on energy and theta with optional per-output weights."""
    e_loss     = tf.reduce_mean(tf.abs(predictions["energy"] - targets["energy"]))
    theta_loss = tf.reduce_mean(tf.abs(predictions["theta"]  - targets["theta"]))
    return weights[0] * e_loss + weights[1] * theta_loss


def classification_loss(predictions, targets):
    """Binary crossentropy: 0=gamma, 1=pi0."""
    bce = tf.keras.losses.BinaryCrossentropy(from_logits=True)
    return bce(targets["label"][:, None], predictions["class_logit"][:, None])


def combined_loss(predictions, targets, alpha=0.75):
    """
    Combined classification + regression loss (paper Eq. 1).
    alpha=0.75: regression weighted more heavily than classification.
    """
    l_reg   = regression_loss(predictions, targets)
    l_class = classification_loss(predictions, targets)
    return (1.0 - alpha) * l_class + alpha * l_reg, l_class, l_reg


def get_loss_fn(cfg):
    """Return appropriate loss function based on task config."""
    task  = cfg["task"]
    alpha = cfg.get("alpha", 0.75)

    if task == "pi0_gamma":
        def loss_fn(predictions, targets):
            total, l_c, l_r = combined_loss(predictions, targets, alpha=alpha)
            return total, {"loss": total, "class_loss": l_c, "reg_loss": l_r}
    else:
        def loss_fn(predictions, targets):
            total = regression_loss(predictions, targets)
            return total, {"loss": total}

    return loss_fn
