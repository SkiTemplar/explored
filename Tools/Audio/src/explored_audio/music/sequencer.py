"""Secuenciador minimo: coloca notas (o frases, para la flauta con
portamento) en una rejilla de compases, humaniza cada una, las mezcla en un
bus estereo y aplica la reverberacion final.

Una `Track` es un instrumento mas su lista de eventos. Un evento normal es
`(start_beat, duration_beats, freq_o_acorde, velocidad, pan)`; el instrumento
"flute" usa en su lugar una lista de notas que se renderizan TODAS JUNTAS
como un unico contorno de tono con portamento (no tiene sentido generar cada
nota de una frase de flauta por separado y luego pegarlas: el propio
deslizamiento entre notas es el timbre)."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable

import numpy as np

from ..reverb import schroeder_reverb
from ..stereo import pan_constant_power
from . import instruments as ins
from .humanize import humanize_timing_s, humanize_velocity, micro_detune_ratio

NoteEvent = tuple[float, float, "float | list[float]", float, float]  # start, dur, freq(s), vel, pan


@dataclass
class Track:
    instrument: str
    events: list[NoteEvent] = field(default_factory=list)
    humanize_timing_s: float = 0.018
    humanize_velocity_amt: float = 0.15
    detune_cents: float = 5.0


_SINGLE_FREQ_INSTRUMENTS: dict[str, Callable] = {
    "marimba": ins.marimba,
    "kalimba": ins.kalimba,
    "bass": ins.bass,
    "wood_block": ins.wood_block,
    "guitar": ins.karplus_strong_pluck,
}
_CHORD_INSTRUMENTS: dict[str, Callable] = {
    "pad": lambda freqs, dur, vel, sr, rng: ins.strings_pad(freqs, dur, vel, sr, rng, voices=4, attack_s=1.3, release_s=1.6, max_harmonics=14),
    "strings": lambda freqs, dur, vel, sr, rng: ins.strings_pad(freqs, dur, vel, sr, rng, voices=5, attack_s=0.9, release_s=1.2, max_harmonics=18),
    "tremolo_strings": lambda freqs, dur, vel, sr, rng: ins.tremolo_strings(freqs, dur, vel, sr, rng),
}
_NO_PITCH_INSTRUMENTS: dict[str, Callable] = {
    "shaker": lambda dur, vel, sr, rng: ins.shaker(dur, vel, sr, rng),
    "kick": lambda dur, vel, sr, rng: ins.soft_kick(dur, vel, sr, rng),
}

TAIL_FADE_S = 0.02


def _apply_tail_fade(x: np.ndarray, sr: int, fade_s: float = TAIL_FADE_S) -> np.ndarray:
    """Fundido de salida corto y universal: evita cualquier clic al recortar
    una nota a la duracion de su hueco en la rejilla, sea cual sea su
    envolvente natural."""
    fade_n = min(int(fade_s * sr), len(x) // 2)
    if fade_n <= 1:
        return x
    ramp = np.linspace(1.0, 0.0, fade_n)
    x = x.copy()
    x[-fade_n:] *= ramp
    return x


def render_note(instrument: str, freq_or_chord, duration_s: float, velocity: float, sr: int, rng: np.random.Generator) -> np.ndarray:
    if instrument in _SINGLE_FREQ_INSTRUMENTS:
        freq = freq_or_chord * micro_detune_ratio(rng)
        audio = _SINGLE_FREQ_INSTRUMENTS[instrument](freq, duration_s, velocity, sr, rng)
    elif instrument in _CHORD_INSTRUMENTS:
        freqs = freq_or_chord if isinstance(freq_or_chord, (list, tuple)) else [freq_or_chord]
        audio = _CHORD_INSTRUMENTS[instrument](list(freqs), duration_s, velocity, sr, rng)
    elif instrument in _NO_PITCH_INSTRUMENTS:
        audio = _NO_PITCH_INSTRUMENTS[instrument](duration_s, velocity, sr, rng)
    else:
        raise KeyError(f"instrumento desconocido: {instrument}")
    return _apply_tail_fade(audio, sr)


def render_flute_phrase(
    notes: list[tuple[float, float, float, float]],  # (start_beat, dur_beats, freq, velocity)
    bpm: float,
    sr: int,
    rng: np.random.Generator,
    portamento_s: float = 0.09,
) -> tuple[np.ndarray, float]:
    """Renderiza una frase entera de flauta como UN contorno continuo de
    tono y amplitud (con deslizamiento entre notas): devuelve (audio, inicio
    en segundos de la frase dentro de la pista)."""
    sec_per_beat = 60.0 / bpm
    start_s = notes[0][0] * sec_per_beat
    end_s = max((n[0] + n[1]) * sec_per_beat for n in notes)
    total_n = max(int((end_s - start_s) * sr), 1)
    freq_env = np.zeros(total_n)
    amp_env = np.zeros(total_n)

    prev_end_sample = 0
    prev_freq = notes[0][2]
    for start_beat, dur_beats, freq, velocity in notes:
        note_start = int((start_beat * sec_per_beat - start_s) * sr)
        note_dur = max(int(dur_beats * sec_per_beat * sr), 1)
        note_end = min(note_start + note_dur, total_n)
        if note_end <= note_start:
            continue
        freq = freq * micro_detune_ratio(rng, max_cents=4.0)
        velocity = humanize_velocity(velocity, rng)

        glide_n = min(int(portamento_s * sr), note_end - note_start)
        if glide_n > 1 and note_start > 0:
            glide = np.linspace(prev_freq, freq, glide_n)
            freq_env[note_start : note_start + glide_n] = glide
            freq_env[note_start + glide_n : note_end] = freq
        else:
            freq_env[note_start:note_end] = freq

        note_env_len = note_end - note_start
        shape = np.ones(note_env_len)
        att = min(int(0.05 * sr), note_env_len // 3 or 1)
        rel = min(int(0.08 * sr), note_env_len // 3 or 1)
        shape[:att] = np.linspace(0.0, 1.0, att)
        shape[-rel:] = np.minimum(shape[-rel:], np.linspace(1.0, 0.0, rel))
        amp_env[note_start:note_end] = np.maximum(amp_env[note_start:note_end], shape * velocity)

        prev_freq = freq
        prev_end_sample = note_end

    freq_env[freq_env == 0] = prev_freq
    audio = ins.bamboo_flute_contour(freq_env, amp_env, sr, rng)
    return audio, start_s


def render_song(
    bpm: float,
    sr: int,
    total_beats: float,
    tracks: list[Track],
    flute_phrases: list[list[tuple[float, float, float, float]]] | None = None,
    seed_rng: np.random.Generator | None = None,
    reverb_wet: float = 0.3,
    reverb_room: float = 0.6,
) -> np.ndarray:
    """Mezcla todas las pistas (mas las frases de flauta) en un bus estereo y
    aplica una reverberacion amplia. `total_beats` ya incluye, si hace falta,
    el margen para la cola de reverberacion o para el material "futuro" que
    luego pliega `seamless_loop` (lo añade la persona que llama, en
    `compose.py`, para controlar el redondeo con un solo calculo)."""
    rng = seed_rng or np.random.default_rng()
    sec_per_beat = 60.0 / bpm
    total_s = total_beats * sec_per_beat
    n_total = int(total_s * sr)
    mix = np.zeros((2, n_total))

    for track in tracks:
        for start_beat, dur_beats, freq_or_chord, velocity, pan in track.events:
            start_s = humanize_timing_s(start_beat * sec_per_beat, rng, track.humanize_timing_s)
            duration_s = max(dur_beats * sec_per_beat, 0.02)
            vel = humanize_velocity(velocity, rng, track.humanize_velocity_amt)
            audio = render_note(track.instrument, freq_or_chord, duration_s, vel, sr, rng)
            stereo = pan_constant_power(audio, pan)
            pos = int(start_s * sr)
            end = min(pos + stereo.shape[-1], n_total)
            length = end - pos
            if length > 0 and pos < n_total:
                mix[:, pos:end] += stereo[:, :length]

    for phrase in flute_phrases or []:
        audio, start_s = render_flute_phrase(phrase, bpm, sr, rng)
        stereo = pan_constant_power(audio, 0.0)
        pos = int(start_s * sr)
        end = min(pos + stereo.shape[-1], n_total)
        length = end - pos
        if length > 0 and pos < n_total:
            mix[:, pos:end] += stereo[:, :length]

    left = schroeder_reverb(mix[0], sr, room_size=reverb_room, damping=0.35, wet=reverb_wet)
    right = schroeder_reverb(mix[1], sr, room_size=reverb_room * 1.04, damping=0.35, wet=reverb_wet)
    return np.stack([left, right])
