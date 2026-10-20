"""Contenido de texto de H4: diarios Halden y diario del jugador.

Referencias cruzadas contra el C++ y el resto de los datos:

- **Islas**: ids en minúsculas de ``LexToString(EIslandArchetype)`` (``ArchipelagoLayout.cpp``),
  con la fase de cada una de biblia 04 §1.3.
- **POI**: ``EPoiType`` y en qué isla los coloca ``PointsOfInterest.cpp`` (``Add(EPoiType::X,
  <Isla>Index, …, FName(TEXT("content_id")))``); los que se colocan con un índice genérico
  (miradores, botellas, petroglifos) valen para cualquier isla.
- **Disparadores** del diario: un suceso declarado en ``journal_entries.json → events``. Los
  disparadores por estadística se validaban contra ``achievements.json``, que salió del juego con
  los logros; quedan sin validar hasta que se reescriba el diario.
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Any

from . import estilo

POI_H = "Source/Explored/WorldGen/PointsOfInterest.h"
POI_CPP = "Source/Explored/WorldGen/PointsOfInterest.cpp"
ARCHIPELAGO_CPP = "Source/Explored/WorldGen/ArchipelagoLayout.cpp"
ID = re.compile(r"^[a-z0-9_]+$")

PHASES = ("AA", "F2", "F3")
# Biblia 04 §1.3: las cuatro islas del acceso anticipado y las tres de la fase 2.
ISLAND_PHASE = {
    "landing": "AA", "emerald": "AA", "smoke": "AA", "teeth": "AA",
    "mangrove": "F2", "whitesands": "F2", "mesa": "F2",
}
HIDDEN_ISLAND = "hidden"
# Variable de índice de isla en PointsOfInterest.cpp → isla. Cualquier otro índice (I,
# IslandIndex, INDEX_NONE) es un POI que puede caer en cualquier isla.
POI_INDEX_VARS = {
    "LandingIndex": "landing", "EmeraldIndex": "emerald", "SmokeIndex": "smoke", "TeethIndex": "teeth",
    "MangroveIndex": "mangrove", "SandsIndex": "whitesands", "MesaIndex": "mesa",
}
ANY_ISLAND = "*"

HALDEN_IDS = [f"halden_{n:02d}" for n in range(1, 6)]  # biblia 04 §7.1
JOURNAL_MIN = 15  # biblia 07 §4.2
CONTENT_FILES = ["halden_diaries.json", "journal_entries.json"]


def _phase_rank(phase: object) -> int:
    return PHASES.index(phase) if phase in PHASES else -1


def cpp_islands(repo_root: Path) -> set[str]:
    path = repo_root / ARCHIPELAGO_CPP
    if not path.exists():
        return set()
    text = path.read_text(encoding="utf-8")
    return {i.lower() for i in re.findall(r'case EIslandArchetype::\w+: return TEXT\("([^"]+)"\);', text)}


def poi_types(repo_root: Path) -> set[str]:
    path = repo_root / POI_H
    if not path.exists():
        return set()
    m = re.search(r"enum class EPoiType[^{]*\{(.*?)\};", path.read_text(encoding="utf-8"), re.S)
    if not m:
        return set()
    body = re.sub(r"//[^\n]*", "", m.group(1))
    return {n for n in re.findall(r"\b([A-Z]\w*)\b", body) if n != "Count"}


def poi_placements(repo_root: Path) -> tuple[dict[str, set[str]], dict[str, tuple[str, str]]]:
    """(islas de cada tipo de POI, ContentId literal → (tipo, isla)). ``*`` = cualquier isla."""
    path = repo_root / POI_CPP
    by_type: dict[str, set[str]] = {}
    by_content: dict[str, tuple[str, str]] = {}
    if not path.exists():
        return by_type, by_content
    for line in path.read_text(encoding="utf-8").splitlines():
        m = re.search(r"Add\(EPoiType::(\w+),\s*(\w+)", line)
        if not m:
            continue
        kind, index = m.groups()
        island = POI_INDEX_VARS.get(index, ANY_ISLAND)
        by_type.setdefault(kind, set()).add(island)
        cm = re.search(r'FName\(TEXT\("([a-z0-9_]+)"\)\)', line)
        if cm:
            by_content[cm.group(1)] = (kind, island)
    return by_type, by_content


def _poi_on_island(by_type: dict[str, set[str]], kind: str, island: str) -> bool:
    places = by_type.get(kind, set())
    return island in places or ANY_ISLAND in places


class _Ctx:
    def __init__(self, ds, r) -> None:
        self.ds = ds
        self.r = r
        root = ds.repo_root
        cpp = cpp_islands(root)
        self.islands = cpp or set(ISLAND_PHASE)
        if cpp and cpp != set(ISLAND_PHASE):
            r.error(f"contenido: las islas de EIslandArchetype {sorted(cpp)} no coinciden con las fases de "
                    f"biblia 04 §1.3 {sorted(ISLAND_PHASE)}")
        self.poi_types = poi_types(root)
        self.poi_by_type, self.poi_by_content = poi_placements(root)
        self.cpp_available = bool(self.poi_types and self.poi_by_type)
        if not self.cpp_available:
            r.warn("contenido: no se encuentra PointsOfInterest.{h,cpp}; no se comprueban los POI")

    def island(self, where: str, value) -> bool:
        if value not in self.islands:
            self.r.error(f"{where}: isla {value!r} desconocida (admite {sorted(self.islands)})")
            return False
        return True

    def poi(self, where: str, kind, island: str) -> None:
        if not self.cpp_available:
            return
        if kind not in self.poi_types:
            self.r.error(f"{where}: POI {kind!r} no es un EPoiType (PointsOfInterest.h)")
        elif not _poi_on_island(self.poi_by_type, kind, island):
            placed = sorted(self.poi_by_type.get(kind, set()))
            self.r.error(f"{where}: PointsOfInterest.cpp no coloca ningún {kind} en «{island}» (sí en {placed})")


def _ids(r, where: str, entries: list) -> list[str]:
    ids = [e.get("id") if isinstance(e, dict) else None for e in entries]
    for i in ids:
        if not isinstance(i, str) or not ID.match(i):
            r.error(f"{where}: id inválido {i!r} (minúsculas ASCII, dígitos y _)")
    for dup in sorted({i for i in ids if isinstance(i, str) and ids.count(i) > 1}):
        r.error(f"{where}: id duplicado «{dup}»")
    return [i for i in ids if isinstance(i, str)]


# --------------------------------------------------------------------------- diarios Halden


def check_halden(ctx: _Ctx) -> None:
    doc = ctx.ds.data.get("halden_diaries.json")
    if doc is None:
        return
    r = ctx.r
    entries = doc.get("entries", [])
    ids = _ids(r, "halden_diaries.json", entries)
    if sorted(ids) != HALDEN_IDS:
        r.error(f"halden_diaries.json: ids {sorted(ids)}; la biblia 04 §7.1 fija {HALDEN_IDS}")
    orders = [e.get("order") for e in entries]
    if sorted(o for o in orders if isinstance(o, int)) != list(range(1, len(entries) + 1)):
        r.error(f"halden_diaries.json: order {orders} debe ser 1..{len(entries)} sin huecos ni repetidos")
    seen_content: dict[str, str] = {}
    for e in entries:
        where = f"halden_diaries.json «{e.get('id')}»"
        day = e.get("expeditionDay")
        if day is not None and (not isinstance(day, int) or isinstance(day, bool) or day < 1):
            r.error(f"{where}: expeditionDay debe ser null o un entero ≥ 1")
        estilo.check_pair(r, where, e, "textEs", "textEn", "diario")
        if isinstance(day, int) and not isinstance(day, bool) and isinstance(e.get("textEs"), str):
            if not e["textEs"].startswith(f"Día {day}."):
                r.error(f"{where}: expeditionDay={day} pero el texto no empieza por «Día {day}.»")
        island = e.get("island")
        if not ctx.island(where, island):
            continue
        ctx.poi(where, e.get("poi"), island)
        cid = e.get("contentId")
        if not ctx.cpp_available:
            continue
        placed = ctx.poi_by_content.get(cid)
        if placed is None:
            r.error(f"{where}: contentId {cid!r} no lo usa ningún POI de PointsOfInterest.cpp")
        elif placed != (e.get("poi"), island):
            r.error(f"{where}: contentId «{cid}» es un {placed[0]} en «{placed[1]}», no un {e.get('poi')} en «{island}»")
        if isinstance(cid, str):
            if cid in seen_content:
                r.error(f"{where}: contentId «{cid}» repetido (ya lo usa «{seen_content[cid]}»)")
            seen_content[cid] = e.get("id")


# --------------------------------------------------------------------------- diario del jugador


def check_journal(ctx: _Ctx) -> None:
    doc = ctx.ds.data.get("journal_entries.json")
    if doc is None:
        return
    r = ctx.r
    events: dict[Any, dict] = {e.get("id"): e for e in doc.get("events", []) if isinstance(e, dict)}
    _ids(r, "journal_entries.json/events", doc.get("events", []))
    for eid, ev in events.items():
        if ev.get("phase") not in PHASES:
            r.error(f"journal_entries.json/events «{eid}»: phase {ev.get('phase')!r} (admite {list(PHASES)})")
        if not ev.get("descriptionEs") or not ev.get("reportedBy"):
            r.error(f"journal_entries.json/events «{eid}»: falta descriptionEs o reportedBy")

    entries = doc.get("entries", [])
    _ids(r, "journal_entries.json/entries", entries)
    if len(entries) < JOURNAL_MIN:
        r.error(f"journal_entries.json: {len(entries)} entradas; la biblia 07 §4.2 pide al menos {JOURNAL_MIN}")
    used: set[str] = set()
    for e in entries:
        where = f"journal_entries.json «{e.get('id')}»"
        phase = e.get("phase")
        if phase not in PHASES:
            r.error(f"{where}: phase {phase!r} (admite {list(PHASES)})")
        estilo.check_pair(r, where, e, "textEs", "textEn", "diario")
        for key in ("textEs", "textEn"):
            text = e.get(key)
            if isinstance(text, str) and "{Day}" not in text:
                r.error(f"{where}: {key} sin el marcador {{Day}} (biblia 07 §4.1)")
        trig = e.get("trigger")
        if isinstance(trig, dict) and set(trig) == {"event"}:
            ev = events.get(trig["event"])
            if ev is None:
                r.error(f"{where}: suceso «{trig['event']}» no está en events")
                continue
            used.add(trig["event"])
            if _phase_rank(ev.get("phase")) > _phase_rank(phase):
                r.error(f"{where}: entrada de {phase} disparada por «{trig['event']}», que es de {ev.get('phase')}")
    for eid in sorted(set(events) - used):
        r.error(f"journal_entries.json/events «{eid}»: ningún disparador lo usa")


def check_content(ds, r) -> None:
    ctx = _Ctx(ds, r)
    check_halden(ctx)
    check_journal(ctx)
    halden = len(ds.data.get("halden_diaries.json", {}).get("entries", []))
    journal = len(ds.data.get("journal_entries.json", {}).get("entries", []))
    r.info.append(f"contenido H4: {halden} cuadernos Halden, {journal} entradas de diario")
