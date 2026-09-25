"""Catalogo minimo (primera tanda): la lista declarativa de que sonido es
cada cosa, en que categoria vive y si es bucle. `build.py` recorre esta lista
para generar, normalizar, exportar y documentar cada fichero."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

import numpy as np

from .generators import ambience, birds, footsteps, impacts, misc_sfx, ui


@dataclass(frozen=True)
class SoundSpec:
    name: str
    category: str  # "Ambiente" (estereo, bucle) o "Efectos" (mono)
    is_loop: bool
    generate: Callable[[str], np.ndarray]


def _footstep_spec(material: str, index: int) -> SoundSpec:
    name = f"sfx_footstep_{material}_{index:02d}"
    return SoundSpec(name, "Efectos", False, lambda n, m=material: footsteps.footstep(n, m))


def _bird_spec(species: str, index: int) -> SoundSpec:
    name = f"sfx_bird_{species}_{index:02d}"
    return SoundSpec(name, "Efectos", False, lambda n, s=species: birds.bird_call(n, s))


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

    return specs
