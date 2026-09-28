"""Generadores de ruido: blanco, rosa y marron (browniano).

El ruido rosa usa el filtro IIR de tercer orden clasico (Trammell / lista
music-dsp) que aproxima una caida de -3 dB/octava en todo el rango audible.
El ruido marron es un integrador con fuga sobre ruido blanco: cae a
-6 dB/octava y no arrastra continua gracias a la fuga.
"""

from __future__ import annotations

import numpy as np
from scipy import signal


def white_noise(n: int, rng: np.random.Generator) -> np.ndarray:
    return rng.standard_normal(n)


_PINK_B = [0.049922035, -0.095993537, 0.050612699, -0.004408786]
_PINK_A = [1.0, -2.494956002, 2.017265875, -0.522189400]


def pink_noise(n: int, rng: np.random.Generator, warmup: int = 2000) -> np.ndarray:
    """Ruido rosa, normalizado a desviacion tipica ~1."""
    white = white_noise(n + warmup, rng)
    pink = np.asarray(signal.lfilter(_PINK_B, _PINK_A, white))[warmup:]
    std = np.std(pink)
    return pink / std if std > 1e-9 else pink


def brown_noise(n: int, rng: np.random.Generator, leak: float = 0.999, warmup: int = 2000) -> np.ndarray:
    """Ruido marron/browniano: integrador de un polo con fuga, normalizado."""
    white = white_noise(n + warmup, rng)
    brown = np.asarray(signal.lfilter([1.0], [1.0, -leak], white))[warmup:]
    std = np.std(brown)
    return brown / std if std > 1e-9 else brown
