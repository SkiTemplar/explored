"""Traduce las `Track`/eventos que arma `compose.py` (sin cambios: la
composicion -que nota, cuando, con que intensidad- sigue siendo la misma) a
General MIDI, para sostenerla con un soundfont acustico real (`soundfont.py`)
en vez de con los sintetizadores de `instruments.py`.

La humanizacion (`humanize.py`) se aplica aqui, nota a nota, con las
cantidades por pista que ya fijaba `compose.py`, y suma tres cosas que una
rejilla MIDI "cruda" no tiene: una deriva lenta de tempo por pista
(`TimingDrift`, el interprete que empuja o frena una frase), un acento
metrico en la velocidad (`metric_accent`) y una entrada escalonada de unos
milisegundos en las notas de los acordes sostenidos.

Limitaciones deliberadas frente a la version sintetizada (documentadas porque
importan para quien retoque esto despues):
- **Percusion en un unico canal MIDI.** El General MIDI reserva la percusion
  al canal 10 (indice 9): shaker, wood_block y kick comparten ese canal, asi
  que la ligera panoramica alterna que tenia `wood_block` entre compases se
  pierde (queda centrada). El resto de instrumentos si conservan su
  panoramica por nota via CC10.
- **Sin portamento continuo en la flauta.** Un slide de tono continuo entre
  notas no es portable entre sintetizadores GM. Se sustituye por notas ligadas
  con un pequeno solape y pedal de sustain (CC64), que da continuidad sin
  depender de un controlador de portamento que no todos los sintetizadores
  soportan igual.

Los timbres salen del banco GM (banco 0) salvo dos que el soundfont trae en
el banco de variaciones GS 8 y que suenan mas cerca de lo que pide el
encargo: el ukelele de verdad (8:24, en vez de aproximarlo con la guitarra de
nailon) y el piano de cola suave (8:0 "Mellow Grand Piano"). El banco se
selecciona con CC0/CC32 antes de cada cambio de programa, que es lo que
FluidSynth espera en su modo de seleccion de banco por defecto ("gs").
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING

import mido
import numpy as np

from .humanize import TimingDrift, humanize_timing_s, humanize_velocity, metric_accent

if TYPE_CHECKING:
    from .sequencer import Track

FluteNote = tuple[float, float, float, float]  # start_beat, dur_beats, freq, velocity

TICKS_PER_BEAT = 480
PERCUSSION_CHANNEL = 9  # Canal MIDI 10 (1-indexado): banco de percusion GM fijo.


@dataclass(frozen=True)
class Voice:
    """Preset del soundfont que sostiene un instrumento del secuenciador."""

    bank: int
    program: int
    label: str


# Presets (banco, programa 0-indexado) comprobados en la tabla de presets de
# FluidR3Mono_GM.sf3. "cuerdas suaves", "guitarra de nailon", "ukelele",
# "vibrafono" y "piano" citan literalmente los timbres del encargo.
VOICES: dict[str, Voice] = {
    "marimba": Voice(0, 12, "Marimba"),
    "vibraphone": Voice(0, 11, "Vibraphone"),
    "bass": Voice(0, 32, "Acoustic Bass"),
    "guitar": Voice(0, 24, "Nylon String Guitar"),
    "ukulele": Voice(8, 24, "Ukulele"),
    "pad": Voice(0, 49, "Slow Strings"),
    "strings": Voice(0, 48, "Strings"),
    "tremolo_strings": Voice(0, 44, "Tremolo Strings"),
    "flute": Voice(0, 73, "Flute"),
    "piano": Voice(8, 0, "Mellow Grand Piano"),
}

PERCUSSION_NOTES: dict[str, int] = {
    "shaker": 70,       # Maracas: percusion de mano ligera.
    "wood_block": 76,   # Hi Wood Block.
    "kick": 64,         # Low Conga: pulso grave de mano, no bombo de bateria.
}

CHORD_INSTRUMENTS = {"pad", "strings", "tremolo_strings"}
PERCUSSION_INSTRUMENTS = set(PERCUSSION_NOTES)

# Orden fijo de asignacion de canal MIDI: un canal por INSTRUMENTO (no por
# Track), asi varias frases del mismo instrumento en una pieza comparten canal
# y programa en vez de agotar los 16 canales disponibles.
_MELODIC_INSTRUMENT_ORDER = [
    "marimba", "vibraphone", "bass", "guitar", "ukulele",
    "pad", "strings", "tremolo_strings", "flute", "piano",
]
assert set(_MELODIC_INSTRUMENT_ORDER) == set(VOICES)
assert len(_MELODIC_INSTRUMENT_ORDER) < 16


def _channel_for_instrument(name: str) -> int:
    idx = _MELODIC_INSTRUMENT_ORDER.index(name)
    return idx if idx < PERCUSSION_CHANNEL else idx + 1


def freq_to_midi_note(freq: float) -> int:
    """Nota MIDI mas cercana (A4=69=440 Hz). Las frecuencias de `theory.py` ya
    caen en semitonos exactos de temperamento igual, asi que redondear no
    introduce error de afinacion."""
    note = 69 + 12 * math.log2(max(freq, 1.0) / 440.0)
    return int(round(min(max(note, 0.0), 127.0)))


def _velocity_to_midi(velocity: float) -> int:
    return int(min(max(round(velocity * 127), 1), 127))


def _pan_to_midi(pan: float) -> int:
    return int(round((min(max(pan, -1.0), 1.0) + 1.0) / 2.0 * 127))


# Orden de los mensajes que caen en el mismo tick: primero se apagan notas,
# despues se cambian controladores y programas, y al final se encienden las
# nuevas. Asi una nota que empieza justo donde acaba otra de la misma altura
# no queda cortada por el `note_off` de la anterior.
_ORDER_NOTE_OFF, _ORDER_CONTROL, _ORDER_NOTE_ON = 0, 1, 2


class _ChannelBuilder:
    """Acumula mensajes MIDI con su instante absoluto en ticks, por canal, y
    solo emite banco/programa/panoramica cuando cambian (evita spam de CC en
    cada nota)."""

    def __init__(self, sec_per_beat: float) -> None:
        self._sec_per_beat = sec_per_beat
        # canal -> [tick, orden, secuencia, mensaje]; lista mutable para poder
        # adelantar un `note_off` ya emitido (ver `add_note`).
        self._events: dict[int, list[list]] = {}
        self._seq = 0
        self._voice: dict[int, Voice] = {}
        self._pan: dict[int, int] = {}
        self._sustain_on: set[int] = set()
        self._open_off: dict[tuple[int, int], list] = {}

    def sec_to_tick(self, seconds: float) -> int:
        return max(int(round(seconds / self._sec_per_beat * TICKS_PER_BEAT)), 0)

    def _emit(self, channel: int, tick: int, order: int, msg: mido.Message) -> list:
        event = [tick, order, self._seq, msg]
        self._seq += 1
        self._events.setdefault(channel, []).append(event)
        return event

    def _ensure_voice(self, channel: int, voice: Voice, tick: int) -> None:
        if self._voice.get(channel) == voice:
            return
        self._emit(channel, tick, _ORDER_CONTROL, mido.Message("control_change", control=0, value=voice.bank, channel=channel))
        self._emit(channel, tick, _ORDER_CONTROL, mido.Message("control_change", control=32, value=0, channel=channel))
        self._emit(channel, tick, _ORDER_CONTROL, mido.Message("program_change", program=voice.program, channel=channel))
        self._voice[channel] = voice

    def _ensure_pan(self, channel: int, pan: float, tick: int) -> None:
        midi_pan = _pan_to_midi(pan)
        if self._pan.get(channel) != midi_pan:
            self._emit(channel, tick, _ORDER_CONTROL, mido.Message("control_change", control=10, value=midi_pan, channel=channel))
            self._pan[channel] = midi_pan

    def add_note(
        self, channel: int, voice: Voice | None, note: int, start_s: float,
        duration_s: float, velocity: float, pan: float,
    ) -> None:
        start_tick = self.sec_to_tick(start_s)
        if voice is not None:
            self._ensure_voice(channel, voice, start_tick)
        self._ensure_pan(channel, pan, start_tick)
        end_tick = max(start_tick + self.sec_to_tick(duration_s), start_tick + 1)
        note = int(min(max(note, 0), 127))
        # La humanizacion puede solapar dos notas seguidas de la misma altura
        # en el mismo canal; el `note_off` de la primera cortaria la segunda.
        # Se adelanta ese `note_off` al inicio de la nueva nota.
        previous_off = self._open_off.get((channel, note))
        if previous_off is not None and previous_off[0] > start_tick:
            previous_off[0] = start_tick
        self._emit(channel, start_tick, _ORDER_NOTE_ON, mido.Message("note_on", note=note, velocity=_velocity_to_midi(velocity), channel=channel))
        self._open_off[(channel, note)] = self._emit(channel, end_tick, _ORDER_NOTE_OFF, mido.Message("note_off", note=note, velocity=0, channel=channel))

    def sustain(self, channel: int, seconds: float, on: bool) -> None:
        if on == (channel in self._sustain_on):
            return
        self._emit(channel, self.sec_to_tick(seconds), _ORDER_CONTROL, mido.Message("control_change", control=64, value=127 if on else 0, channel=channel))
        if on:
            self._sustain_on.add(channel)
        else:
            self._sustain_on.discard(channel)

    def build(self, ticks_per_beat: int, bpm: float) -> mido.MidiFile:
        mid = mido.MidiFile(ticks_per_beat=ticks_per_beat)
        tempo_track = mido.MidiTrack()
        mid.tracks.append(tempo_track)
        tempo_track.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(bpm), time=0))
        for channel in sorted(self._events):
            ordered = sorted(self._events[channel], key=lambda item: (item[0], item[1], item[2]))
            track = mido.MidiTrack()
            mid.tracks.append(track)
            prev_tick = 0
            for tick, _order, _seq, msg in ordered:
                track.append(msg.copy(time=tick - prev_tick))
                prev_tick = tick
            track.append(mido.MetaMessage("end_of_track", time=0))
        return mid


@dataclass(frozen=True)
class _PlayedNote:
    start_s: float
    duration_s: float
    freqs: tuple[float, ...]
    velocity: float
    pan: float


# Deriva de tempo por pista (ver `TimingDrift`): proporcional a lo que la
# pista ya humaniza nota a nota, para que una pista casi "a rejilla" (el
# pulso de tension) siga casi a rejilla.
_DRIFT_PER_JITTER = 0.5
# Separacion maxima entre las notas de un acorde sostenido (cuerdas): una
# seccion real nunca entra exactamente a la vez.
_CHORD_SPREAD_S = 0.012


def _humanized_events(track: Track, sec_per_beat: float, rng: np.random.Generator) -> list[_PlayedNote]:
    drift = TimingDrift(rng, sigma_s=track.humanize_timing_s * _DRIFT_PER_JITTER)
    played: list[_PlayedNote] = []
    for start_beat, dur_beats, freq_or_chord, velocity, pan in sorted(track.events, key=lambda e: e[0]):
        nominal_s = start_beat * sec_per_beat
        start_s = max(humanize_timing_s(nominal_s, rng, track.humanize_timing_s) + drift.offset_at(nominal_s), 0.0)
        duration_s = max(dur_beats * sec_per_beat, 0.02)
        vel = humanize_velocity(velocity * metric_accent(start_beat), rng, track.humanize_velocity_amt)
        freqs = tuple(freq_or_chord) if isinstance(freq_or_chord, (list, tuple)) else (float(freq_or_chord),)
        played.append(_PlayedNote(start_s, duration_s, freqs, vel, pan))
    return played


def _add_track(builder: _ChannelBuilder, track: Track, sec_per_beat: float, rng: np.random.Generator) -> None:
    if not track.events:
        return
    played = _humanized_events(track, sec_per_beat, rng)

    if track.instrument in PERCUSSION_INSTRUMENTS:
        note = PERCUSSION_NOTES[track.instrument]
        for p in played:
            # Panoramica fija: el canal 10 (percusion) es compartido por los
            # tres instrumentos de percusion (ver limitaciones en el docstring).
            builder.add_note(PERCUSSION_CHANNEL, None, note, p.start_s, p.duration_s, p.velocity, 0.0)
        return

    if track.instrument not in VOICES:
        raise KeyError(f"instrumento sin preset en el soundfont: {track.instrument}")
    channel = _channel_for_instrument(track.instrument)
    voice = VOICES[track.instrument]
    spread = _CHORD_SPREAD_S if track.instrument in CHORD_INSTRUMENTS else 0.0
    for p in played:
        for i, freq in enumerate(p.freqs):
            offset = float(rng.uniform(0.0, spread)) if (spread > 0.0 and i > 0) else 0.0
            builder.add_note(channel, voice, freq_to_midi_note(freq), p.start_s + offset, p.duration_s - offset, p.velocity, p.pan)


def _add_flute_phrase(
    builder: _ChannelBuilder, notes: list[FluteNote],
    sec_per_beat: float, rng: np.random.Generator, time_offset_s: float = 0.0, overlap_s: float = 0.05,
) -> None:
    """Frase de flauta como notas ligadas (pequeno solape + pedal de sustain):
    sustituye el contorno de tono continuo de `instruments.bamboo_flute_contour`
    (ver limitaciones del modulo)."""
    channel = _channel_for_instrument("flute")
    voice = VOICES["flute"]
    drift = TimingDrift(rng, sigma_s=0.01, tau_s=3.0)
    last_end_s = 0.0
    for i, (start_beat, dur_beats, freq, velocity) in enumerate(notes):
        # Tiempos relativos a `time_offset_s`: para una frase suelta (la
        # muestra diegetica) eso pone la primera nota en t=0 del MIDI; dentro
        # de una cancion completa `time_offset_s` es 0 y coincide con el
        # instante absoluto de la pieza.
        nominal_s = start_beat * sec_per_beat
        rel_start_s = max(humanize_timing_s(nominal_s, rng, 0.02) + drift.offset_at(nominal_s) - time_offset_s, 0.0)
        has_next = i < len(notes) - 1
        duration_s = max(dur_beats * sec_per_beat, 0.05) + (overlap_s if has_next else 0.0)
        vel = humanize_velocity(velocity * metric_accent(start_beat), rng, 0.15)
        if i == 0:
            builder.sustain(channel, rel_start_s, True)
        builder.add_note(channel, voice if i == 0 else None, freq_to_midi_note(freq), rel_start_s, duration_s, vel, 0.0)
        last_end_s = max(last_end_s, rel_start_s + duration_s)
    builder.sustain(channel, last_end_s + 0.15, False)


def build_song_midi(
    tracks: list[Track], flute_phrases: list[list[FluteNote]] | None, bpm: float, rng: np.random.Generator,
) -> mido.MidiFile:
    """Une todas las `Track` (compose.py) y las frases de flauta en un unico
    MIDI multipista, un canal por instrumento."""
    sec_per_beat = 60.0 / bpm
    builder = _ChannelBuilder(sec_per_beat)
    for track in tracks:
        _add_track(builder, track, sec_per_beat, rng)
    for notes in flute_phrases or []:
        if notes:
            _add_flute_phrase(builder, notes, sec_per_beat, rng)
    return builder.build(TICKS_PER_BEAT, bpm)


def build_flute_phrase_midi(
    notes: list[FluteNote], bpm: float, rng: np.random.Generator,
) -> tuple[mido.MidiFile, float]:
    """MIDI de una unica frase de flauta suelta (usado por
    `compose.flute_note_sample`): el render empieza en el instante de la
    primera nota, igual que hacia `sequencer.render_flute_phrase` original."""
    if not notes:
        raise ValueError("frase de flauta vacia")
    sec_per_beat = 60.0 / bpm
    start_s = notes[0][0] * sec_per_beat
    builder = _ChannelBuilder(sec_per_beat)
    _add_flute_phrase(builder, notes, sec_per_beat, rng, time_offset_s=start_s)
    return builder.build(TICKS_PER_BEAT, bpm), start_s


def write_midi(mid: mido.MidiFile, path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    mid.save(str(path))
    return path
