"""Vocalizaciones de fauna (biblia de contenido §6) mas alla de las aves ya
cubiertas en `birds.py`: mono, cocodrilo, jabali, cangrejo y tortuga. Mismas
tecnicas que el resto del paquete (FM/aditiva para timbres tonales, modal
para golpes/clics, ruido filtrado para gruñidos y soplidos), sin muestras."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length
from ..filters import static_filter
from ..fm import additive_harmonics
from ..granular import render_noise_grains
from ..modal import modal_hit
from ..noise import brown_noise
from ..rng import rng_for

SR = SAMPLE_RATE


def _note_env(n: int, attack: float, release: float, shape: float = 1.5) -> np.ndarray:
    return fit_length(ar_envelope(SR, attack, release, shape=shape), n)


def _monkey(rng: np.random.Generator) -> np.ndarray:
    """Tropa de monos capuchinos: serie de ladridos cortos y agudos, cada uno
    un barrido descendente con un punto de aspereza (armonicos + ruido de
    banda estrecha), como un chillido-parloteo real."""
    n_barks = int(rng.integers(4, 8))
    pieces = []
    for _ in range(n_barks):
        dur = rng.uniform(0.05, 0.09)
        n = int(dur * SR)
        t = np.linspace(0, 1, n)
        f0 = rng.uniform(500, 900)
        f1 = f0 - rng.uniform(100, 300)
        carrier = f0 + (f1 - f0) * t
        amp = _note_env(n, dur * 0.15, dur * 0.6, shape=1.8)
        bark = additive_harmonics(SR, carrier, n_harmonics=6, harmonic_decay=0.7, amp_env=amp)
        rasp = static_filter(rng.standard_normal(n), SR, fc=rng.uniform(1200, 2600), q=0.8, kind="bandpass") * amp * 0.5
        pieces.append(bark * 0.8 + rasp)
        pieces.append(np.zeros(int(rng.uniform(0.02, 0.05) * SR)))
    return np.concatenate(pieces) if pieces else np.zeros(1)


def _crocodile(rng: np.random.Generator) -> np.ndarray:
    """Cocodrilo de estuario: gruñido grave y largo (ruido marron filtrado con
    un pulso lento) sobre un cuerpo resonante muy grave por sintesis modal."""
    dur = rng.uniform(1.2, 1.8)
    n = int(dur * SR)
    t = np.arange(n) / SR
    growl = static_filter(brown_noise(n, rng, leak=0.998), SR, fc=220, q=0.7, kind="lowpass")
    pulse = 0.6 + 0.4 * np.sin(2 * np.pi * rng.uniform(3, 5) * t)
    body = modal_hit(
        SR, dur, base_freq=rng.uniform(55, 80),
        mode_ratios=[1.0, 1.9, 3.1], mode_dampings_s=[0.4, 0.25, 0.15],
        mode_amps=[1.0, 0.4, 0.2], rng=rng, detune=0.02,
    )
    env = _note_env(n, dur * 0.2, dur * 0.7, shape=1.2)
    return (growl * pulse * 0.6 + body * 0.7) * env


def _boar(rng: np.random.Generator) -> np.ndarray:
    """Jabali: resoplido nasal (ruido pasabanda) sobre un gruñido corto y
    grave por sintesis modal."""
    dur = rng.uniform(0.3, 0.5)
    n = int(dur * SR)
    snort = static_filter(rng.standard_normal(n), SR, fc=rng.uniform(250, 450), q=1.1, kind="bandpass")
    snort_env = _note_env(n, 0.01, dur - 0.01, shape=1.6)
    grunt = modal_hit(
        SR, dur, base_freq=rng.uniform(90, 140),
        mode_ratios=[1.0, 1.6, 2.3], mode_dampings_s=[0.06, 0.04, 0.03],
        mode_amps=[1.0, 0.5, 0.3], rng=rng, detune=0.03,
    )
    return snort * snort_env * 0.7 + grunt * 0.6


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
    "monkey": _monkey,
    "crocodile": _crocodile,
    "boar": _boar,
    "crab": _crab,
    "turtle": _turtle,
}


def fauna_call(name: str, species: str) -> np.ndarray:
    rng = rng_for(name)
    return _SPECIES_FUNCS[species](rng)
