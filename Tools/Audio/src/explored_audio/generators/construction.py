"""Construccion (biblia de contenido §3.10): encajar una pieza en su sitio,
el clic de un encaje a presion, el crujido de un techo de hojas/paja, clavar
una clavija con un mazo de piedra y desmontar una pieza."""

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


def build_place(name: str) -> np.ndarray:
    """Encajar una pieza de madera en su sitio. La pieza no cae de una vez:
    toca con un canto, bascula y asienta con un segundo golpe mas flojo
    20-60 ms despues. Cada contacto es un chasquido corto de madera contra
    madera, los modos de flexion de la viga (razones de barra libre 1 :
    2,76 : 5,4 : 8,9, a 180-260 Hz) y un golpe grave de la estructura que
    la recibe. Al final la union cruje un poco al cargar el peso: unos
    pocos micro-deslizamientos a traves de la resonancia de la fibra.

    Antes era un modal de 140-200 Hz con un 83 % de la energia por debajo
    de 150 Hz: en altavoces pequeños casi no se oia y sonaba a bombo, no a
    madera."""
    rng = rng_for(name)
    dur = rng.uniform(0.42, 0.52)
    n = int(dur * SR)
    out = np.zeros(n)
    beam_freq = rng.uniform(180.0, 260.0)
    settle_s = rng.uniform(0.02, 0.06)
    for k, (pos_s, force) in enumerate(((0.0, 1.0), (settle_s, rng.uniform(0.4, 0.55)))):
        body = modal_hit(
            SR, 0.3, base_freq=beam_freq * (1.0 + 0.03 * k),
            mode_ratios=[1.0, 2.76, 5.4, 8.9],
            mode_dampings_s=[0.055, 0.03, 0.014, 0.006],
            mode_amps=[1.0, 0.6, 0.3, 0.12], rng=rng, detune=0.02,
        )
        thump = modal_hit(SR, 0.15, base_freq=rng.uniform(95.0, 125.0), mode_ratios=[1.0], mode_dampings_s=[0.03], mode_amps=[1.0])
        click_n = int(0.004 * SR)
        click = static_filter(rng.standard_normal(click_n), SR, fc=2500.0, q=0.7, kind="highpass") * np.exp(-np.arange(click_n) / SR / 0.0008)
        pos = int(pos_s * SR)
        _place(out, body * 0.62 * force, pos)
        _place(out, thump * 0.3 * force, pos)
        _place(out, click * 0.45 * force, pos)

    creak_start = int((settle_s + rng.uniform(0.07, 0.12)) * SR)
    t = creak_start
    for _ in range(int(rng.integers(4, 8))):
        tick = modal_hit(
            SR, 0.03, base_freq=rng.uniform(900.0, 1500.0),
            mode_ratios=[1.0, 2.3], mode_dampings_s=[0.006, 0.003],
            mode_amps=[1.0, 0.4], rng=rng, detune=0.05,
        )
        _place(out, tick * rng.uniform(0.06, 0.12), t)
        t += int(rng.uniform(0.012, 0.03) * SR)

    # Los senos amortiguados que arrancan en fase cero dejan continua; sin
    # este filtro, `remove_dc` la convertiria en un escalon al final.
    out = static_filter(out, SR, fc=40.0, q=0.707, kind="highpass")
    fade_n = int(0.03 * SR)
    out[-fade_n:] *= np.linspace(1.0, 0.0, fade_n) ** 2
    return out


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


def _place(out: np.ndarray, x: np.ndarray, pos: int) -> None:
    end = min(pos + len(x), len(out))
    if end > pos:
        out[pos:end] += x[: end - pos]


def build_hammer(name: str) -> np.ndarray:
    """Clavar una clavija de madera con un mazo de piedra: 3-4 golpes. Cada
    uno es el chasquido del contacto piedra-madera, el cuerpo resonante de la
    viga y un golpe grave; la clavija entra y el tono sube golpe a golpe
    (queda menos madera libre vibrando), y el ultimo suena mas seco."""
    rng = rng_for(name)
    n_hits = int(rng.integers(3, 5))
    gaps = [rng.uniform(0.3, 0.4) for _ in range(n_hits - 1)]
    n = int((sum(gaps) + 0.45) * SR)
    out = np.zeros(n)
    beam_freq = rng.uniform(260, 340)
    pos = 0
    for i in range(n_hits):
        last = i == n_hits - 1
        freq = beam_freq * (1.0 + 0.06 * i)
        body = modal_hit(
            SR, 0.35, base_freq=freq,
            mode_ratios=[1.0, 2.76, 5.4, 8.9],
            mode_dampings_s=[0.07 * (0.6 if last else 1.0), 0.035, 0.015, 0.007],
            mode_amps=[1.0, 0.5, 0.25, 0.12], rng=rng, detune=0.015,
        )
        low = modal_hit(SR, 0.2, base_freq=rng.uniform(80, 100), mode_ratios=[1.0], mode_dampings_s=[0.035], mode_amps=[1.0])
        click_n = int(0.006 * SR)
        click = static_filter(rng.standard_normal(click_n), SR, fc=3000.0, q=0.7, kind="highpass") * np.exp(-np.arange(click_n) / SR / 0.0012)
        force = rng.uniform(0.8, 1.0) * (1.1 if last else 1.0)
        _place(out, body * 0.7 * force, pos)
        _place(out, low * 0.35 * force, pos)
        _place(out, click * 0.8 * force, pos)
        if not last:
            pos += int(gaps[i] * SR)
    return out


def build_dismantle(name: str) -> np.ndarray:
    """Desmontar: se aflojan las ataduras (crujido de fibra que se estira),
    la pieza se suelta con un golpe sordo y caen un par de trozos sueltos
    que rebotan cada vez mas flojo."""
    rng = rng_for(name)
    dur = rng.uniform(1.1, 1.4)
    n = int(dur * SR)
    out = np.zeros(n)

    creak_n = int(rng.uniform(0.35, 0.5) * SR)
    fibre = pink_noise(creak_n, rng)
    fc = np.linspace(700.0, 1500.0, creak_n) * smooth_random_walk(creak_n, rng, smoothing_hz=12.0, sr=SR, low=0.85, high=1.15)
    creak = time_varying_filter(fibre, SR, fc, q=3.0, kind="bandpass")
    creak *= np.linspace(0.3, 1.0, creak_n) ** 1.5
    _place(out, creak * 0.6, 0)

    release = creak_n
    thud = modal_hit(
        SR, 0.3, base_freq=rng.uniform(110, 150),
        mode_ratios=[1.0, 2.3, 3.6], mode_dampings_s=[0.06, 0.03, 0.015],
        mode_amps=[1.0, 0.4, 0.2], rng=rng, detune=0.02,
    )
    _place(out, thud * 0.9, release)

    t = release + int(rng.uniform(0.1, 0.16) * SR)
    amp = 0.55
    for _ in range(int(rng.integers(3, 6))):
        piece = modal_hit(
            SR, 0.15, base_freq=rng.uniform(300, 700),
            mode_ratios=[1.0, 2.76, 5.4], mode_dampings_s=[0.03, 0.015, 0.006],
            mode_amps=[1.0, 0.4, 0.15], rng=rng, detune=0.03,
        )
        _place(out, piece * amp, t)
        t += int(rng.uniform(0.06, 0.14) * SR)
        amp *= rng.uniform(0.55, 0.75)
    return out * fit_length(ar_envelope(SR, 0.01, 0.06, hold_s=dur - 0.07), n)
