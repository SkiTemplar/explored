"""Catalogo minimo (primera tanda): la lista declarativa de que sonido es
cada cosa, en que categoria vive y si es bucle. `build.py` recorre esta lista
para generar, normalizar, exportar y documentar cada fichero."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

import numpy as np

from .generators import ambience, birds, body, cartography, construction, crafting, fauna, footsteps, garden, impacts, misc_sfx, ui, water
from .music import compose as music_compose


@dataclass(frozen=True)
class SoundSpec:
    name: str
    category: str  # "Ambiente" (estereo, bucle), "Efectos" (mono) o "Musica" (estereo)
    is_loop: bool
    generate: Callable[[str], np.ndarray]


def _footstep_spec(material: str, index: int) -> SoundSpec:
    name = f"sfx_footstep_{material}_{index:02d}"
    return SoundSpec(name, "Efectos", False, lambda n, m=material: footsteps.footstep(n, m))


def _bird_spec(species: str, index: int) -> SoundSpec:
    name = f"sfx_bird_{species}_{index:02d}"
    return SoundSpec(name, "Efectos", False, lambda n, s=species: birds.bird_call(n, s))


def _fauna_spec(species: str, index: int) -> SoundSpec:
    name = f"sfx_{species}_{index:02d}"
    return SoundSpec(name, "Efectos", False, lambda n, s=species: fauna.fauna_call(n, s))


def _music_spec(name: str) -> SoundSpec:
    return SoundSpec(name, "Musica", music_compose.is_loop(name), lambda n: music_compose.generate(n))


def build_catalog() -> list[SoundSpec]:
    specs: list[SoundSpec] = []

    ambience_map = {
        "amb_ocean_calm": ambience.amb_ocean_calm,
        "amb_ocean_rough": ambience.amb_ocean_rough,
        "amb_wind_light": ambience.amb_wind_light,
        "amb_wind_strong": ambience.amb_wind_strong,
        "amb_jungle_day": ambience.amb_jungle_day,
        "amb_jungle_night": ambience.amb_jungle_night,
        "amb_rain_light": ambience.amb_rain_light,
        "amb_rain_heavy": ambience.amb_rain_heavy,
        "amb_rain_on_leaves": ambience.amb_rain_on_leaves,
        "amb_stream": ambience.amb_stream,
        "amb_underwater": ambience.amb_underwater,
    }
    for name, fn in ambience_map.items():
        specs.append(SoundSpec(name, "Ambiente", True, fn))

    for material in ("sand", "grass", "rock", "wood", "water"):
        for i in range(1, 5):
            specs.append(_footstep_spec(material, i))

    for i in range(1, 4):
        specs.append(SoundSpec(f"sfx_wood_chop_{i:02d}", "Efectos", False, impacts.wood_chop))
        specs.append(SoundSpec(f"sfx_stone_hit_{i:02d}", "Efectos", False, impacts.stone_hit))
        specs.append(SoundSpec(f"sfx_pickup_{i:02d}", "Efectos", False, misc_sfx.pickup))
        specs.append(SoundSpec(f"sfx_thunder_{i:02d}", "Efectos", False, misc_sfx.thunder))

    specs.append(SoundSpec("sfx_splash_small", "Efectos", False, misc_sfx.splash_small))
    specs.append(SoundSpec("sfx_splash_big", "Efectos", False, misc_sfx.splash_big))
    specs.append(SoundSpec("sfx_fire_loop", "Efectos", True, misc_sfx.fire_loop))

    for species in ("parrot", "gull", "songbird"):
        for i in range(1, 4):
            specs.append(_bird_spec(species, i))

    specs.append(SoundSpec("sfx_ui_click", "Efectos", False, ui.ui_click))
    specs.append(SoundSpec("sfx_ui_hover", "Efectos", False, ui.ui_hover))
    specs.append(SoundSpec("sfx_ui_open", "Efectos", False, ui.ui_open))
    specs.append(SoundSpec("sfx_ui_close", "Efectos", False, ui.ui_close))
    specs.append(SoundSpec("sfx_ui_journal_open", "Efectos", False, ui.ui_journal_open))
    specs.append(SoundSpec("sfx_ui_page_turn", "Efectos", False, ui.ui_page_turn))
    specs.append(SoundSpec("sfx_ui_discovery_notify", "Efectos", False, ui.ui_discovery_notify))

    # Fauna (biblia de contenido §6), mas alla de las aves ya cubiertas arriba.
    for species in ("monkey", "crocodile", "boar", "crab", "turtle"):
        for i in range(1, 4):
            specs.append(_fauna_spec(species, i))

    # Herramientas y fabricacion (§3.4, §5.3).
    for i in range(1, 3):
        specs.append(SoundSpec(f"sfx_carve_{i:02d}", "Efectos", False, crafting.carve))
        specs.append(SoundSpec(f"sfx_stone_knap_{i:02d}", "Efectos", False, crafting.stone_knap))
    specs.append(SoundSpec("sfx_tie_cord", "Efectos", False, crafting.tie_cord))
    specs.append(SoundSpec("sfx_wood_saw", "Efectos", False, crafting.wood_saw))
    specs.append(SoundSpec("sfx_fire_ignite", "Efectos", False, crafting.fire_ignite))
    specs.append(SoundSpec("sfx_cooking_sizzle_loop", "Efectos", True, crafting.cooking_sizzle))

    # Construccion (§3.10).
    specs.append(SoundSpec("sfx_build_place", "Efectos", False, construction.build_place))
    specs.append(SoundSpec("sfx_build_snap", "Efectos", False, construction.build_snap))
    specs.append(SoundSpec("sfx_build_thatch", "Efectos", False, construction.build_thatch))
    for i in range(1, 3):
        specs.append(SoundSpec(f"sfx_build_hammer_{i:02d}", "Efectos", False, construction.build_hammer))
    specs.append(SoundSpec("sfx_build_dismantle", "Efectos", False, construction.build_dismantle))

    # Huerto: cavar, regar y cosechar.
    for i in range(1, 4):
        specs.append(SoundSpec(f"sfx_garden_dig_{i:02d}", "Efectos", False, garden.garden_dig))
    specs.append(SoundSpec("sfx_garden_water", "Efectos", False, garden.garden_water))
    for i in range(1, 3):
        specs.append(SoundSpec(f"sfx_garden_harvest_{i:02d}", "Efectos", False, garden.garden_harvest))

    # Cartografia: pluma sobre papel, desplegar el mapa y sellar.
    for i in range(1, 3):
        specs.append(SoundSpec(f"sfx_map_pen_scratch_{i:02d}", "Efectos", False, cartography.map_pen_scratch))
    specs.append(SoundSpec("sfx_map_unfold", "Efectos", False, cartography.map_unfold))
    specs.append(SoundSpec("sfx_map_stamp", "Efectos", False, cartography.map_stamp))

    # Agua interactiva (§5.2, `Boats`).
    for i in range(1, 3):
        specs.append(SoundSpec(f"sfx_swim_stroke_{i:02d}", "Efectos", False, water.swim_stroke))
        specs.append(SoundSpec(f"sfx_bubbles_{i:02d}", "Efectos", False, water.bubbles))
        specs.append(SoundSpec(f"sfx_paddle_stroke_{i:02d}", "Efectos", False, water.paddle_stroke))
    specs.append(SoundSpec("sfx_dive_splash", "Efectos", False, water.dive_splash))
    specs.append(SoundSpec("sfx_sail_flap", "Efectos", False, water.sail_flap))

    # Clima: rafaga puntual de viento fuerte (el trueno ya esta arriba, y el
    # colchon continuo de lluvia sobre hojas en `ambience_map`).
    for i in range(1, 3):
        specs.append(SoundSpec(f"sfx_wind_gust_{i:02d}", "Efectos", False, misc_sfx.wind_gust))

    # Cuerpo y necesidades (§5.4, GDD §4.3).
    for i in range(1, 3):
        specs.append(SoundSpec(f"sfx_eat_{i:02d}", "Efectos", False, body.eat))
    specs.append(SoundSpec("sfx_drink", "Efectos", False, body.drink))
    specs.append(SoundSpec("sfx_breath_tired", "Efectos", False, body.breath_tired))
    specs.append(SoundSpec("sfx_heartbeat_low_loop", "Efectos", True, body.heartbeat_low))

    # Musica adaptativa (§9.3): la genera `music.compose`, aqui solo se cablea
    # en el catalogo para que pase por el mismo postproceso, exportacion y
    # manifiesto que el resto de sonidos.
    music_names = [
        "mus_theme",
        *(f"mus_explore_{island}" for island in ("landing", "emerald", "smoke", "teeth", "mangrove", "whitesands", "mesa")),
        "mus_night",
        "mus_tension",
        "mus_storm",
        "mus_sea",
        *(f"mus_discovery_{variant}" for variant in ("01", "02", "03", "04")),
        *(f"mus_finale_{variant}" for variant in ("rescue", "voyage", "stay")),
        "mus_menu",
        "mus_credits",
    ]
    for name in music_names:
        specs.append(_music_spec(name))

    return specs
