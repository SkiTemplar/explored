"""Cantos de pajaros por sintesis FM y aditiva: un trino real es sobre todo un
barrido de tono con armonicos, no ruido filtrado, asi que estas dos tecnicas
(portadora+modulador, y serie de armonicos sobre una frecuencia que barre)
son las que dan timbre creible sin usar muestras."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length
from ..fm import additive_harmonics, fm_chirp
from ..rng import rng_for

SR = SAMPLE_RATE


def _note_env(n: int, attack: float, release: float, shape: float = 1.5) -> np.ndarray:
    return fit_length(ar_envelope(SR, attack, release, shape=shape), n)


def _parrot(rng: np.random.Generator) -> np.ndarray:
    """Graznido aspero: FM con salto erratico de frecuencia e indice alto."""
    dur = rng.uniform(0.35, 0.55)
    n = int(dur * SR)
    t = np.linspace(0, 1, n)
    base = rng.uniform(900, 1400)
    jump = rng.uniform(300, 700) * np.sign(np.sin(2 * np.pi * rng.uniform(6, 10) * t))
    carrier = base + jump
    mod_index = 4.0 + 2.0 * np.abs(np.sin(2 * np.pi * rng.uniform(8, 14) * t))
    amp = _note_env(n, dur * 0.08, dur * 0.7, shape=1.1)
    out = fm_chirp(SR, carrier, mod_ratio=1.7, mod_index_env=mod_index, amp_env=amp)
    rasp = additive_harmonics(SR, carrier, n_harmonics=4, harmonic_decay=0.6, amp_env=amp * 0.4)
    return out * 0.7 + rasp


def _gull(rng: np.random.Generator) -> np.ndarray:
    """Grito largo tipico de gaviota: barrido ascendente-descendente con vibrato."""
    dur = rng.uniform(0.55, 0.85)
    n = int(dur * SR)
    t = np.linspace(0, 1, n)
    sweep = rng.uniform(1100, 1400) + 900 * np.sin(np.pi * t) - 300 * t
    vibrato = 1.0 + 0.03 * np.sin(2 * np.pi * 22 * t)
    carrier = sweep * vibrato
    amp = _note_env(n, dur * 0.15, dur * 0.75, shape=1.3)
    out = fm_chirp(SR, carrier, mod_ratio=2.0, mod_index_env=np.full(n, 1.5), amp_env=amp)
    return out


def _songbird(rng: np.random.Generator) -> np.ndarray:
    """Frase corta de 3-5 notas brillantes, cada una con su propio barrido rapido."""
    n_notes = rng.integers(3, 6)
    pieces = []
    for _ in range(n_notes):
        dur = rng.uniform(0.08, 0.16)
        n = int(dur * SR)
        t = np.linspace(0, 1, n)
        f0 = rng.uniform(2800, 4200)
        f1 = f0 + rng.uniform(-1200, 1800)
        carrier = f0 + (f1 - f0) * t
        amp = _note_env(n, dur * 0.2, dur * 0.7, shape=1.2)
        note = additive_harmonics(SR, carrier, n_harmonics=3, harmonic_decay=0.5, amp_env=amp)
        pieces.append(note)
        gap = int(rng.uniform(0.03, 0.09) * SR)
        pieces.append(np.zeros(gap))
    return np.concatenate(pieces) if pieces else np.zeros(1)


_SPECIES_FUNCS = {
    "parrot": _parrot,
    "gull": _gull,
    "songbird": _songbird,
}


def bird_call(name: str, species: str) -> np.ndarray:
    rng = rng_for(name)
    return _SPECIES_FUNCS[species](rng)
