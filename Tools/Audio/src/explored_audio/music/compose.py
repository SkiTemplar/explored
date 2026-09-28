"""La composicion en si: el leitmotiv, los acordes y el arreglo de cada
pieza (GDD §9.3). La melodia se escribe siempre en una escala PENTATONICA de
5 grados (mayor = brillante, menor = calida): con solo 5 grados nunca hay un
semitono ni un tritono, asi que cualquier nota que se toque encaja. El color
"dorico"/"mixolidio" que pide el encargo vive en la ARMONIA (los acordes,
construidos en semitonos absolutos, incluyen "prestamos" tipicos del modo:
bVII, bVI...), no en la melodia: pentatonica sobre armonia modal es una
combinacion clasica en folk e indie-game y evita el cliche de "escala exotica
entera" sonando forzada.

Cada pieza es una funcion que devuelve `(tracks, flute_phrases, total_bars,
bpm, beats_per_bar, reverb_wet, reverb_room)`. `generate()` la renderiza y
-si es un bucle- pliega la cola sobrante con `seamless_loop`."""

from __future__ import annotations

from typing import NotRequired, TypedDict

import numpy as np

from ..constants import SAMPLE_RATE
from ..loop import seamless_loop
from ..rng import rng_for
from .sequencer import Track, render_song
from .theory import chord_freqs, degree_freq, semitone_freq

SR = SAMPLE_RATE
ROOT = semitone_freq(440.0, -19)  # D3: un registro comodo para marimba/kalimba/bajo.

# ---------------------------------------------------------------------------
# El leitmotiv: 8 compases de 4/4 (32 tiempos), en grados de una pentatonica
# de 5 notas. Perfil en arco (sube hasta el compas 5, vuelve a la tonica):
# la forma mas simple de que una melodia se sienta "reconocible".
# ---------------------------------------------------------------------------
THEME_DEGREES: list[tuple[int | None, float, float]] = [
    (0, 1, 0.90), (2, 1, 0.80), (4, 1, 0.85), (2, 1, 0.75),
    (5, 1, 0.90), (4, 0.5, 0.70), (2, 0.5, 0.65), (0, 2, 0.80),
    (4, 1, 0.85), (2, 1, 0.75), (0, 1, 0.70), (2, 1, 0.75),
    (4, 2, 0.90), (2, 1, 0.75), (0, 1, 0.70),
    (0, 1, 0.85), (2, 1, 0.80), (4, 1, 0.85), (7, 1, 0.95),
    (5, 2, 0.90), (4, 1, 0.80), (2, 1, 0.75),
    (0, 1, 0.80), (-3, 1, 0.60), (0, 1, 0.70), (2, 1, 0.75),
    (0, 2, 0.75), (0, 2, 0.65),
]
assert sum(d[1] for d in THEME_DEGREES) == 32.0

# Progresiones de acordes: (semitonos_sobre_root, calidad, compases). Suman 8
# compases -> una vez por cada repeticion de 8 compases del leitmotiv.
DEFAULT_PROGRESSION = [(0, "min", 2), (10, "maj", 2), (8, "maj", 2), (10, "maj", 2)]  # i-bVII-bVI-bVII (dorico)
BRIGHT_PROGRESSION = [(0, "maj", 2), (5, "maj", 2), (7, "maj", 2), (0, "maj", 2)]  # I-IV-V-I (mixolidio/mayor)
DARK_PROGRESSION = [(0, "min", 2), (3, "maj", 2), (5, "min", 2), (10, "maj", 2)]
CLUSTER_PROGRESSION = [(0, "sus2", 2), (10, "sus4", 2), (8, "sus2", 2), (10, "sus4", 2)]

Degrees = list[tuple[int | None, float, float]]
Progression = list[tuple[int, str, int]]


class IslandConfig(TypedDict):
    """Arreglo de la musica de exploracion de una isla (ver `_explore_variant`)."""

    bpm: int
    scale: str
    progression: Progression
    melody: str | None
    pad: str
    perc: str | None
    arpeggio: NotRequired[bool]
    flute: NotRequired[bool]
    root_shift: NotRequired[int]
    wet: NotRequired[float]
    room: NotRequired[float]


class DiscoveryFragment(TypedDict):
    """Fragmento corto de descubrimiento: una frase suelta de un instrumento."""

    instrument: str
    bpm: int
    octave_shift: int
    degrees: Degrees


class FinaleConfig(TypedDict):
    """Variante del final (rescate, viaje o quedarse en la isla)."""

    bpm: int
    scale: str
    progression: Progression
    root_shift: int
    flute: bool


def add_melody_phrase(
    tracks: list[Track], instrument: str, root_freq: float, scale: str, beats_per_bar: float,
    bar_offset: float, octave_shift: int = 0, pan: float = 0.0, vel_scale: float = 1.0,
    degrees: list[tuple[int | None, float, float]] | None = None,
) -> None:
    from .theory import SCALE_STEPS

    degrees = degrees if degrees is not None else THEME_DEGREES
    track = Track(instrument=instrument)
    start_beat = bar_offset * beats_per_bar
    shift = octave_shift * len(SCALE_STEPS[scale])
    cursor = 0.0
    for degree, dur_beats, vel in degrees:
        if degree is not None:
            freq = degree_freq(root_freq, scale, degree + shift)
            track.events.append((start_beat + cursor, dur_beats * 0.92, freq, vel * vel_scale, pan))
        cursor += dur_beats
    tracks.append(track)


def add_flute_phrase(
    flute_phrases: list, root_freq: float, scale: str, beats_per_bar: float, bar_offset: float,
    octave_shift: int = 1, vel_scale: float = 0.8, degrees: list | None = None,
) -> None:
    from .theory import SCALE_STEPS

    degrees = degrees if degrees is not None else THEME_DEGREES
    shift = octave_shift * len(SCALE_STEPS[scale])
    start_beat = bar_offset * beats_per_bar
    notes = []
    cursor = 0.0
    for degree, dur_beats, vel in degrees:
        if degree is not None:
            freq = degree_freq(root_freq, scale, degree + shift)
            notes.append((start_beat + cursor, dur_beats, freq, vel * vel_scale))
        cursor += dur_beats
    if notes:
        flute_phrases.append(notes)


def add_bass(tracks: list[Track], root_freq: float, beats_per_bar: float, bar_offset: float, progression, vel: float = 0.6, pan: float = 0.0) -> None:
    track = Track(instrument="bass")
    beat_cursor = bar_offset * beats_per_bar
    for offset, _quality, bars in progression:
        freq = semitone_freq(root_freq, offset - 12)
        dur = bars * beats_per_bar
        half = dur / 2
        track.events.append((beat_cursor, half * 0.95, freq, vel, pan))
        track.events.append((beat_cursor + half, half * 0.95, freq, vel * 0.85, pan))
        beat_cursor += dur
    tracks.append(track)


def add_pad(tracks: list[Track], instrument: str, root_freq: float, beats_per_bar: float, bar_offset: float, progression, vel: float = 0.5, octave_shift: int = -1, pan: float = 0.0) -> None:
    track = Track(instrument=instrument, humanize_timing_s=0.03)
    beat_cursor = bar_offset * beats_per_bar
    for offset, quality, bars in progression:
        freqs = [f * (2.0 ** octave_shift) for f in chord_freqs(root_freq, offset, quality)]
        dur = bars * beats_per_bar
        track.events.append((beat_cursor, dur, freqs, vel, pan))
        beat_cursor += dur
    tracks.append(track)


def add_arpeggio(
    tracks: list[Track], instrument: str, root_freq: float, beats_per_bar: float, bar_offset: float,
    progression, pattern: tuple[int, ...] = (0, 1, 2, 1), note_dur: float = 0.5, vel: float = 0.4, pan: float = 0.0,
) -> None:
    """Arpegio (por defecto corchea 1-3-5-3) de las notas del acorde de cada
    compas de la progresion, para un instrumento punteado (la guitarra
    Karplus-Strong): el contraste ritmico natural de un "fingerpicking"
    frente al pad sostenido, en vez de tocar el acorde entero de golpe."""
    track = Track(instrument=instrument, humanize_timing_s=0.02)
    beat_cursor = bar_offset * beats_per_bar
    for offset, quality, bars in progression:
        freqs = chord_freqs(root_freq, offset, quality)
        dur = bars * beats_per_bar
        n_notes = max(int(round(dur / note_dur)), 1)
        for i in range(n_notes):
            freq = freqs[pattern[i % len(pattern)] % len(freqs)]
            track.events.append((beat_cursor + i * note_dur, note_dur * 0.85, freq, vel, pan))
        beat_cursor += dur
    tracks.append(track)


def add_percussion(tracks: list[Track], beats_per_bar: int, bar_offset: float, n_bars: int, density: str = "full") -> None:
    shaker_t = Track(instrument="shaker", humanize_timing_s=0.012)
    wood_t = Track(instrument="wood_block")
    kick_t = Track(instrument="kick", humanize_timing_s=0.01)
    for bar in range(n_bars):
        base = (bar_offset + bar) * beats_per_bar
        if density in ("full", "light"):
            for eighth in range(beats_per_bar * 2):
                shaker_t.events.append((base + eighth * 0.5, 0.4, 1.0, 0.25 if density == "full" else 0.14, 0.0))
        if density == "full":
            wood_t.events.append((base + 1, 0.3, 900.0, 0.55, -0.3))
            wood_t.events.append((base + 3, 0.3, 900.0, 0.5, 0.3))
            kick_t.events.append((base, 0.35, 1.0, 0.75, 0.0))
        elif density == "sparse":
            kick_t.events.append((base, 0.4, 1.0, 0.45, 0.0))
    for t in (shaker_t, wood_t, kick_t):
        if t.events:
            tracks.append(t)


def _theme_arc(root_freq: float, scale: str, beats_per_bar: int) -> tuple[list[Track], list, int]:
    tracks: list[Track] = []
    phrase_bars = 8

    add_melody_phrase(tracks, "marimba", root_freq, scale, beats_per_bar, 0, pan=0.0)
    add_pad(tracks, "pad", root_freq, beats_per_bar, 0, DEFAULT_PROGRESSION, vel=0.32)

    add_melody_phrase(tracks, "marimba", root_freq, scale, beats_per_bar, 8, pan=0.0)
    add_pad(tracks, "pad", root_freq, beats_per_bar, 8, DEFAULT_PROGRESSION, vel=0.42)
    add_bass(tracks, root_freq, beats_per_bar, 8, DEFAULT_PROGRESSION)
    add_percussion(tracks, beats_per_bar, 8, phrase_bars, density="light")

    add_melody_phrase(tracks, "kalimba", root_freq, scale, beats_per_bar, 16, octave_shift=1, vel_scale=0.75, pan=0.25)
    add_melody_phrase(tracks, "marimba", root_freq, scale, beats_per_bar, 16, pan=-0.15)
    add_pad(tracks, "strings", root_freq, beats_per_bar, 16, DEFAULT_PROGRESSION, vel=0.48)
    add_bass(tracks, root_freq, beats_per_bar, 16, DEFAULT_PROGRESSION)
    add_percussion(tracks, beats_per_bar, 16, phrase_bars, density="full")

    add_melody_phrase(tracks, "marimba", root_freq, scale, beats_per_bar, 24, pan=0.0)
    add_pad(tracks, "pad", root_freq, beats_per_bar, 24, DEFAULT_PROGRESSION, vel=0.42)
    add_bass(tracks, root_freq, beats_per_bar, 24, DEFAULT_PROGRESSION)
    add_percussion(tracks, beats_per_bar, 24, phrase_bars, density="light")

    add_melody_phrase(tracks, "marimba", root_freq, scale, beats_per_bar, 32, vel_scale=0.65)
    add_pad(tracks, "pad", root_freq, beats_per_bar, 32, DEFAULT_PROGRESSION, vel=0.28)

    return tracks, [], 40


def _theme() -> tuple:
    bpm, beats_per_bar = 80, 4
    tracks, flute_phrases, total_bars = _theme_arc(ROOT, "minor_pentatonic", beats_per_bar)
    return tracks, flute_phrases, total_bars, bpm, beats_per_bar, 0.32, 0.7


ISLAND_CONFIGS: dict[str, IslandConfig] = {
    "landing": IslandConfig(bpm=82, scale="major_pentatonic", progression=DEFAULT_PROGRESSION, melody="marimba", pad="pad", perc="light", arpeggio=True),
    "emerald": IslandConfig(bpm=84, scale="minor_pentatonic", progression=DEFAULT_PROGRESSION, melody="kalimba", pad="pad", perc="full"),
    "smoke": IslandConfig(bpm=70, scale="minor_pentatonic", progression=DARK_PROGRESSION, melody=None, pad="tremolo_strings", perc="sparse", root_shift=-3, wet=0.4, room=0.75),
    "teeth": IslandConfig(bpm=86, scale="major_pentatonic", progression=DEFAULT_PROGRESSION, melody=None, pad="pad", perc=None, flute=True, wet=0.38),
    "mangrove": IslandConfig(bpm=74, scale="minor_pentatonic", progression=CLUSTER_PROGRESSION, melody="kalimba", pad="pad", perc=None, root_shift=-1, wet=0.45, room=0.8),
    "whitesands": IslandConfig(bpm=88, scale="major_pentatonic", progression=BRIGHT_PROGRESSION, melody="marimba", pad="pad", perc="full", root_shift=2, arpeggio=True),
    "mesa": IslandConfig(bpm=78, scale="minor_pentatonic", progression=DEFAULT_PROGRESSION, melody="marimba", pad="strings", perc="light", wet=0.4, room=0.78),
}


def _explore_variant(island: str) -> tuple:
    cfg = ISLAND_CONFIGS[island]
    beats_per_bar = 4
    bpm = cfg["bpm"]
    root_freq = semitone_freq(ROOT, cfg.get("root_shift", 0))
    phrase_bars = 8
    n_phrases = 5
    total_bars = phrase_bars * n_phrases
    tracks: list[Track] = []
    flute_phrases: list = []

    for p in range(n_phrases):
        bar_offset = p * phrase_bars
        if cfg["melody"]:
            add_melody_phrase(tracks, cfg["melody"], root_freq, cfg["scale"], beats_per_bar, bar_offset, pan=(-0.12 if p % 2 == 0 else 0.12))
        if cfg.get("flute"):
            add_flute_phrase(flute_phrases, root_freq, cfg["scale"], beats_per_bar, bar_offset, octave_shift=1, vel_scale=0.75)
        add_pad(tracks, cfg["pad"], root_freq, beats_per_bar, bar_offset, cfg["progression"], vel=0.55 if cfg["pad"] == "tremolo_strings" else 0.45)
        add_bass(tracks, root_freq, beats_per_bar, bar_offset, cfg["progression"], vel=0.5)
        if cfg.get("arpeggio"):
            add_arpeggio(tracks, "guitar", root_freq, beats_per_bar, bar_offset, cfg["progression"], vel=0.3, pan=0.12 if p % 2 == 0 else -0.12)
        perc = cfg["perc"]
        if perc:
            add_percussion(tracks, beats_per_bar, bar_offset, phrase_bars, density=perc)

    return tracks, flute_phrases, total_bars, bpm, beats_per_bar, cfg.get("wet", 0.32), cfg.get("room", 0.65)


def _night() -> tuple:
    bpm, beats_per_bar = 70, 4
    root_freq = semitone_freq(ROOT, -1)
    scale = "minor_pentatonic"
    bars = 16
    tracks: list[Track] = []
    add_pad(tracks, "pad", root_freq, beats_per_bar, 0, [(0, "min", 8), (8, "maj", 8)], vel=0.26)

    kalimba_t = Track(instrument="kalimba", humanize_timing_s=0.05)
    positions_beats = [2, 9, 15, 23, 30, 40, 47, 55, 60]
    degree_cycle = [0, 4, 7, 2, 9]
    for i, beat in enumerate(positions_beats):
        if beat < bars * beats_per_bar:
            freq = degree_freq(root_freq, scale, degree_cycle[i % len(degree_cycle)])
            kalimba_t.events.append((beat, 3.0, freq, 0.32, 0.0 if i % 2 == 0 else 0.2))
    tracks.append(kalimba_t)

    return tracks, [], bars, bpm, beats_per_bar, 0.45, 0.8


def _tension() -> tuple:
    bpm, beats_per_bar = 90, 4
    root_freq = semitone_freq(ROOT, -2)
    bars = 16
    tracks: list[Track] = []

    pulse = Track(instrument="bass", humanize_timing_s=0.006, humanize_velocity_amt=0.08)
    low_freq = semitone_freq(root_freq, -12)
    for bar in range(bars):
        for beat in range(beats_per_bar):
            pulse.events.append((bar * beats_per_bar + beat, 0.9, low_freq, 0.55, 0.0))
    tracks.append(pulse)

    add_pad(tracks, "tremolo_strings", root_freq, beats_per_bar, 0, [(0, "min", 8), (6, "min", 8)], vel=0.4, octave_shift=0)
    add_percussion(tracks, beats_per_bar, 0, bars, density="sparse")

    return tracks, [], bars, bpm, beats_per_bar, 0.35, 0.7


def _sea() -> tuple:
    bpm, beats_per_bar = 152, 6  # "tiempo" = corchea; pulso con puntillo equivalente a ~76 bpm.
    root_freq = ROOT
    scale = "major_pentatonic"
    bars = 16
    prog = [(0, "maj", 4), (5, "maj", 4), (7, "maj", 4), (0, "maj", 4)]
    tracks: list[Track] = []

    add_pad(tracks, "pad", root_freq, beats_per_bar, 0, prog, vel=0.38)
    add_bass(tracks, root_freq, beats_per_bar, 0, prog, vel=0.55)

    arp = Track(instrument="kalimba", humanize_timing_s=0.02)
    d0, d1, d2 = 0, 2, 4
    for bar in range(bars):
        base = bar * beats_per_bar
        arp.events.append((base, 2.6, degree_freq(root_freq, scale, d0), 0.55, -0.15))
        arp.events.append((base + 3, 1.3, degree_freq(root_freq, scale, d1), 0.4, 0.0))
        arp.events.append((base + 4.5, 1.3, degree_freq(root_freq, scale, d2), 0.35, 0.15))
    tracks.append(arp)

    shaker_t = Track(instrument="shaker", humanize_timing_s=0.015)
    for bar in range(bars):
        base = bar * beats_per_bar
        for e in range(beats_per_bar):
            shaker_t.events.append((base + e, 0.9, 1.0, 0.12, 0.0))
    tracks.append(shaker_t)

    return tracks, [], bars, bpm, beats_per_bar, 0.4, 0.75


# Motivo corto y en penumbra (2 compases) para insinuar melodia bajo la
# tormenta sin distraer de la tension: mismo perfil en arco que el leitmotiv,
# a menor escala.
STORM_MOTIF: list[tuple[int | None, float, float]] = [
    (0, 2, 0.55), (None, 1, 0.0), (3, 1, 0.5),
    (5, 2, 0.6), (3, 1, 0.45), (0, 1, 0.4),
]
assert sum(d[1] for d in STORM_MOTIF) == 8.0


def _storm() -> tuple:
    """Tormenta/ciclon (GDD §5.2, §9.3): peligro atmosferico, no depredador.
    Drone grave sostenido + cuerdas en tremolo sobre armonia en clusters
    (sus2/sus4, ya "inestable" de por si) + rafagas de shaker con intensidad
    que sube y baja cada 4 compases (el oleaje de viento) + un motivo de
    kalimba lejano y apagado. Bucle: la tormenta puede durar lo que tarde el
    jugador en resguardarse."""
    bpm, beats_per_bar = 64, 4
    root_freq = semitone_freq(ROOT, -4)
    bars = 16
    prog = CLUSTER_PROGRESSION * 2  # 8 compases -> dos vueltas para los 16 de la pieza
    tracks: list[Track] = []

    drone = Track(instrument="bass", humanize_timing_s=0.025, humanize_velocity_amt=0.1)
    low_freq = semitone_freq(root_freq, -12)
    for bar in range(bars):
        drone.events.append((bar * beats_per_bar, beats_per_bar * 0.98, low_freq, 0.55, 0.0))
    tracks.append(drone)

    add_pad(tracks, "tremolo_strings", root_freq, beats_per_bar, 0, prog, vel=0.5, octave_shift=0)
    add_percussion(tracks, beats_per_bar, 0, bars, density="sparse")

    gust = Track(instrument="shaker", humanize_timing_s=0.025, humanize_velocity_amt=0.35)
    for bar in range(bars):
        base = bar * beats_per_bar
        intensity = 0.14 + 0.12 * ((bar % 4) / 3.0)  # oleada de intensidad cada 4 compases: rafagas de viento
        for eighth in range(beats_per_bar * 2):
            gust.events.append((base + eighth * 0.5, 0.45, 1.0, intensity, 0.0))
    tracks.append(gust)

    for bar_offset in range(0, bars, 2):
        pan = -0.1 if (bar_offset // 2) % 2 == 0 else 0.1
        add_melody_phrase(tracks, "kalimba", root_freq, "minor_pentatonic", beats_per_bar, bar_offset, vel_scale=0.5, pan=pan, degrees=STORM_MOTIF)

    return tracks, [], bars, bpm, beats_per_bar, 0.42, 0.82


def _credits() -> tuple:
    """Creditos: reprise calida del leitmotiv a tempo de paseo, con la
    guitarra Karplus-Strong llevando un arpegio de fingerpicking bajo el pad
    de cuerdas -la unica pieza donde la guitarra es protagonista y no solo
    color- y la flauta asomando en las frases pares. No es un bucle: tiene
    una duracion fija pensada para acompañar el rodillo de creditos."""
    bpm, beats_per_bar = 76, 4
    root_freq = ROOT
    scale = "major_pentatonic"
    phrase_bars = 8
    n_phrases = 4
    total_bars = phrase_bars * n_phrases
    tracks: list[Track] = []
    flute_phrases: list = []

    for p in range(n_phrases):
        bar_offset = p * phrase_bars
        add_melody_phrase(tracks, "marimba", root_freq, scale, beats_per_bar, bar_offset, pan=-0.12)
        add_melody_phrase(tracks, "kalimba", root_freq, scale, beats_per_bar, bar_offset, octave_shift=1, vel_scale=0.55, pan=0.18)
        add_arpeggio(tracks, "guitar", root_freq, beats_per_bar, bar_offset, BRIGHT_PROGRESSION, vel=0.4, pan=-0.3 if p % 2 == 0 else 0.3)
        add_pad(tracks, "strings", root_freq, beats_per_bar, bar_offset, BRIGHT_PROGRESSION, vel=0.48 + 0.04 * p)
        add_bass(tracks, root_freq, beats_per_bar, bar_offset, BRIGHT_PROGRESSION, vel=0.55)
        add_percussion(tracks, beats_per_bar, bar_offset, phrase_bars, density="full" if p > 0 else "light")
        if p % 2 == 1:
            add_flute_phrase(flute_phrases, root_freq, scale, beats_per_bar, bar_offset, octave_shift=1, vel_scale=0.7)

    return tracks, flute_phrases, total_bars, bpm, beats_per_bar, 0.4, 0.82


DISCOVERY_FRAGMENTS: dict[str, DiscoveryFragment] = {
    "01": DiscoveryFragment(instrument="marimba", bpm=90, octave_shift=1, degrees=[(0, 0.5, 0.9), (4, 0.5, 0.95), (7, 1.0, 1.0)]),
    "02": DiscoveryFragment(instrument="kalimba", bpm=88, octave_shift=1, degrees=[(0, 0.4, 0.8), (2, 0.4, 0.85), (4, 0.4, 0.9), (7, 1.2, 1.0)]),
    "03": DiscoveryFragment(instrument="marimba", bpm=92, octave_shift=1, degrees=[(4, 0.4, 0.9), (7, 0.4, 0.95), (9, 0.4, 1.0), (12, 1.4, 1.0)]),
    "04": DiscoveryFragment(instrument="kalimba", bpm=76, octave_shift=0, degrees=[(7, 0.6, 0.7), (4, 0.6, 0.65), (0, 1.4, 0.6)]),
}


def _discovery(variant: str) -> tuple:
    cfg = DISCOVERY_FRAGMENTS[variant]
    beats_per_bar = 4
    tracks: list[Track] = []
    add_melody_phrase(tracks, cfg["instrument"], ROOT, "major_pentatonic", beats_per_bar, 0, octave_shift=cfg["octave_shift"], degrees=cfg["degrees"])
    total_beats = sum(d[1] for d in cfg["degrees"])
    return tracks, [], total_beats / beats_per_bar, cfg["bpm"], beats_per_bar, 0.35, 0.7


FINALE_CONFIGS: dict[str, FinaleConfig] = {
    "rescue": FinaleConfig(bpm=84, scale="major_pentatonic", progression=BRIGHT_PROGRESSION, root_shift=0, flute=False),
    "voyage": FinaleConfig(bpm=80, scale="minor_pentatonic", progression=DEFAULT_PROGRESSION, root_shift=0, flute=True),
    "stay": FinaleConfig(bpm=72, scale="minor_pentatonic", progression=DEFAULT_PROGRESSION, root_shift=-1, flute=False),
}


def _finale(variant: str) -> tuple:
    cfg = FINALE_CONFIGS[variant]
    beats_per_bar = 4
    bpm = cfg["bpm"]
    root_freq = semitone_freq(ROOT, cfg.get("root_shift", 0))
    phrase_bars = 8
    n_phrases = 4
    total_bars = phrase_bars * n_phrases
    tracks: list[Track] = []
    flute_phrases: list = []

    for p in range(n_phrases):
        bar_offset = p * phrase_bars
        add_melody_phrase(tracks, "marimba", root_freq, cfg["scale"], beats_per_bar, bar_offset, pan=-0.1)
        add_melody_phrase(tracks, "kalimba", root_freq, cfg["scale"], beats_per_bar, bar_offset, octave_shift=1, vel_scale=0.6, pan=0.2)
        add_pad(tracks, "strings", root_freq, beats_per_bar, bar_offset, cfg["progression"], vel=0.52)
        add_bass(tracks, root_freq, beats_per_bar, bar_offset, cfg["progression"], vel=0.58)
        add_percussion(tracks, beats_per_bar, bar_offset, phrase_bars, density="full" if p > 0 else "light")
        if cfg.get("flute") and p % 2 == 1:
            add_flute_phrase(flute_phrases, root_freq, cfg["scale"], beats_per_bar, bar_offset, octave_shift=1, vel_scale=0.7)

    return tracks, flute_phrases, total_bars, bpm, beats_per_bar, 0.38, 0.8


def _menu() -> tuple:
    bpm, beats_per_bar = 72, 4
    tracks: list[Track] = []
    for p in range(2):
        bar_offset = p * 8
        add_melody_phrase(tracks, "kalimba", ROOT, "minor_pentatonic", beats_per_bar, bar_offset, vel_scale=0.8)
        add_pad(tracks, "pad", ROOT, beats_per_bar, bar_offset, DEFAULT_PROGRESSION, vel=0.36)
    return tracks, [], 16, bpm, beats_per_bar, 0.4, 0.75


# ---------------------------------------------------------------------------
# Flauta de bambu diegetica (GDD §8.12): el jugador toca 5 notas de la
# pentatonica mayor una octava sobre ROOT (el mismo registro que las frases
# de flauta de la banda sonora, `add_flute_phrase` con octave_shift=1). Se
# exporta UNA nota en la tonica y el juego la transpone a cada grado con el
# multiplicador de tono 2^(semitonos/12): cinco muestras casi identicas no
# aportarian nada y la transposicion maxima (9 semitonos) no degrada el timbre
# de una flauta sintetica.
# ---------------------------------------------------------------------------
FLUTE_SAMPLE_NAME = "sfx_flute_note"
FLUTE_SCALE = "major_pentatonic"
FLUTE_SAMPLE_FREQ = semitone_freq(ROOT, 12)  # D4
FLUTE_NOTE_BEATS = 3.0
FLUTE_NOTE_BPM = 120.0  # 3 tiempos a 120 bpm: 1,5 s de nota sostenida


def flute_note_sample(name: str = FLUTE_SAMPLE_NAME) -> np.ndarray:
    """Una nota de flauta mono y sostenida en FLUTE_SAMPLE_FREQ, con el mismo
    contorno (ataque, vibrato, soplido) que las frases de la banda sonora."""
    from .sequencer import render_flute_phrase

    rng = rng_for(name)
    # Velocidad 0.45: deja la nota en torno a -15 LUFS, dentro del rango de
    # los efectos cortos (test_loudness) y a la altura de la musica.
    notes = [(0.0, FLUTE_NOTE_BEATS, FLUTE_SAMPLE_FREQ, 0.45)]
    audio, _start = render_flute_phrase(notes, FLUTE_NOTE_BPM, SR, rng)
    # Cola corta: la nota termina en silencio aunque el juego la corte antes.
    fade_n = min(int(0.12 * SR), audio.shape[-1] // 4)
    audio = audio.copy()
    audio[-fade_n:] *= np.linspace(1.0, 0.0, fade_n)
    return audio


def _dispatch(name: str) -> tuple:
    if name == "mus_theme":
        return _theme()
    if name.startswith("mus_explore_"):
        return _explore_variant(name[len("mus_explore_"):])
    if name == "mus_night":
        return _night()
    if name.startswith("mus_discovery_"):
        return _discovery(name[len("mus_discovery_"):])
    if name == "mus_tension":
        return _tension()
    if name == "mus_storm":
        return _storm()
    if name == "mus_sea":
        return _sea()
    if name.startswith("mus_finale_"):
        return _finale(name[len("mus_finale_"):])
    if name == "mus_menu":
        return _menu()
    if name == "mus_credits":
        return _credits()
    raise KeyError(f"pieza de musica desconocida: {name}")


def is_loop(name: str) -> bool:
    return not (name in ("mus_theme", "mus_credits") or name.startswith(("mus_discovery_", "mus_finale_")))


def metadata(name: str) -> dict:
    _tracks, _flute, total_bars, bpm, beats_per_bar, _wet, _room = _dispatch(name)
    return {"bpm": bpm, "bars": round(total_bars, 3), "beats_per_bar": beats_per_bar, "loop": is_loop(name)}


def generate(name: str) -> np.ndarray:
    tracks, flute_phrases, total_bars, bpm, beats_per_bar, wet, room = _dispatch(name)
    rng = rng_for(name)
    sec_per_beat = 60.0 / bpm
    nominal_beats = total_bars * beats_per_bar
    loop = is_loop(name)

    if loop:
        fade_bars = 1.0 if beats_per_bar >= 4 else 2.0
        padded_beats = nominal_beats + fade_bars * beats_per_bar
    else:
        padded_beats = nominal_beats + 3.0 / sec_per_beat  # ~3 s de cola para la reverberacion

    audio = render_song(bpm, SR, padded_beats, tracks, flute_phrases, seed_rng=rng, reverb_wet=wet, reverb_room=room)

    if loop:
        loop_len = int(round(nominal_beats * sec_per_beat * SR))
        fade_len = audio.shape[-1] - loop_len
        audio = seamless_loop(audio, loop_len, fade_len)
    else:
        fade_n = min(int(2.0 * SR), audio.shape[-1] // 4)
        if fade_n > 1:
            ramp = np.linspace(1.0, 0.0, fade_n)
            audio = audio.copy()
            audio[:, -fade_n:] *= ramp

    return audio
