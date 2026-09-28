"""Utilidades estereo: decorrelacion (da anchura a un colchon mono sin
cancelaciones de fase) y panoramizacion por potencia constante para colocar
eventos puntuales dentro de un colchon estereo."""

from __future__ import annotations

from typing import cast

import numpy as np
from scipy import signal

# Por debajo de este corte el colchon se deja identico en los dos canales.
MONO_BASS_HZ = 200.0
INFRASOUND_HZ = 20.0


def _convolve_same(x: np.ndarray, h: np.ndarray) -> np.ndarray:
    """Convolucion centrada con la longitud de `x`.

    `np.convolve(..., mode="same")` devuelve `max(len(x), len(h))` muestras:
    con una señal mas corta que el FIR salia mas larga que los graves y no se
    podian sumar. En el caso normal (señal mas larga) se usa tal cual, para
    no cambiar ni un bit la salida ya exportada."""
    if len(x) >= len(h):
        return np.convolve(x, h, mode="same")
    if len(x) == 0:
        return np.zeros(0)
    start = (len(h) - 1) // 2
    return np.convolve(x, h, mode="full")[start : start + len(x)]


def decorrelate(mono: np.ndarray, rng: np.random.Generator, sr: int, spread_ms: float = 12.0) -> np.ndarray:
    """Par estereo a partir de un mono: un FIR corto y aleatorio distinto por
    canal (decorrelacion espectral) mas un pequeño desfase tipo Haas.

    Solo se decorrelan los medios y agudos. Un FIR aleatorio puede tener
    ganancia en continua de signo contrario en cada canal: los graves
    quedaban en contrafase (L/R -0.9 en `amb_underwater`) y se cancelaban
    al sumar a mono. Los graves van en fase y sin desfase en ambos canales.
    """
    fir_len = 21
    fir_l = rng.normal(0, 1, fir_len)
    fir_l /= np.sum(np.abs(fir_l))
    fir_r = rng.normal(0, 1, fir_len)
    fir_r /= np.sum(np.abs(fir_r))

    # `butter` con la salida por defecto ("ba") devuelve siempre (b, a); los
    # stubs anuncian tambien la variante "zpk"/"sos" o None. Se conserva "ba"
    # (y no "sos") para no alterar ni un bit la salida ya exportada.
    b, a = cast(tuple[np.ndarray, np.ndarray], signal.butter(2, MONO_BASS_HZ, btype="lowpass", fs=sr))
    bass = np.asarray(signal.lfilter(b, a, mono))
    upper = mono - bass
    # Infrasonidos fuera: no se oyen y se comian el margen de pico (el
    # ruido marron de `amb_underwater` tenia el centroide en 7 Hz).
    b_hp, a_hp = cast(tuple[np.ndarray, np.ndarray], signal.butter(2, INFRASOUND_HZ, btype="highpass", fs=sr))
    bass = np.asarray(signal.lfilter(b_hp, a_hp, bass))
    # Los graves conservan la ganancia media que les daban los FIR (el nivel
    # de cada generador se ajusto con ella), pero con el mismo signo.
    bass = bass * 0.5 * (abs(np.sum(fir_l)) + abs(np.sum(fir_r)))

    left = _convolve_same(upper, fir_l)
    right = _convolve_same(upper, fir_r)

    delay_l = int(rng.uniform(0, spread_ms) * sr / 1000.0)
    delay_r = int(rng.uniform(0, spread_ms) * sr / 1000.0)
    left = np.roll(left, delay_l) + bass
    right = np.roll(right, delay_r) + bass
    return np.stack([left, right])


def pan_constant_power(mono: np.ndarray, pan: float) -> np.ndarray:
    """`pan` en [-1, 1]: -1 = izquierda, 0 = centro, 1 = derecha."""
    pan = float(np.clip(pan, -1.0, 1.0))
    angle = (pan + 1.0) * np.pi / 4.0
    return np.stack([mono * np.cos(angle), mono * np.sin(angle)])


def mix_event_into_bed(bed: np.ndarray, mono_event: np.ndarray, position: int, pan: float) -> np.ndarray:
    """Mezcla (suma) un evento mono panoramizado dentro de un colchon estereo."""
    out = bed.copy()
    stereo_event = pan_constant_power(mono_event, pan)
    # Un evento que empieza antes del colchon solo aporta su parte final (un
    # `position` negativo indexaba desde el final del buffer y fallaba).
    skip = max(-position, 0)
    start = position + skip
    end = min(position + stereo_event.shape[-1], out.shape[-1])
    length = end - start
    if length > 0:
        out[:, start:end] += stereo_event[:, skip : skip + length]
    return out
