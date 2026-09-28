"""Descripcion de la musica adaptativa para el juego: `Content/Data/music_layers.json`.

El director de musica del juego (`FMusicDirectorModel`, GDD §14.3) no conoce
nombres de ficheros: lee de aqui que pieza cumple cada papel (menu, tema,
exploracion por isla, noche, tension, tormenta, mar abierto, descubrimiento,
final y creditos), su tempo, sus compases y si es bucle, para cuantizar las
transiciones al compas de la pieza que suena. Tambien describe las
variaciones diurnas de cada isla (la propia y sus hermanas de la misma escala,
para no repetir la misma pieza dos veces seguidas) y la afinacion de la
flauta de bambu (GDD §8.12).

No renderiza audio: `compose.metadata` solo arma la partitura, asi que
generar el JSON tarda milisegundos. `uv run explored-audio music-layers`
lo reescribe; un test comprueba que el fichero del repo esta al dia.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from ..catalog import MUSIC_ISLANDS, MUSIC_NAMES
from . import compose
from .theory import SCALE_STEPS

# Misma raiz que usa `Tools/Unreal/import_audio.py` para la categoria "Musica".
MUSIC_CONTENT_ROOT = "/Game/Generated/Audio/Musica"
EFFECTS_CONTENT_ROOT = "/Game/Generated/Audio/Efectos"

# Cuantas piezas hermanas (misma escala, tempo mas parecido) se suman a la
# propia de cada isla como variaciones diurnas.
DAY_SISTER_COUNT = 2


def asset_path(root: str, name: str) -> str:
    return f"{root}/{name}.{name}"


def role_of(name: str) -> tuple[str, str]:
    """Papel de una pieza y su variante (isla o sufijo), a partir del nombre."""
    simple = {
        "mus_theme": "theme",
        "mus_menu": "menu",
        "mus_night": "night",
        "mus_tension": "tension",
        "mus_storm": "storm",
        "mus_sea": "sea",
        "mus_credits": "credits",
    }
    if name in simple:
        return simple[name], ""
    for prefix, role in (("mus_explore_", "explore"), ("mus_discovery_", "discovery"), ("mus_finale_", "finale")):
        if name.startswith(prefix):
            return role, name[len(prefix):]
    raise KeyError(f"pieza de musica sin papel conocido: {name}")


def piece_entry(name: str) -> dict[str, Any]:
    meta = compose.metadata(name)
    role, variant = role_of(name)
    seconds_per_beat = 60.0 / meta["bpm"]
    seconds_per_bar = seconds_per_beat * meta["beats_per_bar"]
    return {
        "id": name,
        "role": role,
        "variant": variant,
        "asset": asset_path(MUSIC_CONTENT_ROOT, name),
        "bpm": meta["bpm"],
        "beats_per_bar": meta["beats_per_bar"],
        "bars": meta["bars"],
        "loop": meta["loop"],
        "seconds_per_bar": round(seconds_per_bar, 6),
        "duration_s": round(seconds_per_bar * meta["bars"], 6),
    }


def day_variants() -> dict[str, list[str]]:
    """Variaciones diurnas por isla: la pieza propia primero y despues las
    hermanas de la misma escala ordenadas por cercania de tempo (y por nombre
    para desempatar, asi el resultado es estable)."""
    configs = compose.ISLAND_CONFIGS
    out: dict[str, list[str]] = {}
    for island in MUSIC_ISLANDS:
        own = configs[island]
        sisters = sorted(
            (other for other in MUSIC_ISLANDS if other != island and configs[other].scale == own.scale),
            key=lambda other: (abs(configs[other].bpm - own.bpm), other),
        )
        out[island] = [f"mus_explore_{island}", *(f"mus_explore_{s}" for s in sisters[:DAY_SISTER_COUNT])]
    return out


def flute_entry() -> dict[str, Any]:
    steps = SCALE_STEPS[compose.FLUTE_SCALE]
    return {
        "sample": compose.FLUTE_SAMPLE_NAME,
        "asset": asset_path(EFFECTS_CONTENT_ROOT, compose.FLUTE_SAMPLE_NAME),
        "sample_hz": round(compose.FLUTE_SAMPLE_FREQ, 4),
        "scale": compose.FLUTE_SCALE,
        "semitones": list(steps),
    }


def build_payload() -> dict[str, Any]:
    return {
        "version": 1,
        "pieces": [piece_entry(name) for name in MUSIC_NAMES],
        "day_variants": day_variants(),
        "flute": flute_entry(),
    }


def default_output_path() -> Path:
    """`Content/Data/music_layers.json` del repo (Tools/Audio/src/explored_audio/music/layers.py)."""
    return Path(__file__).resolve().parents[5] / "Content" / "Data" / "music_layers.json"


def to_json(payload: dict[str, Any]) -> str:
    return json.dumps(payload, ensure_ascii=False, indent=2) + "\n"


def write_music_layers(out_path: Path | None = None) -> Path:
    out_path = Path(out_path) if out_path is not None else default_output_path()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(to_json(build_payload()), encoding="utf-8")
    return out_path
