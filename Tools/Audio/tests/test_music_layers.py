"""`Content/Data/music_layers.json`: la descripcion de la musica adaptativa que
lee el director de musica del juego. No renderiza audio (solo partituras), asi
que estos tests son rapidos."""

from __future__ import annotations

import pytest

from explored_audio.catalog import MUSIC_ISLANDS, MUSIC_NAMES
from explored_audio.music import compose
from explored_audio.music.layers import build_payload, default_output_path, role_of, to_json
from explored_audio.music.theory import semitone_freq

ROLES = {"theme", "menu", "explore", "night", "tension", "storm", "sea", "discovery", "finale", "credits"}


@pytest.fixture(scope="module")
def payload() -> dict:
    return build_payload()


def test_el_json_del_repo_esta_al_dia(payload):
    """Si falla: `uv run explored-audio music-layers` desde Tools/Audio."""
    path = default_output_path()
    assert path.exists(), f"falta {path}"
    assert path.read_text(encoding="utf-8") == to_json(payload)


def test_cubre_todas_las_piezas_del_catalogo_con_un_papel(payload):
    ids = [p["id"] for p in payload["pieces"]]
    assert ids == MUSIC_NAMES
    assert {p["role"] for p in payload["pieces"]} == ROLES
    for piece in payload["pieces"]:
        assert piece["asset"] == f"/Game/Generated/Audio/Musica/{piece['id']}.{piece['id']}"


def test_una_pieza_de_exploracion_por_isla(payload):
    explore = {p["variant"]: p["id"] for p in payload["pieces"] if p["role"] == "explore"}
    assert set(explore) == set(MUSIC_ISLANDS)


def test_tempo_y_compases_coinciden_con_la_partitura(payload):
    for piece in payload["pieces"]:
        meta = compose.metadata(piece["id"])
        assert piece["bpm"] == meta["bpm"]
        assert piece["beats_per_bar"] == meta["beats_per_bar"]
        assert piece["loop"] == compose.is_loop(piece["id"])
        assert piece["bars"] > 0
        expected_bar = 60.0 / meta["bpm"] * meta["beats_per_bar"]
        assert piece["seconds_per_bar"] == pytest.approx(expected_bar, abs=1e-5)
        assert piece["duration_s"] == pytest.approx(expected_bar * meta["bars"], abs=1e-4)


def test_los_bucles_tienen_compases_enteros(payload):
    """El director cuantiza al compas y cuenta vueltas: un bucle con un compas
    a medias desplazaria la rejilla en cada vuelta."""
    for piece in payload["pieces"]:
        if piece["loop"]:
            assert float(piece["bars"]).is_integer(), piece["id"]


def test_variaciones_diurnas_propia_primero_y_sin_repetir(payload):
    variants = payload["day_variants"]
    assert set(variants) == set(MUSIC_ISLANDS)
    for island, pieces in variants.items():
        assert pieces[0] == f"mus_explore_{island}"
        assert len(pieces) >= 2, "sin hermanas no se puede evitar repetir la misma pieza"
        assert len(set(pieces)) == len(pieces)
        scale = compose.ISLAND_CONFIGS[island].scale
        for other in pieces[1:]:
            assert role_of(other)[0] == "explore"
            assert compose.ISLAND_CONFIGS[role_of(other)[1]].scale == scale


def test_flauta_pentatonica_de_cinco_notas(payload):
    flute = payload["flute"]
    assert flute["sample"] == compose.FLUTE_SAMPLE_NAME
    assert flute["asset"] == f"/Game/Generated/Audio/Efectos/{flute['sample']}.{flute['sample']}"
    assert flute["semitones"] == [0, 2, 4, 7, 9]
    assert flute["sample_hz"] == pytest.approx(semitone_freq(compose.ROOT, 12), abs=1e-3)


def test_nota_de_flauta_mono_determinista_y_en_rango():
    import numpy as np

    from explored_audio.build import render_by_name
    from explored_audio.constants import PEAK_CEILING_LINEAR
    from explored_audio.levels import lufs_approx

    a = render_by_name(compose.FLUTE_SAMPLE_NAME)
    b = render_by_name(compose.FLUTE_SAMPLE_NAME)
    assert a.ndim == 1
    assert np.array_equal(a, b)
    assert np.max(np.abs(a)) <= PEAK_CEILING_LINEAR + 1e-6
    assert np.all(np.isfinite(a))
    assert -36.0 <= lufs_approx(a) <= -12.0
