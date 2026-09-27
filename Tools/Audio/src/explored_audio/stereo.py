"""Utilidades estereo: decorrelacion (da anchura a un colchon mono sin
cancelaciones de fase) y panoramizacion por potencia constante para colocar
eventos puntuales dentro de un colchon estereo."""

from __future__ import annotations

import numpy as np
from scipy import signal


# Por debajo de este corte el colchon se deja identico en los dos canales.
MONO_BASS_HZ = 200.0
INFRASOUND_HZ = 20.0


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

    b, a = signal.butter(2, MONO_BASS_HZ, btype="lowpass", fs=sr)
    bass = signal.lfilter(b, a, mono)
    upper = mono - bass
    # Infrasonidos fuera: no se oyen y se comian el margen de pico (el
    # ruido marron de `amb_underwater` tenia el centroide en 7 Hz).
    b_hp, a_hp = signal.butter(2, INFRASOUND_HZ, btype="highpass", fs=sr)
    bass = signal.lfilter(b_hp, a_hp, bass)
    # Los graves conservan la ganancia media que les daban los FIR (el nivel
    # de cada generador se ajusto con ella), pero con el mismo signo.
    bass = bass * 0.5 * (abs(np.sum(fir_l)) + abs(np.sum(fir_r)))

    left = np.convolve(upper, fir_l, mode="same")
    right = np.convolve(upper, fir_r, mode="same")

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
    end = min(position + stereo_event.shape[-1], out.shape[-1])
    length = end - position
    if length > 0:
        out[:, position:end] += stereo_event[:, :length]
    return out
