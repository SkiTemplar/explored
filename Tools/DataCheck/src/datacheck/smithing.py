"""Metal en estación (biblia 03 §2.2 y §4.3): fuels.json/smeltingLevels y recipes_smithing.json.

Además de esquema y referencias, resuelve de dónde sale cada material procesado. La
simulación de ``crafting`` trata como «en bruto» todo objeto que no fabrica una plantilla,
así que un lingote o un carbón sin receta posible pasaría por recogible del suelo. Aquí:

- Un objeto **procesado** es el resultado de una receta de ``recipes.json`` o de
  ``recipes_smithing.json`` (y no de una plantilla). Solo es obtenible si alguna de sus
  recetas se puede hacer: ingredientes obtenibles, estación y nivel de fuego construibles,
  herramientas y utensilios a mano. Se resuelve por punto fijo, porque el horno de
  fundición cuesta carbón y el yunque cuesta un lingote.
- La fundición no admite cualquier cosa en bruto: cada ingrediente sale de una veta de
  ``mining.json`` con presencia en una isla del acceso anticipado (fase 1), de los restos
  del Albatros (etiqueta ``rescatado``) o de otra receta o plantilla.
- Todo objeto con etiqueta ``mineral`` sale de un estrato de ``mining.json`` y todo
  ``lingote`` de al menos una receta de fundición.
"""

from __future__ import annotations

import re

from . import cooking

FILE = "recipes_smithing.json"
ID_RE = re.compile(r"^[a-z0-9_]+$")
LEVEL_RANGES = (("maxFuelHours", 0.5, 48), ("burnRate", 0.1, 3), ("heat", 0.1, 1.5), ("emberHours", 0, 24),
                ("rainQuench", 0, 10), ("windTolerance", 0, 1), ("smoke", 0, 1))
# Etiquetas de objetos que nunca se recogen en bruto: sin receta posible, no se obtienen.
NEVER_RAW_TAGS = frozenset({"lingote"})
# Lo que un nivel de fundición tiene que superar al último nivel de cocina (biblia 03 §2.2).
ABOVE_COOKING = ("maxFuelHours", "heat", "emberHours")


def _recipes(ds) -> list[dict]:
    doc = ds.data.get(FILE, {})
    recipes = doc.get("recipes", []) if isinstance(doc, dict) else []
    return [rec for rec in recipes if isinstance(rec, dict)] if isinstance(recipes, list) else []


def _entries(value) -> list[dict]:
    return [e for e in value if isinstance(e, dict)] if isinstance(value, list) else []


def _num(value, lo: float, hi: float) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and lo <= value <= hi


def smelting_levels(ds) -> dict[str, dict]:
    return {lv.get("id"): lv for lv in ds.data.get("fuels.json", {}).get("smeltingLevels", [])
            if isinstance(lv, dict)}


def processed_items(ds) -> dict[str, list[dict]]:
    """Objeto procesado → recetas que lo producen (normalizadas), sin los que fabrica una plantilla."""
    templated = {t.get("resultDefinitionId") for t in ds.templates}
    fuels = ds.data.get("fuels.json", {})
    cook_pieces = {lv.get("id"): lv.get("pieceId") for lv in fuels.get("levels", [])}
    vessels = {v.get("id"): v for v in ds.data.get("recipes.json", {}).get("vessels", [])}
    out: dict[str, list[dict]] = {}

    for rec in ds.data.get("recipes.json", {}).get("recipes", []):
        if not isinstance(rec, dict):
            continue
        level = rec.get("minFireLevel")
        out.setdefault(rec.get("result"), []).append({
            "id": rec.get("id"), "file": "recipes.json",
            "ingredients": _entries(rec.get("ingredients")),
            "pieces": [cook_pieces[level]] if level in cook_pieces else [],
            "tools": [],
            "vessels": [vessels.get(v, {}) for v in rec.get("vessels", [])],
        })
    levels = smelting_levels(ds)
    for rec in _recipes(ds):
        pieces = [rec.get("station")]
        level = levels.get(rec.get("minFireLevel"))
        if level is not None:
            pieces.append(level.get("pieceId"))
        out.setdefault(rec.get("result"), []).append({
            "id": rec.get("id"), "file": FILE, "ingredients": _entries(rec.get("ingredients")),
            "pieces": pieces, "tools": rec.get("tools", []), "vessels": [],
        })
    for iid in templated:
        out.pop(iid, None)
    out.pop(None, None)
    return out


def resolve_obtainable(ds, crafted: set[str]) -> tuple[set[str], set[str]]:
    """Punto fijo de objetos obtenibles y piezas construibles.

    ``crafted`` es lo que alcanza la simulación de plantillas (``Reachability.reached_items``),
    que ya incluye los procesados como si fueran en bruto: se quitan y se vuelven a añadir
    solo si alguna de sus recetas se puede hacer.
    """
    processed = processed_items(ds)
    templated = {t.get("resultDefinitionId") for t in ds.templates}
    never_raw = {i["id"] for i in ds.items if NEVER_RAW_TAGS & set(i.get("tags", [])) and i["id"] not in templated}
    obtainable = set(crafted) - set(processed) - never_raw - sourceless_ingredients(ds)
    pieces = {p.get("id"): p for p in ds.building.get("pieces", [])}
    tags_of = {i["id"]: set(i.get("tags", [])) for i in ds.items}
    built: set[str] = set()

    def has(entry: dict) -> bool:
        if entry.get("item") is not None:
            return entry["item"] in obtainable
        tag = entry.get("tag")
        return any(tag in tags and iid in obtainable for iid, tags in tags_of.items())

    def vessel_ok(v: dict) -> bool:
        return any(i in obtainable for i in v.get("items", [])) or v.get("piece") in built

    changed = True
    while changed:
        changed = False
        for pid, p in pieces.items():
            if pid in built:
                continue
            if all(c.get("item") in obtainable for c in p.get("cost", [])) \
                    and all(t in obtainable for t in p.get("tools", [])) \
                    and all(req in built for req in p.get("requiresPieces", [])):
                built.add(pid)
                changed = True
        for iid, recipes in processed.items():
            if iid in obtainable:
                continue
            for rec in recipes:
                if all(has(e) for e in rec["ingredients"]) and all(pc in built for pc in rec["pieces"]) \
                        and all(t in obtainable for t in rec["tools"]) \
                        and (not rec["vessels"] or any(vessel_ok(v) for v in rec["vessels"])):
                    obtainable.add(iid)
                    changed = True
                    break
    return obtainable, built


def sourceless_ingredients(ds) -> set[str]:
    """Ingredientes de fundición que no salen de veta, chatarra, receta ni plantilla."""
    processed = processed_items(ds)
    templated = {t.get("resultDefinitionId") for t in ds.templates}
    sources = _stratum_sources(ds)
    rescued = {i["id"] for i in ds.items if "rescatado" in i.get("tags", [])}
    used = {e.get("item") for rec in _recipes(ds) for e in _entries(rec.get("ingredients"))}
    return {iid for iid in used if isinstance(iid, str)} - set(processed) - templated - set(sources) - rescued


def _stratum_sources(ds) -> dict[str, set[int]]:
    """Objeto de items.json → fases de las islas donde lo deja algún estrato de mining.json."""
    out: dict[str, set[int]] = {}
    for stratum in ds.data.get("mining.json", {}).get("strata", []):
        phases = {o.get("fase") for o in stratum.get("occurrences", []) if isinstance(o, dict)}
        out.setdefault(stratum.get("item"), set()).update(p for p in phases if isinstance(p, int))
    return out


def check_levels(ds, r) -> None:
    fuels = ds.data.get("fuels.json", {})
    levels = fuels.get("smeltingLevels", [])
    if not isinstance(levels, list):
        r.error("fuels.json: smeltingLevels debe ser una lista")
        return
    pieces = {p.get("id"): p for p in ds.building.get("pieces", [])}
    fuel_ids = {f.get("item") for f in fuels.get("fuels", [])}
    last_cooking = (fuels.get("levels") or [{}])[-1]
    seen: set[str] = set()
    for lv in levels:
        lid = lv.get("id")
        where = f"fuels.json smeltingLevels «{lid}»"
        if not isinstance(lid, str) or not ID_RE.match(lid):
            r.error(f"fuels.json smeltingLevels: id inválido {lid!r}")
            continue
        if lid in seen:
            r.error(f"{where}: repetido")
        seen.add(lid)
        if lid in cooking.LEVELS:
            r.error(f"{where}: ya es un nivel de cocina de EFireLevel; va en «levels», no aquí")
        if not lv.get("nameEs"):
            r.error(f"{where}: falta nameEs")
        piece = pieces.get(lv.get("pieceId"))
        if piece is None:
            r.error(f"{where}: pieceId «{lv.get('pieceId')}» no está en building_pieces.json")
        elif piece.get("category") != "produccion":
            r.error(f"{where}: la pieza «{piece.get('id')}» no es de producción")
        for key, lo, hi in LEVEL_RANGES:
            if not _num(lv.get(key), lo, hi):
                r.error(f"{where}: {key}={lv.get(key)!r} fuera de [{lo}, {hi}]")
        for key in ABOVE_COOKING:
            if _num(lv.get(key), 0, 99) and _num(last_cooking.get(key), 0, 99) and lv[key] <= last_cooking[key]:
                r.error(f"{where}: {key}={lv[key]} no supera al horno de cocina «{last_cooking.get('id')}» "
                        f"({last_cooking[key]})")
        if not lv.get("enclosed"):
            r.error(f"{where}: un horno de fundición es cerrado (enclosed)")
        accepted = lv.get("acceptedFuels")
        if not isinstance(accepted, list) or not accepted:
            r.error(f"{where}: acceptedFuels debe ser una lista no vacía")
        else:
            for fid in accepted:
                if fid not in fuel_ids:
                    r.error(f"{where}: acceptedFuels «{fid}» no es un combustible de fuels.json")


def check_recipes(ds, r, obtainable: set[str]) -> None:
    doc = ds.data.get(FILE)
    if doc is None:
        return
    raw = doc.get("recipes") if isinstance(doc, dict) else None
    if not isinstance(raw, list):
        r.error(f"{FILE}: falta la lista «recipes»")
        return
    for rec in raw:
        if not isinstance(rec, dict):
            r.error(f"{FILE}: receta que no es un objeto: {rec!r}")
    items = {i["id"]: i for i in ds.items}
    pieces = {p.get("id"): p for p in ds.building.get("pieces", [])}
    levels = smelting_levels(ds)
    sources = _stratum_sources(ds)
    processed = processed_items(ds)
    templated = {t.get("resultDefinitionId") for t in ds.templates}
    fire_stations = {lv.get("pieceId"): lid for lid, lv in levels.items()}
    seen: set[str] = set()

    for rec in _recipes(ds):
        rid = rec.get("id")
        where = f"{FILE} «{rid}»"
        if not isinstance(rid, str) or not ID_RE.match(rid):
            r.error(f"{FILE}: id inválido {rid!r}")
            continue
        if rid in seen:
            r.error(f"{FILE}: receta repetida «{rid}»")
        seen.add(rid)
        for key in ("nameEs", "nameEn"):
            if not isinstance(rec.get(key), str) or not rec[key].strip():
                r.error(f"{where}: falta {key}")
        station = pieces.get(rec.get("station"))
        if station is None:
            r.error(f"{where}: estación «{rec.get('station')}» no está en building_pieces.json")
        elif station.get("category") != "produccion":
            r.error(f"{where}: la estación «{station.get('id')}» no es una pieza de producción")
        level_id = rec.get("minFireLevel")
        if level_id is not None:
            level = levels.get(level_id)
            if level is None:
                r.error(f"{where}: minFireLevel «{level_id}» no está en fuels.json/smeltingLevels")
            elif level.get("pieceId") != rec.get("station"):
                r.error(f"{where}: el fuego «{level_id}» arde en «{level.get('pieceId')}», no en la estación "
                        f"«{rec.get('station')}»")
        elif rec.get("station") in fire_stations:
            r.error(f"{where}: se trabaja en «{rec.get('station')}» sin encender su fuego "
                    f"(minFireLevel «{fire_stations[rec['station']]}»)")
        for tool in rec.get("tools", []):
            if tool not in items:
                r.error(f"{where}: herramienta «{tool}» no está en items.json")
        ingredients = rec.get("ingredients", [])
        if not isinstance(ingredients, list) or not ingredients:
            r.error(f"{where}: sin ingredientes")
            ingredients = []
        for ing in ingredients:
            if not isinstance(ing, dict):
                r.error(f"{where}: ingrediente que no es un objeto: {ing!r}")
                continue
            iid = ing.get("item")
            if set(ing) != {"item", "count"}:
                r.error(f"{where}: cada ingrediente lleva exactamente «item» y «count»")
            if not isinstance(ing.get("count"), int) or isinstance(ing.get("count"), bool) or not 1 <= ing["count"] <= 20:
                r.error(f"{where}: count de «{iid}» fuera de [1, 20]")
            if iid not in items:
                r.error(f"{where}: ingrediente «{iid}» no está en items.json")
                continue
            if iid == rec.get("result"):
                r.error(f"{where}: «{iid}» es a la vez ingrediente y resultado")
            if iid in processed or iid in templated:
                continue
            if iid in sources:
                if 1 not in sources[iid]:
                    r.error(f"{where}: «{iid}» solo sale de vetas de islas de fase 2 o 3; la receta es del "
                            "acceso anticipado")
            elif "rescatado" not in items[iid].get("tags", []):
                r.error(f"{where}: «{iid}» no sale de ninguna fuente: ni de una veta de mining.json, ni de los "
                        "restos del Albatros (rescatado), ni de otra receta o plantilla")
        if rec.get("result") not in items:
            r.error(f"{where}: resultado «{rec.get('result')}» no está en items.json")
        count = rec.get("resultCount")
        if not isinstance(count, int) or isinstance(count, bool) or not 1 <= count <= 50:
            r.error(f"{where}: resultCount fuera de [1, 50]")
        if not _num(rec.get("minutes"), 1, 600):
            r.error(f"{where}: minutes fuera de [1, 600]")

    # Cada receta que no se puede hacer nunca, con lo que le falta (aunque su resultado salga de otra).
    _, built = resolve_obtainable(ds, obtainable)
    for rec in _recipes(ds):
        missing = [str(e.get("item")) for e in _entries(rec.get("ingredients")) if e.get("item") not in obtainable]
        missing += [f"herramienta {t}" for t in rec.get("tools") or [] if t not in obtainable]
        level = levels.get(rec.get("minFireLevel"))
        for pid in (rec.get("station"), level.get("pieceId") if level else None):
            if pid is not None and pid in pieces and pid not in built:
                missing.append(f"pieza {pid} (su coste no se puede reunir)")
        if missing:
            r.error(f"{where_of(rec)}: nunca se puede hacer; no sale de ninguna fuente: {', '.join(missing)}")


def where_of(rec: dict) -> str:
    return f"{FILE} «{rec.get('id')}»"


def check_sources(ds, r, obtainable: set[str]) -> None:
    """Procesados que nunca se pueden hacer y minerales sin veta."""
    processed = processed_items(ds)
    for iid, recipes in sorted(processed.items()):
        if iid not in obtainable:
            ids = ", ".join(f"{rec['file']} «{rec['id']}»" for rec in recipes)
            r.error(f"items.json «{iid}»: no sale de ninguna fuente; ninguna de sus recetas se puede hacer ({ids})")
    sources = _stratum_sources(ds)
    templated = {t.get("resultDefinitionId") for t in ds.templates}
    for item in ds.items:
        iid = item["id"]
        if "mineral" in item.get("tags", []) and iid not in sources and iid not in processed and iid not in templated:
            r.error(f"items.json «{iid}»: es un mineral y ningún estrato de mining.json lo deja")
    smelted = {rec.get("result") for rec in _recipes(ds) if rec.get("minFireLevel") is not None}
    for item in ds.items:
        if "lingote" in item.get("tags", []) and item["id"] not in smelted:
            r.error(f"items.json «{item['id']}»: es un lingote y ninguna receta de fundición lo produce")


def check_smithing(ds, r, obtainable: set[str]) -> None:
    check_levels(ds, r)
    check_recipes(ds, r, obtainable)
    check_sources(ds, r, obtainable)
