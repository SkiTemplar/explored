"""Sintesis modal: suma de sinusoides con decaimiento exponencial propio, el
modelo estandar para el "golpe" de un objeto resonante (madera, piedra...)."""

from __future__ import annotations

import numpy as np


def modal_hit(
    sr: int,
    duration_s: float,
    base_freq: float,
    mode_ratios: list[float],
    mode_dampings_s: list[float],
    mode_amps: list[float],
    rng: np.random.Generator | None = None,
    detune: float = 0.0,
) -> np.ndarray:
    """Genera un golpe por sintesis modal (varios modos resonantes independientes)."""
    n = max(int(duration_s * sr), 1)
    t = np.arange(n) / sr
    out = np.zeros(n)
    for ratio, tau, amp in zip(mode_ratios, mode_dampings_s, mode_amps):
        freq = base_freq * ratio
        if detune and rng is not None:
            freq *= 1.0 + rng.uniform(-detune, detune)
        out += amp * np.exp(-t / max(tau, 1e-4)) * np.sin(2 * np.pi * freq * t)
    return out
