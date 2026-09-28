"""Sonoridad integrada ITU-R BS.1770-4 con puertas, para musica grabada.

`explored_audio.levels.lufs_approx` no tiene puertas y usa los coeficientes
fijos a 48 kHz: basta para efectos sinteticos, pero una grabacion de piano
con silencios largos quedaria medida varios dB por debajo de lo real. Aqui
se calcula el filtro K para cualquier frecuencia de muestreo y se aplican
la puerta absoluta (-70 LUFS) y la relativa (-10 LU) de la norma.
"""

from __future__ import annotations

import math

import numpy as np
from scipy import signal

ABSOLUTE_GATE_LUFS = -70.0
RELATIVE_GATE_LU = -10.0
BLOCK_S = 0.4
OVERLAP = 0.75


def k_weighting(fs: int) -> tuple[tuple[np.ndarray, np.ndarray], tuple[np.ndarray, np.ndarray]]:
    """Coeficientes (b, a) de las dos etapas del filtro K a `fs` Hz.

    Parametros de la norma (los mismos que dan, a 48 kHz, los coeficientes
    tabulados en BS.1770): estante alto de +4 dB en 1681,97 Hz y paso alto de
    segundo orden en 38,14 Hz."""
    # Etapa 1: estante alto.
    f0, gain_db, q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    k = math.tan(math.pi * f0 / fs)
    vh = 10.0 ** (gain_db / 20.0)
    vb = vh**0.4996667741545416
    a0 = 1.0 + k / q + k * k
    b1 = np.array([(vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0])
    a1 = np.array([1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0])
    # Etapa 2: paso alto (RLB).
    f0, q = 38.13547087602444, 0.5003270373238773
    k = math.tan(math.pi * f0 / fs)
    a0 = 1.0 + k / q + k * k
    b2 = np.array([1.0, -2.0, 1.0])
    a2 = np.array([1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0])
    return (b1, a1), (b2, a2)


def _as_channels(x: np.ndarray) -> np.ndarray:
    """Devuelve (canales, muestras). Acepta mono 1D o (muestras, canales)
    como lo entrega soundfile."""
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 1:
        return x[np.newaxis, :]
    if x.ndim != 2:
        raise ValueError("se esperaba audio 1D o 2D")
    return x.T


def integrated_lufs(x: np.ndarray, fs: int) -> float:
    """Sonoridad integrada en LUFS. Devuelve -inf si nada supera la puerta
    absoluta (silencio, o una senal mas corta que un bloque de 400 ms)."""
    if fs <= 0:
        raise ValueError("fs debe ser positiva")
    ch = _as_channels(x)
    if not np.all(np.isfinite(ch)):
        raise ValueError("el audio contiene NaN o infinitos")
    (b1, a1), (b2, a2) = k_weighting(fs)
    y = signal.lfilter(b2, a2, signal.lfilter(b1, a1, ch, axis=-1), axis=-1)

    block = int(round(BLOCK_S * fs))
    step = int(round(BLOCK_S * (1.0 - OVERLAP) * fs))
    n = y.shape[-1]
    if block <= 0 or n < block:
        return float("-inf")
    # Energia media por bloque y canal con sumas acumuladas (O(n)).
    csum = np.concatenate([np.zeros((y.shape[0], 1)), np.cumsum(y * y, axis=-1)], axis=-1)
    starts = np.arange(0, n - block + 1, step)
    z = (csum[:, starts + block] - csum[:, starts]) / block
    # Pesos de canal G = 1 para L, R y C; los envolventes no se usan aqui.
    power = z.sum(axis=0)
    with np.errstate(divide="ignore"):
        loud = -0.691 + 10.0 * np.log10(power)

    gated = power[loud > ABSOLUTE_GATE_LUFS]
    if gated.size == 0:
        return float("-inf")
    relative = -0.691 + 10.0 * math.log10(float(gated.mean())) + RELATIVE_GATE_LU
    final = power[(loud > ABSOLUTE_GATE_LUFS) & (loud > relative)]
    if final.size == 0:
        return float("-inf")
    return -0.691 + 10.0 * math.log10(float(final.mean()))


def sample_peak_dbfs(x: np.ndarray) -> float:
    p = float(np.max(np.abs(x))) if np.size(x) else 0.0
    return 20.0 * math.log10(p) if p > 0 else float("-inf")
