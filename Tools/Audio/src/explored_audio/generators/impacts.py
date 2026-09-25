"""Golpes de herramienta: transitorio de impacto (ruido de banda ancha muy
corto) mas un cuerpo resonante por sintesis modal (madera hueca / piedra dura)."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import fit_length
from ..filters import static_filter
from ..modal import modal_hit
from ..rng import rng_for

SR = SAMPLE_RATE


def _transient(rng: np.random.Generator, n: int, fc: float, decay_s: float) -> np.ndarray:
    t = np.arange(n) / SR
    click = static_filter(rng.standard_normal(n), SR, fc=fc, q=0.7, kind="highpass")
    return click * fit_length(np.exp(-t / decay_s), n)


def wood_chop(name: str) -> np.ndarray:
    rng = rng_for(name)
    dur = rng.uniform(0.45, 0.65)
    n = int(dur * SR)
    transient = _transient(rng, n, fc=1200, decay_s=0.01)
    body = modal_hit(
        SR, dur, base_freq=rng.uniform(180, 260),
        mode_ratios=[1.0, 2.8, 4.9, 7.1], mode_dampings_s=[0.09, 0.06, 0.04, 0.025],
        mode_amps=[1.0, 0.55, 0.35, 0.18], rng=rng, detune=0.02,
    )
    return transient * 0.6 + body * 0.75


def stone_hit(name: str) -> np.ndarray:
    rng = rng_for(name)
    dur = rng.uniform(0.3, 0.45)
    n = int(dur * SR)
    transient = _transient(rng, n, fc=2500, decay_s=0.006)
    body = modal_hit(
        SR, dur, base_freq=rng.uniform(700, 1100),
        mode_ratios=[1.0, 1.83, 2.6, 3.9], mode_dampings_s=[0.05, 0.035, 0.02, 0.012],
        mode_amps=[1.0, 0.5, 0.3, 0.15], rng=rng, detune=0.03,
    )
    return transient * 0.7 + body * 0.6
