"""Recogida, chapoteos, rafagas y truenos (el fuego vive en `fire.py`):
piezas cortas que combinan ruido filtrado, envolventes y, en el trueno,
sintesis modal para el retumbe grave."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, exp_decay, fit_length, smooth_random_walk
from ..filters import static_filter
from ..granular import render_noise_grains
from ..noise import brown_noise, pink_noise
from ..rng import rng_for

SR = SAMPLE_RATE


def pickup(name: str) -> np.ndarray:
    """Roce de tela/objeto al recogerlo: un par de barridos suaves de ruido."""
    rng = rng_for(name)
    dur = rng.uniform(0.35, 0.5)
    n = int(dur * SR)
    swish = render_noise_grains(
        n, SR, rng, rate_hz=rng.uniform(6, 10), grain_len_s_range=(0.05, 0.12),
        band_hz_range=(700, 3200), q=0.8, amp_scale=0.8,
    )
    env = fit_length(ar_envelope(SR, dur * 0.2, dur * 0.8, shape=1.3), n)
    return swish * env


def _splash(rng: np.random.Generator, size: str) -> np.ndarray:
    if size == "small":
        dur = rng.uniform(0.4, 0.55)
        fc_burst, decay = 1600, 0.03
    else:
        dur = rng.uniform(0.7, 0.95)
        fc_burst, decay = 650, 0.09
    n = int(dur * SR)
    burst = static_filter(rng.standard_normal(n), SR, fc=fc_burst, q=0.6, kind="bandpass")
    burst *= fit_length(np.exp(-np.arange(n) / SR / decay), n)
    droplets = render_noise_grains(
        n, SR, rng, rate_hz=25.0 if size == "small" else 45.0,
        grain_len_s_range=(0.01, 0.03), band_hz_range=(2500, 8000), q=2.0,
        amp_scale=0.4 if size == "small" else 0.6,
    )
    droplets *= fit_length(ar_envelope(SR, 0.01, dur - 0.01, shape=0.6), n)
    return burst * (0.9 if size == "small" else 1.1) + droplets


def splash_small(name: str) -> np.ndarray:
    return _splash(rng_for(name), "small")


def splash_big(name: str) -> np.ndarray:
    return _splash(rng_for(name), "big")


def wind_gust(name: str) -> np.ndarray:
    """Rafaga puntual de viento fuerte (complementa el colchon continuo
    `amb_wind_strong`): hinchazon de ruido pasabanda con un aullido resonante
    superpuesto, para anuncios de temporal o momentos de guion."""
    rng = rng_for(name)
    dur = rng.uniform(2.0, 3.0)
    n = int(dur * SR)
    body = static_filter(rng.standard_normal(n), SR, fc=1600, q=0.7, kind="bandpass")
    body = static_filter(body, SR, fc=250, q=0.6, kind="highpass")
    env = fit_length(ar_envelope(SR, dur * 0.35, dur * 0.6, shape=1.6), n)
    howl = static_filter(pink_noise(n, rng), SR, fc=900, q=2.5, kind="bandpass")
    return body * env * 0.8 + howl * env * 0.3


def thunder(name: str) -> np.ndarray:
    """Trueno: chasquido inicial de banda ancha seguido de un retumbe grave largo."""
    rng = rng_for(name)
    dur = rng.uniform(3.5, 5.5)
    n = int(dur * SR)

    crack_len = int(0.06 * SR)
    crack = static_filter(rng.standard_normal(crack_len), SR, fc=1800, q=0.6, kind="highpass")
    crack *= np.exp(-np.arange(crack_len) / SR / 0.008)

    rumble = static_filter(brown_noise(n, rng, leak=0.9997), SR, fc=110, q=0.7, kind="lowpass")
    rumble_shape = fit_length(exp_decay(SR, dur, tau_s=dur * 0.45), n)
    growl = 0.6 + 0.4 * smooth_random_walk(n, rng, smoothing_hz=2.5, sr=SR, low=0.0, high=1.0)
    body = rumble * rumble_shape * growl

    out = body.copy()
    out[:crack_len] += crack * 0.9
    # Un retumbe secundario mas lejano, ligeramente retrasado, da sensacion de eco natural.
    delay = int(rng.uniform(0.25, 0.5) * SR)
    if delay < n:
        out[delay:] += body[: n - delay] * 0.35
    return out
