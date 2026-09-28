"""Bucles continuos por fundido cruzado "hacia el futuro".

Se genera la señal con `loop_len + fade_len` muestras del mismo proceso
continuo (mismo ruido filtrado, misma envolvente, sin reiniciar nada en
`loop_len`). El final del bucle es exactamente `x[loop_len - 1]`; el
principio se mezcla con `x[loop_len + n]`, que es la muestra que de forma
natural habria seguido a `x[loop_len - 1]` si la señal hubiera seguido
sonando. La curva de fundido es coseno elevado (derivada nula en ambos
extremos), asi que la propia mezcla no introduce un clic: el resultado es
indistinguible, en el punto de union, de la señal continua original.
"""

from __future__ import annotations

import numpy as np


def raised_cosine_fade(n: int) -> np.ndarray:
    t = np.linspace(0.0, 1.0, n, endpoint=False)
    return 0.5 * (1.0 - np.cos(np.pi * t))


def seamless_loop(x_extended: np.ndarray, loop_len: int, fade_len: int) -> np.ndarray:
    """x_extended: (..., loop_len + fade_len) -> bucle continuo (..., loop_len)."""
    if x_extended.shape[-1] < loop_len + fade_len:
        raise ValueError("la señal extendida es mas corta que loop_len + fade_len")
    if fade_len > loop_len:
        raise ValueError("el fundido cruzado no puede ser mas largo que el bucle")

    fade_in = raised_cosine_fade(fade_len)
    fade_out = 1.0 - fade_in

    out = x_extended[..., :loop_len].copy()
    tail_future = x_extended[..., loop_len : loop_len + fade_len]
    out[..., :fade_len] = out[..., :fade_len] * fade_in + tail_future * fade_out
    return out
