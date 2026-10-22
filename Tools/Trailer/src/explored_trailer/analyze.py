"""Análisis ligero de una pieza para elegir puntos de corte por frases.

Devuelve la envolvente de sonoridad (RMS en dB por ventana) y la novedad espectral
(spectral flux) para localizar ataques: un corte o un fundido cruzado se coloca justo
antes de un ataque fuerte y en un valle de energía, nunca en mitad de una nota.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from .audio_io import read_audio, to_mono


@dataclass(frozen=True)
class Envelope:
    hop_seconds: float
    rms_db: np.ndarray
    flux: np.ndarray

    def time_of(self, index: int) -> float:
        return index * self.hop_seconds


def rms_envelope(mono: np.ndarray, sample_rate: int, hop_seconds: float = 0.5) -> np.ndarray:
    hop = max(1, int(round(hop_seconds * sample_rate)))
    count = len(mono) // hop
    if count == 0:
        return np.zeros(0)
    frames = mono[: count * hop].reshape(count, hop)
    rms = np.sqrt(np.mean(frames**2, axis=1))
    return 20.0 * np.log10(np.maximum(rms, 1e-9))


def spectral_flux(mono: np.ndarray, sample_rate: int, hop_seconds: float = 0.5) -> np.ndarray:
    """Suma de los incrementos positivos del espectro de magnitud entre ventanas."""
    hop = max(1, int(round(hop_seconds * sample_rate)))
    count = len(mono) // hop
    if count < 2:
        return np.zeros(count)
    frames = mono[: count * hop].reshape(count, hop) * np.hanning(hop)
    mags = np.log1p(np.abs(np.fft.rfft(frames, axis=1)))
    diff = np.diff(mags, axis=0)
    flux = np.concatenate([[0.0], np.sum(np.maximum(diff, 0.0), axis=1)])
    peak = float(np.max(flux))
    return flux / peak if peak > 0 else flux


def analyze(path: str, hop_seconds: float = 0.5) -> Envelope:
    audio, sample_rate = read_audio(path)
    mono = to_mono(audio)
    return Envelope(hop_seconds, rms_envelope(mono, sample_rate, hop_seconds), spectral_flux(mono, sample_rate, hop_seconds))


def format_envelope(env: Envelope, start: float = 0.0, end: float | None = None) -> str:
    """Tabla de texto: tiempo, RMS en dB y una barra con la novedad espectral."""
    lines = []
    for i, (level, novelty) in enumerate(zip(env.rms_db, env.flux, strict=True)):
        t = env.time_of(i)
        if t < start or (end is not None and t > end):
            continue
        bar = "#" * int(round(novelty * 30))
        lines.append(f"{t:7.1f}s {level:6.1f} dB {bar}")
    return "\n".join(lines)
