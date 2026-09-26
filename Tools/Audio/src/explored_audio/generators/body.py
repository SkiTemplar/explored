"""Cuerpo y necesidades (biblia de contenido §5.4, GDD §4.3): comer, beber,
respiracion cansada y el latido con la salud baja. Efectos discretos e
intimos (poco "mundo", mucho "estado interno"), a diferencia de la fauna o
el ambiente."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length
from ..filters import static_filter
from ..granular import render_noise_grains
from ..loop import seamless_loop
from ..modal import modal_hit
from ..rng import rng_for

SR = SAMPLE_RATE


def eat(name: str) -> np.ndarray:
    """Comer: 2-3 mordiscos/crujidos cortos, con hueco entre ellos."""
    rng = rng_for(name)
    n_bites = int(rng.integers(2, 4))
    pieces = []
    for _ in range(n_bites):
        dur = rng.uniform(0.1, 0.16)
        n = int(dur * SR)
        crunch = render_noise_grains(n, SR, rng, rate_hz=90.0, grain_len_s_range=(0.004, 0.012), band_hz_range=(900, 3500), q=1.2, amp_scale=0.6)
        env = fit_length(ar_envelope(SR, 0.005, dur - 0.005, shape=1.8), n)
        pieces.append(crunch * env)
        pieces.append(np.zeros(int(rng.uniform(0.08, 0.16) * SR)))
    return np.concatenate(pieces) if pieces else np.zeros(1)


def drink(name: str) -> np.ndarray:
    """Beber: dos tragos (golpe modal corto en la garganta) sobre un gorgoteo
    de liquido."""
    rng = rng_for(name)
    dur = rng.uniform(0.5, 0.7)
    n = int(dur * SR)
    gulp = np.zeros(n)
    for i in range(2):
        pos = int(i * dur * 0.45 * SR)
        g = modal_hit(
            SR, 0.09, base_freq=rng.uniform(220, 320),
            mode_ratios=[1.0, 1.6], mode_dampings_s=[0.03, 0.02], mode_amps=[1.0, 0.4],
            rng=rng, detune=0.02,
        )
        end = min(pos + len(g), n)
        gulp[pos:end] += g[: end - pos] * 0.6
    gurgle = render_noise_grains(n, SR, rng, rate_hz=12.0, grain_len_s_range=(0.02, 0.05), band_hz_range=(500, 1800), q=1.2, amp_scale=0.25)
    return gulp + gurgle


def breath_tired(name: str) -> np.ndarray:
    """Respiracion cansada: dos ciclos de inhalar/exhalar, ruido filtrado
    modulado por la propia forma del ciclo respiratorio."""
    rng = rng_for(name)
    dur = rng.uniform(1.4, 1.9)
    n = int(dur * SR)
    t = np.arange(n) / SR
    cycle_hz = 1.0 / (dur / 2.0)
    breath_shape = 0.5 + 0.5 * np.sin(2 * np.pi * cycle_hz * t - np.pi / 2)
    body = static_filter(rng.standard_normal(n), SR, fc=700, q=0.6, kind="lowpass")
    body = static_filter(body, SR, fc=180, q=0.6, kind="highpass")
    rasp = static_filter(rng.standard_normal(n), SR, fc=1500, q=1.0, kind="bandpass")
    return body * breath_shape * 0.6 + rasp * breath_shape * 0.15


def heartbeat_low(name: str) -> np.ndarray:
    """Latido con la salud baja: patron lub-dub en bucle, con una pizca de
    variacion de ritmo (nunca perfectamente regular, como un pulso real)."""
    rng = rng_for(name)
    loop_s, fade_s = 4.0, 0.6
    loop_len = int(loop_s * SR)
    fade_len = int(fade_s * SR)
    n = loop_len + fade_len
    bpm = 85.0
    beat_period = 60.0 / bpm
    out = np.zeros(n)
    t = 0.0
    while int(t * SR) < n:
        jitter = rng.uniform(-0.01, 0.01)
        for delay, amp, freq in ((0.0, 1.0, 55.0), (0.14, 0.55, 48.0)):  # lub, dub
            pos = int((t + delay + jitter) * SR)
            if pos >= n:
                continue
            thump = modal_hit(
                SR, 0.12, base_freq=freq, mode_ratios=[1.0, 1.7], mode_dampings_s=[0.045, 0.02],
                mode_amps=[1.0, 0.3], rng=rng, detune=0.01,
            )
            end = min(pos + len(thump), n)
            out[pos:end] += thump[: end - pos] * amp
        t += beat_period + rng.uniform(-0.02, 0.02)
    return seamless_loop(out, loop_len, fade_len)
