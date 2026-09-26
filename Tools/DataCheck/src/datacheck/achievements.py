"""Comprobaciones de Content/Data/achievements.json (GDD §16) y de su catálogo de estadísticas.

Replica las reglas de validación de FAchievementsModel::Configure (estadística conocida y de
tipo compatible) y añade las de diseño: 30 logros, ids ASCII, textos en ES y EN, los ejemplos
del GDD y el catálogo sincronizado con docs/tecnico/estadisticas.md.
"""

from __future__ import annotations

import math
import re
from pathlib import Path

ACHIEVEMENT_COUNT = 30
ID_RE = re.compile(r"^[a-z0-9_]+$")
STAT_KINDS = {"counter", "max", "set", "flag"}
STAT_SCOPES = {"profile", "run"}
COMPARE_OPS = {">=", ">", "<=", "<", "=="}
VALUE_SOURCES = {"items", "plants", "building_pieces"}
STATS_DOC = Path("docs") / "tecnico" / "estadisticas.md"
DOC_ROW = re.compile(r"^\|\s*`([a-z0-9_]+)`\s*\|\s*([a-z]+)\s*\|\s*([a-z]+)\s*\|")

# Conjuntos cuyos ids son los de otro catálogo: tienen que coincidir exactamente (P-WIRE).
CATALOG_SETS = {
    "wayfinding_techniques": ("ruins.json → techniques",
                              lambda ds: [t.get("id") for t in ds.data.get("ruins.json", {}).get("techniques", [])]),
    "legendary_catches": ("fish.json → legendary",
                          lambda ds: [f.get("id") for f in ds.data.get("fish.json", {}).get("legendary", [])]),
    "boats_built": ("boats.json → boats", lambda ds: [b.get("id") for b in ds.boats]),
}

# Ejemplos del GDD §16: tienen que existir con estos ids.
REQUIRED = {
    "primer_fuego", "tierra_firme", "cartografo", "coleccionista", "rey_del_cocotero",
    "bajo_el_volcan", "luz_en_el_agua", "ojo_de_ciclon", "wayfinder", "el_limonero",
    "limon_zarpa", "naufrago_de_verdad", "sin_mapa",
}
TEXT_FIELDS = ("nameEs", "nameEn", "descriptionEs", "descriptionEn")


def _is_number(value) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def allowed_values(ds, stat: dict) -> set[str] | None:
    """Ids admitidos en un conjunto, o None si es abierto."""
    if "values" in stat:
        return set(stat["values"])
    source = stat.get("valuesFrom")
    if source == "items":
        return set(ds.item_ids)
    if source == "plants":
        return {p.get("id") for p in ds.plants}
    if source == "building_pieces":
        return {p.get("id") for p in ds.building.get("pieces", [])}
    return None


def doc_stats(repo_root: Path) -> dict[str, tuple[str, str]] | None:
    path = repo_root / STATS_DOC
    if not path.exists():
        return None
    rows = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        m = DOC_ROW.match(line)
        if m:
            rows[m.group(1)] = (m.group(2), m.group(3))
    return rows


def _stats_used(cond, out: set[str]) -> None:
    if not isinstance(cond, dict):
        return
    for key in ("stat", "flag"):
        if isinstance(cond.get(key), str):
            out.add(cond[key])
    for key in ("all", "any"):
        for child in cond.get(key, []) if isinstance(cond.get(key), list) else []:
            _stats_used(child, out)
    if "not" in cond:
        _stats_used(cond["not"], out)


def check_condition(ds, cond, stats: dict[str, dict], where: str, r) -> None:
    if not isinstance(cond, dict):
        r.error(f"{where}: la condición debe ser un objeto")
        return
    keys = set(cond)
    if keys == {"stat", "op", "value"}:
        stat = stats.get(cond["stat"])
        if stat is None:
            r.error(f"{where}: estadística desconocida «{cond['stat']}»")
            return
        if stat.get("kind") == "flag":
            r.error(f"{where}: «{cond['stat']}» es una marca; usa {{\"flag\": ...}}")
        if cond["op"] not in COMPARE_OPS:
            r.error(f"{where}: operador «{cond['op']}» desconocido")
        if not _is_number(cond["value"]):
            r.error(f"{where}: value debe ser un número finito")
            return
        values = allowed_values(ds, stat) if stat.get("kind") == "set" else None
        if values is not None and cond["op"] in {">=", ">"} and cond["value"] > len(values):
            r.error(f"{where}: pide {cond['value']} de «{cond['stat']}», que solo admite {len(values)} ids")
    elif keys == {"stat", "contains"}:
        stat = stats.get(cond["stat"])
        if stat is None:
            r.error(f"{where}: estadística desconocida «{cond['stat']}»")
            return
        if stat.get("kind") != "set":
            r.error(f"{where}: «{cond['stat']}» no es un conjunto")
            return
        values = allowed_values(ds, stat)
        if values is not None and cond["contains"] not in values:
            r.error(f"{where}: «{cond['contains']}» no es un id admitido en «{cond['stat']}»")
    elif keys == {"flag"}:
        stat = stats.get(cond["flag"])
        if stat is None:
            r.error(f"{where}: estadística desconocida «{cond['flag']}»")
        elif stat.get("kind") != "flag":
            r.error(f"{where}: «{cond['flag']}» no es una marca")
    elif keys in ({"all"}, {"any"}):
        (key,) = keys
        children = cond[key]
        if not isinstance(children, list) or not children:
            r.error(f"{where}: «{key}» necesita una lista no vacía")
            return
        for child in children:
            check_condition(ds, child, stats, where, r)
    elif keys == {"not"}:
        check_condition(ds, cond["not"], stats, where, r)
    else:
        r.error(f"{where}: condición con claves {sorted(keys)} no reconocida")


def check_achievements(ds, r) -> None:
    doc = ds.data.get("achievements.json")
    if doc is None:
        return
    modes = set(doc.get("modes", []))
    if not {"Explorer", "Survivor", "Castaway"} <= modes:
        r.error("achievements.json: «modes» debe incluir Explorer, Survivor y Castaway (GDD §11)")

    # Catálogo de estadísticas.
    stats: dict[str, dict] = {}
    for stat in doc.get("stats", []):
        sid = stat.get("id")
        if not isinstance(sid, str) or not ID_RE.match(sid):
            r.error(f"achievements.json: id de estadística inválido «{sid}»")
            continue
        if sid in stats:
            r.error(f"achievements.json: estadística duplicada «{sid}»")
        stats[sid] = stat
        if stat.get("kind") not in STAT_KINDS:
            r.error(f"achievements.json: estadística «{sid}» con kind «{stat.get('kind')}» (admite {sorted(STAT_KINDS)})")
        if stat.get("scope") not in STAT_SCOPES:
            r.error(f"achievements.json: estadística «{sid}» con scope «{stat.get('scope')}» (admite {sorted(STAT_SCOPES)})")
        if not stat.get("descriptionEs"):
            r.error(f"achievements.json: estadística «{sid}» sin descriptionEs")
        if ("values" in stat or "valuesFrom" in stat) and stat.get("kind") != "set":
            r.error(f"achievements.json: «{sid}» tiene values/valuesFrom pero no es un conjunto")
        if "values" in stat:
            vals = stat["values"]
            if not isinstance(vals, list) or not vals or len(set(vals)) != len(vals):
                r.error(f"achievements.json: «{sid}».values debe ser una lista no vacía sin repetidos")
        if "valuesFrom" in stat and stat["valuesFrom"] not in VALUE_SOURCES:
            r.error(f"achievements.json: «{sid}».valuesFrom «{stat['valuesFrom']}» (admite {sorted(VALUE_SOURCES)})")

    # Conjuntos que reflejan otro catálogo (lo que informa el juego sale de ahí).
    for sid, (source, ids_of) in CATALOG_SETS.items():
        stat = stats.get(sid)
        ids = [i for i in ids_of(ds) if isinstance(i, str)]
        if stat is None or not ids or not isinstance(stat.get("values"), list):
            continue
        if set(stat["values"]) != set(ids):
            r.error(f"achievements.json: «{sid}».values {sorted(stat['values'])} no coincide con {source} {sorted(ids)}")

    # Logros.
    achievements = doc.get("achievements", [])
    if len(achievements) != ACHIEVEMENT_COUNT:
        r.error(f"achievements.json: {len(achievements)} logros; el GDD §16 fija {ACHIEVEMENT_COUNT}")
    seen: set[str] = set()
    used: set[str] = set()
    for ach in achievements:
        aid = ach.get("id")
        where = f"achievements.json «{aid}»"
        if not isinstance(aid, str) or not ID_RE.match(aid):
            r.error(f"achievements.json: id inválido «{aid}» (ASCII en minúsculas, dígitos y _)")
        if aid in seen:
            r.error(f"achievements.json: id duplicado «{aid}»")
        seen.add(aid)
        for key in TEXT_FIELDS:
            text = ach.get(key)
            if not isinstance(text, str) or not text.strip():
                r.error(f"{where}: falta {key}")
            elif "almudena" in text.lower():
                r.error(f"{where}: {key} usa el nombre de la dedicatoria; va solo en el menú y los créditos")
        if not isinstance(ach.get("hidden"), bool):
            r.error(f"{where}: hidden debe ser true o false")
        icon = ach.get("icon")
        if not isinstance(icon, str) or not ID_RE.match(icon):
            r.error(f"{where}: icon debe ser una palabra ASCII en minúsculas")
        if "modes" in ach:
            ach_modes = ach["modes"]
            if not isinstance(ach_modes, list) or not ach_modes or not set(ach_modes) <= modes:
                r.error(f"{where}: modes {ach_modes!r} no es una lista no vacía de {sorted(modes)}")
        check_condition(ds, ach.get("condition"), stats, where, r)
        _stats_used(ach.get("condition"), used)

    missing = REQUIRED - seen
    if missing:
        r.error(f"achievements.json: faltan los logros del GDD §16 {sorted(missing)}")
    by_id = {a.get("id"): a for a in achievements}
    if "naufrago_de_verdad" in by_id and by_id["naufrago_de_verdad"].get("modes") != ["Castaway"]:
        r.error("achievements.json «naufrago_de_verdad»: debe limitarse al modo Castaway")
    if "sin_mapa" in by_id:
        cond = by_id["sin_mapa"].get("condition", {})
        children = cond.get("all", []) if isinstance(cond, dict) else []
        if {"flag": "hidden_island_reached"} not in children or {"not": {"flag": "coast_drawn"}} not in children:
            r.error("achievements.json «sin_mapa»: debe ser hidden_island_reached y no coast_drawn")

    for sid in sorted(set(stats) - used):
        r.warn(f"achievements.json: la estadística «{sid}» no la usa ningún logro")

    # Catálogo documentado.
    documented = doc_stats(ds.repo_root)
    if documented is None:
        r.error(f"Falta {STATS_DOC.as_posix()} con el catálogo de estadísticas")
    else:
        for sid, stat in stats.items():
            if sid not in documented:
                r.error(f"{STATS_DOC.as_posix()}: falta la estadística «{sid}» de achievements.json")
            elif documented[sid] != (stat.get("kind"), stat.get("scope")):
                r.error(f"{STATS_DOC.as_posix()}: «{sid}» documentada como {documented[sid]} pero achievements.json dice "
                        f"({stat.get('kind')}, {stat.get('scope')})")
        for sid in sorted(set(documented) - set(stats)):
            r.error(f"{STATS_DOC.as_posix()}: «{sid}» no está en el catálogo de achievements.json")

    hidden = sum(1 for a in achievements if a.get("hidden"))
    r.info.append(f"achievements.json: {len(achievements)} logros ({hidden} ocultos), {len(stats)} estadísticas")
