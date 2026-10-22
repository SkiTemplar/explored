"""Mezcla del tráiler: música como cama, ambientes por plano, efectos puntuales y bocina.

Cadena: cada fuente se normaliza a -16 LUFS → ganancias del plan (dB) → la música se
atenúa bajo la bocina → suma → fundidos de entrada (1 s) y salida (2 s) → ganancia a
-16 LUFS integrados → limitador de pico verdadero (-2 dBTP, margen sobre -1,5) → WAV de
24 bits a 48 kHz. La medida final la da `ffmpeg loudnorm`.
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from .audio_io import (
    SAMPLE_RATE,
    fade,
    loop_to_length,
    place,
    read_audio,
    seconds_to_samples,
    to_stereo,
    write_audio,
)
from .layers import HORN, Layer, build_layers
from .loudness import (
    db_to_linear,
    integrated_lufs,
    limiter_gain,
    sample_peaks,
    true_peak_db,
    window_lufs,
)
from .music import MusicCut, align_cadence, music_plan, render_music
from .timeline import Timeline

TARGET_LUFS = -16.0
CEILING_DBTP = -2.0
SOURCE_LUFS = -16.0
MASTER_FADE_IN = 1.0
MASTER_FADE_OUT = 2.0
HORN_MARGIN_DB = 6.0
HORN_DUCK_DB = 5.0
LOOP_CROSSFADE = 0.25

Loader = Callable[[str], np.ndarray]


@dataclass(frozen=True)
class MixResult:
    mix: np.ndarray
    music: np.ndarray
    horn: np.ndarray
    cuts: tuple[MusicCut, ...]
    horn_at: float
    horn_window: tuple[float, float]
    horn_margin_db: float
    master_gain_db: float
    own_lufs: float
    own_true_peak_db: float
    splices: tuple[float, ...] = field(default_factory=tuple)


def normalize(audio: np.ndarray, target_lufs: float = SOURCE_LUFS) -> np.ndarray:
    stereo = to_stereo(audio)
    level = integrated_lufs(stereo)
    if not np.isfinite(level):
        return stereo
    return stereo * db_to_linear(target_lufs - level)


def make_loader(audio_root: Path, music_root: Path) -> Loader:
    """Carga con caché: `mus_*` desde la carpeta de grabaciones, el resto relativo a Audio."""
    cache: dict[str, np.ndarray] = {}

    def load(key: str) -> np.ndarray:
        if key not in cache:
            path = music_root / f"{key}.wav" if key.startswith("mus_") else audio_root / key
            audio, rate = read_audio(path)
            if rate != SAMPLE_RATE:
                raise ValueError(f"{path} está a {rate} Hz; el tráiler trabaja a {SAMPLE_RATE}")
            cache[key] = to_stereo(audio) if key.startswith("mus_") else normalize(audio)
        return cache[key]

    return load


def gain_curve(points: tuple[tuple[float, float], ...], start: float, length: int) -> np.ndarray:
    """Automatización de ganancia lineal (interpola en dB, mantiene los extremos)."""
    times = start + np.arange(length) / SAMPLE_RATE
    xs = np.array([p[0] for p in points])
    ys = np.array([p[1] for p in points])
    return 10.0 ** (np.interp(times, xs, ys) / 20.0)


def render_layer(layer: Layer, source: np.ndarray, total: int) -> np.ndarray:
    start = seconds_to_samples(max(layer.start, 0.0))
    end = min(seconds_to_samples(layer.end), total)
    length = end - start
    if length <= 0:
        return np.zeros((total, 2))
    if layer.loop:
        clip = loop_to_length(source, length, seconds_to_samples(LOOP_CROSSFADE), seconds_to_samples(layer.offset))
    else:
        clip = source[:length]
    clip = clip * gain_curve(layer.gain_points, start / SAMPLE_RATE, len(clip))[:, None]
    clip = fade(clip, seconds_to_samples(layer.fade_in), seconds_to_samples(layer.fade_out))
    return place(total, clip, start)


def active_window(audio: np.ndarray, threshold_db: float = 10.0, block_s: float = 0.4) -> tuple[float, float]:
    """Tramo (s) en que la sonoridad por bloques está a menos de `threshold_db` del máximo."""
    block = seconds_to_samples(block_s)
    count = max(1, len(audio) // block)
    levels = np.array([window_lufs(audio, i * block, (i + 1) * block) for i in range(count)])
    loud = np.flatnonzero(levels >= np.max(levels) - threshold_db)
    return float(loud[0] * block_s), float((loud[-1] + 1) * block_s)


def duck_curve(total: int, start: float, end: float, depth_db: float, ramp: float = 0.5) -> np.ndarray:
    times = np.arange(total) / SAMPLE_RATE
    xs = [0.0, start - ramp, start, end, end + ramp, max(end + ramp + 1.0, times[-1] if total else 0.0)]
    ys = [0.0, 0.0, -depth_db, -depth_db, 0.0, 0.0]
    return 10.0 ** (np.interp(times, xs, ys) / 20.0)


def master_fades(total: int) -> np.ndarray:
    ones = np.ones((total, 1))
    return fade(ones, seconds_to_samples(MASTER_FADE_IN), seconds_to_samples(MASTER_FADE_OUT))[:, 0]


def build_mix(tl: Timeline, load: Loader, trim_db: float = 0.0) -> MixResult:
    """Mezcla completa; `trim_db` corrige la ganancia final tras medir con ffmpeg."""
    total = seconds_to_samples(tl.duration)
    sources = {key: load(key) for key in {c.piece_id for c in music_plan(tl)}}
    cuts = align_cadence(music_plan(tl), sources)
    music = render_music(cuts, sources, total)

    horn_src = load(HORN.relpath)
    rel_start, rel_end = active_window(horn_src)
    horn_at = tl.horn_time()
    window = (horn_at + rel_start, horn_at + rel_end)
    music = music * duck_curve(total, window[0], window[1], HORN_DUCK_DB)[:, None]

    ambience = np.zeros((total, 2))
    for layer in build_layers(tl):
        ambience = ambience + render_layer(layer, load(layer.relpath), total)

    w0, w1 = (seconds_to_samples(t) for t in window)
    w1 = min(w1, total)
    horn_level = window_lufs(horn_src, seconds_to_samples(rel_start), seconds_to_samples(rel_end))
    music_level = window_lufs(music, w0, w1)
    horn_gain = music_level + HORN_MARGIN_DB - horn_level
    horn = render_layer(Layer(HORN.folder, HORN.name, horn_at, tl.duration, ((horn_at, horn_gain),),
                              HORN.fade_in, HORN.fade_out, loop=False), horn_src, total)

    fades = master_fades(total)[:, None]
    premix = (music + ambience + horn) * fades
    master_gain = TARGET_LUFS - integrated_lufs(premix) + trim_db
    scaled = premix * db_to_linear(master_gain)
    limiter = limiter_gain(sample_peaks(scaled), CEILING_DBTP)[:, None]
    limited = scaled * limiter
    gain = fades * db_to_linear(master_gain) * limiter
    margin = window_lufs(horn * gain, w0, w1) - window_lufs(music * gain, w0, w1)
    splices = tuple(sorted({round(c.dst_start, 3) for c in cuts} | {round(c.dst_end, 3) for c in cuts}))
    return MixResult(limited, music * gain, horn * gain, cuts, horn_at, window, margin, master_gain,
                     integrated_lufs(limited), true_peak_db(limited), splices)


def discontinuity_ratio(audio: np.ndarray, at: float, half_window_s: float = 0.002, context_s: float = 0.1) -> float:
    """Salto máximo entre muestras junto a `at` dividido por el percentil 99,9 de su entorno.

    Un empalme sin clic da ~1 o menos; un escalón seco se dispara por encima de 3.
    """
    center = seconds_to_samples(at)
    near = seconds_to_samples(half_window_s)
    ctx = seconds_to_samples(context_s)
    lo, hi = max(1, center - ctx), min(len(audio), center + ctx)
    if hi - lo < 4:
        return 0.0
    diffs = np.max(np.abs(np.diff(audio[lo - 1 : hi], axis=0)), axis=1)
    local = diffs[max(0, center - near - lo) : center + near - lo]
    reference = float(np.percentile(diffs, 99.9))
    return float(np.max(local)) / reference if reference > 1e-12 and local.size else 0.0


def write_mix(result: MixResult, out_path: Path) -> Path:
    return write_audio(out_path, result.mix)
