"""Recogida, chapoteos, rafagas y truenos (el fuego vive en `fire.py`):
piezas cortas que combinan ruido filtrado, envolventes y, en el trueno,
sintesis modal para el retumbe grave."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, exp_decay, fit_length, smooth_random_walk
from ..filters import static_filter
from ..granular import render_noise_grains
from ..levels import k_weighted_momentary_max
from ..noise import brown_noise, pink_noise
from ..rng import rng_for
from .footsteps import _bubble, _click, _droplet, _grain_burst, _swish, _thump

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


# Sonoridad objetivo de los chapoteos: maximo de sonoridad momentanea (100 ms,
# K-weighting de `levels.py`), las mismas cifras que daba la version de ruido
# en banda, para que el cambio de timbre no mueva el nivel en el juego.
_SPLASH_TARGET_MOMENTARY = {"small": -15.0, "big": -13.0}
_SPLASH_PEAK_CAP = 0.85


def _splash(rng: np.random.Generator, size: str) -> np.ndarray:
    """Objeto que cae al agua, por fases como un chapoteo real (Minnaert,
    Pumphrey y Crum; el paso en agua somera usa el mismo modelo):

    - el objeto golpea la superficie: palmetazo corto y ancho y, si es
      grande, el golpe grave del agua que empuja;
    - la corona y la lamina de agua que salta: un racimo breve de granos
      brillantes;
    - la cavidad que abre el objeto se cierra y atrapa aire: una burbuja
      grande con el tono subiendo (el «plonc»), mas grave cuanto mayor es,
      y una nube de burbujas menores al romperse el chorro;
    - las gotas levantadas vuelven a caer de 80 a 500 ms despues: tics con
      su burbuja aguda, cada vez mas escasas.

    La version anterior era ruido en banda (centroide ~4,5 kHz, sin energia
    tonal ni grave): sonaba a siseo mas que a agua.
    """
    big = size == "big"
    n = int(rng.uniform(0.75, 0.95) * SR) if big else int(rng.uniform(0.45, 0.55) * SR)
    out = np.zeros(n)
    onset = int(0.004 * SR)

    # Golpe en la superficie.
    out += (0.30 if big else 0.22) * _click(n, onset, rng, fc=rng.uniform(700, 1200), dur_s=0.010 if big else 0.006)
    if big:
        out += 0.35 * _thump(n, onset, rng, fc=rng.uniform(140, 200), tau_s=0.045)
    else:
        out += 0.12 * _thump(n, onset, rng, fc=rng.uniform(250, 350), tau_s=0.02)

    # Corona: lamina que salta, brillante y breve.
    out += (0.30 if big else 0.18) * _grain_burst(
        n, onset, rng, count=int(rng.integers(30, 45) if big else rng.integers(15, 25)),
        spread_s=0.035 if big else 0.018, grain_len_s=(0.002, 0.007), band_hz=(1500, 10000), q=1.2,
    )
    # Agua desplazada que se abre y vuelve: roce ancho y corto.
    out += (0.20 if big else 0.10) * _swish(n, onset, rng, 0.22 if big else 0.10, 300 if big else 500, 1600, q=0.7)

    # Cavidad que se cierra: burbuja grande y su nube.
    pos = onset + int(rng.uniform(0.03, 0.05) * SR if big else rng.uniform(0.015, 0.03) * SR)
    freq = rng.uniform(140, 260) if big else rng.uniform(320, 560)
    out += (0.38 if big else 0.34) * _bubble(n, pos, freq, rng.uniform(0.04, 0.06) if big else rng.uniform(0.02, 0.035),
                                             rng.uniform(0.5, 1.0))
    for _ in range(int(rng.integers(4, 8) if big else rng.integers(1, 3))):
        p2 = pos + int(rng.uniform(0.005, 0.08 if big else 0.04) * SR)
        f2 = freq * rng.uniform(1.6, 4.0)
        out += rng.uniform(0.12, 0.25) * _bubble(n, min(p2, n - 64), f2, rng.uniform(0.01, 0.025), rng.uniform(0.2, 0.7))

    # Gotas que vuelven a caer: densas al principio, luego sueltas.
    first, tail = (0.12, 0.16) if big else (0.07, 0.09)
    for _ in range(int(rng.integers(35, 55) if big else rng.integers(14, 24))):
        delay = first + min(rng.exponential(tail), 4.0 * tail)
        amp = rng.uniform(0.12, 0.30) * (1.5 if big else 1.0) * np.exp(-(delay - first) / (tail * 1.5))
        out += amp * _droplet(n, onset + int(delay * SR), rng, 1.0)

    # Recorta el silencio final y cierra con 10 ms de fundido; luego fija la
    # sonoridad con el mismo criterio que los pasos.
    above = np.nonzero(np.abs(out) > np.max(np.abs(out)) * 1e-3)[0]
    if above.size:
        out = out[: min(int(above[-1]) + int(0.02 * SR), len(out))]
    fade = min(int(0.01 * SR), len(out))
    out[-fade:] *= np.linspace(1.0, 0.0, fade)
    target = _SPLASH_TARGET_MOMENTARY[size] + rng.uniform(-0.5, 0.5)
    out = out * 10.0 ** ((target - k_weighted_momentary_max(out)) / 20.0)
    peak = np.max(np.abs(out))
    return out * (_SPLASH_PEAK_CAP / peak) if peak > _SPLASH_PEAK_CAP else out


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
