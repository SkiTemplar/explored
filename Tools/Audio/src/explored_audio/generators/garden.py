"""Huerto (GDD: cultivos de la base): cavar con pala, regar con un recipiente
y cosechar arrancando la planta. Todo es tierra, agua y hojas: se construye
con granos de ruido filtrado (terrones, salpicaduras, desgarro de raices),
un golpe grave para el peso de la pala y burbujas resonantes para el chorro
de agua."""

from __future__ import annotations

import numpy as np
from scipy.signal import fftconvolve

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


def _impact_bank(rng: np.random.Generator, count: int, length_s: float, fc_range: tuple[float, float],
                 q: float, tau_range: tuple[float, float], kind: str) -> list[np.ndarray]:
    """Nucleos de impacto de gota: rafaga de ruido de pocos ms con caida
    exponencial y filtrada. Se preparan unas pocas variantes y cada gota usa
    una al azar (convolucion por variante: miles de gotas sin filtrar una a una)."""
    n = int(length_s * SR)
    t = np.arange(n) / SR
    bank = []
    for _ in range(count):
        burst = rng.standard_normal(n) * np.exp(-t / rng.uniform(*tau_range))
        burst = static_filter(burst, SR, fc=rng.uniform(*fc_range), q=q, kind=kind)
        bank.append(burst / (np.abs(burst).max() + 1e-12))
    return bank


def _scatter_impacts(n: int, rate: np.ndarray, rng: np.random.Generator, bank: list[np.ndarray]) -> tuple[np.ndarray, list[int]]:
    """Gotas de Poisson con tasa instantanea `rate` (gotas/s por muestra):
    cada una suena con un nucleo del banco y amplitud aleatoria. Devuelve la
    senal y las posiciones (para colgar burbujas de algunas de ellas)."""
    hits = rng.random(n) < rate / SR
    positions = np.nonzero(hits)[0]
    trains = np.zeros((len(bank), n))
    choice = rng.integers(0, len(bank), len(positions))
    amps = rng.uniform(0.3, 1.0, len(positions)) ** 1.5 * rng.choice([-1.0, 1.0], len(positions))
    trains[choice, positions] = amps
    out = np.zeros(n)
    for train, kernel in zip(trains, bank):
        out += fftconvolve(train, kernel)[:n]
    return out, positions.tolist()


def garden_water(name: str) -> np.ndarray:
    """Regar con un recipiente (coco, calabaza o cantimplora): el agua no es
    un soplido continuo sino miles de gotas.

    - Caudal Q(t): sube al inclinar el recipiente, se sostiene y se corta al
      enderezarlo. Cada pocos decimos de segundo entra aire por la boca del
      recipiente: el chorro titubea y suena el "glup" grave de esa burbuja
      grande (Minnaert de 8-14 mm, 230-400 Hz), el borboteo tipico de verter.
    - Impactos: gotas de Poisson con tasa proporcional al caudal. Sobre la
      tierra mojada son sordas (paso bajo); una fraccion salpica las hojas del
      cultivo y suena mas brillante.
    - Charco: se llena con el caudal y lo absorbe la tierra; cuanto mas lleno,
      mas gotas dejan una burbuja pequena (1-3 kHz, el "chapoteo" del agua
      sobre agua).
    - Cola: al cortar quedan unas gotas sueltas que caen al charco (plic) y el
      siseo de la tierra que bebe."""
    rng = rng_for(name)
    dur = rng.uniform(2.1, 2.5)
    n = int(dur * SR)
    t = np.arange(n) / SR

    # --- Caudal -----------------------------------------------------------
    t_on = rng.uniform(0.12, 0.18)
    t_off = 0.76 * dur
    taper = rng.uniform(0.14, 0.2)
    flow = np.clip(t / t_on, 0.0, 1.0) ** 1.5
    flow *= np.clip(1.0 - (t - t_off) / taper, 0.0, 1.0) ** 1.2
    flow *= smooth_random_walk(n, rng, smoothing_hz=2.0, sr=SR, low=0.8, high=1.0)

    # Borboteo: el aire entra por la boca a 4-7 veces por segundo.
    glug_rate = rng.uniform(4.0, 6.5)
    glug_times = []
    g = t_on * 0.8
    while g < t_off:
        glug_times.append(g)
        g += rng.uniform(0.7, 1.3) / glug_rate
    stall = np.zeros(n)
    glugs = np.zeros(n)
    for gt in glug_times:
        pos = int(gt * SR)
        width = int(rng.uniform(0.04, 0.06) * SR)
        dip = np.hanning(2 * width)
        end = min(pos + len(dip), n)
        stall[pos:end] = np.maximum(stall[pos:end], dip[: end - pos] * rng.uniform(0.4, 0.55))
        # Burbuja grande que entra por la boca: resuena mas que las del charco
        # (25-45 ms) y sube de tono al ascender por el recipiente.
        f0 = 3260.0 / rng.uniform(8.0, 14.0)
        tau = rng.uniform(0.025, 0.045)
        gt_n = int(tau * 5 * SR)
        tt = np.arange(gt_n) / SR
        phase = 2 * np.pi * np.cumsum(f0 * (1.0 + rng.uniform(0.3, 0.7) * tt / (tau * 5))) / SR
        big = np.sin(phase) * np.exp(-tt / tau) * np.clip(tt / 0.003, 0.0, 1.0)
        _place(glugs, big * rng.uniform(0.6, 1.0), pos + int(0.01 * SR))
    flow *= 1.0 - stall

    # --- Impactos del chorro ----------------------------------------------
    soil_bank = _impact_bank(rng, 6, 0.012, (900.0, 2600.0), 0.8, (0.0008, 0.0025), "lowpass")
    leaf_bank = _impact_bank(rng, 6, 0.010, (2400.0, 5200.0), 1.6, (0.0006, 0.0016), "bandpass")
    soil, soil_pos = _scatter_impacts(n, 650.0 * flow, rng, soil_bank)
    leaves, _ = _scatter_impacts(n, 90.0 * flow, rng, leaf_bank)

    # Peso de la columna de agua que entra en la tierra: grave y sordo, sigue al caudal.
    plunge = static_filter(pink_noise(n, rng), SR, fc=420.0, q=0.7, kind="lowpass")
    plunge = static_filter(plunge, SR, fc=90.0, q=0.7, kind="highpass") * flow

    # --- Charco y burbujas ------------------------------------------------
    puddle = np.zeros(n)
    level = 0.0
    fill, soak = 1.4 / SR, 0.9 / SR
    for i in range(0, n, 64):
        level = min(1.0, level + (flow[i] * fill - level * soak) * 64)
        puddle[i : i + 64] = level
    bubbles = np.zeros(n)
    for pos in soil_pos:
        if rng.random() < 0.02 + 0.22 * puddle[pos]:
            _place(bubbles, _bubble(rng, rng.uniform(1.1, 3.0)) * rng.uniform(0.2, 0.55), pos)

    # --- Cola: gotas sueltas y tierra que bebe -------------------------------
    tail = np.zeros(n)
    drip_t = t_off + taper + rng.uniform(0.02, 0.06)
    gap = rng.uniform(0.07, 0.1)
    while drip_t < dur - 0.08:
        pos = int(drip_t * SR)
        drop = soil_bank[int(rng.integers(0, len(soil_bank)))] * rng.uniform(0.35, 0.6)
        _place(tail, drop, pos)
        _place(tail, _bubble(rng, rng.uniform(1.4, 2.6)) * rng.uniform(0.35, 0.6), pos + int(0.002 * SR))
        drip_t += gap
        gap *= rng.uniform(1.3, 1.7)
    soak_env = np.clip((t - t_off) / 0.2, 0.0, 1.0) * np.exp(-np.clip(t - t_off - 0.2, 0.0, None) / 0.35)
    fizz = render_noise_grains(n, SR, rng, rate_hz=260.0, grain_len_s_range=(0.0005, 0.0015), band_hz_range=(3000, 7000), q=1.2, amp_scale=0.08)
    tail += fizz * soak_env

    out = soil * 0.55 + leaves * 0.6 + plunge * 0.15 + bubbles * 0.4 + glugs * 0.35 + tail
    out = static_filter(out, SR, fc=70.0, q=0.7, kind="highpass")
    fade = fit_length(ar_envelope(SR, 0.004, 0.04, hold_s=dur - 0.044), n)
    return out * fade * 0.5


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
