"""Secuenciador: coloca notas (o frases, para la flauta) en una rejilla de
compases, humaniza cada una y las renderiza con un soundfont acustico real via
FluidSynth (`midi_render.py` + `soundfont.py`), en vez de sintetizarlas con
`instruments.py`.

Una `Track` es un instrumento mas su lista de eventos: esto NO ha cambiado
respecto a la version sintetizada, y tampoco lo ha hecho `compose.py` (la
partitura -que nota, cuando, con que progresion de acordes- es exactamente la
misma). Lo unico que cambia es qué sostiene esa partitura: antes, osciladores
numpy; ahora, muestras reales via MIDI. Ver `midi_render.py` para el mapeo de
instrumentos a presets del soundfont y sus limitaciones documentadas
(percusion a un solo canal, flauta sin portamento continuo)."""

from __future__ import annotations

import tempfile
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from ..reverb import room_reverb
from . import midi_render, soundfont
from .midi_render import FluteNote

NoteEvent = tuple[float, float, "float | list[float]", float, float]  # start, dur, freq(s), vel, pan


@dataclass
class Track:
    instrument: str
    events: list[NoteEvent] = field(default_factory=list)
    humanize_timing_s: float = 0.018
    humanize_velocity_amt: float = 0.15
    # Vestigial desde la migracion a soundfont: la micro-desafinacion por nota
    # tenia sentido para dar cuerpo a un oscilador (chorus barato); con
    # muestras reales cada nota ya suena a instrumento real de por si, asi que
    # `midi_render` no lo usa. Se mantiene el campo para no romper a quien
    # construya un `Track` pasandolo por nombre.
    detune_cents: float = 5.0


_TAIL_FADE_S = 0.02


def _fade_tail(x: np.ndarray, sr: int, fade_s: float = _TAIL_FADE_S) -> np.ndarray:
    """Fundido de salida corto: evita un clic si el render se trunca (o se
    rellena con silencio) al ajustarlo a la duracion exacta que espera
    `compose.py`."""
    fade_n = min(int(fade_s * sr), x.shape[-1] // 2)
    if fade_n <= 1:
        return x
    ramp = np.linspace(1.0, 0.0, fade_n)
    x = x.copy()
    x[..., -fade_n:] *= ramp
    return x


def render_flute_phrase(
    notes: list[FluteNote],
    bpm: float,
    sr: int,
    rng: np.random.Generator,
    portamento_s: float = 0.09,
) -> tuple[np.ndarray, float]:
    """Renderiza una frase de flauta suelta (usado por
    `compose.flute_note_sample`) y devuelve `(audio_mono, start_s)`, con
    `start_s` el instante (en segundos) de la primera nota en la rejilla de
    compases original -mismo contrato que la version sintetizada.

    `portamento_s` ya no se usa (el portamento continuo por oscilador no es
    portable a MIDI/soundfont; ver limitaciones en `midi_render.py`): se
    mantiene el parametro solo por compatibilidad de firma."""
    del portamento_s
    mid, start_s = midi_render.build_flute_phrase_midi(notes, bpm, rng)
    with tempfile.TemporaryDirectory(prefix="explored_audio_flute_") as tmp:
        midi_path = midi_render.write_midi(mid, Path(tmp) / "flute.mid")
        rendered = soundfont.render_midi_to_stereo(midi_path, sr)
    mono = rendered.mean(axis=0)
    return mono, start_s


def render_song(
    bpm: float,
    sr: int,
    total_beats: float,
    tracks: list[Track],
    flute_phrases: list[list[FluteNote]] | None = None,
    seed_rng: np.random.Generator | None = None,
    reverb_wet: float = 0.3,
    reverb_room: float = 0.6,
) -> np.ndarray:
    """Une todas las pistas (mas las frases de flauta) en un unico MIDI
    multicanal, lo renderiza con el soundfont acustico y lo coloca en una
    sala (`reverb.room_reverb`, convolucion con una respuesta sintetica):
    un unico punto de control sobre el espacio de cada pieza. La sonoridad
    final (-16 LUFS) y el limitador los pone `build.finalize`, sobre la
    pieza ya cerrada en bucle.

    `total_beats` ya incluye, si hace falta, el margen para la cola de
    reverberacion/liberacion natural del instrumento o para el material
    "futuro" que luego pliega `seamless_loop` (lo añade quien llama, en
    `compose.py`)."""
    rng = seed_rng or np.random.default_rng()
    sec_per_beat = 60.0 / bpm
    total_s = total_beats * sec_per_beat
    n_total = int(total_s * sr)

    mid = midi_render.build_song_midi(tracks, flute_phrases, bpm, rng)
    with tempfile.TemporaryDirectory(prefix="explored_audio_song_") as tmp:
        midi_path = midi_render.write_midi(mid, Path(tmp) / "song.mid")
        rendered = soundfont.render_midi_to_stereo(midi_path, sr)

    # El render real (notas + liberacion natural del instrumento) puede durar
    # algo mas o menos que `n_total`: se ajusta al tamano exacto que espera
    # `compose.py` (recorte o relleno de silencio), con un fundido corto para
    # no dejar un clic si toca recortar una cola en pleno decaimiento.
    mix = np.zeros((2, n_total))
    length = min(rendered.shape[-1], n_total)
    mix[:, :length] = rendered[:, :length]
    mix = _fade_tail(mix, sr)

    room_seed = int(rng.integers(0, 2**31 - 1))
    return room_reverb(mix, sr, room_size=reverb_room, damping=0.35, wet=reverb_wet, seed=room_seed)
