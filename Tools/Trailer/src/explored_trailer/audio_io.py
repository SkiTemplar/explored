"""Lectura y escritura de audio y utilidades de forma: canales, bucles y fundidos.

Convención: los buffers son float64 con forma (muestras, canales). Todo es inmutable:
cada función devuelve un array nuevo.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
import soundfile as sf

SAMPLE_RATE = 48_000


def read_audio(path: str | Path) -> tuple[np.ndarray, int]:
    data, sample_rate = sf.read(str(path), dtype="float64", always_2d=True)
    return np.asarray(data), int(sample_rate)


def write_audio(path: str | Path, audio: np.ndarray, sample_rate: int = SAMPLE_RATE) -> Path:
    out = Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(out), audio, sample_rate, subtype="PCM_24")
    return out


def to_mono(audio: np.ndarray) -> np.ndarray:
    return audio.mean(axis=1) if audio.ndim == 2 else audio.copy()


def to_stereo(audio: np.ndarray) -> np.ndarray:
    if audio.ndim == 1:
        return np.stack([audio, audio], axis=1)
    if audio.shape[1] == 1:
        return np.repeat(audio, 2, axis=1)
    if audio.shape[1] == 2:
        return audio.copy()
    raise ValueError(f"solo se admiten 1 o 2 canales, no {audio.shape[1]}")


def seconds_to_samples(seconds: float, sample_rate: int = SAMPLE_RATE) -> int:
    return int(round(seconds * sample_rate))


def equal_power_curve(length: int, rising: bool) -> np.ndarray:
    """Curva seno/coseno de potencia constante: la suma de potencias de dos curvas cruzadas es 1."""
    if length <= 0:
        return np.zeros(0)
    phase = (np.arange(length) + 0.5) / length * (np.pi / 2.0)
    return np.sin(phase) if rising else np.cos(phase)


def fade(audio: np.ndarray, fade_in: int = 0, fade_out: int = 0) -> np.ndarray:
    """Fundidos de potencia constante en los extremos (en muestras)."""
    out = audio.copy()
    n = len(out)
    fade_in = min(max(fade_in, 0), n)
    fade_out = min(max(fade_out, 0), n)
    if fade_in:
        out[:fade_in] *= equal_power_curve(fade_in, rising=True)[:, None]
    if fade_out:
        out[n - fade_out :] *= equal_power_curve(fade_out, rising=False)[:, None]
    return out


def loop_to_length(audio: np.ndarray, length: int, crossfade: int, offset: int = 0) -> np.ndarray:
    """Repite un bucle hasta `length` muestras empalmando cada vuelta con un fundido cruzado.

    `offset` desplaza el punto de partida para que dos usos del mismo bucle no suenen
    en fase. El fundido cruzado de potencia constante evita clics aunque el bucle no
    fuese perfecto.
    """
    if len(audio) == 0:
        raise ValueError("bucle vacío")
    if length <= 0:
        return np.zeros((0, audio.shape[1]))
    source = np.roll(audio, -(offset % len(audio)), axis=0)
    crossfade = int(min(max(crossfade, 0), len(source) // 2))
    step = len(source) - crossfade
    out = np.zeros((length + len(source), audio.shape[1]))
    pos = 0
    first = True
    while pos < length:
        piece = source if first else fade(source, fade_in=crossfade)
        if crossfade:
            piece = fade(piece, fade_out=crossfade)
        out[pos : pos + len(piece)] += piece
        pos += step
        first = False
    return out[:length]


def place(
    canvas_length: int, clip: np.ndarray, start: int, gain: float = 1.0
) -> np.ndarray:
    """Devuelve un buffer de `canvas_length` muestras con `clip` colocado en `start` (recortado)."""
    out = np.zeros((canvas_length, clip.shape[1]))
    if start >= canvas_length or start + len(clip) <= 0:
        return out
    src_from = max(0, -start)
    dst_from = max(0, start)
    count = min(len(clip) - src_from, canvas_length - dst_from)
    out[dst_from : dst_from + count] = clip[src_from : src_from + count] * gain
    return out
