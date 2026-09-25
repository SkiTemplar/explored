"""Utilidades estereo: decorrelacion (da anchura a un colchon mono sin
cancelaciones de fase) y panoramizacion por potencia constante para colocar
eventos puntuales dentro de un colchon estereo."""

from __future__ import annotations

import numpy as np


def decorrelate(mono: np.ndarray, rng: np.random.Generator, sr: int, spread_ms: float = 12.0) -> np.ndarray:
    """Par estereo a partir de un mono: un FIR corto y aleatorio distinto por
    canal (decorrelacion espectral) mas un pequeño desfase tipo Haas."""
    fir_len = 21
    fir_l = rng.normal(0, 1, fir_len)
    fir_l /= np.sum(np.abs(fir_l))
    fir_r = rng.normal(0, 1, fir_len)
    fir_r /= np.sum(np.abs(fir_r))

    left = np.convolve(mono, fir_l, mode="same")
    right = np.convolve(mono, fir_r, mode="same")

    delay_l = int(rng.uniform(0, spread_ms) * sr / 1000.0)
    delay_r = int(rng.uniform(0, spread_ms) * sr / 1000.0)
    left = np.roll(left, delay_l)
    right = np.roll(right, delay_r)
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
