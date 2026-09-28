"""Tratamiento de una grabacion: remuestreo, recorte de silencios,
normalizacion a la sonoridad objetivo y limitador de picos.

Todo es determinista (sin aleatoriedad ni dependencia del reloj): la misma
entrada da exactamente las mismas muestras, y por tanto el mismo OGG.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from fractions import Fraction

import numpy as np
from scipy import ndimage, signal

from ..constants import SAMPLE_RATE
from .loudness import integrated_lufs, sample_peak_dbfs

# Recorte de silencios: RMS en ventanas de 50 ms por debajo de este umbral
# cuenta como silencio. -60 dBFS deja pasar la cola de reverberacion util
# de un piano y quita el soplido de sala y los clics de principio y final.
SILENCE_THRESHOLD_DBFS = -60.0
SILENCE_WINDOW_S = 0.05
# Margen que se conserva a cada lado del sonido detectado.
KEEP_PAD_S = 0.15
FADE_IN_S = 0.02
FADE_OUT_S = 0.6

# Limitador: ventana de anticipacion y suavizado de la ganancia.
LIMITER_LOOKAHEAD_S = 0.005
LIMITER_SMOOTH_S = 0.02
# Ganancia maxima que se aplica para subir una grabacion floja.
MAX_GAIN_DB = 24.0
# Iteraciones de ajuste fino tras limitar (limitar baja un poco la sonoridad).
MAX_PASSES = 6


@dataclass(frozen=True)
class ProcessReport:
    input_seconds: float
    output_seconds: float
    trimmed_head_s: float
    trimmed_tail_s: float
    lufs_in: float
    lufs_out: float
    peak_out_dbfs: float
    gain_db: float
    limited_db: float


def to_stereo(x: np.ndarray) -> np.ndarray:
    """(muestras,) o (muestras, canales) -> (muestras, 2) float64."""
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 1:
        return np.stack([x, x], axis=1)
    if x.shape[1] == 1:
        return np.repeat(x, 2, axis=1)
    if x.shape[1] == 2:
        return x
    # Mas de dos canales: mezcla simple a estereo (no se espera en la lista).
    left = x[:, 0::2].mean(axis=1)
    right = x[:, 1::2].mean(axis=1)
    return np.stack([left, right], axis=1)


def resample(x: np.ndarray, fs_in: int, fs_out: int = SAMPLE_RATE) -> np.ndarray:
    if fs_in == fs_out:
        return x
    ratio = Fraction(fs_out, fs_in).limit_denominator(1000)
    return signal.resample_poly(x, ratio.numerator, ratio.denominator, axis=0)


def find_sound_bounds(x: np.ndarray, fs: int, threshold_dbfs: float = SILENCE_THRESHOLD_DBFS) -> tuple[int, int]:
    """Indices [inicio, fin) del tramo con sonido. Devuelve (0, 0) si todo
    es silencio."""
    mono = np.max(np.abs(x), axis=1) if x.ndim == 2 else np.abs(x)
    win = max(1, int(round(SILENCE_WINDOW_S * fs)))
    power = ndimage.uniform_filter1d(mono * mono, size=win, mode="constant")
    threshold = 10.0 ** (threshold_dbfs / 10.0)  # umbral en potencia
    loud = np.flatnonzero(power > threshold)
    if loud.size == 0:
        return 0, 0
    pad = int(round(KEEP_PAD_S * fs))
    start = max(0, int(loud[0]) - win // 2 - pad)
    end = min(len(mono), int(loud[-1]) + win // 2 + pad + 1)
    return start, end


def trim_silence(x: np.ndarray, fs: int) -> tuple[np.ndarray, int, int]:
    start, end = find_sound_bounds(x, fs)
    if end <= start:
        raise ValueError("la grabacion es silencio de principio a fin")
    y = x[start:end].copy()
    n = len(y)
    fade_in = min(n // 4, int(round(FADE_IN_S * fs)))
    fade_out = min(n // 4, int(round(FADE_OUT_S * fs)))
    if fade_in > 0:
        y[:fade_in] *= np.linspace(0.0, 1.0, fade_in, endpoint=False)[:, np.newaxis]
    if fade_out > 0:
        # Coseno: cae suave al final, sin el codo audible de una rampa lineal.
        ramp = 0.5 * (1.0 + np.cos(np.linspace(0.0, math.pi, fade_out)))
        y[n - fade_out :] *= ramp[:, np.newaxis]
    return y, start, len(x) - end


def limit_peaks(x: np.ndarray, fs: int, ceiling_dbfs: float) -> np.ndarray:
    """Limitador con anticipacion: la ganancia nunca supera la necesaria en
    ninguna muestra (garantizado por construccion) y cambia de forma suave.

    Cada muestra pide una ganancia g = min(1, techo/|x|). Un filtro de minimo
    de ancho 2L+1 seguido de una media movil de ancho L+1 (ambos centrados)
    da una curva suave que, en cada muestra, promedia solo valores <= g de
    esa muestra, asi que nunca deja pasar un pico por encima del techo."""
    ceiling = 10.0 ** (ceiling_dbfs / 20.0)
    level = np.max(np.abs(x), axis=1) if x.ndim == 2 else np.abs(x)
    need = np.minimum(1.0, ceiling / np.maximum(level, 1e-12))
    if np.all(need >= 1.0):
        return x
    half = max(1, int(round(max(LIMITER_LOOKAHEAD_S, LIMITER_SMOOTH_S) * fs)))
    g = ndimage.minimum_filter1d(need, size=2 * half + 1, mode="nearest")
    g = ndimage.uniform_filter1d(g, size=half + 1, mode="nearest")
    g = np.minimum(g, need)  # red de seguridad frente a redondeo
    return x * (g[:, np.newaxis] if x.ndim == 2 else g)


def normalize(
    x: np.ndarray, fs: int, target_lufs: float, ceiling_dbfs: float, tolerance: float = 0.1
) -> tuple[np.ndarray, float, float]:
    """Lleva `x` a `target_lufs` sin pasar de `ceiling_dbfs` de pico.

    Devuelve (audio, ganancia total en dB, reduccion maxima del limitador en
    dB). Tras limitar, la sonoridad baja un poco; se reajusta la ganancia
    sobre el original unas pocas veces hasta quedar a `tolerance` LU."""
    lufs = integrated_lufs(x, fs)
    if not math.isfinite(lufs):
        raise ValueError("no se puede normalizar: la grabacion no supera la puerta absoluta")
    gain_db = float(np.clip(target_lufs - lufs, -MAX_GAIN_DB, MAX_GAIN_DB))
    y = x
    for _ in range(MAX_PASSES):
        boosted = x * 10.0 ** (gain_db / 20.0)
        y = limit_peaks(boosted, fs, ceiling_dbfs)
        err = target_lufs - integrated_lufs(y, fs)
        if abs(err) <= tolerance:
            break
        new_gain = float(np.clip(gain_db + err, -MAX_GAIN_DB, MAX_GAIN_DB))
        if new_gain == gain_db:
            break
        gain_db = new_gain
    boosted_peak = sample_peak_dbfs(x) + gain_db
    limited_db = max(0.0, boosted_peak - sample_peak_dbfs(y))
    return y, gain_db, limited_db


def process(
    x: np.ndarray, fs_in: int, target_lufs: float, ceiling_dbfs: float
) -> tuple[np.ndarray, ProcessReport]:
    """Cadena completa. Entrada tal como la da soundfile; salida estereo a
    48 kHz en float64, lista para escribir."""
    x = np.asarray(x, dtype=np.float64)
    if x.size == 0:
        raise ValueError("la grabacion esta vacia")
    if not np.all(np.isfinite(x)):
        raise ValueError("la grabacion contiene NaN o infinitos")
    stereo = to_stereo(x)
    input_seconds = len(stereo) / fs_in
    y = resample(stereo, fs_in, SAMPLE_RATE)
    y = y - y.mean(axis=0, keepdims=True)
    y, head, tail = trim_silence(y, SAMPLE_RATE)
    lufs_in = integrated_lufs(y, SAMPLE_RATE)
    y, gain_db, limited_db = normalize(y, SAMPLE_RATE, target_lufs, ceiling_dbfs)
    report = ProcessReport(
        input_seconds=input_seconds,
        output_seconds=len(y) / SAMPLE_RATE,
        trimmed_head_s=head / SAMPLE_RATE,
        trimmed_tail_s=tail / SAMPLE_RATE,
        lufs_in=lufs_in,
        lufs_out=integrated_lufs(y, SAMPLE_RATE),
        peak_out_dbfs=sample_peak_dbfs(y),
        gain_db=gain_db,
        limited_db=limited_db,
    )
    return y, report
