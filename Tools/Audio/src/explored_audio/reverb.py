"""Reverberacion de Schroeder: cuatro filtros de peine en paralelo seguidos de
dos filtros todo-paso en cascada. Es una respuesta sintetica (no una
convolucion con una IR grabada), pero es exactamente el diseño clasico de
Schroeder (1962) que pide el encargo.

Los filtros de peine/todo-paso de retardo entero se resuelven "por fase": un
retardo de D muestras con realimentacion es, en cada una de las D fases
(indice mod D), una sencilla IIR de primer orden en el tiempo lento (de D en
D muestras). Reorganizando el buffer en una matriz (pasos, fase) y filtrando
por columnas con `scipy.signal.lfilter(..., axis=0)` se evita el bucle
muestra a muestra (que con miles de muestras de retardo seria demasiado
lento) sin perder precision.
"""

from __future__ import annotations

import numpy as np
from scipy import signal


def _by_phase(x: np.ndarray, delay: int, b: list[float], a: list[float]) -> np.ndarray:
    n = len(x)
    pad = (-n) % delay
    xp = np.pad(x, (0, pad)) if pad else x
    reshaped = xp.reshape(-1, delay)
    filtered = signal.lfilter(b, a, reshaped, axis=0)
    return filtered.reshape(-1)[:n]


def _comb_filter(x: np.ndarray, delay: int, gain: float) -> np.ndarray:
    return _by_phase(x, delay, [1.0], [1.0, -gain])


def _allpass_filter(x: np.ndarray, delay: int, gain: float) -> np.ndarray:
    return _by_phase(x, delay, [-gain, 1.0], [1.0, -gain])


def schroeder_reverb(
    x: np.ndarray,
    sr: int,
    room_size: float = 0.5,
    damping: float = 0.3,
    wet: float = 0.25,
) -> np.ndarray:
    """Reverberacion sintetica de Schroeder (4 peines + 2 todo-paso) mezclada
    con la señal seca. `room_size` en [0,1] alarga los peines y sube su
    realimentacion; `damping` amortigua algo la cola."""
    comb_ms = [29.7, 37.1, 41.1, 43.7]
    allpass_ms = [5.0, 1.7]
    size = 0.7 + 0.6 * room_size
    comb_gain = min(0.55 + 0.35 * room_size, 0.97) * (1.0 - 0.15 * damping)

    wet_sig = np.zeros(len(x))
    for ms in comb_ms:
        delay = max(int(sr * ms * size / 1000.0), 1)
        wet_sig += _comb_filter(x, delay, comb_gain)
    wet_sig /= len(comb_ms)

    for ms in allpass_ms:
        delay = max(int(sr * ms / 1000.0), 1)
        wet_sig = _allpass_filter(wet_sig, delay, 0.5)

    wet_peak = np.max(np.abs(wet_sig))
    dry_peak = np.max(np.abs(x))
    if wet_peak > 1e-9 and dry_peak > 1e-9:
        wet_sig *= dry_peak / wet_peak

    return (1.0 - wet) * x + wet * wet_sig
