"""Envolventes y moduladores lentos: ataque/mantenimiento/relajacion, decaimiento
exponencial (para sintesis modal) y paseos aleatorios suavizados (para oleaje,
rafagas de viento y variaciones lentas de densidad en los ambientes)."""

from __future__ import annotations

import numpy as np
from scipy import signal


def ar_envelope(sr: int, attack_s: float, release_s: float, hold_s: float = 0.0, shape: float = 1.0) -> np.ndarray:
    """Envolvente ataque-mantenimiento-relajacion con curva `shape` (>1 = mas percusiva)."""
    a_n = max(int(attack_s * sr), 1)
    h_n = max(int(hold_s * sr), 0)
    r_n = max(int(release_s * sr), 1)
    attack = np.linspace(0.0, 1.0, a_n) ** shape
    hold = np.ones(h_n)
    release = np.linspace(1.0, 0.0, r_n) ** shape
    return np.concatenate([attack, hold, release])


def exp_decay(sr: int, duration_s: float, tau_s: float) -> np.ndarray:
    """Decaimiento exponencial puro exp(-t/tau) usado por la sintesis modal."""
    n = max(int(duration_s * sr), 1)
    t = np.arange(n) / sr
    return np.exp(-t / max(tau_s, 1e-6))


def smooth_random_walk(
    n: int,
    rng: np.random.Generator,
    smoothing_hz: float,
    sr: int,
    low: float = 0.0,
    high: float = 1.0,
) -> np.ndarray:
    """Paseo aleatorio suavizado (filtro de un polo) normalizado al rango [low, high]."""
    steps = rng.standard_normal(n)
    alpha = np.exp(-2.0 * np.pi * smoothing_hz / sr)
    # Sin `zi`, `lfilter` devuelve solo el array (los stubs anuncian tambien la tupla).
    walk = np.asarray(signal.lfilter([1 - alpha], [1, -alpha], steps))
    walk -= walk.mean()
    peak = np.max(np.abs(walk))
    if peak > 1e-9:
        walk /= peak
    return low + (walk * 0.5 + 0.5) * (high - low)


def sine_lfo(n: int, sr: int, freq_hz: float, phase: float = 0.0) -> np.ndarray:
    t = np.arange(n) / sr
    return np.sin(2 * np.pi * freq_hz * t + phase)


def fit_length(x: np.ndarray, n: int) -> np.ndarray:
    """Ajusta `x` a exactamente `n` muestras (recorta o repite el ultimo valor).

    Los redondeos de `int(segundos * sr)` al combinar varias envolventes
    pueden desajustar la longitud en un puñado de muestras; esto evita el
    error de broadcasting al multiplicar señales que "deberian" medir igual.
    """
    if len(x) == n:
        return x
    if len(x) > n:
        return x[:n]
    pad_value = x[-1] if len(x) else 0.0
    return np.pad(x, (0, n - len(x)), mode="constant", constant_values=pad_value)
