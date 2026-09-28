"""Minería manual (GDD v2 §3.4, biblia 02 §2): ``Content/Data/mining.json``.

- Materiales: espejo de ``ETerrainMaterial`` y ``FTerrainEditModel::MaterialInfo``
  (dureza y nivel mínimo), de ``SecondsPerPickaxeHit``, ``TierBonus`` y
  ``MinHitsPerCubicMeter``; la tabla ``hitsPerM3`` sigue la fórmula del GDD.
- Estratos: material y objeto existentes, islas del C++ (``EIslandArchetype``), capa y
  profundidad coherentes, vetas finitas bien formadas, y todos los estratos del GDD.
- Herramientas: niveles 0-4, cada cabeza de pico la acepta la plantilla ``pico`` y
  produce de verdad un pico (no la captura otra plantilla); toda pieza que cabe como
  cabeza tiene nivel asignado.
- Progresión: cada nivel se alcanza sin ciclos (la cabeza sale de la superficie o de un
  estrato que cava un nivel inferior) y, en acceso anticipado, solo con islas de fase 1.
"""

from __future__ import annotations

import copy
import json
import re

from . import crafting

TERRAIN_H = "Source/Explored/WorldGen/TerrainEditModel.h"
TERRAIN_CPP = "Source/Explored/WorldGen/TerrainEditModel.cpp"
ARCHIPELAGO_CPP = "Source/Explored/WorldGen/ArchipelagoLayout.cpp"
PICK_TEMPLATE = "pico"
# GDD v2 §3.4: estrato -> id de mining.json.
GDD_STRATA = {
    "tierra y arena": ("tierra", "arena"), "arcilla": ("arcilla",), "caliza": ("caliza",),
    "basalto": ("basalto",), "obsidiana": ("obsidiana",), "veta de cobre": ("veta_cobre",),
    "hierro de meteorito": ("hierro_meteorito",), "azufre": ("azufre",), "cristal": ("cristal",),
}
MAX_TIER = 4
PHASES = (1, 2, 3)


def _read(ds, rel: str) -> str:
    path = ds.repo_root / rel
    return path.read_text(encoding="utf-8") if path.exists() else ""


def design_hits(hardness: float, min_tier: int, tier: int, bonus: float, floor: float) -> float:
    """Réplica de FTerrainEditModel::DesignHitsPerCubicMeter."""

    factor = bonus ** (tier - min_tier) / hardness
    return max(6.0 / factor, floor)


def cpp_materials(ds) -> dict[str, tuple[float, int]] | None:
    h, cpp = _read(ds, TERRAIN_H), _read(ds, TERRAIN_CPP)
    if not h or not cpp:
        return None
    enum = re.search(r"enum class ETerrainMaterial : uint8\s*\{(.*?)\};", h, re.S)
    table = re.search(r"MaterialInfo\(ETerrainMaterial Material\)\s*\{.*?Table\[\] = \{(.*?)\};", cpp, re.S)
    if not enum or not table:
        return None
    names = [n for n in re.findall(r"(\w+),", enum.group(1)) if n != "Count"]
    rows = re.findall(r"\{\s*([\d.]+)f,\s*(\d+)\s*\}", table.group(1))
    # Sin strict: si el enum tiene más valores que filas, los sobrantes aparecen como «no coincide».
    return {n: (float(a), int(b)) for n, (a, b) in zip(names, rows, strict=False)}


def _cpp_const(ds, name: str) -> float | None:
    m = re.search(rf"static constexpr float {name} = ([\d.]+)f;", _read(ds, TERRAIN_H))
    return float(m.group(1)) if m else None


def cpp_islands(ds) -> set[str]:
    text = _read(ds, ARCHIPELAGO_CPP)
    return {i.lower() for i in re.findall(r'case EIslandArchetype::\w+: return TEXT\("([^"]+)"\);', text)}


def _pick_mango() -> crafting.Instance:
    """Mango ya atado de referencia (palo recto + liana), como en CraftingSpec.cpp."""

    return crafting.Instance("atado_generico", (("Ata", 3.0), ("Largo", 3.0), ("Rigido", 3.0)), frozenset({"interno"}))


def check_mining(ds, r) -> None:
    doc = ds.data.get("mining.json")
    if not doc:
        return
    items = {i["id"]: i for i in ds.items}
    templates = {t["id"]: t for t in ds.templates}

    # ------------------------------------------------------------------ materiales
    bonus, floor = doc.get("tierBonus"), doc.get("minHitsPerM3")
    materials: dict[str, dict] = {}
    for m in doc.get("materials", []):
        mid = m.get("id")
        if mid in materials:
            r.error(f"mining.json/materials: id repetido «{mid}»")
        materials[mid] = m
        hardness, min_tier = m.get("hardness"), m.get("minToolTier")
        if not isinstance(hardness, (int, float)) or hardness <= 0 or min_tier not in range(1, MAX_TIER + 1):
            r.error(f"mining.json/materials «{mid}»: dureza {hardness!r} o nivel mínimo {min_tier!r} inválidos")
            continue
        table = m.get("hitsPerM3", {})
        table = table if isinstance(table, dict) else {}
        expected_tiers = {str(t) for t in range(min_tier, MAX_TIER + 1)}
        if set(table) != expected_tiers:
            r.error(f"mining.json/materials «{mid}»: hitsPerM3 debe tener los niveles {sorted(expected_tiers)}, no {sorted(table)}")
        if isinstance(bonus, (int, float)) and isinstance(floor, (int, float)):
            for tier, hits in table.items():
                if not str(tier).isdigit():
                    continue  # nivel no numérico: ya informado arriba (hitsPerM3 debe tener los niveles)
                want = design_hits(hardness, min_tier, int(tier), bonus, floor)
                if not isinstance(hits, (int, float)) or abs(hits - want) > 0.05:
                    r.error(f"mining.json/materials «{mid}»: {hits!r} golpes/m³ con nivel {tier}, la fórmula del GDD da {want:.2f}")

    cpp = cpp_materials(ds)
    if cpp is None:
        r.warn("mining.json: no se lee ETerrainMaterial/MaterialInfo de TerrainEditModel; no se compara con el C++")
    else:
        by_cpp = {m.get("cpp"): m for m in materials.values()}
        if set(by_cpp) != set(cpp):
            # key=str: un material sin «cpp» (None) no puede romper la ordenación.
            r.error(f"mining.json/materials {sorted(by_cpp, key=str)} no coincide con ETerrainMaterial {sorted(cpp)}")
        for name, (hardness, min_tier) in cpp.items():
            m = by_cpp.get(name)
            if m and (abs(m.get("hardness", -1) - hardness) > 1e-6 or m.get("minToolTier") != min_tier):
                r.error(f"mining.json «{m['id']}»: dureza/nivel {m.get('hardness')}/{m.get('minToolTier')} "
                        f"pero MaterialInfo dice {hardness}/{min_tier}")
    for key, name in (("secondsPerHit", "SecondsPerPickaxeHit"), ("tierBonus", "TierBonus"), ("minHitsPerM3", "MinHitsPerCubicMeter")):
        v = _cpp_const(ds, name)
        if v is not None and doc.get(key) != v:
            r.error(f"mining.json: {key}={doc.get(key)!r} pero TerrainEditModel.h dice {name}={v}")

    # ------------------------------------------------------------------ estratos
    layers = {layer.get("id"): layer for layer in doc.get("layers", [])}
    islands = cpp_islands(ds)
    strata: dict[str, dict] = {}
    for s in doc.get("strata", []):
        sid = s.get("id")
        where = f"mining.json/strata «{sid}»"
        if sid in strata:
            r.error(f"{where}: id repetido")
        strata[sid] = s
        if not s.get("nameEs") or not s.get("nameEn"):
            r.error(f"{where}: falta nameEs o nameEn")
        if s.get("material") not in materials:
            r.error(f"{where}: material «{s.get('material')}» no está en materials")
        if s.get("item") not in items:
            r.error(f"{where}: objeto «{s.get('item')}» no está en items.json")
        vein = s.get("vein")
        if vein is not None:
            units, respawn = vein.get("veinUnits"), vein.get("respawnDays")
            if not isinstance(units, int) or units <= 0:
                r.error(f"{where}: veinUnits={units!r} debe ser un entero > 0")
            if respawn is not None and (not isinstance(respawn, (int, float)) or respawn <= 0):
                r.error(f"{where}: respawnDays={respawn!r} debe ser > 0 o null (no reaparece)")
        if not s.get("occurrences"):
            r.error(f"{where}: sin ninguna isla donde aparezca")
        for occ in s.get("occurrences", []):
            isl, layer, depth = occ.get("island"), occ.get("layer"), occ.get("depthM")
            if islands and isl not in islands:
                r.error(f"{where}: isla «{isl}» no es un EIslandArchetype ({', '.join(sorted(islands))})")
            if occ.get("fase") not in PHASES:
                r.error(f"{where}/{isl}: fase {occ.get('fase')!r} debe ser 1, 2 o 3")
            if layer not in layers:
                r.error(f"{where}/{isl}: capa «{layer}» no está en layers")
            if not (isinstance(depth, list) and len(depth) == 2 and all(isinstance(d, (int, float)) for d in depth)
                    and 0 <= depth[0] < depth[1]):
                r.error(f"{where}/{isl}: depthM={depth!r} debe ser [mín, máx] con 0 ≤ mín < máx")
            elif layer in layers:
                lo, hi = layers[layer].get("depthM", [0, 0])
                if depth[1] <= lo or depth[0] >= hi:
                    r.error(f"{where}/{isl}: {depth[0]}-{depth[1]} m no toca la capa «{layer}» [{lo}, {hi})")
    for name, cands in GDD_STRATA.items():
        missing = [c for c in cands if c not in strata]
        if missing:
            r.error(f"mining.json: falta el estrato «{name}» del GDD v2 §3.4 ({', '.join(missing)})")

    # ------------------------------------------------------------------ herramientas
    tier_heads: dict[int, set[str]] = {}
    head_tier: dict[str, int] = {}
    for tool in doc.get("tools", []):
        tier, tid = tool.get("tier"), tool.get("id")
        if tier not in range(0, MAX_TIER + 1):
            r.error(f"mining.json/tools «{tid}»: nivel {tier!r} fuera de 0-{MAX_TIER}")
            continue
        tpl = tool.get("template")
        if tpl is not None and tpl not in templates:
            r.error(f"mining.json/tools «{tid}»: plantilla «{tpl}» no está en templates.json")
        tier_heads.setdefault(tier, set())
        for h in tool.get("heads", []):
            if h not in items:
                r.error(f"mining.json/tools «{tid}»: cabeza «{h}» no está en items.json")
                continue
            if h in head_tier:
                r.error(f"mining.json/tools: la cabeza «{h}» está en dos niveles ({head_tier[h]} y {tier})")
            head_tier[h] = tier
            tier_heads[tier].add(h)
        frag = tool.get("fragile")
        if frag and not (0 < frag.get("chance", 0) < 1 and frag.get("durabilityLoss", 0) > 0):
            r.error(f"mining.json/tools «{tid}»: fragile mal formado {frag!r}")
    for tier in range(0, MAX_TIER + 1):
        if tier not in tier_heads:
            r.error(f"mining.json/tools: falta el nivel {tier} (GDD v2 §3.4)")

    pick = templates.get(PICK_TEMPLATE)
    if pick is None:
        r.error("templates.json: falta la plantilla «pico» (GDD v2 §3.4, biblia 02 §2.2)")
        return
    head_slot = next((s for s in pick.get("slots", []) if s.get("role") == "Cabeza"), None)
    if head_slot is None:
        r.error("templates.json «pico»: sin slot Cabeza")
        return
    for iid, item in items.items():
        if crafting.slot_satisfied(head_slot, crafting.leaf(item)) and iid not in head_tier:
            r.error(f"mining.json/tools: «{iid}» cabe como cabeza de pico pero no tiene nivel asignado")
    mango = _pick_mango()
    for h, tier in sorted(head_tier.items()):
        if tier < 2:
            continue
        head = crafting.leaf(items[h])
        if not crafting.slot_satisfied(head_slot, head):
            r.error(f"mining.json/tools: «{h}» (nivel {tier}) no cumple la Cabeza de «pico»")
            continue
        for verb in pick.get("verbs", []):
            best = crafting.best_template(ds.templates, verb, head, mango)
            if best is None or best.get("resultDefinitionId") != pick.get("resultDefinitionId"):
                got = best["id"] if best else "nada"
                r.error(f"mining.json/tools: «{h}» + mango atado con «{verb}» produce «{got}», no un pico")

    # ------------------------------------------------------------------ progresión
    _check_progression(ds, doc, materials, strata, tier_heads, r)


def _check_progression(ds, doc, materials, strata, tier_heads, r) -> None:
    """Niveles alcanzables sin ciclos, en total y solo con datos de fase 1."""

    produced = {t["resultDefinitionId"] for t in ds.templates}
    mining_only = {s["item"] for s in strata.values() if not s.get("surfaceSource")}
    base = {i["id"] for i in ds.items if i["id"] not in produced and i["id"] not in mining_only}
    recipe_inputs = _head_inputs(ds)

    def surface_phase(item: str) -> int:
        """Primera fase en la que un estrato con afloramiento deja ``item`` en superficie."""

        phases = [o.get("fase", 9) for s in strata.values() if s.get("surfaceSource") and s.get("item") == item
                  for o in s.get("occurrences", [])]
        return min(phases) if phases else 0

    def reachable_tiers(max_phase: int) -> tuple[int, dict[str, int]]:
        # Lo que aflora en superficie solo cuenta si su isla es de esta fase o anterior.
        have = {i for i in base if surface_phase(i) <= max_phase}
        tier = 1 if "pala" in produced or "pala" in have else 0
        mined_at: dict[str, int] = {}
        changed = True
        while changed:
            changed = False
            for s in strata.values():
                if not any(o.get("fase", 9) <= max_phase for o in s.get("occurrences", [])):
                    continue
                need = 0 if s.get("handPickable") else materials.get(s.get("material"), {}).get("minToolTier", 99)
                if need <= tier and s["id"] not in mined_at:
                    have.add(s["item"])
                    mined_at[s["id"]] = tier
                    changed = True
            for t in range(tier + 1, MAX_TIER + 1):
                if any(_head_available(h, have, recipe_inputs) for h in tier_heads.get(t, ())):
                    tier = t
                    changed = True
        # Saltar un nivel (p. ej. obsidiana suelta en superficie) no vale por el nivel saltado:
        # cada pico de 2 a MAX_TIER necesita alguna cabeza obtenible por sí mismo.
        for t in range(2, MAX_TIER + 1):
            if t <= tier and not any(_head_available(h, have, recipe_inputs) for h in tier_heads.get(t, ())):
                tier = t - 1
                break
        return tier, mined_at

    best, _ = reachable_tiers(max(PHASES))
    if best < MAX_TIER:
        r.error(f"mining.json: la progresión de picos se queda en el nivel {best} (ciclo o cabeza inobtenible)")
    ea, mined = reachable_tiers(1)
    if ea < MAX_TIER:
        r.error(f"mining.json: con solo islas de fase 1 el pico llega al nivel {ea}; el acceso anticipado exige {MAX_TIER}")
    for s in strata.values():
        if any(o.get("fase") == 1 for o in s.get("occurrences", [])) and s["id"] not in mined:
            r.error(f"mining.json/strata «{s['id']}»: aparece en fase 1 pero ninguna herramienta de fase 1 lo cava")


# Memoria de _head_inputs por contenido (objetos, plantillas y piezas): es pura y recorre todos
# los pares de objetos en bruto, lo más caro de check_mining con el catálogo real.
_HEAD_INPUTS_CACHE: dict[str, dict[str, list[set[str]]]] = {}


def _head_inputs(ds) -> dict[str, list[set[str]]]:
    pieces = ds.data.get("building_pieces.json", {}).get("pieces", [])
    key = json.dumps([ds.items, ds.templates, pieces], sort_keys=True, default=repr)
    if key not in _HEAD_INPUTS_CACHE:
        if len(_HEAD_INPUTS_CACHE) >= 8:
            _HEAD_INPUTS_CACHE.pop(next(iter(_HEAD_INPUTS_CACHE)))
        _HEAD_INPUTS_CACHE[key] = _compute_head_inputs(ds)
    return copy.deepcopy(_HEAD_INPUTS_CACHE[key])


def _compute_head_inputs(ds) -> dict[str, list[set[str]]]:
    """Para objetos fabricados que sirven de cabeza: conjuntos de objetos que los producen en un paso.

    Si la plantilla exige estación (``station``), su coste de construcción entra en el conjunto.
    """

    items = {i["id"]: i for i in ds.items}
    pieces = {p.get("id"): p for p in ds.data.get("building_pieces.json", {}).get("pieces", [])}
    out: dict[str, list[set[str]]] = {}
    leaves = [crafting.leaf(i) for i in ds.items if "interno" not in i.get("tags", [])]
    for t in ds.templates:
        res = t.get("resultDefinitionId")
        if res not in items or "mineria" not in items[res].get("tags", []):
            continue
        station_cost = {c.get("item") for c in pieces.get(t.get("station"), {}).get("cost", [])}
        for verb in t.get("verbs", []):
            for a in leaves:
                for b in leaves:
                    best = crafting.best_template(ds.templates, verb, a, b)
                    if best is t:
                        out.setdefault(res, []).append({a.definition, b.definition} | station_cost)
    return out


def _head_available(head: str, have: set[str], recipe_inputs: dict[str, list[set[str]]]) -> bool:
    if head in have:
        return True
    return any(inputs <= have for inputs in recipe_inputs.get(head, []))
