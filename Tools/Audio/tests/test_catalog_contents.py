"""El catalogo debe contener exactamente la primera tanda pedida: comprueba
los nombres, no solo el recuento total."""

from __future__ import annotations


def test_ambientes_esperados(catalog):
    names = {spec.name for spec in catalog if spec.category == "Ambiente"}
    assert names == {
        "amb_ocean_calm", "amb_ocean_rough", "amb_wind_light", "amb_wind_strong",
        "amb_jungle_day", "amb_jungle_night", "amb_rain_light", "amb_rain_heavy",
        "amb_rain_on_leaves", "amb_rain_on_thatch", "amb_wind_palms", "amb_stream", "amb_underwater",
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
    assert {
        "sfx_ui_click", "sfx_ui_hover", "sfx_ui_open", "sfx_ui_close",
        "sfx_ui_journal_open", "sfx_ui_page_turn", "sfx_ui_discovery_notify",
    } <= names


def test_fauna_esperada(catalog):
    names = {spec.name for spec in catalog}
    for species in ("crab", "turtle"):
        for i in range(1, 4):
            assert f"sfx_{species}_{i:02d}" in names


def test_sin_fauna_terrestre(catalog):
    # GDD §10 y §12: el archipielago no tiene fauna terrestre.
    for spec in catalog:
        for species in ("monkey", "crocodile", "boar"):
            assert species not in spec.name, f"{spec.name}: fauna terrestre en el catalogo"


def test_herramientas_y_fabricacion_esperadas(catalog):
    names = {spec.name for spec in catalog}
    assert {
        "sfx_carve_01", "sfx_carve_02", "sfx_stone_knap_01", "sfx_stone_knap_02",
        "sfx_tie_cord", "sfx_wood_saw", "sfx_fire_ignite", "sfx_cooking_sizzle_loop",
    } <= names


def test_construccion_esperada(catalog):
    names = {spec.name for spec in catalog}
    assert {
        "sfx_build_place", "sfx_build_snap", "sfx_build_thatch",
        "sfx_build_hammer_01", "sfx_build_hammer_02", "sfx_build_dismantle",
    } <= names


def test_huerto_esperado(catalog):
    names = {spec.name for spec in catalog}
    assert {
        "sfx_garden_dig_01", "sfx_garden_dig_02", "sfx_garden_dig_03",
        "sfx_garden_water", "sfx_garden_harvest_01", "sfx_garden_harvest_02",
    } <= names


def test_cartografia_esperada(catalog):
    names = {spec.name for spec in catalog}
    assert {
        "sfx_map_pen_scratch_01", "sfx_map_pen_scratch_02", "sfx_map_unfold", "sfx_map_stamp",
    } <= names


def test_agua_esperada(catalog):
    names = {spec.name for spec in catalog}
    assert {
        "sfx_swim_stroke_01", "sfx_swim_stroke_02", "sfx_dive_splash",
        "sfx_bubbles_01", "sfx_bubbles_02", "sfx_paddle_stroke_01", "sfx_paddle_stroke_02",
        "sfx_sail_flap",
    } <= names


def test_clima_esperado(catalog):
    names = {spec.name for spec in catalog}
    assert {"sfx_wind_gust_01", "sfx_wind_gust_02"} <= names


def test_cuerpo_esperado(catalog):
    names = {spec.name for spec in catalog}
    assert {
        "sfx_eat_01", "sfx_eat_02", "sfx_drink", "sfx_breath_tired", "sfx_heartbeat_low_loop",
    } <= names


def test_musica_esperada(catalog):
    names = {spec.name for spec in catalog if spec.category == "Musica"}
    assert names == {
        "mus_theme",
        "mus_explore_landing", "mus_explore_emerald", "mus_explore_smoke", "mus_explore_teeth",
        "mus_explore_mangrove", "mus_explore_whitesands", "mus_explore_mesa",
        "mus_night", "mus_tension", "mus_storm", "mus_sea",
        "mus_discovery_01", "mus_discovery_02", "mus_discovery_03", "mus_discovery_04",
        "mus_finale_rescue", "mus_finale_voyage", "mus_finale_stay",
        "mus_menu", "mus_credits",
    }


def test_flauta_diegetica(catalog):
    flute = [spec for spec in catalog if spec.name == "sfx_flute_note"]
    assert len(flute) == 1 and flute[0].category == "Efectos" and not flute[0].is_loop
