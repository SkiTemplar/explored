"""Montaje musical: tres piezas de la banda sonora encadenadas por frases.

Puntos de corte medidos con `explored-trailer analyze` (envolvente RMS y novedad espectral)
sobre los WAV tratados de `Art/Export/Audio/Grabada` (`explored-music`):

- Gymnopédie n.º 1 (Satie, CC0): la exposición de piano, de 0 a 22,5 s. La frase cierra en la
  nota de 21,0 s y su caída acaba antes del ataque de 22,6 s; el fundido termina ahí.
- Sonata para violonchelo y piano op. 65, III. Largo (Chopin, CC0): desde la primera entrada
  (1,2 s) el violonchelo crece hasta la cresta de 27-29,6 s y respira en 30-31 s. Se ancla para
  que esa respiración caiga justo cuando se enciende el faro.
- Canon in D Major (MacLeod, CC BY 4.0): bajo de ocho blancas (Re La Si Fa# Sol Re Sol La) a
  80 negras/min, ciclo de 12 s con fase 0,12 s. El tramo de clímax empieza en el Sol de la
  segunda mitad de un ciclo con la textura completa (270,12 s) y, 12 s después y en la misma
  fase, salta a la cadencia final (330,12 s: Sol Re Sol La → Re), que cae en el título.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from .audio_io import SAMPLE_RATE, fade, place, seconds_to_samples, to_mono
from .loudness import db_to_linear
from .timeline import Timeline

GYMNOPEDIE = "mus_rec_gymnopedie_1"
LARGO = "mus_rec_cello_sonata_largo"
CANON = "mus_rec_macleod_canon_in_d"

GYMNOPEDIE_END = 22.5
GYMNOPEDIE_FADE = 1.7
LARGO_FIRST = 0.9
LARGO_BREATH = 29.8
LARGO_END = 31.2
CANON_CLIMAX = 270.12
CANON_CYCLE = 12.0
CANON_CADENCE = 330.12
CANON_TAIL_END = 341.6
SPLICE = 0.3
ALIGN_SEARCH = 0.025


@dataclass(frozen=True)
class MusicCut:
    piece_id: str
    src_start: float
    src_end: float
    dst_start: float
    fade_in: float
    fade_out: float
    gain_db: float = 0.0
    note: str = ""

    @property
    def dst_end(self) -> float:
        return self.dst_start + (self.src_end - self.src_start)


def music_plan(timeline: Timeline) -> tuple[MusicCut, ...]:
    """Cortes de música anclados al plano 7 (encendido): todo lo demás cuelga de ahí."""
    entry = timeline.shot(7).start
    largo_offset = entry - LARGO_BREATH
    jump = entry + CANON_CYCLE
    return (
        MusicCut(GYMNOPEDIE, 0.0, GYMNOPEDIE_END, 0.0, 0.02, GYMNOPEDIE_FADE, 0.0, "piano solo: mar y cala"),
        MusicCut(LARGO, LARGO_FIRST, LARGO_END, LARGO_FIRST + largo_offset, 0.25, LARGO_END - LARGO_BREATH, -1.0,
                 "violonchelo que crece: faro, ciclón y carguero"),
        MusicCut(CANON, CANON_CLIMAX - SPLICE, CANON_CLIMAX + CANON_CYCLE + SPLICE, entry - SPLICE, SPLICE,
                 2 * SPLICE, 0.5, "clímax: se enciende la llama"),
        MusicCut(CANON, CANON_CADENCE - SPLICE, CANON_TAIL_END, jump - SPLICE, 2 * SPLICE, 0.4, 0.5,
                 "cadencia final: Mistral, bocina y título"),
    )


def best_alignment(a: np.ndarray, b: np.ndarray, max_lag: int) -> int:
    """Retardo (muestras) a sumar al inicio de `b` para que case en fase con `a` (correlación)."""
    if max_lag <= 0 or len(a) == 0 or len(b) <= 2 * max_lag:
        return 0
    core = b[max_lag : len(b) - max_lag]
    best_lag, best_score = 0, -np.inf
    usable = min(len(a), len(core))
    for lag in range(-max_lag, max_lag + 1, 8):
        seg = b[max_lag + lag : max_lag + lag + usable]
        score = float(np.dot(a[:usable], seg))
        if score > best_score:
            best_lag, best_score = lag, score
    return best_lag


def align_cadence(cuts: tuple[MusicCut, ...], sources: dict[str, np.ndarray]) -> tuple[MusicCut, ...]:
    """Afina el salto del canon a la cadencia (±25 ms) para que el fundido cruzado no desfase."""
    canon = sources.get(CANON)
    if canon is None:
        return cuts
    mono = to_mono(canon)
    window = seconds_to_samples(0.5)
    search = seconds_to_samples(ALIGN_SEARCH)
    at_a = seconds_to_samples(CANON_CLIMAX + CANON_CYCLE)
    at_b = seconds_to_samples(CANON_CADENCE)
    lag = best_alignment(mono[at_a - window : at_a + window],
                         mono[at_b - window - search : at_b + window + search], search)
    shift = lag / SAMPLE_RATE
    return tuple(
        MusicCut(c.piece_id, c.src_start + shift, c.src_end, c.dst_start, c.fade_in, c.fade_out, c.gain_db, c.note)
        if c.src_start > CANON_CADENCE - 1.0 else c
        for c in cuts
    )


def render_cut(cut: MusicCut, source: np.ndarray, length: int) -> np.ndarray:
    first = seconds_to_samples(cut.src_start)
    last = min(seconds_to_samples(cut.src_end), len(source))
    clip = source[first:last]
    clip = fade(clip, seconds_to_samples(cut.fade_in), seconds_to_samples(cut.fade_out))
    return place(length, clip, seconds_to_samples(cut.dst_start), db_to_linear(cut.gain_db))


def render_music(cuts: tuple[MusicCut, ...], sources: dict[str, np.ndarray], length: int) -> np.ndarray:
    out = np.zeros((length, 2))
    for cut in cuts:
        out = out + render_cut(cut, sources[cut.piece_id], length)
    return out
