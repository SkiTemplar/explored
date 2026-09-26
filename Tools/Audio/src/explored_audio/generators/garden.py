"""Huerto (GDD: cultivos de la base): cavar con pala, regar con un recipiente
y cosechar arrancando la planta. Todo es tierra, agua y hojas: se construye
con granos de ruido filtrado (terrones, salpicaduras, desgarro de raices),
un golpe grave para el peso de la pala y burbujas resonantes para el chorro
de agua."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length, smooth_random_walk
from ..filters import static_filter, time_varying_filter
from ..granular import render_noise_grains
from ..modal import modal_hit
from ..noise import pink_noise
from ..rng import rng_for

SR = SAMPLE_RATE


def _place(out: np.ndarray, x: np.ndarray, pos: int) -> None:
    end = min(pos + len(x), len(out))
    if end > pos:
        out[pos:end] += x[: end - pos]


def _bubble(rng: np.random.Generator, radius_mm: float) -> np.ndarray:
    """Burbuja de Minnaert: seno amortiguado cuya frecuencia sube al
    acercarse a la superficie (f0 ~ 3.26 kHz / radio en mm)."""
    f0 = 3260.0 / radius_mm
    tau = 0.004 + 0.012 * rng.random()
    n = int(tau * 6 * SR)
    t = np.arange(n) / SR
    rise = 1.0 + rng.uniform(0.5, 1.8) * t / (tau * 6)
    phase = 2 * np.pi * np.cumsum(f0 * rise) / SR
    return np.sin(phase) * np.exp(-t / tau)


def garden_dig(name: str) -> np.ndarray:
    """Cavar: la hoja de la pala entra en la tierra (golpe grave y roce
    denso de terrones), se levanta la palada y los terrones caen sueltos
    un instante despues."""
    rng = rng_for(name)
    dur = rng.uniform(0.9, 1.2)
    n = int(dur * SR)
    out = np.zeros(n)

    # Entrada de la hoja: peso sordo mas el "chac" de la tierra compacta.
    thump = modal_hit(
        SR, 0.18, base_freq=rng.uniform(75, 105),
        mode_ratios=[1.0, 2.1, 3.3], mode_dampings_s=[0.05, 0.025, 0.012],
        mode_amps=[1.0, 0.35, 0.15], rng=rng, detune=0.03,
    )
    _place(out, thump * 0.9, 0)
    bite_n = int(rng.uniform(0.16, 0.22) * SR)
    bite = static_filter(pink_noise(bite_n, rng), SR, fc=1400.0, q=0.6, kind="lowpass")
    bite *= fit_length(ar_envelope(SR, 0.004, bite_n / SR * 0.8, shape=2.0), bite_n)
    _place(out, bite * 0.8, 0)
    grind = render_noise_grains(bite_n, SR, rng, rate_hz=260.0, grain_len_s_range=(0.002, 0.008), band_hz_range=(400, 2600), q=1.1, amp_scale=0.6)
    grind *= fit_length(ar_envelope(SR, 0.01, bite_n / SR * 0.9, shape=1.5), bite_n)
    _place(out, grind, 0)

    # Palada que se levanta y terrones que caen: granos dispersos con su
    # propio golpecito grave, cada vez mas espaciados.
    fall_start = int(rng.uniform(0.35, 0.5) * SR)
    fall_n = n - fall_start
    clods = render_noise_grains(fall_n, SR, rng, rate_hz=70.0, grain_len_s_range=(0.004, 0.015), band_hz_range=(900, 4500), q=1.0, amp_scale=0.45)
    clods *= fit_length(ar_envelope(SR, 0.02, fall_n / SR * 0.95, shape=2.2), fall_n)
    _place(out, clods, fall_start)
    for _ in range(int(rng.integers(3, 6))):
        pos = fall_start + int(rng.uniform(0.0, 0.25) * SR)
        tap = modal_hit(SR, 0.06, base_freq=rng.uniform(140, 260), mode_ratios=[1.0, 2.4], mode_dampings_s=[0.015, 0.008], mode_amps=[1.0, 0.3], rng=rng, detune=0.05)
        _place(out, tap * rng.uniform(0.15, 0.3), pos)

    return out * fit_length(ar_envelope(SR, 0.001, 0.05, hold_s=dur - 0.051), n)


def garden_water(name: str) -> np.ndarray:
    """Regar con un recipiente: chorro que sale (ruido con resonancia que
    se mueve despacio), burbujas donde el agua cae sobre la que ya empapa
    la tierra y salpicaduras finas sobre hojas. Entra y se corta suave."""
    rng = rng_for(name)
    dur = rng.uniform(1.8, 2.3)
    n = int(dur * SR)

    stream = pink_noise(n, rng)
    centre = smooth_random_walk(n, rng, smoothing_hz=2.5, sr=SR, low=700.0, high=1500.0)
    body = time_varying_filter(stream, SR, centre, q=2.2, kind="bandpass")
    body = static_filter(body, SR, fc=180.0, q=0.7, kind="highpass")

    splatter = render_noise_grains(n, SR, rng, rate_hz=140.0, grain_len_s_range=(0.002, 0.006), band_hz_range=(2500, 7500), q=1.3, amp_scale=0.35)

    bubbles = np.zeros(n)
    t = 0.0
    while True:
        t += rng.exponential(1.0 / 55.0)
        pos = int(t * SR)
        if pos >= n:
            break
        _place(bubbles, _bubble(rng, rng.uniform(1.2, 4.0)) * rng.uniform(0.2, 0.6), pos)

    # Un vaiven lento del caudal: el brazo no vierte a pulso perfecto.
    flow = smooth_random_walk(n, rng, smoothing_hz=1.5, sr=SR, low=0.7, high=1.0)
    env = fit_length(ar_envelope(SR, 0.18, 0.35, hold_s=dur - 0.53, shape=1.3), n)
    return (body * 1.2 + splatter + bubbles * 0.35) * flow * env * 0.3


def garden_harvest(name: str) -> np.ndarray:
    """Cosechar: tiron de la planta (tension creciente de raices que se
    desgarran), el "pop" grave cuando la raiz cede y un sacudon de hojas
    con tierra suelta despues."""
    rng = rng_for(name)
    dur = rng.uniform(0.9, 1.2)
    n = int(dur * SR)
    out = np.zeros(n)

    pull_n = int(rng.uniform(0.3, 0.42) * SR)
    tear = render_noise_grains(pull_n, SR, rng, rate_hz=320.0, grain_len_s_range=(0.001, 0.005), band_hz_range=(700, 3800), q=1.4, amp_scale=0.55)
    # Tension creciente: sube y corta en seco al soltarse la raiz.
    tear *= np.linspace(0.15, 1.0, pull_n) ** 1.6
    _place(out, tear, 0)

    pop = modal_hit(
        SR, 0.2, base_freq=rng.uniform(95, 130),
        mode_ratios=[1.0, 1.9, 3.1], mode_dampings_s=[0.04, 0.02, 0.01],
        mode_amps=[1.0, 0.4, 0.2], rng=rng, detune=0.03,
    )
    snap_n = int(0.03 * SR)
    snap = static_filter(rng.standard_normal(snap_n), SR, fc=1800.0, q=0.8, kind="bandpass") * np.exp(-np.arange(snap_n) / SR / 0.006)
    _place(out, pop * 0.9, pull_n)
    _place(out, snap * 0.6, pull_n)

    shake_start = pull_n + int(0.05 * SR)
    shake_n = n - shake_start
    leaves = render_noise_grains(shake_n, SR, rng, rate_hz=90.0, grain_len_s_range=(0.008, 0.025), band_hz_range=(2500, 7000), q=1.3, amp_scale=0.35)
    soil = render_noise_grains(shake_n, SR, rng, rate_hz=40.0, grain_len_s_range=(0.004, 0.012), band_hz_range=(800, 3000), q=1.0, amp_scale=0.3)
    shake_env = fit_length(ar_envelope(SR, 0.03, shake_n / SR * 0.9, shape=1.8), shake_n)
    _place(out, (leaves + soil) * shake_env, shake_start)

    return out * fit_length(ar_envelope(SR, 0.002, 0.04, hold_s=dur - 0.042), n)
