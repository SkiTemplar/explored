"""Fauna salvaje terrestre (GDD v2 §3.7, biblia 02 §11, 05 §5): ``Content/Data/fauna.json``.

- ``lod``: espejo de los valores por defecto de ``FFaunaLodSettings`` (``Fauna/FaunaSpawning.h``).
- Especies: ``cppSpecies`` existe en ``EFaunaSpecies``; si aún no existe, lleva un
  ``cppSpeciesPropuesto`` nuevo. Vida, percepción, huida, ataque (propiedad conocida),
  rutina que cubre las 24 h sin solaparse, botín y nidos con objetos de ``items.json``.
- Islas: las de ``EIslandArchetype``; las cuatro del acceso anticipado (GDD v2 §6.2) tienen
  ficha; una isla de fase 1 no puede depender de una especie de fase 2/3 y toda especie de
  fase 1 vive en alguna isla de fase 1.
- Red (biblia 08 §2.7): cada especie dice si es fauna de ambiente con ancla de grupo o un
  actor replicado; lo que ataca lo tira el servidor, lo que se caza y despieza se replica y
  lo que deja nidos o recogidas guarda ese estado en el servidor.
"""

from __future__ import annotations

import re

from . import mining

FAUNA_H = "Source/Explored/Fauna/FaunaTypes.h"
SPAWNING_H = "Source/Explored/Fauna/FaunaSpawning.h"
EA_ISLANDS = ("landing", "emerald", "smoke", "teeth")
DISPOSITIONS = {"huidizo", "defensivo", "territorial"}
LOD_TIERS = ("Full", "Reduced", "Frozen")
SEASONS = {"seca", "primeras_lluvias", "monzon", "ciclones"}
PHASES = (1, 2, 3)
NET_CLASSES = {"ambiente", "replicada"}
NET_ANCHORS = {"bandada", "colonia", "banco", "enjambre"}


def cpp_species(ds) -> set[str]:
    text = mining._read(ds, FAUNA_H)
    m = re.search(r"enum class EFaunaSpecies : uint8\s*\{(.*?)\};", text, re.S)
    if not m:
        return set()
    body = re.sub(r"//[^\n]*", "", m.group(1))
    return {n for n in re.findall(r"(\w+)\s*,", body) if n != "Count"}


def cpp_lod(ds) -> dict[str, float]:
    text = mining._read(ds, SPAWNING_H)
    m = re.search(r"struct EXPLORED_API FFaunaLodSettings\s*\{(.*?)\};", text, re.S)
    if not m:
        return {}
    out = {}
    for name, value in re.findall(r"(?:float|int32) (\w+) = ([\d.]+)f?;", m.group(1)):
        out[name] = float(value)
    return out


def _hours_ok(routine: list) -> bool:
    covered = [0] * 24
    for step in routine:
        h = step.get("hours")
        if not (isinstance(h, list) and len(h) == 2 and all(isinstance(x, int) for x in h) and 0 <= h[0] < h[1] <= 24):
            return False
        for hour in range(h[0], h[1]):
            covered[hour] += 1
    return all(c == 1 for c in covered)


def check_fauna(ds, r, properties: set[str]) -> None:
    doc = ds.data.get("fauna.json")
    if not doc:
        return
    items = {i["id"] for i in ds.items}

    lod = doc.get("lod", {})
    cpp = cpp_lod(ds)
    if not cpp:
        r.warn("fauna.json: no se lee FFaunaLodSettings; no se compara con el C++")
    for key, name in (("fullRadiusCm", "FullRadiusCm"), ("reducedRadiusCm", "ReducedRadiusCm"),
                      ("hysteresis", "Hysteresis"), ("reducedInterval", "ReducedInterval")):
        if name in cpp and lod.get(key) != cpp[name]:
            r.error(f"fauna.json/lod: {key}={lod.get(key)!r} pero FFaunaLodSettings dice {name}={cpp[name]:g}")
    if [t.get("id") for t in lod.get("tiers", [])] != list(LOD_TIERS):
        r.error(f"fauna.json/lod: los niveles deben ser {list(LOD_TIERS)} (EFaunaLodTier)")

    enum = cpp_species(ds)
    species: dict[str, dict] = {}
    for s in doc.get("species", []):
        sid = s.get("id")
        where = f"fauna.json «{sid}»"
        if sid in species:
            r.error(f"{where}: id repetido")
        species[sid] = s
        if not s.get("nameEs") or not s.get("nameEn"):
            r.error(f"{where}: falta nameEs o nameEn")
        if s.get("fase") not in PHASES:
            r.error(f"{where}: fase {s.get('fase')!r} debe ser 1, 2 o 3")
        cs, prop = s.get("cppSpecies"), s.get("cppSpeciesPropuesto")
        if cs is not None and enum and cs not in enum:
            r.error(f"{where}: cppSpecies «{cs}» no está en EFaunaSpecies")
        if cs is None and (not prop or prop in enum):
            r.error(f"{where}: sin cppSpecies necesita un cppSpeciesPropuesto nuevo (hoy: {prop!r})")
        if not isinstance(s.get("healthPoints"), (int, float)) or s["healthPoints"] <= 0:
            r.error(f"{where}: healthPoints debe ser > 0")
        if s.get("disposition") not in DISPOSITIONS:
            r.error(f"{where}: disposition {s.get('disposition')!r} no es {sorted(DISPOSITIONS)}")
        per = s.get("perception", {})
        for key in ("sightConeDeg", "sightM", "hearingM", "smellM"):
            if not isinstance(per.get(key), (int, float)) or per[key] < 0:
                r.error(f"{where}: perception.{key} inválido")
        if not 0 < per.get("sightConeDeg", 0) <= 360:
            r.error(f"{where}: sightConeDeg fuera de (0, 360]")
        if not _hours_ok(s.get("routine", [])):
            r.error(f"{where}: la rutina debe cubrir las 24 h exactamente una vez")
        flee = s.get("flee", {})
        hf = flee.get("healthFraction")
        if hf is not None and not 0 < hf <= 1:
            r.error(f"{where}: flee.healthFraction={hf!r} fuera de (0, 1]")
        attack = s.get("attack")
        if attack is not None:
            eq = attack.get("equiv", {})
            if eq.get("property") not in properties or not 0 < eq.get("value", 0) <= 5:
                r.error(f"{where}: attack.equiv {eq!r} no es una propiedad 1-5 (biblia 05 §3.0)")
            if not isinstance(attack.get("damage"), (int, float)) or attack["damage"] <= 0:
                r.error(f"{where}: attack.damage debe ser > 0")
        if s.get("disposition") == "territorial" and (attack is None or not attack.get("territoryM")):
            r.error(f"{where}: territorial sin attack.territoryM")
        for entry in s.get("loot", []):
            if entry.get("item") not in items:
                r.error(f"{where}: botín «{entry.get('item')}» no está en items.json")
            if entry.get("tool") is not None and entry["tool"] not in items:
                r.error(f"{where}: herramienta de despiece «{entry['tool']}» no está en items.json")
            if not 0 <= entry.get("min", -1) <= entry.get("max", -1):
                r.error(f"{where}: botín «{entry.get('item')}» con min/max incoherentes")
        for key in ("nest", "groundPickup"):
            block = s.get(key)
            if block is None:
                continue
            if block.get("item") not in items:
                r.error(f"{where}: {key} con objeto «{block.get('item')}» que no está en items.json")
            if not 0 <= block.get("min", -1) <= block.get("max", -1) or block.get("everyDays", 0) <= 0:
                r.error(f"{where}: {key} con min/max/everyDays incoherentes")
            bad = set(block.get("seasons", [])) - SEASONS
            if bad:
                r.error(f"{where}: {key} con estaciones desconocidas {sorted(bad)}")

        _check_net(s, where, r)

    islands = mining.cpp_islands(ds)
    listed: dict[str, dict] = {}
    home_phases: dict[str, set[int]] = {}
    for isl in doc.get("islands", []):
        iid = isl.get("island")
        if islands and iid not in islands:
            r.error(f"fauna.json/islands: «{iid}» no es un EIslandArchetype")
        if iid in listed:
            r.error(f"fauna.json/islands: «{iid}» repetida")
        listed[iid] = isl
        for sid in isl.get("species", []):
            sp = species.get(sid)
            if sp is None:
                r.error(f"fauna.json/islands «{iid}»: especie «{sid}» no existe")
                continue
            home_phases.setdefault(sid, set()).add(isl.get("fase"))
            if isl.get("fase") == 1 and sp.get("fase", 1) > 1:
                r.error(f"fauna.json/islands «{iid}»: isla de fase 1 con «{sid}» de fase {sp['fase']}")
    for iid in EA_ISLANDS:
        if iid not in listed:
            r.error(f"fauna.json: falta la ficha de «{iid}», isla del acceso anticipado (GDD v2 §6.2)")
        elif listed[iid].get("fase") != 1:
            r.error(f"fauna.json/islands «{iid}»: es del acceso anticipado, fase debe ser 1")
    for sid, sp in species.items():
        phases = home_phases.get(sid)
        if not phases:
            r.error(f"fauna.json «{sid}»: no vive en ninguna isla")
        elif sp.get("fase") == 1 and 1 not in phases:
            r.error(f"fauna.json «{sid}»: es de fase 1 pero solo vive en islas de fase {sorted(phases)}")


def _check_net(s: dict, where: str, r) -> None:
    net = s.get("red")
    if not isinstance(net, dict):
        r.error(f"{where}: falta «red» (biblia 08 §2.7: fauna de ambiente con ancla o replicada)")
        return
    cls = net.get("clase")
    if cls not in NET_CLASSES:
        r.error(f"{where}: red.clase {cls!r} no es {sorted(NET_CLASSES)}")
    if cls == "ambiente" and net.get("ancla") not in NET_ANCHORS:
        r.error(f"{where}: fauna de ambiente sin red.ancla de grupo {sorted(NET_ANCHORS)}")
    if cls == "replicada" and net.get("ancla") is not None:
        r.error(f"{where}: una especie replicada no lleva ancla de grupo")
    if s.get("attack") is not None and net.get("tiradaDano") != "servidor":
        r.error(f"{where}: ataca, así que red.tiradaDano debe ser «servidor» (como ReefSharkAttackRoll)")
    if s.get("loot") and cls != "replicada":
        r.error(f"{where}: se caza y despieza (loot), así que debe ser replicada (biblia 08 §2.7 b)")
    if (s.get("nest") or s.get("groundPickup")) and net.get("recogidas") != "servidor":
        r.error(f"{where}: nidos o recogidas sin red.recogidas «servidor» (biblia 08 §2.3)")
