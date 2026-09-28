"""Aceptacion del render acustico de la musica (soundfont real + FluidSynth,
ver `music/soundfont.py` y `music/midi_render.py`).

`test_basic_properties.py` ya comprueba, para TODO el catalogo (incluida la
musica), que nada esta en silencio ni clipea; `test_loudness.py` ya comprueba
que la sonoridad cae en su rango. Lo que anaden estos tests es especifico del
cambio de sintesis a muestras reales: que el timbre sea plausible como
instrumento acustico -sin poder escucharlo, se valida por metricas- y que la
duracion de lo generado coincida con lo que promete `music_layers.json`."""

from __future__ import annotations

from pathlib import Path

import numpy as np
import pytest

from explored_audio.constants import SAMPLE_RATE
from explored_audio.music import compose, soundfont
from explored_audio.music.layers import build_payload


def _spectral_centroid(audio: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    mono = audio.mean(axis=0) if audio.ndim == 2 else audio
    spec = np.abs(np.fft.rfft(mono))
    freqs = np.fft.rfftfreq(len(mono), d=1.0 / sr)
    total = float(np.sum(spec))
    return float(np.sum(freqs * spec) / total) if total > 0 else 0.0


CENTROID_CHECK_NAMES = ["mus_theme", "mus_storm", "mus_menu", "mus_sea", "mus_explore_landing"]


@pytest.mark.parametrize("name", CENTROID_CHECK_NAMES)
def test_centroide_espectral_en_rango_acustico(rendered, name):
    """Ni un grave sordo (ya lo detectarian `test_sin_continua`/
    `test_sin_clipping` si degenerase a silencio o continua) ni un agudo
    aserrado de sintesis barata sin filtrar: el centroide de una mezcla real
    de marimba/guitarra/cuerdas/percusion cae en un rango medio."""
    centroid = _spectral_centroid(rendered[name])
    assert 300.0 <= centroid <= 6000.0, f"{name}: centroide {centroid:.0f} Hz fuera de lo esperado"


def test_nota_de_flauta_no_es_onda_pura(rendered):
    """La muestra de flauta diegetica (`sfx_flute_note`, patch GM Flute del
    soundfont) debe tener el espectro irregular de un instrumento acustico
    real: armonicos pares presentes (una onda cuadrada solo tiene impares) y
    energia fuera de la rejilla armonica exacta -soplido, vibrato, formantes-
    (una sierra matematica pondria casi toda su energia justo sobre esa
    rejilla)."""
    audio = rendered[compose.FLUTE_SAMPLE_NAME]
    n = len(audio)
    spec = np.abs(np.fft.rfft(audio * np.hanning(n)))
    freqs = np.fft.rfftfreq(n, d=1.0 / SAMPLE_RATE)
    f0 = compose.FLUTE_SAMPLE_FREQ

    def band_energy(center: float, bw: float = 8.0) -> float:
        mask = (freqs >= center - bw) & (freqs <= center + bw)
        return float(np.sum(spec[mask] ** 2))

    odd = sum(band_energy(f0 * k) for k in (1, 3, 5, 7, 9))
    even = sum(band_energy(f0 * k) for k in (2, 4, 6, 8, 10))
    assert odd > 0.0, "sin energia ni siquiera en el fundamental: la nota esta rota"
    assert even / odd > 0.2, (
        f"armonicos pares casi ausentes (ratio {even / odd:.3f}): eso es una onda "
        f"cuadrada, no una muestra de flauta"
    )

    total_energy = float(np.sum(spec**2))
    grid_energy = sum(band_energy(f0 * k, bw=8.0) for k in range(1, 20))
    off_grid_frac = 1.0 - grid_energy / total_energy
    assert off_grid_frac > 0.03, (
        f"casi toda la energia ({1 - off_grid_frac:.1%}) cae justo sobre la rejilla "
        f"armonica: eso es sintesis periodica pura, no una muestra real"
    )


def test_duracion_coincide_con_music_layers(catalog, rendered):
    """Cruza la duracion realmente renderizada con la que declara
    `Content/Data/music_layers.json` (que el director de musica del juego usa
    para cuantizar transiciones): si el renderizador acustico se desvia de la
    partitura, tiene que notarse aqui."""
    payload = build_payload()
    duration_by_id = {p["id"]: p for p in payload["pieces"]}
    checked = 0
    for spec in catalog:
        if spec.category != "Musica":
            continue
        piece = duration_by_id[spec.name]
        actual_s = rendered[spec.name].shape[-1] / SAMPLE_RATE
        if piece["loop"]:
            assert actual_s == pytest.approx(piece["duration_s"], abs=0.05), spec.name
        else:
            # Las piezas que no son bucle conservan cola de reverberacion y
            # liberacion natural del instrumento mas alla de la duracion
            # "nominal" que cuenta el JSON: nunca deberian salir mas cortas.
            assert actual_s >= piece["duration_s"] - 0.05, spec.name
        checked += 1
    assert checked == len(payload["pieces"])


def test_soundfont_cacheado_y_verificado():
    path = soundfont.ensure_soundfont()
    assert path.exists()
    assert soundfont._sha256(path) == soundfont.SOUNDFONT_SHA256


def test_licencia_del_soundfont_documentada():
    doc = Path(__file__).resolve().parents[1] / "THIRD_PARTY_SOUNDFONT.md"
    assert doc.exists()
    text = doc.read_text(encoding="utf-8")
    assert "MIT" in text
    assert soundfont.SOUNDFONT_FILENAME in text
