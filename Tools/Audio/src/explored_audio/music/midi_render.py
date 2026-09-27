"""Traduce las `Track`/eventos que arma `compose.py` (sin cambios: la
composicion -que nota, cuando, con que intensidad- sigue siendo la misma) a
General MIDI, para sostenerla con un soundfont acustico real (`soundfont.py`)
en vez de con los sintetizadores de `instruments.py`.

La humanizacion (`humanize.py`: microtiming y variacion de velocidad) se
aplica aqui con las mismas funciones y las mismas cantidades por pista que
usaba `render_song` original, asi que el "toque" de cada pieza no cambia.

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
- **Ukelele aproximado.** El soundfont elegido (FluidR3Mono_GM) no incluye un
  patch de ukelele: se usa la guitarra de nailon transportada una octava
  arriba (mismo timbre pulsado, registro mas parecido al de un ukelele).
"""

from __future__ import annotations

import math
from pathlib import Path

import mido

from .humanize import humanize_timing_s, humanize_velocity

TICKS_PER_BEAT = 480
PERCUSSION_CHANNEL = 9  # Canal MIDI 10 (1-indexado): banco de percusion GM fijo.

# Programas General MIDI (0-indexados). "cuerdas suaves" y "guitarra de nailon"
# citan literalmente los timbres pedidos en el encargo; "tremolo_strings" usa
# el patch de tremolo real del soundfont en vez de simular el tremolo a mano.
PROGRAMS: dict[str, int] = {
    "marimba": 12,           # Marimba.
    "kalimba": 108,          # Kalimba (extension GM2, presente en FluidR3Mono_GM).
    "bass": 32,              # Acoustic Bass.
    "guitar": 24,            # Acoustic Guitar (nylon).
    "ukulele": 24,           # Aproximado con la guitarra de nailon (ver docstring del modulo).
    "pad": 49,               # Slow Strings.
    "strings": 48,           # String Ensemble 1.
    "tremolo_strings": 44,   # Tremolo Strings.
    "flute": 73,             # Flute.
    "piano": 0,              # Acoustic Grand Piano.
}
UKULELE_OCTAVE_SHIFT = 12  # semitonos: registro de ukelele sobre la guitarra de nailon base.

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
    "marimba", "kalimba", "bass", "guitar", "ukulele",
    "pad", "strings", "tremolo_strings", "flute", "piano",
]


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


class _ChannelBuilder:
    """Acumula mensajes MIDI con su instante absoluto en ticks, por canal, y
    solo emite `program_change`/`control_change` de panoramica cuando cambian
    (evita spam de CC en cada nota)."""

    def __init__(self, sec_per_beat: float) -> None:
        self._sec_per_beat = sec_per_beat
        self._events: dict[int, list[tuple[int, mido.Message]]] = {}
        self._program: dict[int, int] = {}
        self._pan: dict[int, int] = {}
        self._sustain_on: set[int] = set()

    def sec_to_tick(self, seconds: float) -> int:
        return max(int(round(seconds / self._sec_per_beat * TICKS_PER_BEAT)), 0)

    def _emit(self, channel: int, tick: int, msg: mido.Message) -> None:
        self._events.setdefault(channel, []).append((tick, msg))

    def _ensure_program(self, channel: int, program: int, tick: int) -> None:
        if self._program.get(channel) != program:
            self._emit(channel, tick, mido.Message("program_change", program=program, channel=channel, time=0))
            self._program[channel] = program

    def _ensure_pan(self, channel: int, pan: float, tick: int) -> None:
        midi_pan = _pan_to_midi(pan)
        if self._pan.get(channel) != midi_pan:
            self._emit(channel, tick, mido.Message("control_change", control=10, value=midi_pan, channel=channel, time=0))
            self._pan[channel] = midi_pan

    def add_note(
        self, channel: int, program: int | None, note: int, start_s: float,
        duration_s: float, velocity: float, pan: float,
    ) -> None:
        start_tick = self.sec_to_tick(start_s)
        if program is not None:
            self._ensure_program(channel, program, start_tick)
        self._ensure_pan(channel, pan, start_tick)
        end_tick = max(start_tick + self.sec_to_tick(duration_s), start_tick + 1)
        note = int(min(max(note, 0), 127))
        self._emit(channel, start_tick, mido.Message("note_on", note=note, velocity=_velocity_to_midi(velocity), channel=channel, time=0))
        self._emit(channel, end_tick, mido.Message("note_off", note=note, velocity=0, channel=channel, time=0))

    def sustain(self, channel: int, seconds: float, on: bool) -> None:
        tick = self.sec_to_tick(seconds)
        if on and channel in self._sustain_on:
            return
        if not on and channel not in self._sustain_on:
            return
        self._emit(channel, tick, mido.Message("control_change", control=64, value=127 if on else 0, channel=channel, time=0))
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
            ordered = sorted(self._events[channel], key=lambda item: item[0])
            track = mido.MidiTrack()
            mid.tracks.append(track)
            prev_tick = 0
            for tick, msg in ordered:
                track.append(msg.copy(time=max(tick - prev_tick, 0)))
                prev_tick = tick
            track.append(mido.MetaMessage("end_of_track", time=0))
        return mid


def _humanized_events(track, sec_per_beat: float, rng) -> list[tuple[float, float, object, float, float]]:
    events = []
    for start_beat, dur_beats, freq_or_chord, velocity, pan in track.events:
        start_s = humanize_timing_s(start_beat * sec_per_beat, rng, track.humanize_timing_s)
        duration_s = max(dur_beats * sec_per_beat, 0.02)
        vel = humanize_velocity(velocity, rng, track.humanize_velocity_amt)
        events.append((start_s, duration_s, freq_or_chord, vel, pan))
    return events


def _add_track(builder: _ChannelBuilder, track, sec_per_beat: float, rng) -> None:
    if not track.events:
        return
    events = _humanized_events(track, sec_per_beat, rng)

    if track.instrument in PERCUSSION_INSTRUMENTS:
        note = PERCUSSION_NOTES[track.instrument]
        for start_s, duration_s, _freq, vel, _pan in events:
            # Panoramica fija: el canal 10 (percusion) es compartido por los
            # tres instrumentos de percusion (ver limitaciones en el docstring).
            builder.add_note(PERCUSSION_CHANNEL, None, note, start_s, duration_s, vel, 0.0)
        return

    if track.instrument in CHORD_INSTRUMENTS:
        channel = _channel_for_instrument(track.instrument)
        program = PROGRAMS[track.instrument]
        for start_s, duration_s, freq_or_chord, vel, pan in events:
            freqs = freq_or_chord if isinstance(freq_or_chord, (list, tuple)) else [freq_or_chord]
            for freq in freqs:
                builder.add_note(channel, program, freq_to_midi_note(freq), start_s, duration_s, vel, pan)
        return

    instrument = track.instrument
    channel = _channel_for_instrument(instrument)
    program = PROGRAMS[instrument]
    shift = UKULELE_OCTAVE_SHIFT if instrument == "ukulele" else 0
    for start_s, duration_s, freq, vel, pan in events:
        builder.add_note(channel, program, freq_to_midi_note(freq) + shift, start_s, duration_s, vel, pan)


def _add_flute_phrase(
    builder: _ChannelBuilder, notes: list[tuple[float, float, float, float]],
    sec_per_beat: float, rng, time_offset_s: float = 0.0, overlap_s: float = 0.05,
) -> None:
    """Frase de flauta como notas ligadas (pequeno solape + pedal de sustain):
    sustituye el contorno de tono continuo de `instruments.bamboo_flute_contour`
    (ver limitaciones del modulo)."""
    channel = _channel_for_instrument("flute")
    program = PROGRAMS["flute"]
    last_end_s = 0.0
    for i, (start_beat, dur_beats, freq, velocity) in enumerate(notes):
        # Tiempos relativos a `time_offset_s`: para una frase suelta (la
        # muestra diegetica) eso pone la primera nota en t=0 del MIDI; dentro
        # de una cancion completa `time_offset_s` es 0 y coincide con el
        # instante absoluto de la pieza.
        rel_start_s = max(humanize_timing_s(start_beat * sec_per_beat, rng, 0.02) - time_offset_s, 0.0)
        has_next = i < len(notes) - 1
        duration_s = max(dur_beats * sec_per_beat, 0.05) + (overlap_s if has_next else 0.0)
        vel = humanize_velocity(velocity, rng, 0.15)
        note = freq_to_midi_note(freq)
        if i == 0:
            builder.sustain(channel, rel_start_s, True)
        builder.add_note(channel, program if i == 0 else None, note, rel_start_s, duration_s, vel, 0.0)
        last_end_s = max(last_end_s, rel_start_s + duration_s)
    builder.sustain(channel, last_end_s + 0.15, False)


def build_song_midi(tracks, flute_phrases, bpm: float, rng) -> mido.MidiFile:
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
    notes: list[tuple[float, float, float, float]], bpm: float, rng,
) -> tuple[mido.MidiFile, float]:
    """MIDI de una unica frase de flauta suelta (usado por
    `compose.flute_note_sample`): el render empieza en el instante de la
    primera nota, igual que hacia `sequencer.render_flute_phrase` original."""
    sec_per_beat = 60.0 / bpm
    start_s = notes[0][0] * sec_per_beat
    builder = _ChannelBuilder(sec_per_beat)
    _add_flute_phrase(builder, notes, sec_per_beat, rng, time_offset_s=start_s)
    return builder.build(TICKS_PER_BEAT, bpm), start_s


def write_midi(mid: mido.MidiFile, path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    mid.save(str(path))
    return path
