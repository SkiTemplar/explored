"""Huerto (GDD v2 §3.6, biblia 02 §10.1): reglas de ``plants.json`` contra ``FFarmModel``.

- ``rules``: espejo de las constantes de ``Source/Explored/Farming/FarmModel.h``
  (crecimiento fuera de estación, días secos hasta marchitarse y morir, lluvia que
  equivale a un riego, radio del espantapájaros y picoteo de las aves). El modelo puro
  no lee JSON: si cambia una constante, este bloque tiene que cambiar con ella.
- Cosecha neta: un cultivo que se planta con el mismo objeto que cosecha y se arranca al
  cosechar tiene que devolver más de lo que costó; si no, el huerto no produce nada.
"""

from __future__ import annotations

import re

FARM_MODEL_H = "Source/Explored/Farming/FarmModel.h"

# clave de plants.json/rules -> (constante de FFarmModel, tipo C++, factor datos -> C++)
RULES = {
    "outOfSeasonGrowth": ("OutOfSeasonGrowth", "float", 1.0),
    "dryDaysToWilt": ("DryDaysToWilt", "int32", 1.0),
    "dryDaysToDie": ("DryDaysToDie", "int32", 1.0),
    "rainHoursPerWatering": ("RainHoursPerWatering", "float", 1.0),
    "scarecrowRadiusM": ("ScarecrowRadius", "double", 100.0),  # el C++ va en cm
    "birdChance": ("BirdChance", "float", 1.0),
    "birdShare": ("BirdShare", "float", 1.0),
}


def _cpp_constants(text: str) -> dict[str, float]:
    out: dict[str, float] = {}
    for name, ctype, _ in RULES.values():
        m = re.search(rf"static constexpr {ctype} {name} = ([0-9.]+)f?;", text)
        if m:
            out[name] = float(m.group(1))
    return out


def check_farm(ds, r) -> None:
    doc = ds.data.get("plants.json", {})
    if not doc:
        return
    rules = doc.get("rules")
    if not isinstance(rules, dict):
        r.error("plants.json: falta el bloque «rules» (espejo de FFarmModel, biblia 02 §10.1)")
    else:
        header = ds.repo_root / FARM_MODEL_H
        cpp = _cpp_constants(header.read_text(encoding="utf-8")) if header.exists() else {}
        for key, (name, _, factor) in RULES.items():
            v = rules.get(key)
            if not isinstance(v, (int, float)) or isinstance(v, bool) or v < 0:
                r.error(f"plants.json/rules: {key}={v!r} debe ser un número ≥ 0")
                continue
            if name not in cpp:
                if header.exists():
                    r.error(f"plants.json/rules: no encuentro FFarmModel::{name} en FarmModel.h")
                continue
            if abs(v * factor - cpp[name]) > 1e-6:
                r.error(f"plants.json/rules: {key}={v} y FFarmModel::{name}={cpp[name]:g}"
                        + (" cm" if factor != 1.0 else ""))
        wilt, die = rules.get("dryDaysToWilt"), rules.get("dryDaysToDie")
        if isinstance(wilt, int) and isinstance(die, int) and not 0 < wilt < die:
            r.error(f"plants.json/rules: la planta se marchita ({wilt} días) antes de morir ({die})")
        for key in ("outOfSeasonGrowth", "birdChance", "birdShare"):
            v = rules.get(key)
            if isinstance(v, (int, float)) and not 0 <= v <= 1:
                r.error(f"plants.json/rules: {key}={v} fuera de [0, 1]")

    for plant in doc.get("plants", []):
        h = plant.get("harvest") or {}
        if plant.get("plantedFrom") != h.get("item") or h.get("everyDays", 0) != 0:
            continue
        lo = h.get("min")
        if isinstance(lo, int) and lo < 2:
            r.error(f"plants.json «{plant.get('id')}»: se planta con «{h.get('item')}», se arranca al cosechar"
                    f" y da como mínimo {lo}: cosecha neta nula (min ≥ 2 o everyDays > 0)")
