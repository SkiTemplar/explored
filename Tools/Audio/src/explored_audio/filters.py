"""Filtros de estado variable (paso bajo/alto/banda), estaticos y con barrido.

Con corte fijo, un filtro de estado variable (topologia de Chamberlin) es
equivalente a un biquad de segundo orden con los coeficientes clasicos del
"Audio EQ Cookbook" de Robert Bristow-Johnson: se implementa asi porque
`scipy.signal.lfilter` lo resuelve en C sobre todo el buffer de una vez.

Para barridos de frecuencia de corte (rafagas de viento, silbido de una ola
al retirarse) el corte cambia con el tiempo: se recalculan los coeficientes
por bloques pequeños (10 ms) y se conserva el estado del filtro (`zi`) entre
bloques, que es la tecnica habitual para modular un filtro de forma barata
sin caer a un bucle muestra a muestra.
"""

from __future__ import annotations

import numpy as np
from scipy import signal


def _coeffs(sr: float, fc: float, q: float, kind: str):
    fc = min(max(fc, 1.0), sr * 0.49)
    w0 = 2 * np.pi * fc / sr
    cos_w0, sin_w0 = np.cos(w0), np.sin(w0)
    alpha = sin_w0 / (2.0 * max(q, 1e-3))

    if kind == "lowpass":
        b0 = (1 - cos_w0) / 2
        b1 = 1 - cos_w0
        b2 = (1 - cos_w0) / 2
        a0 = 1 + alpha
        a1 = -2 * cos_w0
        a2 = 1 - alpha
    elif kind == "highpass":
        b0 = (1 + cos_w0) / 2
        b1 = -(1 + cos_w0)
        b2 = (1 + cos_w0) / 2
        a0 = 1 + alpha
        a1 = -2 * cos_w0
        a2 = 1 - alpha
    elif kind == "bandpass":
        b0 = alpha
        b1 = 0.0
        b2 = -alpha
        a0 = 1 + alpha
        a1 = -2 * cos_w0
        a2 = 1 - alpha
    else:
        raise ValueError(f"tipo de filtro desconocido: {kind}")

    b = np.array([b0, b1, b2]) / a0
    a = np.array([1.0, a1 / a0, a2 / a0])
    return b, a


def static_filter(x: np.ndarray, sr: float, fc: float, q: float = 0.707, kind: str = "lowpass") -> np.ndarray:
    """Filtro de estado variable de corte fijo (equivalente a biquad RBJ)."""
    b, a = _coeffs(sr, fc, q, kind)
    return signal.lfilter(b, a, x)


def time_varying_filter(
    x: np.ndarray,
    sr: float,
    cutoff_track: np.ndarray,
    q: float = 0.707,
    kind: str = "lowpass",
    block_size: int = 512,
) -> np.ndarray:
    """Filtro de estado variable con corte que varia en el tiempo.

    `cutoff_track` debe tener la misma longitud que `x`. Se recalculan los
    coeficientes cada `block_size` muestras (a 48 kHz, ~10.6 ms: mucho mas
    fino que cualquier rafaga de viento u ola perceptible) y se propaga el
    estado del filtro para no introducir clics entre bloques.
    """
    n = len(x)
    out = np.empty(n)
    zi = np.zeros(2)
    for start in range(0, n, block_size):
        end = min(start + block_size, n)
        fc = float(np.mean(cutoff_track[start:end]))
        b, a = _coeffs(sr, fc, q, kind)
        out[start:end], zi = signal.lfilter(b, a, x[start:end], zi=zi)
    return out
