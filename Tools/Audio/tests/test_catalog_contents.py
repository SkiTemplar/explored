"""El catalogo debe contener exactamente la primera tanda pedida: comprueba
los nombres, no solo el recuento total."""

from __future__ import annotations


def test_ambientes_esperados(catalog):
    names = {spec.name for spec in catalog if spec.category == "Ambiente"}
    assert names == {
        "amb_ocean_calm", "amb_ocean_rough", "amb_wind_light", "amb_wind_strong",
        "amb_jungle_day", "amb_jungle_night", "amb_rain_light", "amb_rain_heavy",
        "amb_stream", "amb_underwater",
    }


def test_pasos_esperados(catalog):
    names = {spec.name for spec in catalog}
    for material in ("sand", "grass", "rock", "wood", "water"):
        for i in range(1, 5):
            assert f"sfx_footstep_{material}_{i:02d}" in names


def test_golpes_y_recogida_esperados(catalog):
    names = {spec.name for spec in catalog}
    for i in range(1, 4):
        assert f"sfx_wood_chop_{i:02d}" in names
        assert f"sfx_stone_hit_{i:02d}" in names
        assert f"sfx_pickup_{i:02d}" in names
        assert f"sfx_thunder_{i:02d}" in names


def test_chapoteos_fuego_y_pajaros_esperados(catalog):
    names = {spec.name for spec in catalog}
    assert {"sfx_splash_small", "sfx_splash_big", "sfx_fire_loop"} <= names
    for species in ("parrot", "gull", "songbird"):
        for i in range(1, 4):
            assert f"sfx_bird_{species}_{i:02d}" in names


def test_ui_esperados(catalog):
    names = {spec.name for spec in catalog}
    assert {"sfx_ui_click", "sfx_ui_hover", "sfx_ui_open", "sfx_ui_close"} <= names
