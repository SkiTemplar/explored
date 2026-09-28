"""Determinismo: generar el mismo sonido dos veces (dos llamadas independientes,
sin compartir estado) debe producir exactamente los mismos bytes de WAV.

La primera generacion es la cache de sesion (`rendered`, hecha al principio
de la bateria, antes de todos los demas tests); la segunda se hace aqui. Asi
se compara todo el catalogo con una sola generacion extra y, ademas, se
detecta cualquier estado global que otro test o generador haya alterado
entre medias."""

from __future__ import annotations

import numpy as np

from explored_audio.build import render_sound
from explored_audio.io_utils import to_wav_bytes
from explored_audio.rng import rng_for, seed_for

# Muestra variada (ambientes, pasos, golpes, pajaros, bucles, UI, musica) en
# la que ademas se comparan los bytes del WAV exportado.
WAV_BYTES_NAMES = {
    "amb_ocean_calm",
    "amb_jungle_night",
    "sfx_footstep_rock_02",
    "sfx_wood_chop_01",
    "sfx_bird_gull_03",
    "sfx_fire_loop",
    "sfx_ui_click",
    "sfx_crab_02",
    "amb_wind_palms",
    "sfx_cooking_sizzle_loop",
    "mus_theme",
    "mus_storm",
}


def test_todo_el_catalogo_es_determinista(catalog, rendered):
    names = {spec.name for spec in catalog}
    assert names >= WAV_BYTES_NAMES
    for spec in catalog:
        audio_a = rendered[spec.name]
        audio_b = render_sound(spec)
        assert np.array_equal(audio_a, audio_b), f"{spec.name} no es determinista"
        if spec.name in WAV_BYTES_NAMES:
            assert to_wav_bytes(audio_a) == to_wav_bytes(audio_b), f"{spec.name}: dos generaciones dieron WAV distinto"


def test_la_semilla_depende_solo_del_nombre():
    assert seed_for("sfx_ui_click") == seed_for("sfx_ui_click")
    assert seed_for("sfx_ui_click") != seed_for("sfx_ui_hover")
    assert 0 <= seed_for("amb_ocean_calm") <= 0xFFFFFFFF
    # Nombres no ASCII tambien dan una semilla estable (UTF-8).
    assert seed_for("sfx_piña") == seed_for("sfx_piña")
    np.testing.assert_array_equal(rng_for("x").standard_normal(8), rng_for("x").standard_normal(8))
    assert not np.array_equal(rng_for("x").standard_normal(8), rng_for("y").standard_normal(8))
