"""Instrumentos sintetizados: cada funcion renderiza UNA nota (o un acorde
sostenido, para pad/cuerdas) a partir de su frecuencia, duracion y
velocidad. Todas devuelven mono; el secuenciador se encarga de panoramizar,
mezclar y reverberar.

Tecnicas por instrumento (las que pide el encargo):
- Marimba / kalimba: sintesis modal (varias sinusoides con su propio
  decaimiento exponencial) con un transitorio de percusion añadido.
- Pads y cuerdas: sintesis aditiva de una "sierra" limitada en banda (suma
  de armonicos hasta Nyquist, sin aliasing) en varias voces al unisono
  ligeramente desafinadas entre si (chorus), con vibrato lento.
- Flauta de bambu: la misma sierra limitada en banda, suavizada con un
  filtro paso bajo resonante, vibrato y ruido de soplido; con portamento
  entre notas (lo aplica el secuenciador sobre el contorno de tono).
- Bajo: fundamental + segundo armonico suave, filtrado, para un timbre
  redondo sin aristas de sierra.
- Percusion: shaker (granular), caja de madera (modal corto) y bombo grave
  (barrido de tono + transitorio filtrado).
"""

from __future__ import annotations

import numpy as np

from ..envelopes import ar_envelope, exp_decay, fit_length
from ..filters import static_filter
from ..granular import render_noise_grains
from ..modal import modal_hit
from .humanize import micro_detune_ratio


def _t(duration_s: float, sr: int) -> np.ndarray:
    n = max(int(duration_s * sr), 1)
    return np.arange(n) / sr


def band_limited_saw(t: np.ndarray, freq: float, sr: int, max_harmonics: int = 40) -> np.ndarray:
    """Sierra por sintesis aditiva (serie de Fourier truncada en Nyquist):
    sin armonicos por encima de la frecuencia de muestreo no hay aliasing,
    a diferencia de una sierra "de libro de texto" generada por modulo."""
    n_harmonics = max(1, min(int((sr * 0.45) / max(freq, 1.0)), max_harmonics))
    out = np.zeros_like(t)
    for k in range(1, n_harmonics + 1):
        out += ((-1.0) ** (k + 1)) / k * np.sin(2 * np.pi * k * freq * t)
    return out * (2.0 / np.pi)


def unison_tone(
    t: np.ndarray,
    freq: float,
    sr: int,
    rng: np.random.Generator,
    voices: int = 3,
    detune_cents: float = 9.0,
    vibrato_hz: float = 0.35,
    vibrato_cents: float = 12.0,
    max_harmonics: int = 20,
) -> np.ndarray:
    """Varias sierras limitadas en banda, desafinadas entre si (unisono) y
    con un vibrato lento y compartido: el "chorus lento" de un pad calido."""
    vibrato = np.sin(2 * np.pi * vibrato_hz * t + rng.uniform(0, 2 * np.pi))
    out = np.zeros_like(t)
    spread = np.linspace(-1.0, 1.0, voices) if voices > 1 else np.array([0.0])
    for offset in spread:
        cents = offset * detune_cents + vibrato * vibrato_cents
        inst_freq = freq * (2.0 ** (cents / 1200.0))
        # Fase acumulada a partir de una frecuencia instantanea que varia
        # (vibrato): hay que integrar, no solo escalar freq*t.
        phase = 2 * np.pi * np.cumsum(inst_freq) / sr
        n_harm = max(1, min(int((sr * 0.45) / max(freq, 1.0)), max_harmonics))
        for k in range(1, n_harm + 1):
            out += ((-1.0) ** (k + 1)) / k * np.sin(k * phase)
    return out * (2.0 / np.pi) / voices


def marimba(freq: float, duration_s: float, velocity: float, sr: int, rng: np.random.Generator | None = None) -> np.ndarray:
    """Barra de madera: modos muy inarmonicos, ataque brillante, caida rapida."""
    rng = rng or np.random.default_rng()
    n = max(int(duration_s * sr), 1)
    t = np.arange(n) / sr
    mallet = static_filter(rng.standard_normal(min(n, int(0.006 * sr) or 1)), sr, fc=2500, q=0.6, kind="highpass")
    mallet = fit_length(mallet * np.exp(-np.arange(len(mallet)) / sr / 0.003), n)
    body = modal_hit(
        sr, duration_s, base_freq=freq,
        mode_ratios=[1.0, 3.93, 9.4], mode_dampings_s=[0.35, 0.16, 0.08],
        mode_amps=[1.0, 0.35, 0.12], rng=rng, detune=0.003,
    )
    return (mallet * 0.5 + body) * velocity


def kalimba(freq: float, duration_s: float, velocity: float, sr: int, rng: np.random.Generator | None = None) -> np.ndarray:
    """Lengueta metalica: mas armonica que la marimba, sustain mas largo,
    pellizco suave en vez de golpe de mazo."""
    rng = rng or np.random.default_rng()
    n = max(int(duration_s * sr), 1)
    pluck_n = max(int(0.004 * sr), 1)
    pluck = static_filter(rng.standard_normal(pluck_n), sr, fc=3500, q=0.5, kind="bandpass")
    pluck = fit_length(pluck * np.exp(-np.arange(pluck_n) / sr / 0.004), n)
    body = modal_hit(
        sr, duration_s, base_freq=freq,
        mode_ratios=[1.0, 2.01, 3.22, 5.4], mode_dampings_s=[1.1, 0.7, 0.4, 0.2],
        mode_amps=[1.0, 0.4, 0.22, 0.1], rng=rng, detune=0.002,
    )
    return (pluck * 0.35 + body) * velocity


def bass(freq: float, duration_s: float, velocity: float, sr: int, rng: np.random.Generator | None = None) -> np.ndarray:
    """Bajo redondo: fundamental + un poco de segundo armonico, sin sierra."""
    rng = rng or np.random.default_rng()
    t = _t(duration_s, sr)
    n = len(t)
    tone = np.sin(2 * np.pi * freq * t) + 0.22 * np.sin(2 * np.pi * 2 * freq * t)
    env = fit_length(ar_envelope(sr, attack_s=0.012, release_s=max(duration_s - 0.012, 0.05), shape=0.7), n)
    tone = static_filter(tone, sr, fc=min(freq * 6, 900), q=0.6, kind="lowpass")
    return tone * env * velocity


def bamboo_flute_contour(
    freq_env: np.ndarray, amp_env: np.ndarray, sr: int, rng: np.random.Generator,
    vibrato_hz: float = 5.5, vibrato_cents: float = 25.0,
) -> np.ndarray:
    """Flauta sobre un CONTORNO de tono ya continuo (con portamento incluido,
    lo arma el secuenciador): sierra limitada en banda suavizada con un paso
    bajo resonante, vibrato y ruido de soplido."""
    n = len(freq_env)
    t = np.arange(n) / sr
    vibrato = 1.0 + (vibrato_cents / 1200.0) * np.log(2.0) * 0.5 * np.sin(2 * np.pi * vibrato_hz * t)
    inst_freq = freq_env * vibrato
    phase = 2 * np.pi * np.cumsum(inst_freq) / sr
    avg_freq = max(float(np.mean(freq_env)), 1.0)
    n_harm = max(1, min(int((sr * 0.45) / avg_freq), 25))
    tone = np.zeros(n)
    for k in range(1, n_harm + 1):
        tone += ((-1.0) ** (k + 1)) / k * np.sin(k * phase)
    tone *= 2.0 / np.pi
    tone = static_filter(tone, sr, fc=avg_freq * 3.2, q=0.9, kind="lowpass")

    breath = static_filter(rng.standard_normal(n), sr, fc=1800, q=0.6, kind="bandpass")
    breath = static_filter(breath, sr, fc=300, q=0.6, kind="highpass")

    return (tone * 0.85 + breath * 0.12) * amp_env


def strings_pad(
    freqs: list[float], duration_s: float, velocity: float, sr: int, rng: np.random.Generator,
    voices: int = 3, attack_s: float = 1.4, release_s: float = 2.0, max_harmonics: int = 16,
) -> np.ndarray:
    """Colchon calido/cuerdas: acorde sostenido, unisono con vibrato lento y
    envolvente de ataque y relajacion largos (nunca percusivo)."""
    t = _t(duration_s, sr)
    n = len(t)
    out = np.zeros(n)
    for freq in freqs:
        detuned = freq * micro_detune_ratio(rng, max_cents=4.0)
        out += unison_tone(t, detuned, sr, rng, voices=voices, max_harmonics=max_harmonics)
    out /= max(len(freqs), 1)
    env = fit_length(ar_envelope(sr, attack_s=min(attack_s, duration_s * 0.4), release_s=max(duration_s - min(attack_s, duration_s * 0.4), 0.05), shape=1.0), n)
    out = static_filter(out, sr, fc=2600, q=0.6, kind="lowpass")
    return out * env * velocity


def tremolo_strings(freqs: list[float], duration_s: float, velocity: float, sr: int, rng: np.random.Generator, tremolo_hz: float = 7.0) -> np.ndarray:
    """Variante en tremolo de `strings_pad`, para tension: la misma sierra en
    unisono pero con la amplitud pulsando (arco rapido, no sostenido)."""
    base = strings_pad(freqs, duration_s, 1.0, sr, rng, voices=3, attack_s=0.3, release_s=0.3)
    t = _t(duration_s, sr)
    n = min(len(t), len(base))
    trem = 0.65 + 0.35 * np.sin(2 * np.pi * tremolo_hz * t[:n] + rng.uniform(0, 2 * np.pi))
    return base[:n] * trem * velocity


def karplus_strong_pluck(
    freq: float, duration_s: float, velocity: float, sr: int, rng: np.random.Generator | None = None,
    tau_s: float = 1.3, brightness: float = 0.55,
) -> np.ndarray:
    """Guitarra pulsada por sintesis de cuerda digital de Karplus-Strong:
    linea de retardo de un periodo, realimentada a traves de un promedio
    movil de dos muestras. Ese promedio amortigua los armonicos altos mas
    deprisa que la fundamental en cada vuelta -el "brillo que se apaga hacia
    un cuerpo calido" de una cuerda real pellizcada-, a diferencia de una
    sierra o una senoidal con una envolvente exponencial encima, que suenan
    siempre igual de "afiladas" mientras decaen.

    Se implementa con el buffer circular clasico del algoritmo (Karplus &
    Strong, 1983): cada posicion del buffer se lee y se reescribe UNA vez por
    periodo, así que el coste es O(muestras) y no O(muestras * periodo) como
    seria resolver la misma ecuacion en diferencias con un filtro IIR
    generico (su orden es el periodo: cientos de coeficientes para las notas
    graves)."""
    rng = rng or np.random.default_rng()
    n = max(int(duration_s * sr), 1)
    period = max(int(round(sr / max(freq, 1.0))), 2)

    excitation = rng.uniform(-1.0, 1.0, period)
    excitation = static_filter(excitation, sr, fc=700.0 + 3200.0 * brightness, q=0.7, kind="lowpass")

    # Ganancia de bucle que hace decaer la fundamental a 1/e en `tau_s`: cada
    # vuelta por la linea de retardo dura `periodo/sr` segundos.
    loop_gain = float(np.exp(-1.0 / (max(freq, 1.0) * max(tau_s, 1e-3))))

    buf = excitation.copy()
    out = np.empty(n)
    cur, nxt = 0, 1 % period
    for i in range(n):
        out[i] = buf[cur]
        buf[cur] = loop_gain * 0.5 * (buf[cur] + buf[nxt])
        cur += 1
        nxt += 1
        if cur == period:
            cur = 0
        if nxt == period:
            nxt = 0

    peak = np.max(np.abs(out))
    if peak > 1e-9:
        out = out / peak
    out = static_filter(out, sr, fc=3800.0, q=0.6, kind="lowpass")  # calidez: quita el filo del excitador de ruido
    return out * velocity


def wood_block(freq: float, duration_s: float, velocity: float, sr: int, rng: np.random.Generator | None = None) -> np.ndarray:
    rng = rng or np.random.default_rng()
    return modal_hit(
        sr, duration_s, base_freq=freq,
        mode_ratios=[1.0, 2.9], mode_dampings_s=[0.03, 0.015], mode_amps=[1.0, 0.4],
        rng=rng, detune=0.02,
    ) * velocity


def shaker(duration_s: float, velocity: float, sr: int, rng: np.random.Generator | None = None) -> np.ndarray:
    rng = rng or np.random.default_rng()
    n = max(int(duration_s * sr), 1)
    body = static_filter(rng.standard_normal(n), sr, fc=5500, q=0.6, kind="highpass")
    env = fit_length(ar_envelope(sr, attack_s=0.006, release_s=max(duration_s - 0.006, 0.02), shape=1.6), n)
    return body * env * velocity


def soft_kick(duration_s: float, velocity: float, sr: int, rng: np.random.Generator | None = None) -> np.ndarray:
    rng = rng or np.random.default_rng()
    n = max(int(duration_s * sr), 1)
    t = np.arange(n) / sr
    pitch = 150.0 * np.exp(-t / 0.05) + 45.0
    tone = np.sin(2 * np.pi * np.cumsum(pitch) / sr)
    thump = static_filter(rng.standard_normal(n), sr, fc=180, q=0.6, kind="lowpass")
    env = fit_length(exp_decay(sr, duration_s, tau_s=0.16), n)
    return (tone * 0.85 + thump * 0.3) * env * velocity
