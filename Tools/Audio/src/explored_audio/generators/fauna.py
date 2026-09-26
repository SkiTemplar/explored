"""Vocalizaciones de fauna (biblia de contenido §6) mas alla de las aves ya
cubiertas en `birds.py`: cangrejo y tortuga. El GDD no tiene fauna terrestre
(§10, §12): solo especies marinas o de orilla. Mismas
tecnicas que el resto del paquete (FM/aditiva para timbres tonales, modal
para golpes/clics, ruido filtrado para gruñidos y soplidos), sin muestras."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length
from ..filters import static_filter
from ..granular import render_noise_grains
from ..modal import modal_hit
from ..rng import rng_for

SR = SAMPLE_RATE


def _note_env(n: int, attack: float, release: float, shape: float = 1.5) -> np.ndarray:
    return fit_length(ar_envelope(SR, attack, release, shape=shape), n)


def _crab(rng: np.random.Generator) -> np.ndarray:
    """Cangrejo: serie entrecortada de clics agudos de pinza (golpes modales
    muy cortos y muy amortiguados)."""
    n_clicks = int(rng.integers(2, 5))
    pieces = []
    for _ in range(n_clicks):
        dur = rng.uniform(0.02, 0.04)
        click = modal_hit(
            SR, dur, base_freq=rng.uniform(2200, 3800),
            mode_ratios=[1.0, 1.7], mode_dampings_s=[0.006, 0.004],
            mode_amps=[1.0, 0.5], rng=rng, detune=0.05,
        )
        pieces.append(click)
        pieces.append(np.zeros(int(rng.uniform(0.03, 0.09) * SR)))
    return np.concatenate(pieces) if pieces else np.zeros(1)


def _turtle(rng: np.random.Generator) -> np.ndarray:
    """Tortuga marina: casi silenciosa a proposito (no se caza, GDD §6) - un
    soplido suave de exhalacion mas el roce leve de las aletas en la arena."""
    dur = rng.uniform(0.8, 1.3)
    n = int(dur * SR)
    breath = static_filter(rng.standard_normal(n), SR, fc=500, q=0.6, kind="lowpass")
    breath = static_filter(breath, SR, fc=150, q=0.6, kind="highpass")
    env = _note_env(n, dur * 0.3, dur * 0.6, shape=1.1)
    drag = render_noise_grains(n, SR, rng, rate_hz=18.0, grain_len_s_range=(0.02, 0.05), band_hz_range=(600, 2200), q=1.0, amp_scale=0.25)
    return breath * env * 0.5 + drag * 0.3


_SPECIES_FUNCS = {
    "crab": _crab,
    "turtle": _turtle,
}


def fauna_call(name: str, species: str) -> np.ndarray:
    rng = rng_for(name)
    return _SPECIES_FUNCS[species](rng)
