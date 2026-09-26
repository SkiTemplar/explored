"""Construccion (biblia de contenido §3.10): encajar una pieza en su sitio,
el clic de un encaje a presion y el crujido de un techo de hojas/paja."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length
from ..filters import static_filter
from ..granular import render_noise_grains
from ..modal import modal_hit
from ..rng import rng_for

SR = SAMPLE_RATE


def build_place(name: str) -> np.ndarray:
    """Encajar una pieza de madera: golpe sordo (modal grave) mas un crujido
    breve de asentamiento."""
    rng = rng_for(name)
    dur = rng.uniform(0.35, 0.5)
    n = int(dur * SR)
    thud = modal_hit(
        SR, dur, base_freq=rng.uniform(140, 200),
        mode_ratios=[1.0, 2.3, 3.6], mode_dampings_s=[0.05, 0.03, 0.02],
        mode_amps=[1.0, 0.4, 0.2], rng=rng, detune=0.02,
    )
    creak = render_noise_grains(n, SR, rng, rate_hz=20.0, grain_len_s_range=(0.01, 0.03), band_hz_range=(500, 2000), q=1.0, amp_scale=0.25)
    return thud * 0.75 + creak


def build_snap(name: str) -> np.ndarray:
    """Encaje a presion: clic corto y nitido de confirmacion."""
    rng = rng_for(name)
    dur = rng.uniform(0.08, 0.12)
    n = int(dur * SR)
    click = modal_hit(
        SR, dur, base_freq=rng.uniform(1600, 2200),
        mode_ratios=[1.0, 1.5], mode_dampings_s=[0.012, 0.008],
        mode_amps=[1.0, 0.4], rng=rng, detune=0.02,
    )
    snap_noise = static_filter(rng.standard_normal(n), SR, fc=3000, q=0.7, kind="highpass")
    snap_env = fit_length(np.exp(-np.arange(n) / SR / 0.01), n)
    return click * 0.7 + snap_noise * snap_env * 0.4


def build_thatch(name: str) -> np.ndarray:
    """Techo de hojas/paja: crujido breve al colocar o pisar una plancha de
    techado."""
    rng = rng_for(name)
    dur = rng.uniform(0.4, 0.6)
    n = int(dur * SR)
    rustle = render_noise_grains(n, SR, rng, rate_hz=45.0, grain_len_s_range=(0.01, 0.03), band_hz_range=(2200, 6500), q=1.4, amp_scale=0.6)
    env = fit_length(ar_envelope(SR, 0.03, dur - 0.05, shape=1.2), n)
    return rustle * env
