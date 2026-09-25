"""Sintesis granular: granos de ruido filtrado en banda, esparcidos con una
distribucion tipo Poisson (mas natural que una rejilla regular). Se usa para
crujidos de pasos, gotas de lluvia y chisporroteo de fuego."""

from __future__ import annotations

import numpy as np

from .filters import static_filter


def place_grains(
    total_len: int,
    sr: int,
    rng: np.random.Generator,
    rate_hz: float,
    jitter: float = 0.75,
) -> list[tuple[int, float]]:
    """Posiciones (en muestras) y amplitud aleatoria por grano, en [0.5, 1.0].

    `jitter` interpola entre una rejilla regular (0.0, huecos = 1/rate_hz
    exactos) y un proceso de Poisson puro (1.0, huecos exponenciales: el
    ritmo irregular tipico de las olas o las gotas de lluvia)."""
    positions: list[tuple[int, float]] = []
    t = 0.0
    mean_gap = 1.0 / max(rate_hz, 1e-3)
    while True:
        gap = mean_gap * (1.0 - jitter) + rng.exponential(mean_gap) * jitter
        t += max(gap, 1e-4)
        pos = int(t * sr)
        if pos >= total_len:
            break
        positions.append((pos, float(rng.uniform(0.5, 1.0))))
    return positions


def render_noise_grains(
    total_len: int,
    sr: int,
    rng: np.random.Generator,
    rate_hz: float,
    grain_len_s_range: tuple[float, float],
    band_hz_range: tuple[float, float],
    q: float = 0.9,
    amp_scale: float = 1.0,
) -> np.ndarray:
    """Mezcla granos de ruido pasabanda, cada uno con su propia envolvente de Hann."""
    out = np.zeros(total_len)
    for pos, amp in place_grains(total_len, sr, rng, rate_hz):
        glen = max(int(rng.uniform(*grain_len_s_range) * sr), 8)
        noise = rng.standard_normal(glen)
        fc = rng.uniform(*band_hz_range)
        grain = static_filter(noise, sr, fc, q=q, kind="bandpass")
        grain *= np.hanning(glen) * amp * amp_scale
        end = min(pos + glen, total_len)
        out[pos:end] += grain[: end - pos]
    return out
