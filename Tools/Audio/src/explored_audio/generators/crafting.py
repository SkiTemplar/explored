"""Herramientas y fabricacion (biblia de contenido §3.4, §5.3): tallar, atar,
golpear piedra (talla de pedernal), cortar madera con sierra, encender una
hoguera y el chisporroteo de cocinar. Reutiliza el mismo instrumental DSP que
los golpes de `impacts.py` (transitorio + modal) y las texturas granulares de
`footsteps.py`/`ambience.py`."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length, smooth_random_walk
from ..filters import static_filter, time_varying_filter
from ..granular import render_noise_grains
from ..loop import seamless_loop
from ..modal import modal_hit
from ..noise import pink_noise
from ..rng import rng_for

SR = SAMPLE_RATE


def carve(name: str) -> np.ndarray:
    """Tallar: varias pasadas de rasguño (granulado de banda alta), cada una
    con su propia envolvente de vaiven."""
    rng = rng_for(name)
    n_strokes = int(rng.integers(3, 6))
    pieces = []
    for _ in range(n_strokes):
        dur = rng.uniform(0.12, 0.22)
        n = int(dur * SR)
        scrape = render_noise_grains(n, SR, rng, rate_hz=60.0, grain_len_s_range=(0.006, 0.018), band_hz_range=(1800, 5200), q=1.3, amp_scale=0.5)
        env = fit_length(ar_envelope(SR, dur * 0.15, dur * 0.7, shape=1.3), n)
        pieces.append(scrape * env)
        pieces.append(np.zeros(int(rng.uniform(0.04, 0.09) * SR)))
    return np.concatenate(pieces) if pieces else np.zeros(1)


def tie_cord(name: str) -> np.ndarray:
    """Atar: roce de cordel retorciendose (filtro pasabanda con el corte
    barriendo, como si la cuerda girase) mas dos tirones finales al apretar
    el nudo."""
    rng = rng_for(name)
    dur = rng.uniform(0.6, 0.9)
    n = int(dur * SR)
    twist = pink_noise(n, rng)
    cutoff = smooth_random_walk(n, rng, smoothing_hz=3.0, sr=SR, low=800.0, high=2600.0)
    creak = time_varying_filter(twist, SR, cutoff, q=1.4, kind="bandpass")
    env = fit_length(ar_envelope(SR, 0.05, dur - 0.1, shape=1.1), n)
    tug = modal_hit(SR, 0.05, base_freq=180.0, mode_ratios=[1.0, 1.6], mode_dampings_s=[0.02, 0.015], mode_amps=[1.0, 0.4], rng=rng, detune=0.02)
    out = creak * env * 0.5
    pos = max(n - len(tug), 0)
    out[pos:] += tug[: n - pos] * 0.5
    return out


def stone_knap(name: str) -> np.ndarray:
    """Golpear piedra (talla de pedernal): mas agudo y de decaimiento mas
    rapido que `sfx_stone_hit` (impacto generico de herramienta): una lasca
    saltando, no un golpe sordo."""
    rng = rng_for(name)
    dur = rng.uniform(0.15, 0.25)
    n = int(dur * SR)
    chip_n = max(int(0.006 * SR), 1)
    chip = static_filter(rng.standard_normal(chip_n), SR, fc=3500, q=0.6, kind="highpass")
    chip = fit_length(chip * np.exp(-np.arange(chip_n) / SR / 0.003), n)
    body = modal_hit(
        SR, dur, base_freq=rng.uniform(1400, 2200),
        mode_ratios=[1.0, 2.2, 3.4], mode_dampings_s=[0.02, 0.014, 0.009],
        mode_amps=[1.0, 0.4, 0.2], rng=rng, detune=0.04,
    )
    return chip * 0.5 + body * 0.8


def wood_saw(name: str) -> np.ndarray:
    """Cortar madera con sierra de dientes de tiburon: ruido rasposo
    modulado en amplitud al ritmo de la pasada, no un golpe (eso ya es
    `sfx_wood_chop`)."""
    rng = rng_for(name)
    dur = rng.uniform(1.4, 2.0)
    n = int(dur * SR)
    t = np.arange(n) / SR
    rasp = static_filter(rng.standard_normal(n), SR, fc=2200, q=0.9, kind="bandpass")
    rasp = static_filter(rasp, SR, fc=500, q=0.6, kind="highpass")
    stroke_hz = rng.uniform(3.6, 4.4)
    stroke = (0.5 + 0.5 * np.sin(2 * np.pi * stroke_hz * t - np.pi / 2)) ** 1.5
    env = fit_length(ar_envelope(SR, 0.08, dur - 0.16, shape=1.0), n)
    return rasp * stroke * env * 0.8


def fire_ignite(name: str) -> np.ndarray:
    """Hoguera encendiendose (complementa el bucle de crepitar,
    `sfx_fire_loop`): unas pocas chispas sueltas que se espesan en un breve
    soplo de llama prendiendo."""
    rng = rng_for(name)
    dur = rng.uniform(1.0, 1.4)
    n = int(dur * SR)
    sparks = np.zeros(n)
    n_sparks = int(rng.integers(5, 9))
    for i in range(n_sparks):
        pos = int((i / n_sparks) * n * rng.uniform(0.6, 0.9))
        glen = max(int(rng.uniform(0.008, 0.02) * SR), 8)
        pop = static_filter(rng.standard_normal(glen), SR, fc=rng.uniform(2000, 5000), q=1.4, kind="highpass")
        pop = pop * np.exp(-np.arange(glen) / SR / 0.012)
        end = min(pos + glen, n)
        sparks[pos:end] += pop[: end - pos] * rng.uniform(0.4, 0.7)
    whoosh = static_filter(pink_noise(n, rng), SR, fc=1600, q=0.6, kind="lowpass")
    whoosh_env = fit_length(ar_envelope(SR, dur * 0.55, dur * 0.4, shape=1.4), n)
    return sparks * 0.8 + whoosh * whoosh_env * 0.35


def cooking_sizzle(name: str) -> np.ndarray:
    """Cocinar: chisporroteo denso en banda alta (comida sobre piedra
    caliente), en bucle mientras dura la coccion."""
    rng = rng_for(name)
    loop_s, fade_s = 8.0, 1.5
    loop_len = int(loop_s * SR)
    fade_len = int(fade_s * SR)
    n = loop_len + fade_len
    sizzle = render_noise_grains(n, SR, rng, rate_hz=140.0, grain_len_s_range=(0.006, 0.02), band_hz_range=(3000, 9000), q=2.2, amp_scale=0.35)
    amp = smooth_random_walk(n, rng, smoothing_hz=0.4, sr=SR, low=0.55, high=1.0)
    mono = sizzle * amp
    return seamless_loop(mono, loop_len, fade_len)
