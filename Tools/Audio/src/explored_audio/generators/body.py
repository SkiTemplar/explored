"""Cuerpo y necesidades (biblia de contenido §5.4, GDD §4.3): comer, beber,
respiracion cansada, el latido con la salud baja y las tripas con hambre. Efectos discretos e
intimos (poco "mundo", mucho "estado interno"), a diferencia de la fauna o
el ambiente."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length, smooth_random_walk
from ..filters import static_filter, time_varying_filter
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


def _gut_phrase(rng: np.random.Generator, dur: float, f0_start: float, f0_end: float, formant_start: float, formant_end: float) -> np.ndarray:
    """Una frase del gruñido: una bolsa de gas empujada por el peristaltismo.

    La fuente es un tren de pulsos grave (30-90 Hz), como una voz en «fritura»,
    con jitter y shimmer: el gas atraviesa la estrechez a borbotones. La tripa
    es un tubo blando cuya resonancia se desliza mientras se contrae, así que
    los pulsos pasan por dos formantes móviles (~180-420 Hz y ~2,2× más arriba).
    """
    n = int(dur * SR)
    t = np.linspace(0.0, 1.0, n)
    glide = f0_start + (f0_end - f0_start) * (0.5 - 0.5 * np.cos(np.pi * t))
    f0 = glide * smooth_random_walk(n, rng, smoothing_hz=9.0, sr=SR, low=0.88, high=1.12)
    phase = np.cumsum(2.0 * np.pi * f0 / SR)
    # Pulso liso (1 - cos)^6: estrecho y sin aristas, con armónicos hasta ~1 kHz
    # para que se oiga también en altavoces pequeños.
    pulses = (0.5 - 0.5 * np.cos(phase)) ** 6
    pulses -= pulses.mean()
    shimmer = smooth_random_walk(n, rng, smoothing_hz=18.0, sr=SR, low=0.55, high=1.0)
    source = pulses * shimmer

    formant = formant_start + (formant_end - formant_start) * t
    low = time_varying_filter(source, SR, formant, q=3.5, kind="bandpass", block_size=256)
    high = time_varying_filter(source, SR, formant * rng.uniform(2.1, 2.4), q=5.0, kind="bandpass", block_size=256)
    # Soplo húmedo del gas por el líquido, siempre por debajo de los pulsos.
    wet = static_filter(rng.standard_normal(n), SR, fc=float(np.mean(formant)) * 1.6, q=1.2, kind="bandpass")
    wet *= 0.5 + 0.5 * shimmer
    body = low + 0.45 * high + 0.05 * wet

    attack = rng.uniform(0.08, 0.16)
    env = fit_length(ar_envelope(SR, attack, dur - attack, shape=1.6), n)
    shaped = body * env
    # Normalizada a pico 1: la resonancia estrecha deja la frase a -25 dB de la
    # fuente, y sin esto los borboteos taparían el gruñido.
    return shaped / max(float(np.max(np.abs(shaped))), 1e-9)


def _gut_bubble(rng: np.random.Generator) -> np.ndarray:
    """Borboteo: una burbuja que se suelta y sube de tono al encogerse
    (seno con glissando ascendente y caída de 15-35 ms, ataque de 1 ms)."""
    f_start = rng.uniform(260, 620)
    rise = rng.uniform(0.6, 1.4)
    tau = rng.uniform(0.015, 0.035)
    n = int(tau * 7 * SR)
    t = np.arange(n) / SR
    freq = f_start * (1.0 + rise * t / (tau * 7))
    tone = np.sin(2 * np.pi * np.cumsum(freq) / SR)
    env = np.exp(-t / tau) * np.minimum(1.0, t / 0.001)
    env[-int(0.002 * SR):] *= np.linspace(1.0, 0.0, int(0.002 * SR))
    return tone * env


def stomach_growl(name: str) -> np.ndarray:
    """Tripas que suenan con hambre (borborigmo): dos o tres frases de gas
    que suben o bajan de tono, la última más larga, y un remate de borboteos.

    Es un sonido íntimo, como la respiración: sin reverberación de mundo, en
    graves-medios (el 90 % de la energía bajo 1 kHz) y a una sonoridad
    parecida a `sfx_breath_tired`.
    """
    rng = rng_for(name)
    phrases: list[tuple[int, np.ndarray]] = []
    cursor = 0.0
    n_phrases = int(rng.integers(2, 4))
    for i in range(n_phrases):
        last = i == n_phrases - 1
        dur = rng.uniform(0.55, 0.8) if last else rng.uniform(0.22, 0.38)
        if rng.random() < 0.5:
            f0s, f0e = rng.uniform(35, 50), rng.uniform(60, 85)
        else:
            f0s, f0e = rng.uniform(60, 85), rng.uniform(32, 45)
        fm_s, fm_e = rng.uniform(170, 260), rng.uniform(260, 420)
        if rng.random() < 0.5:
            fm_s, fm_e = fm_e, fm_s
        amp = 1.0 if last else rng.uniform(0.55, 0.8)
        phrases.append((int(cursor * SR), amp * _gut_phrase(rng, dur, f0s, f0e, fm_s, fm_e)))
        cursor += dur + (rng.uniform(0.04, 0.12) if not last else -dur * 0.25)

    bubbles_start = cursor
    n_bubbles = int(rng.integers(3, 6))
    bubble_times = bubbles_start + np.sort(rng.uniform(0.0, 0.35, n_bubbles))
    total = int((bubble_times[-1] + 0.3) * SR)
    out = np.zeros(total)
    for pos, phrase in phrases:
        out[pos:pos + len(phrase)] += phrase
    for bt in bubble_times:
        b = _gut_bubble(rng) * rng.uniform(0.2, 0.4)
        pos = int(bt * SR)
        end = min(pos + len(b), total)
        out[pos:end] += b[: end - pos]

    out = static_filter(out, SR, fc=40.0, q=0.7, kind="highpass")
    out = static_filter(out, SR, fc=1400.0, q=0.7, kind="lowpass")
    fade = int(0.04 * SR)
    out[-fade:] *= 0.5 + 0.5 * np.cos(np.linspace(0.0, np.pi, fade))
    return out / max(float(np.max(np.abs(out))), 1e-9) * 0.3
