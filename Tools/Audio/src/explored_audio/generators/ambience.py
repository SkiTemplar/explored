"""Diez paisajes sonoros en bucle estereo (30-60 s), todos sintetizados por
codigo: ruido rosa/marron filtrado, moduladores lentos, granulado para
gotas/insectos y una pizca de reverberacion de Schroeder para dar espacio.

Cada funcion genera `loop_s + fade_s` segundos de proceso continuo y lo
cierra con `seamless_loop` (ver `loop.py`): así el bucle no tiene clic.
"""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, smooth_random_walk
from ..filters import static_filter, time_varying_filter
from ..granular import place_grains, render_noise_grains
from ..loop import seamless_loop
from ..modal import modal_hit
from ..noise import brown_noise, pink_noise
from ..reverb import schroeder_reverb
from ..rng import rng_for
from ..stereo import decorrelate

SR = SAMPLE_RATE


def _lens(loop_s: float, fade_s: float) -> tuple[int, int, int]:
    loop_len = int(loop_s * SR)
    fade_len = int(fade_s * SR)
    return loop_len + fade_len, loop_len, fade_len


def _wave_envelope(n: int, rng: np.random.Generator, rate_hz: float, dur_range: tuple[float, float]) -> np.ndarray:
    """Rachas de olas/rafagas con ritmo irregular (proceso de Poisson)."""
    env = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz, jitter=0.8):
        dur = rng.uniform(*dur_range)
        piece = ar_envelope(SR, attack_s=dur * 0.3, release_s=dur * 0.7, shape=1.6) * amp
        end = min(pos + len(piece), n)
        env[pos:end] += piece[: end - pos]
    return np.clip(env, 0.0, None)


def amb_ocean_calm(name: str) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(42.0, 5.0)

    rumble = static_filter(brown_noise(n, rng, leak=0.9995), SR, fc=90, q=0.7, kind="lowpass")
    swell = smooth_random_walk(n, rng, smoothing_hz=0.045, sr=SR, low=0.55, high=1.0)

    hiss = pink_noise(n, rng)
    surf = static_filter(hiss, SR, fc=2200, q=0.7, kind="lowpass")
    surf = static_filter(surf, SR, fc=180, q=0.7, kind="highpass")
    wave_env = _wave_envelope(n, rng, rate_hz=0.15, dur_range=(2.5, 5.0))
    cutoff_track = 900.0 + wave_env * 2800.0
    surf = time_varying_filter(surf, SR, cutoff_track, q=0.8, kind="lowpass")

    mono = rumble * 0.45 * swell + surf * (0.35 + 0.9 * wave_env)
    mono = schroeder_reverb(mono, SR, room_size=0.3, damping=0.5, wet=0.08)
    stereo = decorrelate(mono, rng, SR, spread_ms=18)
    return seamless_loop(stereo, loop_len, fade_len)


def amb_ocean_rough(name: str) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(42.0, 5.0)

    rumble = static_filter(brown_noise(n, rng, leak=0.9993), SR, fc=110, q=0.8, kind="lowpass")
    swell = smooth_random_walk(n, rng, smoothing_hz=0.09, sr=SR, low=0.7, high=1.15)

    hiss = pink_noise(n, rng)
    surf = static_filter(hiss, SR, fc=3200, q=0.7, kind="lowpass")
    surf = static_filter(surf, SR, fc=160, q=0.6, kind="highpass")
    wave_env = _wave_envelope(n, rng, rate_hz=0.35, dur_range=(1.6, 3.4))
    cutoff_track = 1400.0 + wave_env * 4200.0
    surf = time_varying_filter(surf, SR, cutoff_track, q=0.7, kind="lowpass")

    whitecaps = render_noise_grains(
        n, SR, rng, rate_hz=1.4, grain_len_s_range=(0.3, 0.8),
        band_hz_range=(1500, 5500), q=0.6, amp_scale=0.6,
    )

    mono = rumble * 0.55 * swell + surf * (0.5 + 1.1 * wave_env) + whitecaps
    mono = schroeder_reverb(mono, SR, room_size=0.4, damping=0.4, wet=0.1)
    stereo = decorrelate(mono, rng, SR, spread_ms=22)
    return seamless_loop(stereo, loop_len, fade_len)


def amb_wind_light(name: str) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(36.0, 4.0)

    body = pink_noise(n, rng)
    cutoff_track = smooth_random_walk(n, rng, smoothing_hz=0.12, sr=SR, low=350.0, high=1100.0)
    swept = time_varying_filter(body, SR, cutoff_track, q=1.1, kind="bandpass")
    hiss = static_filter(pink_noise(n, rng), SR, fc=900, q=0.6, kind="lowpass")
    amp = smooth_random_walk(n, rng, smoothing_hz=0.06, sr=SR, low=0.5, high=1.0)

    mono = (swept * 0.8 + hiss * 0.3) * amp
    stereo = decorrelate(mono, rng, SR, spread_ms=25)
    return seamless_loop(stereo, loop_len, fade_len)


def amb_wind_strong(name: str) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(36.0, 4.0)

    body = pink_noise(n, rng)
    cutoff_track = smooth_random_walk(n, rng, smoothing_hz=0.2, sr=SR, low=300.0, high=1800.0)
    swept = time_varying_filter(body, SR, cutoff_track, q=1.6, kind="bandpass")
    howl_track = smooth_random_walk(n, rng, smoothing_hz=0.08, sr=SR, low=350.0, high=1400.0)
    howl = time_varying_filter(pink_noise(n, rng), SR, howl_track, q=3.2, kind="bandpass")
    rumble = static_filter(brown_noise(n, rng, leak=0.999), SR, fc=150, q=0.7, kind="lowpass")
    amp = smooth_random_walk(n, rng, smoothing_hz=0.1, sr=SR, low=0.65, high=1.15)

    mono = (swept * 0.9 + howl * 0.45 + rumble * 0.5) * amp
    stereo = decorrelate(mono, rng, SR, spread_ms=28)
    return seamless_loop(stereo, loop_len, fade_len)


def _cricket_voice(n: int, rng: np.random.Generator, freq: float, pulse_hz: float) -> np.ndarray:
    t = np.arange(n) / SR
    tone = np.sin(2 * np.pi * freq * t)
    pulse = np.clip(np.sin(2 * np.pi * pulse_hz * t) * 3.0, -1.0, 1.0)
    pulse = (pulse + 1.0) * 0.5
    presence = smooth_random_walk(n, rng, smoothing_hz=0.04, sr=SR, low=0.0, high=1.0) ** 2
    return tone * pulse * presence


def amb_jungle_day(name: str) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(45.0, 5.0)

    insects = render_noise_grains(
        n, SR, rng, rate_hz=18.0, grain_len_s_range=(0.02, 0.06),
        band_hz_range=(3200, 7500), q=3.0, amp_scale=0.35,
    )
    leaves = static_filter(pink_noise(n, rng), SR, fc=1400, q=0.6, kind="lowpass")
    leaves = static_filter(leaves, SR, fc=450, q=0.6, kind="highpass")
    leaves_amp = smooth_random_walk(n, rng, smoothing_hz=0.15, sr=SR, low=0.15, high=0.5)

    distant_birds = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz=0.06, jitter=0.7):
        dur = rng.uniform(0.25, 0.6)
        dn = max(int(dur * SR), 8)
        t = np.arange(dn) / SR
        sweep = rng.uniform(1800, 3200) + t / dur * rng.uniform(-400, 700)
        chirp = np.sin(2 * np.pi * np.cumsum(sweep) / SR) * np.hanning(dn) * amp * 0.25
        end = min(pos + dn, n)
        distant_birds[pos:end] += chirp[: end - pos]

    mono = insects + leaves * leaves_amp + distant_birds
    mono = schroeder_reverb(mono, SR, room_size=0.25, damping=0.6, wet=0.06)
    stereo = decorrelate(mono, rng, SR, spread_ms=20)
    return seamless_loop(stereo, loop_len, fade_len)


def amb_jungle_night(name: str) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(45.0, 5.0)

    crickets = np.zeros(n)
    for _ in range(7):
        freq = rng.uniform(3800, 5200)
        pulse = rng.uniform(18, 32)
        crickets += _cricket_voice(n, rng, freq, pulse) * rng.uniform(0.08, 0.16)

    frogs = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz=0.9, jitter=0.7):
        croak = modal_hit(
            SR, 0.18, base_freq=rng.uniform(180, 320),
            mode_ratios=[1.0, 1.5, 2.0], mode_dampings_s=[0.05, 0.03, 0.02],
            mode_amps=[1.0, 0.5, 0.3],
        )
        croak *= amp * 0.5
        end = min(pos + len(croak), n)
        frogs[pos:end] += croak[: end - pos]

    owl = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz=0.02, jitter=0.5):
        dur = 0.5
        dn = int(dur * SR)
        t = np.arange(dn) / SR
        freq = 550 - t / dur * 150
        hoot = np.sin(2 * np.pi * np.cumsum(freq) / SR) * ar_envelope(SR, 0.08, 0.35, shape=1.4)[:dn]
        hoot *= amp * 0.3
        end = min(pos + dn, n)
        owl[pos:end] += hoot[: end - pos]

    mono = crickets + frogs + owl
    mono = schroeder_reverb(mono, SR, room_size=0.35, damping=0.5, wet=0.15)
    stereo = decorrelate(mono, rng, SR, spread_ms=20)
    return seamless_loop(stereo, loop_len, fade_len)


def amb_rain_light(name: str) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(36.0, 4.0)

    drops = render_noise_grains(
        n, SR, rng, rate_hz=45.0, grain_len_s_range=(0.008, 0.025),
        band_hz_range=(2500, 8500), q=2.0, amp_scale=0.5,
    )
    wash = static_filter(pink_noise(n, rng), SR, fc=6000, q=0.7, kind="highpass")
    wash_amp = smooth_random_walk(n, rng, smoothing_hz=0.1, sr=SR, low=0.08, high=0.18)

    mono = drops + wash * wash_amp
    stereo = decorrelate(mono, rng, SR, spread_ms=15)
    return seamless_loop(stereo, loop_len, fade_len)


def amb_rain_heavy(name: str) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(36.0, 4.0)

    drops = render_noise_grains(
        n, SR, rng, rate_hz=180.0, grain_len_s_range=(0.006, 0.02),
        band_hz_range=(2000, 9000), q=2.2, amp_scale=0.4,
    )
    wash = static_filter(pink_noise(n, rng), SR, fc=4500, q=0.6, kind="highpass")
    wash_amp = smooth_random_walk(n, rng, smoothing_hz=0.15, sr=SR, low=0.35, high=0.55)
    low_rumble = static_filter(brown_noise(n, rng, leak=0.998), SR, fc=200, q=0.6, kind="lowpass")

    mono = drops * 1.1 + wash * wash_amp + low_rumble * 0.15
    stereo = decorrelate(mono, rng, SR, spread_ms=20)
    return seamless_loop(stereo, loop_len, fade_len)


def amb_stream(name: str) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(36.0, 4.0)

    body = pink_noise(n, rng)
    cutoff_track = smooth_random_walk(n, rng, smoothing_hz=0.7, sr=SR, low=500.0, high=3200.0)
    burble = time_varying_filter(body, SR, cutoff_track, q=1.3, kind="bandpass")
    sparkle = render_noise_grains(
        n, SR, rng, rate_hz=22.0, grain_len_s_range=(0.01, 0.04),
        band_hz_range=(3500, 8000), q=2.5, amp_scale=0.35,
    )

    mono = burble * 0.8 + sparkle
    stereo = decorrelate(mono, rng, SR, spread_ms=30)
    return seamless_loop(stereo, loop_len, fade_len)


def amb_rain_on_leaves(name: str) -> np.ndarray:
    """Lluvia sobre el dosel de la selva: mas repiqueteo agudo y resonante
    que `amb_rain_light`/`amb_rain_heavy` (las hojas dispersan cada gota en
    varios impactos), con goterones ocasionales cayendo de las puntas de las
    hojas."""
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(34.0, 4.0)

    patter = render_noise_grains(n, SR, rng, rate_hz=70.0, grain_len_s_range=(0.006, 0.018), band_hz_range=(3000, 9000), q=2.4, amp_scale=0.45)
    drips = render_noise_grains(n, SR, rng, rate_hz=3.5, grain_len_s_range=(0.02, 0.05), band_hz_range=(1200, 3200), q=1.6, amp_scale=0.7)
    canopy = static_filter(pink_noise(n, rng), SR, fc=2500, q=0.6, kind="lowpass")
    canopy = static_filter(canopy, SR, fc=500, q=0.6, kind="highpass")
    amp = smooth_random_walk(n, rng, smoothing_hz=0.12, sr=SR, low=0.5, high=0.85)

    mono = patter + drips * 0.5 + canopy * amp * 0.3
    stereo = decorrelate(mono, rng, SR, spread_ms=16)
    return seamless_loop(stereo, loop_len, fade_len)


def amb_underwater(name: str) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(32.0, 4.0)

    body = static_filter(brown_noise(n, rng, leak=0.9997), SR, fc=420, q=0.6, kind="lowpass")
    breathing = smooth_random_walk(n, rng, smoothing_hz=0.08, sr=SR, low=0.6, high=1.0)

    bubbles = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz=0.5, jitter=0.7):
        blub = modal_hit(
            SR, 0.2, base_freq=rng.uniform(90, 160),
            mode_ratios=[1.0, 1.8], mode_dampings_s=[0.06, 0.03], mode_amps=[1.0, 0.4],
        )
        blub *= amp * 0.4
        end = min(pos + len(blub), n)
        bubbles[pos:end] += blub[: end - pos]

    mono = body * breathing + bubbles
    mono = static_filter(mono, SR, fc=750, q=0.6, kind="lowpass")
    stereo = decorrelate(mono, rng, SR, spread_ms=10)
    return seamless_loop(stereo, loop_len, fade_len)
