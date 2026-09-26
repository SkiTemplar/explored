"""Teoria musical minima: temperamento igual, escalas pentatonicas y modales,
y triadas. Todo en semitonos relativos a una nota raiz en Hz, para no
depender de nombres de nota."""

from __future__ import annotations

# Grados de cada escala en semitonos sobre la tonica. Las pentatonicas evitan
# el semitono y el tritono: suenan bien pase lo que pase, ideales para una
# melodia "de isla" sin aristas. Dorico y mixolidio dan el color modal
# (ni mayor "de anuncio" ni menor "triste") que pide el encargo.
SCALE_STEPS: dict[str, list[int]] = {
    "major_pentatonic": [0, 2, 4, 7, 9],
    "minor_pentatonic": [0, 3, 5, 7, 10],
    "dorian": [0, 2, 3, 5, 7, 9, 10],
    "mixolydian": [0, 2, 4, 5, 7, 9, 10],
    "aeolian": [0, 2, 3, 5, 7, 8, 10],
}

TRIAD_INTERVALS: dict[str, list[int]] = {
    "maj": [0, 4, 7],
    "min": [0, 3, 7],
    "sus2": [0, 2, 7],
    "sus4": [0, 5, 7],
    "maj7": [0, 4, 7, 11],
    "min7": [0, 3, 7, 10],
}


def semitone_freq(root_freq: float, semitones: float) -> float:
    """Frecuencia a `semitones` (puede ser fraccionario, para micro-afinacion)
    de `root_freq`, en temperamento igual."""
    return root_freq * (2.0 ** (semitones / 12.0))


def degree_freq(root_freq: float, mode: str, degree: int) -> float:
    """Frecuencia del grado `degree` de la escala `mode` sobre `root_freq`.

    `degree` es un indice de escala (no un semitono): 0 es la tonica, y los
    indices negativos o mayores que el numero de grados envuelven a octavas
    inferiores/superiores automaticamente (divmod ya redondea hacia -inf,
    que es justo el comportamiento que hace falta aqui)."""
    steps = SCALE_STEPS[mode]
    octave, pos = divmod(degree, len(steps))
    return semitone_freq(root_freq, steps[pos] + 12 * octave)


def chord_freqs(root_freq: float, root_semitones: float, quality: str) -> list[float]:
    """Frecuencias de una triada (o tetrada) construida en semitonos sobre
    `root_freq` (no sobre la escala: permite acordes "prestados", habituales
    en armonia modal, como el bVII o el bVI)."""
    return [semitone_freq(root_freq, root_semitones + iv) for iv in TRIAD_INTERVALS[quality]]
