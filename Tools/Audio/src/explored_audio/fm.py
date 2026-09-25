"""Sintesis FM y aditiva, usadas para los cantos de pajaros: un trino real es
sobre todo un barrido de tono con armonicos y una modulacion rapida, que FM
(portadora + modulador con indice variable) y una serie aditiva reproducen
mucho mejor que ruido filtrado."""

from __future__ import annotations

import numpy as np


def fm_chirp(
    sr: int,
    carrier_hz_env: np.ndarray,
    mod_ratio: float,
    mod_index_env: np.ndarray,
    amp_env: np.ndarray,
) -> np.ndarray:
    """`carrier_hz_env`, `mod_index_env` y `amp_env` deben tener la misma longitud."""
    dt = 1.0 / sr
    carrier_phase = 2 * np.pi * np.cumsum(carrier_hz_env) * dt
    mod_phase = 2 * np.pi * np.cumsum(carrier_hz_env * mod_ratio) * dt
    modulator = mod_index_env * np.sin(mod_phase)
    return amp_env * np.sin(carrier_phase + modulator)


def additive_harmonics(
    sr: int,
    base_freq_env: np.ndarray,
    n_harmonics: int,
    harmonic_decay: float,
    amp_env: np.ndarray,
) -> np.ndarray:
    """Serie de armonicos sobre una frecuencia base variable (barrido de trino)."""
    dt = 1.0 / sr
    phase_base = 2 * np.pi * np.cumsum(base_freq_env) * dt
    out = np.zeros_like(base_freq_env)
    for h in range(1, n_harmonics + 1):
        out += (harmonic_decay ** (h - 1)) * np.sin(h * phase_base)
    peak = np.max(np.abs(out))
    if peak > 1e-9:
        out = out / peak
    return amp_env * out
