"""Sonidos del final de Baliza Siete (GDD v3 §2 y §10): la bocina del correo
«Mistral» y el giro de la lente de la baliza.

- Bocina: un barco de línea pequeño lleva una bocina de aire grave. Se sintetiza
  como una sierra de banda limitada a ~92 Hz (con su quinta muy baja de nivel),
  un ataque con el «arranque» de presión (el tono sube un cuarto de tono en los
  primeros 200 ms), un vibrato lento y casi imperceptible, un resonador de bocina
  (paso banda ancho hacia 380 Hz) y la distancia: paso bajo a 2,4 kHz, eco
  tardío contra la isla y una cola de reverberación exterior larga. Dos toques,
  uno largo y otro corto, como el saludo de un barco que llega a puerto.
- Lente: el mecanismo de relojería que hace girar la óptica. Bucle de 4 s con el
  tictac del escape (8 golpes por segundo, alternando dos timbres de latón), el
  rumor del rodamiento (ruido marrón filtrado y modulado a la vuelta del
  engranaje) y un zumbido metálico muy bajo.
"""

from __future__ import annotations

import numpy as np
from scipy import signal

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length, sine_lfo
from ..filters import static_filter
from ..loop import seamless_loop
from ..modal import modal_hit
from ..noise import brown_noise, pink_noise
from ..rng import rng_for

SR = SAMPLE_RATE

HORN_FUNDAMENTAL_HZ = 92.0
HORN_LONG_S = 3.1
HORN_SHORT_S = 1.1
HORN_GAP_S = 0.55
HORN_TAIL_S = 3.0
LENS_LOOP_S = 4.0
LENS_TICKS_PER_S = 8


def _band_limited_saw(freq: np.ndarray, sr: int, max_hz: float) -> np.ndarray:
    """Sierra aditiva con la frecuencia variable `freq` (Hz por muestra)."""
    phase = 2.0 * np.pi * np.cumsum(freq) / sr
    top = float(np.max(freq))
    harmonics = max(1, int(max_hz // max(top, 1.0)))
    out = np.zeros_like(phase)
    for k in range(1, harmonics + 1):
        out += np.sin(k * phase) / k
    return out


def _horn_blast(rng: np.random.Generator, seconds: float) -> np.ndarray:
    n = int(seconds * SR)
    t = np.arange(n) / SR
    # Arranque de presión: el tono sube un cuarto de tono y se asienta.
    glide = 1.0 - 0.03 * np.exp(-t / 0.08)
    vibrato = 1.0 + 0.002 * sine_lfo(n, SR, rng.uniform(4.2, 5.0))
    freq = HORN_FUNDAMENTAL_HZ * glide * vibrato
    body = _band_limited_saw(freq, SR, 4200.0)
    fifth = _band_limited_saw(freq * 1.5, SR, 3000.0) * 0.18
    tone = body + fifth
    # Resonador de la bocina y algo de aire.
    tone = static_filter(tone, SR, 380.0, q=0.9, kind="bandpass") * 2.2 + static_filter(tone, SR, 900.0, q=0.6) * 0.5
    air = static_filter(pink_noise(n, rng), SR, 600.0, q=0.7, kind="bandpass") * 0.06
    env = fit_length(ar_envelope(SR, 0.18, 0.35, hold_s=max(0.0, seconds - 0.53), shape=1.4), n)
    return (tone + air) * env


def _outdoor_tail(dry: np.ndarray, rng: np.random.Generator) -> np.ndarray:
    """Distancia y espacio abierto: paso bajo, eco contra la costa y cola larga."""
    far = static_filter(dry, SR, 2400.0, q=0.6)
    out = far.copy()
    echo_at = int(0.42 * SR)
    out[echo_at:] += static_filter(far[: len(far) - echo_at], SR, 1200.0) * 0.32
    # Cola: ruido rosa con caída exponencial convolucionado con la señal (IR sintética).
    ir_len = int(2.6 * SR)
    ir = pink_noise(ir_len, rng) * np.exp(-np.arange(ir_len) / (0.75 * SR))
    ir = static_filter(ir, SR, 1800.0, q=0.6)
    ir /= max(1e-9, float(np.sqrt(np.sum(ir**2))))
    wet = signal.oaconvolve(far, ir)[: len(far)]
    wet *= 0.55 * float(np.sqrt(np.mean(far**2))) / max(1e-9, float(np.sqrt(np.mean(wet**2))))
    return out + wet


def ship_horn(name: str) -> np.ndarray:
    """Bocina del Mistral: un toque largo y uno corto, con la isla delante."""
    rng = rng_for(name)
    total = HORN_LONG_S + HORN_GAP_S + HORN_SHORT_S + HORN_TAIL_S
    out = np.zeros(int(total * SR))
    long_blast = _horn_blast(rng, HORN_LONG_S)
    out[: len(long_blast)] += long_blast
    start = int((HORN_LONG_S + HORN_GAP_S) * SR)
    short_blast = _horn_blast(rng, HORN_SHORT_S) * 0.9
    out[start : start + len(short_blast)] += short_blast
    out = _outdoor_tail(out, rng)
    return out / max(1e-9, float(np.max(np.abs(out)))) * 0.8


def lens_rotation(name: str) -> np.ndarray:
    """Bucle del mecanismo de relojería que gira la lente."""
    rng = rng_for(name)
    loop_len = int(LENS_LOOP_S * SR)
    fade_len = int(0.25 * SR)
    n = loop_len + fade_len
    t = np.arange(n) / SR
    out = np.zeros(n)
    period = SR // LENS_TICKS_PER_S
    for i, pos in enumerate(range(0, n, period)):
        freq = 2350.0 if i % 2 == 0 else 1980.0
        tick = modal_hit(SR, 0.06, freq, [1.0, 2.76, 5.4], [0.03, 0.015, 0.008], [1.0, 0.45, 0.2], rng=rng, detune=0.01)
        tick *= rng.uniform(0.75, 1.0) * (1.0 if i % 2 == 0 else 0.7)
        end = min(n, pos + len(tick))
        out[pos:end] += tick[: end - pos]
    out = static_filter(out, SR, 5200.0, q=0.7) * 0.35
    rumble = static_filter(brown_noise(n, rng), SR, 240.0, q=0.8) * (0.8 + 0.2 * np.sin(2.0 * np.pi * 0.5 * t))
    rumble /= max(1e-9, float(np.max(np.abs(rumble))))
    hum = np.sin(2.0 * np.pi * 120.0 * t) * 0.03 + np.sin(2.0 * np.pi * 240.0 * t) * 0.012
    mixed = out + rumble * 0.22 + hum
    looped = seamless_loop(mixed, loop_len, fade_len)
    return looped / max(1e-9, float(np.max(np.abs(looped)))) * 0.6
