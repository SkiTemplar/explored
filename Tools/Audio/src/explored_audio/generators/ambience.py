"""Diez paisajes sonoros en bucle estereo (30-60 s), todos sintetizados por
codigo: ruido rosa/marron filtrado, moduladores lentos, granulado para
gotas/insectos y una pizca de reverberacion de Schroeder para dar espacio.

Cada funcion genera `loop_s + fade_s` segundos de proceso continuo y lo
cierra con `seamless_loop` (ver `loop.py`): así el bucle no tiene clic.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length, smooth_random_walk
from ..filters import static_filter, time_varying_filter
from ..granular import place_grains, render_noise_grains
from ..loop import seamless_loop
from ..modal import modal_hit
from ..noise import brown_noise, pink_noise
from ..reverb import schroeder_reverb
from ..rng import rng_for
from ..stereo import decorrelate, pan_constant_power

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


@dataclass(frozen=True)
class _SurfStyle:
    """Caracter de una orilla: cada cuanto rompe una ola y con que fuerza."""

    gap_s: tuple[float, float]  # hueco entre olas sucesivas
    approach_s: tuple[float, float]  # la ola que se acerca y crece
    attack_s: tuple[float, float]  # rompiente: de nada al maximo
    wash_s: tuple[float, float]  # espuma que sube por la arena
    backwash_s: tuple[float, float]  # resaca que se retira
    crash_top_hz: float  # brillo del golpe de la rompiente
    thump: float  # golpe grave de la masa de agua (0 = orilla mansa)
    foam: float  # densidad del burbujeo de la espuma
    foam_level: float  # nivel de la espuma frente al golpe
    size: tuple[float, float]  # amplitud relativa de cada ola


_CALM = _SurfStyle(
    gap_s=(4.2, 7.0), approach_s=(1.4, 2.4), attack_s=(0.18, 0.35),
    wash_s=(1.8, 2.8), backwash_s=(1.8, 2.8), crash_top_hz=3200.0,
    thump=0.0, foam=0.8, foam_level=0.18, size=(0.45, 1.0),
)
_ROUGH = _SurfStyle(
    gap_s=(3.2, 6.0), approach_s=(1.0, 1.8), attack_s=(0.05, 0.12),
    wash_s=(2.4, 3.6), backwash_s=(1.8, 3.0), crash_top_hz=7000.0,
    thump=1.0, foam=1.3, foam_level=0.28, size=(0.6, 1.0),
)


def _foam_fizz(n: int, rng: np.random.Generator, density: float) -> np.ndarray:
    """Burbujeo de espuma: chasquidos diminutos de 2-9 kHz muy seguidos. La
    densidad y el brillo de cada chasquido varian al azar, no es un siseo."""
    hiss = static_filter(rng.standard_normal(n), SR, fc=2400, q=0.6, kind="highpass")
    hiss = static_filter(hiss, SR, fc=6500, q=0.6, kind="lowpass")
    # Moduladora a saltos: ruido paso bajo elevado a una potencia alta deja
    # picos cortos y aislados (cada pico es una burbuja que revienta).
    crackle = static_filter(rng.standard_normal(n), SR, fc=140.0 * density, q=0.5, kind="lowpass")
    crackle = np.abs(crackle) / (np.std(crackle) + 1e-9)
    return hiss * crackle**1.6


def _surf_wave(rng: np.random.Generator, style: _SurfStyle) -> tuple[np.ndarray, int]:
    """Una ola completa (mono) y la muestra donde rompe dentro del evento."""
    approach = int(rng.uniform(*style.approach_s) * SR)
    attack = int(rng.uniform(*style.attack_s) * SR)
    wash = int(rng.uniform(*style.wash_s) * SR)
    backwash = int(rng.uniform(*style.backwash_s) * SR)
    n = approach + attack + wash + backwash
    t_break = approach + attack
    size = rng.uniform(*style.size)

    # 1. La ola se acerca: rumor medio-grave que crece y se abre.
    grow = np.zeros(n)
    grow[:t_break] = np.linspace(0.0, 1.0, t_break) ** 2.5
    grow[t_break:] = np.exp(-np.arange(n - t_break) / (0.35 * SR))
    body_cut = np.full(n, 350.0)
    body_cut[:t_break] = 250.0 + 900.0 * np.linspace(0.0, 1.0, t_break) ** 2
    body = time_varying_filter(pink_noise(n, rng), SR, body_cut, q=0.7, kind="lowpass")
    body *= grow * 0.35

    # 2. Rompiente: ruido ancho que se enciende en `attack` y cae deprisa;
    # el corte baja a la vez que el nivel (el golpe es lo mas brillante).
    crash_env = np.zeros(n)
    crash_env[approach:t_break] = np.linspace(0.0, 1.0, attack) ** 1.5
    tail = n - t_break
    crash_env[t_break:] = np.exp(-np.arange(tail) / (0.3 * wash))
    crash_cut = np.full(n, 600.0)
    crash_cut[approach:] = 600.0 + (style.crash_top_hz - 600.0) * np.clip(crash_env[approach:], 0.0, 1.0)
    crash = time_varying_filter(pink_noise(n, rng), SR, crash_cut, q=0.6, kind="lowpass")
    crash = static_filter(crash, SR, fc=160, q=0.6, kind="highpass") * crash_env * 0.6

    # Golpe grave de la masa de agua (solo en mar de fondo).
    thump = np.zeros(n)
    if style.thump > 0.0:
        thump_env = np.zeros(n)
        thump_env[approach:t_break] = np.linspace(0.0, 1.0, attack)
        thump_env[t_break:] = np.exp(-np.arange(tail) / (0.25 * SR))
        thump = static_filter(brown_noise(n, rng, leak=0.995), SR, fc=110, q=0.8, kind="lowpass")
        thump *= thump_env * style.thump * 0.9

    # 3. Espuma que sube por la arena: burbujeo que arranca con la rompiente
    # y se apaga con la subida.
    wash_env = np.zeros(n)
    rise = max(int(0.12 * SR), 1)
    wash_env[t_break : t_break + rise] = np.linspace(0.0, 1.0, rise)
    decay_len = n - t_break - rise
    wash_env[t_break + rise :] = np.exp(-np.arange(decay_len) / (0.5 * (wash + backwash * 0.5)))
    foam = _foam_fizz(n, rng, style.foam) * wash_env * style.foam_level

    # 4. Resaca: siseo de agua que se retira por la arena, cada vez mas sordo.
    back_env = np.zeros(n)
    b0 = t_break + wash // 2
    blen = n - b0
    back_env[b0:] = np.sin(np.linspace(0.0, np.pi, blen)) ** 1.5
    back_cut = np.full(n, 800.0)
    back_cut[b0:] = np.linspace(2600.0, 700.0, blen)
    backwash_noise = static_filter(rng.standard_normal(n), SR, fc=400, q=0.6, kind="highpass")
    backwash_noise = time_varying_filter(backwash_noise, SR, back_cut, q=0.8, kind="lowpass")
    backwash_noise *= back_env * 0.4

    wave = body + crash + thump + foam + backwash_noise
    # Cierre suave: sin el, la cola de la espuma se cortaba en seco.
    fade = min(int(0.8 * SR), n)
    wave[n - fade :] *= np.linspace(1.0, 0.0, fade) ** 2
    # Hueco final para que la reverberacion se apague sin cortarse.
    return np.concatenate([wave * size, np.zeros(int(0.6 * SR))]), t_break


def _surf(name: str, style: _SurfStyle, bed_level: float, reverb_room: float) -> np.ndarray:
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(42.0, 5.0)

    # Lecho: mar de fondo lejano, siempre presente, que respira despacio.
    swell = smooth_random_walk(n, rng, smoothing_hz=0.05, sr=SR, low=0.6, high=1.0)
    rumble = static_filter(brown_noise(n, rng, leak=0.9995), SR, fc=120, q=0.7, kind="lowpass")
    distant = static_filter(pink_noise(n, rng), SR, fc=900, q=0.7, kind="lowpass")
    distant = static_filter(distant, SR, fc=150, q=0.7, kind="highpass")
    bed = (rumble * 0.5 + distant * 0.25) * swell * bed_level
    bed = schroeder_reverb(bed, SR, room_size=reverb_room, damping=0.5, wet=0.08)
    stereo = decorrelate(bed, rng, SR, spread_ms=18)

    # Rumor de orilla continuo: agua que se mueve sobre la arena entre ola y
    # ola. Sin el, el mar quedaba mudo en los huecos (-40 dB entre olas).
    lap = static_filter(pink_noise(n, rng), SR, fc=700, q=0.6, kind="highpass")
    lap = static_filter(lap, SR, fc=4500, q=0.6, kind="lowpass")
    lap *= smooth_random_walk(n, rng, smoothing_hz=0.25, sr=SR, low=0.4, high=1.0) * bed_level * 0.12
    lap_side = static_filter(np.roll(lap, int(0.011 * SR)), SR, fc=900, q=0.7, kind="highpass")
    stereo += np.stack([lap + 0.7 * lap_side, lap - 0.7 * lap_side])

    # Olas: cada una rompe en un punto distinto de la orilla (pan) y su
    # espuma se abre hacia los lados al subir por la arena.
    t = rng.uniform(0.0, style.gap_s[0]) * SR
    while t < n:
        wave, t_break = _surf_wave(rng, style)
        start = int(t)
        pan = rng.uniform(-0.55, 0.55)
        wet = schroeder_reverb(wave, SR, room_size=reverb_room, damping=0.45, wet=0.12)
        # Cada ola tiene anchura propia (graves en fase, agudos decorrelados)
        # y se coloca en su punto de la orilla; la espuma se abre hacia el
        # lado contrario mientras sube por la arena.
        side = static_filter(np.roll(wet, int(0.007 * SR)), SR, fc=350, q=0.7, kind="highpass") * 0.6
        end = min(start + len(wet), n)
        seg = np.stack([wet + side, wet - side])[:, : end - start]
        spread = np.clip((np.arange(seg.shape[-1]) - t_break) / (1.5 * SR), 0.0, 1.0)
        pos = pan * (1.0 - 0.6 * spread)
        angle = (pos + 1.0) * np.pi / 4.0
        stereo[0, start:end] += seg[0] * np.cos(angle) * np.sqrt(2.0)
        stereo[1, start:end] += seg[1] * np.sin(angle) * np.sqrt(2.0)
        t += rng.uniform(*style.gap_s) * SR
    return seamless_loop(stereo, loop_len, fade_len)


def amb_ocean_calm(name: str) -> np.ndarray:
    """Orilla de laguna: olas pequeñas que rompen suave y espumean en la arena."""
    return _surf(name, _CALM, bed_level=0.3, reverb_room=0.3)


def amb_ocean_rough(name: str) -> np.ndarray:
    """Mar de fondo: rompientes con golpe grave, espuma larga y resaca."""
    return _surf(name, _ROUGH, bed_level=0.5, reverb_room=0.4)


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


def _add_panned(bed: np.ndarray, mono_event: np.ndarray, position: int, pan: float) -> None:
    """Suma en sitio un evento mono panoramizado (sin copiar el colchon entero
    por evento, como hace `stereo.mix_event_into_bed`)."""
    end = min(position + len(mono_event), bed.shape[-1])
    if end > position:
        bed[:, position:end] += pan_constant_power(mono_event[: end - position], pan)


def _water_drop(rng: np.random.Generator) -> np.ndarray:
    """Gota que cae en un charco: la burbuja que atrapa resuena con un tono que
    SUBE en pocos milisegundos (modelo de Minnaert), no con un golpe de ruido."""
    dur = rng.uniform(0.05, 0.11)
    dn = int(dur * SR)
    t = np.arange(dn) / SR
    f0 = rng.uniform(900, 1900)
    freq = f0 * (1.0 + rng.uniform(0.5, 1.2) * (1.0 - np.exp(-t / 0.012)))
    tone = np.sin(2 * np.pi * np.cumsum(freq) / SR) * np.exp(-t / rng.uniform(0.012, 0.03))
    splat_len = int(0.004 * SR)
    splat = static_filter(rng.standard_normal(splat_len), SR, fc=3000, q=0.7, kind="highpass")
    tone[:splat_len] += splat * np.exp(-np.arange(splat_len) / SR / 0.0012) * 0.4
    return tone


def amb_rain_on_thatch(name: str) -> np.ndarray:
    """Lluvia oida desde dentro de un refugio con tejado de palma.

    La paja es fibrosa: absorbe el agudo del impacto, asi que el repiqueteo es
    mas sordo y denso que sobre hojas (`amb_rain_on_leaves`), con un lavado
    medio continuo y un leve retumbe del armazon. Lo que da la sensacion de
    estar "a cubierto" son los goteos del alero: unos pocos puntos fijos que
    gotean casi con periodo propio sobre charcos, colocados en el estereo."""
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(40.0, 4.0)

    patter = render_noise_grains(
        n, SR, rng, rate_hz=260.0, grain_len_s_range=(0.004, 0.012),
        band_hz_range=(700, 3000), q=1.1, amp_scale=0.3,
    )
    wash = static_filter(pink_noise(n, rng), SR, fc=1300, q=0.8, kind="bandpass")
    wash = static_filter(wash, SR, fc=3500, q=0.7, kind="lowpass")
    intensity = smooth_random_walk(n, rng, smoothing_hz=0.05, sr=SR, low=0.7, high=1.0)
    frame = static_filter(brown_noise(n, rng, leak=0.998), SR, fc=160, q=0.7, kind="lowpass")

    mono = (patter + wash * 0.35 + frame * 0.12) * intensity
    stereo = decorrelate(mono, rng, SR, spread_ms=14)

    # Goteos del alero: cada punto tiene su periodo (el agua se acumula en la
    # punta de la hoja a ritmo casi constante) con algo de irregularidad.
    for _ in range(5):
        pan = rng.uniform(-0.9, 0.9)
        period = rng.uniform(0.35, 1.4)
        level = rng.uniform(0.15, 0.4)
        t = rng.uniform(0.0, period)
        while t < n / SR:
            drop = _water_drop(rng) * level * rng.uniform(0.7, 1.0)
            _add_panned(stereo, drop, int(t * SR), pan)
            t += period * rng.uniform(0.8, 1.25)

    return seamless_loop(stereo, loop_len, fade_len)


def _frond_click(rng: np.random.Generator) -> np.ndarray:
    glen = max(int(rng.uniform(0.003, 0.01) * SR), 16)
    click = static_filter(rng.standard_normal(glen), SR, fc=rng.uniform(1400, 4200), q=2.5, kind="bandpass")
    return click * np.exp(-np.arange(glen) / SR / rng.uniform(0.001, 0.003))


def _trunk_creak(rng: np.random.Generator) -> np.ndarray:
    """Crujido del tronco al cimbrearse: friccion de adherencia-deslizamiento,
    un tren de pulsos irregular (25-60 por segundo) filtrado por la madera."""
    dur = rng.uniform(0.6, 1.4)
    dn = int(dur * SR)
    pulses = np.zeros(dn)
    t = 0.0
    rate = rng.uniform(25, 60)
    while t < dur:
        pos = int(t * SR)
        if pos < dn:
            pulses[pos] = rng.uniform(0.5, 1.0)
        t += (1.0 / rate) * rng.uniform(0.7, 1.3)
        rate *= rng.uniform(0.98, 1.03)
    body = static_filter(pulses, SR, fc=rng.uniform(450, 900), q=4.0, kind="bandpass")
    body += static_filter(pulses, SR, fc=rng.uniform(1300, 2000), q=5.0, kind="bandpass") * 0.4
    env = fit_length(ar_envelope(SR, dur * 0.4, dur * 0.6, shape=1.2), dn)
    return body * env


def amb_wind_palms(name: str) -> np.ndarray:
    """Viento entre palmeras: a diferencia de `amb_wind_light`, que es solo
    aire, aqui suenan las hojas. Los foliolos de palma son rigidos: con cada
    racha aletean (ruido agudo modulado a 8-20 Hz) y se golpean entre si con
    un tableteo seco que solo aparece cuando la racha es fuerte. De vez en
    cuando cruje un tronco."""
    rng = rng_for(name)
    n, loop_len, fade_len = _lens(40.0, 4.0)

    gust = smooth_random_walk(n, rng, smoothing_hz=0.07, sr=SR, low=0.0, high=1.0)
    gust = gust ** 2.0

    air_track = 380.0 + gust * 700.0
    air = time_varying_filter(pink_noise(n, rng), SR, air_track, q=1.0, kind="bandpass")

    flutter = smooth_random_walk(n, rng, smoothing_hz=14.0, sr=SR, low=0.3, high=1.0)
    rustle = static_filter(pink_noise(n, rng), SR, fc=2200, q=0.7, kind="highpass")
    rustle = static_filter(rustle, SR, fc=8000, q=0.7, kind="lowpass")

    clatter = np.zeros(n)
    for pos, amp in place_grains(n, SR, rng, rate_hz=90.0, jitter=1.0):
        if rng.random() > gust[pos] ** 2:
            continue
        piece = _frond_click(rng) * amp
        end = min(pos + len(piece), n)
        clatter[pos:end] += piece[: end - pos]

    mono = air * (0.15 + 0.6 * gust) + rustle * (0.03 + 0.6 * gust) * flutter + clatter * 1.0
    stereo = decorrelate(mono, rng, SR, spread_ms=24)

    for pos, _amp in place_grains(n, SR, rng, rate_hz=0.05, jitter=0.6):
        _add_panned(stereo, _trunk_creak(rng) * 0.35, pos, rng.uniform(-0.7, 0.7))

    return seamless_loop(stereo, loop_len, fade_len)
