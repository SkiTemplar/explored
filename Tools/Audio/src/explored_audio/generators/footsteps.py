"""Veinte pasos mono (5 materiales x 4 variaciones): granulado para la textura
de la superficie y, donde el material lo pide, un golpe modal breve para el
"cuerpo" del paso (piedra, madera)."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length
from ..filters import static_filter
from ..granular import render_noise_grains
from ..modal import modal_hit
from ..rng import rng_for

SR = SAMPLE_RATE


def _footstep_sand(rng: np.random.Generator) -> np.ndarray:
    dur = rng.uniform(0.16, 0.22)
    n = int(dur * SR)
    grains = render_noise_grains(n, SR, rng, rate_hz=55.0, grain_len_s_range=(0.01, 0.03), band_hz_range=(700, 3200), q=0.7)
    thump = static_filter(rng.standard_normal(n), SR, fc=140, q=0.7, kind="lowpass")
    thump *= fit_length(ar_envelope(SR, 0.005, dur - 0.005, shape=2.5), n)
    return grains * 0.7 + thump * 0.25


def _footstep_grass(rng: np.random.Generator) -> np.ndarray:
    dur = rng.uniform(0.15, 0.22)
    n = int(dur * SR)
    grains = render_noise_grains(n, SR, rng, rate_hz=70.0, grain_len_s_range=(0.006, 0.02), band_hz_range=(1500, 6500), q=1.2)
    return grains * 0.8


def _footstep_rock(rng: np.random.Generator) -> np.ndarray:
    dur = rng.uniform(0.12, 0.18)
    n = int(dur * SR)
    click = modal_hit(
        SR, dur, base_freq=rng.uniform(900, 1500),
        mode_ratios=[1.0, 2.4, 3.7], mode_dampings_s=[0.015, 0.01, 0.007],
        mode_amps=[1.0, 0.45, 0.25], rng=rng, detune=0.02,
    )
    scrape = render_noise_grains(n, SR, rng, rate_hz=30.0, grain_len_s_range=(0.006, 0.015), band_hz_range=(1200, 4000), q=1.0, amp_scale=0.3)
    return click * 0.7 + scrape


def _footstep_wood(rng: np.random.Generator) -> np.ndarray:
    dur = rng.uniform(0.14, 0.2)
    n = int(dur * SR)
    knock = modal_hit(
        SR, dur, base_freq=rng.uniform(220, 320),
        mode_ratios=[1.0, 2.76, 5.4, 8.9], mode_dampings_s=[0.05, 0.035, 0.02, 0.012],
        mode_amps=[1.0, 0.5, 0.3, 0.15], rng=rng, detune=0.015,
    )
    creak = render_noise_grains(n, SR, rng, rate_hz=25.0, grain_len_s_range=(0.008, 0.02), band_hz_range=(400, 2200), q=1.0, amp_scale=0.2)
    return knock * 0.65 + creak


def _footstep_water(rng: np.random.Generator) -> np.ndarray:
    dur = rng.uniform(0.22, 0.32)
    n = int(dur * SR)
    splash = static_filter(rng.standard_normal(n), SR, fc=rng.uniform(900, 1800), q=0.8, kind="bandpass")
    splash *= fit_length(ar_envelope(SR, 0.005, dur - 0.02, hold_s=0.0, shape=1.8), n)
    plop = modal_hit(SR, dur, base_freq=rng.uniform(140, 220), mode_ratios=[1.0, 1.7], mode_dampings_s=[0.05, 0.03], mode_amps=[0.8, 0.3])
    return splash * 0.6 + plop * 0.35


_MATERIAL_FUNCS = {
    "sand": _footstep_sand,
    "grass": _footstep_grass,
    "rock": _footstep_rock,
    "wood": _footstep_wood,
    "water": _footstep_water,
}


def footstep(name: str, material: str) -> np.ndarray:
    rng = rng_for(name)
    return _MATERIAL_FUNCS[material](rng)
