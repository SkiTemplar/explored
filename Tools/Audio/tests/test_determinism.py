"""Determinismo: generar el mismo sonido dos veces (dos llamadas independientes,
sin compartir estado) debe producir exactamente los mismos bytes de WAV."""

from __future__ import annotations

import numpy as np
import pytest

from explored_audio.build import render_sound
from explored_audio.io_utils import to_wav_bytes


@pytest.mark.parametrize(
    "name",
    [
        "amb_ocean_calm",
        "amb_jungle_night",
        "sfx_footstep_rock_02",
        "sfx_wood_chop_01",
        "sfx_bird_gull_03",
        "sfx_fire_loop",
        "sfx_ui_click",
        "sfx_crab_02",
        "sfx_cooking_sizzle_loop",
        "mus_theme",
        "mus_storm",
    ],
)
def test_misma_semilla_produce_bytes_identicos(specs_by_name, name):
    spec = specs_by_name[name]
    audio_a = render_sound(spec)
    audio_b = render_sound(spec)

    assert np.array_equal(audio_a, audio_b), f"{name}: dos generaciones dieron arrays distintos"
    assert to_wav_bytes(audio_a) == to_wav_bytes(audio_b), f"{name}: dos generaciones dieron WAV distinto"


def test_todo_el_catalogo_es_determinista(catalog):
    for spec in catalog:
        audio_a = render_sound(spec)
        audio_b = render_sound(spec)
        assert np.array_equal(audio_a, audio_b), f"{spec.name} no es determinista"
