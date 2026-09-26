"""Agua interactiva (biblia de contenido §5.2, §12.2 `Boats`): nadar, bucear,
burbujas puntuales, remada y vela. Complementa los colchones continuos de
`ambience.py` (`amb_ocean_*`, `amb_underwater`) con eventos puntuales."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length
from ..filters import static_filter
from ..granular import place_grains, render_noise_grains
from ..modal import modal_hit
from ..noise import brown_noise, pink_noise
from ..rng import rng_for

SR = SAMPLE_RATE


def swim_stroke(name: str) -> np.ndarray:
    """Una brazada: tiron de agua (ruido rosa filtrado) mas salpicadura
    granular breve."""
    rng = rng_for(name)
    dur = rng.uniform(0.5, 0.7)
    n = int(dur * SR)
    pull = static_filter(pink_noise(n, rng), SR, fc=1400, q=0.6, kind="lowpass")
    pull_env = fit_length(ar_envelope(SR, 0.05, dur - 0.1, shape=1.3), n)
    splash = render_noise_grains(n, SR, rng, rate_hz=30.0, grain_len_s_range=(0.01, 0.03), band_hz_range=(2000, 7000), q=1.8, amp_scale=0.4)
    return pull * pull_env * 0.6 + splash * 0.5


def dive_splash(name: str) -> np.ndarray:
    """Zambullida: entrada en superficie (estallido pasabanda) que se apaga
    hacia el retumbe amortiguado de quedar bajo el agua."""
    rng = rng_for(name)
    dur = rng.uniform(0.9, 1.2)
    n = int(dur * SR)
    burst_n = max(int(0.25 * SR), 1)
    burst = static_filter(rng.standard_normal(burst_n), SR, fc=1200, q=0.6, kind="bandpass")
    burst = fit_length(burst * np.exp(-np.arange(burst_n) / SR / 0.06), n)
    droplets = render_noise_grains(n, SR, rng, rate_hz=40.0, grain_len_s_range=(0.008, 0.025), band_hz_range=(2500, 8000), q=2.0, amp_scale=0.35)
    muffled_tail = static_filter(brown_noise(n, rng, leak=0.998), SR, fc=350, q=0.6, kind="lowpass")
    tail_env = fit_length(ar_envelope(SR, dur * 0.3, dur * 0.65, shape=1.0), n)
    return burst * 0.8 + droplets * 0.6 + muffled_tail * tail_env * 0.4


def bubbles(name: str) -> np.ndarray:
    """Racimo de burbujas puntual (mas denso que el lecho continuo de
    `amb_underwater`): varios "blub" modales esparcidos tipo Poisson."""
    rng = rng_for(name)
    dur = rng.uniform(0.6, 0.9)
    n = int(dur * SR)
    out = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz=6.0, jitter=0.8):
        blub = modal_hit(
            SR, 0.15, base_freq=rng.uniform(150, 420),
            mode_ratios=[1.0, 1.8], mode_dampings_s=[0.05, 0.03], mode_amps=[1.0, 0.4],
            rng=rng, detune=0.02,
        )
        blub = blub * amp * 0.6
        end = min(pos + len(blub), n)
        out[pos:end] += blub[: end - pos]
    return out


def paddle_stroke(name: str) -> np.ndarray:
    """Remada de canoa: golpe corto de madera (el remo contra la borda) mas
    el remolino de agua al empujar."""
    rng = rng_for(name)
    dur = rng.uniform(0.6, 0.85)
    n = int(dur * SR)
    knock = modal_hit(
        SR, 0.06, base_freq=rng.uniform(280, 380),
        mode_ratios=[1.0, 2.4], mode_dampings_s=[0.015, 0.01], mode_amps=[1.0, 0.35],
        rng=rng, detune=0.02,
    )
    swirl = static_filter(pink_noise(n, rng), SR, fc=1000, q=0.6, kind="lowpass")
    swirl_env = fit_length(ar_envelope(SR, 0.08, dur - 0.14, shape=1.2), n)
    out = swirl * swirl_env * 0.6
    knock_n = min(len(knock), n)
    out[:knock_n] += knock[:knock_n] * 0.5
    return out


def sail_flap(name: str) -> np.ndarray:
    """Vela hinchandose con el viento: chasquido inicial de lona (pasabanda
    corto) que se asienta en un aleteo filtrado."""
    rng = rng_for(name)
    dur = rng.uniform(0.5, 0.75)
    n = int(dur * SR)
    crack_n = max(int(0.02 * SR), 1)
    crack = static_filter(rng.standard_normal(crack_n), SR, fc=900, q=0.6, kind="bandpass")
    crack = fit_length(crack * np.exp(-np.arange(crack_n) / SR / 0.01), n)
    flap = static_filter(pink_noise(n, rng), SR, fc=1800, q=0.7, kind="lowpass")
    flap = static_filter(flap, SR, fc=300, q=0.6, kind="highpass")
    flap_env = fit_length(ar_envelope(SR, 0.02, dur - 0.05, shape=1.5), n)
    return crack * 0.6 + flap * flap_env * 0.5
