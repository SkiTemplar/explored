"""Sonidos de interfaz: tonos limpios y muy cortos (sintesis aditiva simple),
sin ruido, para que se sientan sinteticos y no se confundan con el mundo."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length
from ..rng import rng_for

SR = SAMPLE_RATE


def _tone(n: int, freq_env: np.ndarray, harmonics: tuple[float, ...] = (1.0,)) -> np.ndarray:
    dt = 1.0 / SR
    phase = 2 * np.pi * np.cumsum(freq_env) * dt
    out = np.zeros(n)
    for h, weight in enumerate(harmonics, start=1):
        out += weight * np.sin(h * phase)
    return out / sum(harmonics)


def ui_click(name: str) -> np.ndarray:
    rng = rng_for(name)
    dur = 0.045
    n = int(dur * SR)
    freq = np.full(n, rng.uniform(1400, 1700))
    tone = _tone(n, freq, harmonics=(1.0, 0.3))
    env = fit_length(ar_envelope(SR, 0.002, dur - 0.002, shape=3.0), n)
    return tone * env


def ui_hover(name: str) -> np.ndarray:
    rng = rng_for(name)
    dur = 0.06
    n = int(dur * SR)
    freq = np.full(n, rng.uniform(2000, 2300))
    tone = _tone(n, freq, harmonics=(1.0, 0.2))
    env = fit_length(ar_envelope(SR, 0.008, dur - 0.008, shape=2.0), n)
    return tone * env * 0.6


def ui_open(name: str) -> np.ndarray:
    rng = rng_for(name)
    dur = 0.14
    n = int(dur * SR)
    t = np.linspace(0, 1, n)
    freq = rng.uniform(420, 500) + t * rng.uniform(380, 460)
    tone = _tone(n, freq, harmonics=(1.0, 0.35, 0.15))
    env = fit_length(ar_envelope(SR, 0.01, dur - 0.01, shape=1.4), n)
    return tone * env * 0.8


def ui_close(name: str) -> np.ndarray:
    rng = rng_for(name)
    dur = 0.14
    n = int(dur * SR)
    t = np.linspace(0, 1, n)
    freq = rng.uniform(800, 900) - t * rng.uniform(380, 460)
    tone = _tone(n, freq, harmonics=(1.0, 0.35, 0.15))
    env = fit_length(ar_envelope(SR, 0.01, dur - 0.01, shape=1.4), n)
    return tone * env * 0.8
