"""Sonoridad (ITU-R BS.1770-4), pico verdadero, limitador y medida de referencia con ffmpeg.

La medida propia (K-weighting + puertas) sirve para equilibrar capas durante la mezcla;
la cifra que se publica en el informe es la de `ffmpeg -af loudnorm` en modo análisis.
"""

from __future__ import annotations

import json
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from scipy import ndimage, signal

from .audio_io import SAMPLE_RATE

# Coeficientes del K-weighting a 48 kHz (BS.1770-4, tabla 1 y 2).
_SHELF_B = np.array([1.53512485958697, -2.69169618940638, 1.19839281085285])
_SHELF_A = np.array([1.0, -1.69065929318241, 0.73248077421585])
_HPF_B = np.array([1.0, -2.0, 1.0])
_HPF_A = np.array([1.0, -1.99004745483398, 0.99007225036621])

ABSOLUTE_GATE_LUFS = -70.0
RELATIVE_GATE_LU = -10.0
OVERSAMPLE = 4


def db_to_linear(db: float) -> float:
    return float(10.0 ** (db / 20.0))


def linear_to_db(value: float) -> float:
    return float(20.0 * np.log10(max(value, 1e-12)))


def k_weight(audio: np.ndarray, sample_rate: int = SAMPLE_RATE) -> np.ndarray:
    if sample_rate != SAMPLE_RATE:
        raise ValueError(f"el K-weighting está calculado para {SAMPLE_RATE} Hz, no {sample_rate}")
    stage = np.asarray(signal.lfilter(_SHELF_B, _SHELF_A, audio, axis=0))
    return np.asarray(signal.lfilter(_HPF_B, _HPF_A, stage, axis=0))


def _block_powers(weighted: np.ndarray, sample_rate: int, block_s: float = 0.4, overlap: float = 0.75) -> np.ndarray:
    block = int(round(block_s * sample_rate))
    step = max(1, int(round(block * (1.0 - overlap))))
    if len(weighted) < block:
        return np.zeros(0)
    squares = np.sum(weighted**2, axis=1)
    cumulative = np.concatenate([[0.0], np.cumsum(squares)])
    starts = np.arange(0, len(weighted) - block + 1, step)
    return (cumulative[starts + block] - cumulative[starts]) / block


def _power_to_lufs(power: float) -> float:
    return -0.691 + 10.0 * float(np.log10(max(power, 1e-20)))


def integrated_lufs(audio: np.ndarray, sample_rate: int = SAMPLE_RATE) -> float:
    """Sonoridad integrada con puerta absoluta (-70 LUFS) y relativa (-10 LU)."""
    powers = _block_powers(k_weight(audio, sample_rate), sample_rate)
    if powers.size == 0:
        return -np.inf
    loud = np.array([_power_to_lufs(p) for p in powers])
    gated = powers[loud > ABSOLUTE_GATE_LUFS]
    if gated.size == 0:
        return -np.inf
    relative = _power_to_lufs(float(np.mean(gated))) + RELATIVE_GATE_LU
    final = powers[(loud > ABSOLUTE_GATE_LUFS) & (loud > relative)]
    return _power_to_lufs(float(np.mean(final))) if final.size else -np.inf


def window_lufs(audio: np.ndarray, start: int, end: int, sample_rate: int = SAMPLE_RATE) -> float:
    """Sonoridad K-weighted sin puertas en [start, end) (muestras), con el filtro asentado antes."""
    lead = min(start, sample_rate // 2)
    weighted = k_weight(audio[start - lead : end], sample_rate)[lead:]
    if weighted.size == 0:
        return -np.inf
    return _power_to_lufs(float(np.mean(np.sum(weighted**2, axis=1))))


def sample_peaks(audio: np.ndarray, oversample: int = OVERSAMPLE) -> np.ndarray:
    """Pico por muestra original estimado con sobremuestreo (aproximación al pico verdadero)."""
    if oversample <= 1:
        return np.max(np.abs(audio), axis=1)
    up = signal.resample_poly(audio, oversample, 1, axis=0)
    up = np.abs(up[: len(audio) * oversample]).reshape(len(audio), oversample, audio.shape[1])
    return np.max(up, axis=(1, 2))


def true_peak_db(audio: np.ndarray) -> float:
    return linear_to_db(float(np.max(sample_peaks(audio)))) if audio.size else -np.inf


def limiter_gain(peaks: np.ndarray, ceiling_db: float, ramp_s: float = 0.02,
                 sample_rate: int = SAMPLE_RATE) -> np.ndarray:
    """Curva de ganancia del limitador con anticipación (una muestra por muestra de audio).

    La ganancia pedida por muestra se extiende con un filtro de mínimos de ±W y después se
    promedia en una ventana de W+1: cualquier ventana de media que contenga un pico cae
    entera dentro de su zona de mínimos, así que en el pico la ganancia aplicada nunca supera
    la pedida (sin sobrepasos), y la curva solo cambia en rampas lineales de W (sin clics).
    """
    required = np.minimum(1.0, db_to_linear(ceiling_db) / np.maximum(peaks, 1e-12))
    if float(np.min(required)) >= 1.0:
        return np.ones_like(required)
    width = max(1, int(round(ramp_s * sample_rate)))
    held = ndimage.minimum_filter1d(required, size=2 * width + 1, mode="nearest")
    smooth = ndimage.uniform_filter1d(held, size=width + 1, mode="nearest")
    return np.minimum(smooth, 1.0)


def limit_true_peak(audio: np.ndarray, ceiling_db: float, ramp_s: float = 0.02,
                    sample_rate: int = SAMPLE_RATE) -> np.ndarray:
    gain = limiter_gain(sample_peaks(audio), ceiling_db, ramp_s, sample_rate)
    return audio * gain[:, None]


@dataclass(frozen=True)
class LoudnormReport:
    integrated_lufs: float
    true_peak_dbtp: float
    lra: float
    threshold: float


_JSON_BLOCK = re.compile(r"\{[^{}]*\"input_i\"[^{}]*\}", re.S)


def parse_loudnorm(stderr: str) -> LoudnormReport:
    match = _JSON_BLOCK.search(stderr)
    if not match:
        raise ValueError("ffmpeg loudnorm no devolvió el bloque JSON de análisis")
    data = json.loads(match.group(0))
    return LoudnormReport(
        integrated_lufs=float(data["input_i"]),
        true_peak_dbtp=float(data["input_tp"]),
        lra=float(data["input_lra"]),
        threshold=float(data["input_thresh"]),
    )


def ffmpeg_loudnorm(path: str | Path, stream: str = "a:0") -> LoudnormReport:
    """Mide con `loudnorm` en modo análisis (print_format=json); no escribe nada."""
    cmd = [
        "ffmpeg", "-hide_banner", "-nostats", "-i", str(path), "-map", f"0:{stream}",
        "-af", "loudnorm=I=-16:TP=-1.5:LRA=11:print_format=json", "-f", "null", "-",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", check=False)
    if result.returncode != 0:
        raise RuntimeError(f"ffmpeg loudnorm falló ({result.returncode}): {result.stderr[-400:]}")
    return parse_loudnorm(result.stderr)
