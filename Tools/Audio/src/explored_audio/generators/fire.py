"""Hoguera en bucle: el sonido que mas acompaña al jugador de noche.

Un fuego real no es "ruido con chasquidos a ritmo fijo" (la version anterior):
tiene cuatro capas con comportamientos distintos, y es la mezcla la que lo
hace creible.

- Rugido: el aire que sube por la llama. Grave (90-400 Hz), con un "respirar"
  irregular de 1-3 Hz (el parpadeo de la llama) sobre una deriva lenta.
- Chisporroteo: la madera que se agrieta no suelta chasquidos sueltos, sino
  racimos (una grieta se abre en varios saltos de pocos milisegundos). Cada
  chasquido es un impulso cortisimo y agudo, a veces con una pizca de
  resonancia (brasa que "tintinea").
- Estallidos: bolsas de savia o de vapor que revientan de vez en cuando; un
  golpe con cuerpo de madera (sintesis modal, 500-1400 Hz) mas un clic.
- Siseo: savia o humedad que hierve en un extremo del tronco; episodios de
  ruido agudo de medio segundo a dos segundos que suben y bajan.
- Asentamiento: muy de vez en cuando un tronco se hunde en las brasas y
  arrastra ceniza: un roce grave y breve.
"""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length, smooth_random_walk
from ..filters import static_filter
from ..granular import place_grains
from ..loop import seamless_loop
from ..modal import modal_hit
from ..noise import brown_noise, pink_noise
from ..rng import rng_for

SR = SAMPLE_RATE


def _add(out: np.ndarray, piece: np.ndarray, pos: int) -> None:
    end = min(pos + len(piece), len(out))
    if end > pos:
        out[pos:end] += piece[: end - pos]


def _roar(n: int, rng: np.random.Generator) -> np.ndarray:
    body = static_filter(brown_noise(n, rng, leak=0.9985), SR, fc=380, q=0.7, kind="lowpass")
    body = static_filter(body, SR, fc=90, q=0.7, kind="highpass")
    # La llama "lame": una banda media que aparece y se va con el parpadeo.
    lick = static_filter(pink_noise(n, rng), SR, fc=520, q=0.9, kind="bandpass")
    drift = smooth_random_walk(n, rng, smoothing_hz=0.15, sr=SR, low=0.6, high=1.0)
    flicker = smooth_random_walk(n, rng, smoothing_hz=2.2, sr=SR, low=0.35, high=1.0)
    return (body * 0.55 + lick * 0.22 * flicker) * drift * (0.7 + 0.3 * flicker)


def _crackle_click(rng: np.random.Generator) -> np.ndarray:
    glen = max(int(rng.uniform(0.0015, 0.006) * SR), 16)
    t = np.arange(glen) / SR
    click = rng.standard_normal(glen) * np.exp(-t / rng.uniform(0.0006, 0.0022))
    click = static_filter(click, SR, fc=rng.uniform(1800, 5200), q=0.7, kind="highpass")
    if rng.random() < 0.3:
        # Brasa que tintinea: resonancia aguda muy amortiguada.
        ring = modal_hit(SR, glen / SR, base_freq=rng.uniform(2600, 5200), mode_ratios=[1.0, 1.52],
                         mode_dampings_s=[0.003, 0.0015], mode_amps=[0.5, 0.2])
        click = click + fit_length(ring, glen)
    return click


def _crackles(n: int, rng: np.random.Generator, activity: np.ndarray) -> np.ndarray:
    out = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz=7.0, jitter=0.95):
        # Mas racimos cuando el fuego esta "vivo" (la actividad sube y baja).
        if rng.random() > activity[pos]:
            continue
        count = int(rng.integers(1, 7))
        cursor = pos
        level = amp * rng.uniform(0.35, 1.0)
        for _ in range(count):
            _add(out, _crackle_click(rng) * level, cursor)
            cursor += int(rng.uniform(0.004, 0.045) * SR)
            level *= rng.uniform(0.55, 1.05)
    return out


def _pops(n: int, rng: np.random.Generator) -> np.ndarray:
    out = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz=0.3, jitter=0.9):
        dur = rng.uniform(0.05, 0.12)
        body = modal_hit(SR, dur, base_freq=rng.uniform(500, 1400), mode_ratios=[1.0, 2.3, 3.9],
                         mode_dampings_s=[0.018, 0.009, 0.005], mode_amps=[1.0, 0.45, 0.25],
                         rng=rng, detune=0.04)
        crack_len = int(0.004 * SR)
        crack = static_filter(rng.standard_normal(crack_len), SR, fc=2500, q=0.7, kind="highpass")
        crack *= np.exp(-np.arange(crack_len) / SR / 0.001)
        body[:crack_len] += crack * 1.4
        _add(out, body * amp * 0.9, pos)
    return out


def _hiss(n: int, rng: np.random.Generator) -> np.ndarray:
    out = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz=0.09, jitter=0.8):
        dur = rng.uniform(0.6, 2.2)
        dn = int(dur * SR)
        noise = static_filter(rng.standard_normal(dn), SR, fc=rng.uniform(4200, 7000), q=1.4, kind="bandpass")
        wobble = smooth_random_walk(dn, rng, smoothing_hz=6.0, sr=SR, low=0.5, high=1.0)
        env = fit_length(ar_envelope(SR, dur * 0.35, dur * 0.65, shape=1.4), dn)
        _add(out, noise * env * wobble * amp * 0.16, pos)
    return out


def _settle(n: int, rng: np.random.Generator) -> np.ndarray:
    out = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz=0.05, jitter=0.6):
        dur = rng.uniform(0.35, 0.7)
        dn = int(dur * SR)
        rustle = static_filter(pink_noise(dn, rng), SR, fc=rng.uniform(350, 700), q=0.8, kind="bandpass")
        env = fit_length(ar_envelope(SR, 0.02, dur - 0.02, shape=2.0), dn)
        thud = modal_hit(SR, 0.15, base_freq=rng.uniform(110, 170), mode_ratios=[1.0, 2.6],
                         mode_dampings_s=[0.04, 0.015], mode_amps=[1.0, 0.3])
        piece = rustle * env * 0.5
        piece[: len(thud)] += thud * 0.6
        _add(out, piece * amp, pos)
    return out


def fire_loop(name: str) -> np.ndarray:
    """Bucle mono de hoguera (22 s): rugido, racimos de chisporroteo,
    estallidos de savia, episodios de siseo y algun asentamiento de troncos."""
    rng = rng_for(name)
    loop_s, fade_s = 22.0, 3.0
    loop_len = int(loop_s * SR)
    fade_len = int(fade_s * SR)
    n = loop_len + fade_len

    activity = smooth_random_walk(n, rng, smoothing_hz=0.25, sr=SR, low=0.25, high=1.0)
    mono = (
        _roar(n, rng) * 0.6
        + _crackles(n, rng, activity) * 1.3
        + _pops(n, rng) * 0.7
        + _hiss(n, rng)
        + _settle(n, rng) * 0.35
    )
    return seamless_loop(mono, loop_len, fade_len)
