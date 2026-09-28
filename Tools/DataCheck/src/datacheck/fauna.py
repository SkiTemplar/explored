"""Fauna salvaje terrestre (GDD v2 §3.7, biblia 02 §11, 05 §5): ``Content/Data/fauna.json``.

- ``lod``: espejo de los valores por defecto de ``FFaunaLodSettings`` (``Fauna/FaunaSpawning.h``).
- Especies: ``cppSpecies`` existe en ``EFaunaSpecies``; si aún no existe, lleva un
  ``cppSpeciesPropuesto`` nuevo. Vida, percepción, huida, ataque (propiedad conocida),
  rutina que cubre las 24 h sin solaparse, botín y nidos con objetos de ``items.json``.
- Islas: las de ``EIslandArchetype``; las cuatro del acceso anticipado (GDD v2 §6.2) tienen
  ficha; una isla de fase 1 no puede depender de una especie de fase 2/3 y toda especie de
  fase 1 vive en alguna isla de fase 1.
- Nivel de detalle: cada especie dice qué hace en Full, Reduced y Frozen (subconjunto de
  ``lod.tiers[].does``); huir o cargar solo en Full, la rutina sigue en Reduced y la
  navegación depende de ``locomotion`` (suelo navega, vuelo no).
- Población por isla (propuesta): grupos y tamaño de grupo de especies que viven allí,
  dentro del tope duro de biblia 08 §2.7 (12 + 24 terrestres replicados, 24 grupos de
  ambiente) y con días de reposición > 0.
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
LOCOMOTION = {"suelo", "vuelo"}
NAV_FULL, NAV_REDUCED = "navegacion_fina", "navegacion_gruesa"
# Biblia 08 §2.7 b: tope duro por cliente de terrestres replicados (10 Hz cerca, 2 Hz lejos)
# y §2.7 a: grupos de ambiente con ancla.
REPLICATED_FULL_CAP = 12
REPLICATED_CAP = REPLICATED_FULL_CAP + 24
AMBIENT_GROUP_CAP = 24
PICKUP_BLOCKS = ("nest", "groundPickup", "deposit")
# fauna_terrestre.json (ids para packs_catalogo.json, #60) y fauna.json deben nombrar igual a
# las especies que comparten; su «phase» usa AA/F2/F3 y sus islas, EIslandArchetype.
REGISTRY = "fauna_terrestre.json"
REGISTRY_PHASES = {"AA": 1, "F2": 2, "F3": 3}


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

    tiers = {t.get("id"): set(t.get("does", [])) for t in lod.get("tiers", [])}
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
        for key in PICKUP_BLOCKS:
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
            if block.get("tool") is not None and block["tool"] not in items:
                r.error(f"{where}: {key} con herramienta «{block['tool']}» que no está en items.json")

        _check_lod(s, where, tiers, r)
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
        _check_population(isl, species, r)
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
    homes = {sid: {iid for iid, isl in listed.items() if sid in isl.get("species", [])} for sid in species}
    _check_registry(ds, species, homes, r)


def _check_registry(ds, species: dict[str, dict], homes: dict[str, set[str]], r) -> None:
    """Las especies de fauna_terrestre.json coinciden con fauna.json en id, nombres, fase e islas."""
    for reg in ds.data.get(REGISTRY, {}).get("species", []):
        rid = reg.get("id")
        sp = species.get(rid)
        if sp is None:
            if reg.get("wild"):
                r.error(f"{REGISTRY} «{rid}»: especie salvaje que no está en fauna.json (mismo id en los dos)")
            continue
        where = f"fauna.json «{rid}»"
        for key in ("nameEs", "nameEn"):
            if sp.get(key) != reg.get(key):
                r.error(f"{where}: {key} «{sp.get(key)}» y {REGISTRY} dice «{reg.get(key)}»")
        if REGISTRY_PHASES.get(reg.get("phase")) != sp.get("fase"):
            r.error(f"{where}: fase {sp.get('fase')} y {REGISTRY} dice {reg.get('phase')}")
        reg_islands = {i.lower() for i in reg.get("islands", [])}
        if reg.get("wild") and reg_islands != homes.get(rid, set()):
            r.error(f"{where}: vive en {sorted(homes.get(rid, set()))} y {REGISTRY} dice {sorted(reg_islands)}")


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
    if any(s.get(k) for k in PICKUP_BLOCKS) and net.get("recogidas") != "servidor":
        r.error(f"{where}: nidos o recogidas sin red.recogidas «servidor» (biblia 08 §2.3)")


def _check_lod(s: dict, where: str, tiers: dict[str, set[str]], r) -> None:
    """Qué hace la especie en cada nivel de FFaunaLod (biblia 02 §11.1)."""
    loco = s.get("locomotion")
    if loco not in LOCOMOTION:
        r.error(f"{where}: locomotion {loco!r} no es {sorted(LOCOMOTION)}")
    beh = s.get("lodBehavior")
    if not isinstance(beh, dict) or set(beh) != set(LOD_TIERS):
        r.error(f"{where}: lodBehavior debe tener exactamente {list(LOD_TIERS)}")
        return
    for tier in LOD_TIERS:
        acts = beh[tier]
        if not isinstance(acts, list) or len(set(acts)) != len(acts):
            r.error(f"{where}: lodBehavior.{tier} debe ser una lista sin repetidos")
            continue
        extra = set(acts) - tiers.get(tier, set())
        if extra:
            r.error(f"{where}: lodBehavior.{tier} hace {sorted(extra)}, que el nivel {tier} no permite")
    full, reduced = set(beh["Full"]), set(beh["Reduced"])
    if beh["Frozen"]:
        r.error(f"{where}: en Frozen no se actualiza nada (biblia 02 §11.1)")
    if "rutina" not in full or "rutina" not in reduced:
        r.error(f"{where}: la rutina sigue en Full y en Reduced (el reloj no se para al alejarse)")
    reacts = s.get("attack") is not None or any(v for v in (s.get("flee") or {}).values())
    if reacts and "huida_o_carga" not in full:
        r.error(f"{where}: huye o ataca, así que Full necesita «huida_o_carga»")
    if reacts and "percepcion" not in full:
        r.error(f"{where}: huye o ataca, así que Full necesita «percepcion»")
    if loco == "suelo" and (NAV_FULL not in full or NAV_REDUCED not in reduced):
        r.error(f"{where}: camina, así que necesita «{NAV_FULL}» en Full y «{NAV_REDUCED}» en Reduced")
    if loco == "vuelo" and (full | reduced) & {NAV_FULL, NAV_REDUCED}:
        r.error(f"{where}: vuela y no usa el campo de navegación del suelo (se reconstruye por chunk minado)")


def _check_population(isl: dict, species: dict[str, dict], r) -> None:
    """Grupos por isla dentro del tope de red de biblia 08 §2.7 (propuesta de balance)."""
    iid = isl.get("island")
    where = f"fauna.json/islands «{iid}»"
    pop = isl.get("population")
    if not isinstance(pop, list):
        r.error(f"{where}: falta «population» (lista, vacía si no hay fauna)")
        return
    seen: set[str] = set()
    replicated = ambient_groups = 0
    for entry in pop:
        sid = entry.get("species")
        if sid not in isl.get("species", []):
            r.error(f"{where}: población de «{sid}», que no está en species de la isla")
            continue
        if sid in seen:
            r.error(f"{where}: población de «{sid}» repetida")
        seen.add(sid)
        groups, size, days = entry.get("groups"), entry.get("groupSize"), entry.get("respawnDays")
        if not (isinstance(groups, int) and groups >= 1):
            r.error(f"{where} «{sid}»: groups={groups!r} debe ser un entero ≥ 1")
            continue
        if not (isinstance(size, list) and len(size) == 2 and all(isinstance(x, int) for x in size)
                and 1 <= size[0] <= size[1]):
            r.error(f"{where} «{sid}»: groupSize={size!r} debe ser [min, max] con 1 ≤ min ≤ max")
            continue
        if not (isinstance(days, (int, float)) and not isinstance(days, bool) and days > 0):
            r.error(f"{where} «{sid}»: respawnDays={days!r} debe ser > 0")
        net = (species.get(sid) or {}).get("red") or {}
        if net.get("clase") == "replicada":
            replicated += groups * size[1]
            if size[1] > REPLICATED_FULL_CAP:
                r.error(f"{where} «{sid}»: un grupo de {size[1]} no cabe en los {REPLICATED_FULL_CAP} "
                        "replicados a 10 Hz (biblia 08 §2.7 b)")
        else:
            ambient_groups += groups
    missing = set(isl.get("species", [])) - seen
    if missing:
        r.error(f"{where}: especies sin población {sorted(missing)}")
    if replicated > REPLICATED_CAP:
        r.error(f"{where}: hasta {replicated} terrestres replicados vivos; el tope por cliente es "
                f"{REPLICATED_CAP} (biblia 08 §2.7 b)")
    if ambient_groups > AMBIENT_GROUP_CAP:
        r.error(f"{where}: {ambient_groups} grupos de ambiente; el tope es {AMBIENT_GROUP_CAP} (biblia 08 §2.7 a)")
