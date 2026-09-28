"""Contenido de texto de H4: diarios Halden, diario del jugador, pistas del mapa, museo y ruinas.

Referencias cruzadas contra el C++ y el resto de los datos:

- **Islas**: ids en minúsculas de ``LexToString(EIslandArchetype)`` (``ArchipelagoLayout.cpp``),
  con la fase de cada una de biblia 04 §1.3.
- **POI**: ``EPoiType`` y en qué isla los coloca ``PointsOfInterest.cpp`` (``Add(EPoiType::X,
  <Isla>Index, …, FName(TEXT("content_id")))``); los que se colocan con un índice genérico
  (miradores, botellas, petroglifos) valen para cualquier isla.
- **Disparadores** del diario: el lenguaje de condiciones de ``achievements.json`` sobre sus
  estadísticas, o un suceso declarado en ``journal_entries.json → events``.
"""

from __future__ import annotations

import re
from pathlib import Path

from . import achievements, estilo

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
CLUE_SOURCES = {"petroglyph", "altar", "diary"}
MUSEUM_FILES = {
    # fichero: (colección, formas de conseguir una pieza que admite)
    "shells.json": ("conchas", {"recoger_bajamar", "recoger_playa", "bucear"}),
    "herbarium.json": ("herbario", {"cortar_y_prensar"}),
    "insects.json": ("insectos", {"observar_y_dibujar"}),
    "fossils.json": ("fosiles", {"minar", "recoger_playa"}),
    "minerals.json": ("minerales", {"minar", "recoger_playa"}),
}
COLLECTIONS = {"peces", "conchas", "herbario", "insectos", "artefactos", "tesoros", "minerales", "fosiles"}
PIECE_RARITIES = {"comun", "infrecuente", "raro"}

CONTENT_FILES = [
    "halden_diaries.json", "journal_entries.json", "map_clues.json", "museum_collections.json",
    *MUSEUM_FILES,
]


def _phase_rank(phase: str) -> int:
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
    events = {e.get("id"): e for e in doc.get("events", []) if isinstance(e, dict)}
    _ids(r, "journal_entries.json/events", doc.get("events", []))
    for eid, ev in events.items():
        if ev.get("phase") not in PHASES:
            r.error(f"journal_entries.json/events «{eid}»: phase {ev.get('phase')!r} (admite {list(PHASES)})")
        if not ev.get("descriptionEs") or not ev.get("reportedBy"):
            r.error(f"journal_entries.json/events «{eid}»: falta descriptionEs o reportedBy")

    ach = ctx.ds.data.get("achievements.json", {})
    stats = {s.get("id"): s for s in ach.get("stats", []) if isinstance(s, dict)}

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
        else:
            achievements.check_condition(ctx.ds, trig, stats, where, r)
    for eid in sorted(set(events) - used):
        r.error(f"journal_entries.json/events «{eid}»: ningún disparador lo usa")


# --------------------------------------------------------------------------- pistas del mapa


def check_clues(ctx: _Ctx) -> None:
    doc = ctx.ds.data.get("map_clues.json")
    if doc is None:
        return
    r = ctx.r
    art_doc = ctx.ds.data.get("artifacts.json", {})
    artifacts = {a.get("id"): a for a in art_doc.get("artifacts", []) if isinstance(a, dict)}
    provenances = {p.get("id") for p in art_doc.get("provenances", [])}
    treasure = set(art_doc.get("treasureRarities", []))
    ruins = {s.get("id"): s for s in ctx.ds.data.get("ruins.json", {}).get("sites", [])}
    diaries = {d.get("id"): d for d in ctx.ds.data.get("halden_diaries.json", {}).get("entries", [])}

    hidden_in = doc.get("hiddenIn", {})
    for kind, provs in hidden_in.items():
        for p in provs if isinstance(provs, list) else [provs]:
            if p not in provenances:
                r.error(f"map_clues.json/hiddenIn «{kind}»: procedencia «{p}» no está en artifacts.json")
    covered = {p for provs in hidden_in.values() if isinstance(provs, list) for p in provs}
    for p in sorted(provenances - covered):
        r.error(f"map_clues.json/hiddenIn: ninguna forma de esconder un tesoro de procedencia «{p}»")

    clues = doc.get("clues", [])
    _ids(r, "map_clues.json", clues)
    seen_artifacts: set[str] = set()
    for c in clues:
        where = f"map_clues.json «{c.get('id')}»"
        estilo.check_pair(r, where, c, "clueEs", "clueEn", "pista")
        aid = c.get("artifact")
        art = artifacts.get(aid)
        if art is None:
            r.error(f"{where}: el tesoro «{aid}» no está en artifacts.json")
        else:
            if aid in seen_artifacts:
                r.error(f"{where}: «{aid}» ya tiene pista")
            seen_artifacts.add(aid)
            if art.get("rarity") not in treasure:
                r.error(f"{where}: «{aid}» es {art.get('rarity')}; las pistas son para los tesoros "
                        f"({sorted(treasure)}, biblia 04 §6)")
            kind = c.get("hiddenIn")
            if kind not in hidden_in:
                r.error(f"{where}: hiddenIn {kind!r} no está en hiddenIn ({sorted(hidden_in)})")
            elif art.get("provenance") not in hidden_in[kind]:
                r.error(f"{where}: «{aid}» es de procedencia «{art.get('provenance')}», no cabe en hiddenIn «{kind}»")
        island = c.get("island")
        if not ctx.island(where, island):
            continue
        if c.get("poi") is not None:
            ctx.poi(where, c["poi"], island)
        src = c.get("source")
        if not isinstance(src, dict) or src.get("kind") not in CLUE_SOURCES:
            r.error(f"{where}: source.kind debe ser uno de {sorted(CLUE_SOURCES)}")
            continue
        if src["kind"] == "diary":
            diary = diaries.get(src.get("diary"))
            if diary is None:
                r.error(f"{where}: el cuaderno «{src.get('diary')}» no está en halden_diaries.json")
            elif diary.get("island") != island:
                r.error(f"{where}: la pista es de «{island}» pero el cuaderno «{src['diary']}» está en «{diary.get('island')}»")
        else:
            site = ruins.get(src.get("site"))
            if site is None:
                r.error(f"{where}: la ruina «{src.get('site')}» no está en ruins.json")
            elif site.get("island") != island:
                r.error(f"{where}: la pista es de «{island}» pero la ruina «{src['site']}» está en «{site.get('island')}»")


# --------------------------------------------------------------------------- ruinas


def check_ruin_assignment(ctx: _Ctx) -> None:
    doc = ctx.ds.data.get("ruins.json")
    if not doc:
        return
    r = ctx.r
    techniques = [t.get("id") for t in doc.get("techniques", [])]
    sites = doc.get("sites", [])
    targets: dict[str, str] = {}
    star = 0
    taught: dict[str, list[str]] = {}
    for s in sites:
        sid = s.get("id")
        where = f"ruins.json/sites «{sid}»"
        island = s.get("island")
        if not ctx.island(where, island):
            continue
        expected = "smoke" if sid == "ruin_compass" else (sid or "").removeprefix("ruin_")
        if island != expected:
            r.error(f"{where}: island «{island}» pero el id dice «{expected}»")
        teaches = s.get("teaches")
        target = s.get("starPathTarget")
        if teaches not in techniques:
            r.error(f"{where}: teaches {teaches!r} no es una técnica de ruins.json")
            continue
        taught.setdefault(teaches, []).append(sid)
        if teaches != "star_path":
            if target is not None:
                r.error(f"{where}: enseña «{teaches}» y no debe tener starPathTarget")
            continue
        star += 1
        if target != HIDDEN_ISLAND and target not in ctx.islands:
            r.error(f"{where}: starPathTarget {target!r} no es una isla ni «{HIDDEN_ISLAND}»")
        elif target == island:
            r.error(f"{where}: el camino de estrellas apunta a su propia isla")
        elif target in targets:
            r.error(f"{where}: starPathTarget «{target}» repetido (ya lo enseña «{targets[target]}»)")
        else:
            targets[target] = sid
    for t in techniques:
        if t != "star_path" and len(taught.get(t, [])) != 1:
            r.error(f"ruins.json: la técnica «{t}» la enseñan {taught.get(t, [])}; debe enseñarla una sola ruina")
    required = doc.get("requiredStarPaths")
    if isinstance(required, int) and star < required:
        r.error(f"ruins.json: {star} caminos de estrellas asignados; requiredStarPaths={required}")
    if sites and HIDDEN_ISLAND not in targets:
        r.error("ruins.json: ninguna ruina enseña el camino de estrellas hacia la isla oculta (biblia 04 §2.7)")
    r.info.append("ruins.json: teaches/starPathTarget son la asignación de biblia 04 §2; FRuinsLayout todavía "
                  "la baraja por semilla (la brújula del Humo apunta allí a la isla oculta)")


# --------------------------------------------------------------------------- museo


def _check_museum_piece(ctx: _Ctx, where: str, p: dict, collection_phase: str, obtain: set[str]) -> None:
    r = ctx.r
    estilo.check_pair(r, where, p, "nameEs", "nameEn", "nombre_vitrina")
    estilo.check_pair(r, where, p, "descriptionEs", "descriptionEn", "ficha")
    for name, desc in (("nameEs", "descriptionEs"), ("nameEn", "descriptionEn")):
        if isinstance(p.get(name), str) and isinstance(p.get(desc), str) and not p[desc].startswith(p[name] + "."):
            r.error(f"{where}: {desc} debe empezar por «{p[name]}.» (el nombre de la vitrina)")
    islands = p.get("islands")
    if not isinstance(islands, list) or not islands:
        r.error(f"{where}: islands debe ser una lista no vacía")
        islands = []
    ok = [i for i in islands if ctx.island(where, i)]
    if len(set(islands)) != len(islands):
        r.error(f"{where}: islas repetidas {islands}")
    if not isinstance(p.get("habitat"), str) or not ID.match(p["habitat"]):
        r.error(f"{where}: habitat debe ser un id ASCII")
    if p.get("obtain") not in obtain:
        r.error(f"{where}: obtain {p.get('obtain')!r} (esta colección admite {sorted(obtain)})")
    if "rarity" in p and p["rarity"] not in PIECE_RARITIES:
        r.error(f"{where}: rarity {p['rarity']!r} (admite {sorted(PIECE_RARITIES)})")
    if "item" in p and p["item"] not in ctx.ds.item_ids:
        r.error(f"{where}: item «{p['item']}» no está en items.json")
    if ok:
        earliest = min(ok, key=lambda i: _phase_rank(ISLAND_PHASE.get(i, "F3")))
        expected = PHASES[max(_phase_rank(ISLAND_PHASE.get(earliest, "F3")), _phase_rank(collection_phase))]
        if p.get("phase") != expected:
            r.error(f"{where}: phase {p.get('phase')!r}; por sus islas y su colección es {expected}")


def check_museum(ctx: _Ctx) -> None:
    doc = ctx.ds.data.get("museum_collections.json")
    if doc is None:
        return
    r = ctx.r
    data = ctx.ds.data
    pieces = {p.get("id"): p for p in ctx.ds.building.get("pieces", [])}
    museum_pieces = {pid for pid, p in pieces.items() if p.get("category") == "museo"}
    collections = {c.get("id"): c for c in doc.get("collections", []) if isinstance(c, dict)}
    ids = _ids(r, "museum_collections.json", doc.get("collections", []))
    if set(ids) != COLLECTIONS:
        r.error(f"museum_collections.json: colecciones {sorted(ids)}; la biblia 07 §3.2 fija {sorted(COLLECTIONS)}")
    shown: set[str] = set()
    all_piece_ids: dict[str, str] = {}
    for cid, c in collections.items():
        where = f"museum_collections.json «{cid}»"
        estilo.check_pair(r, where, c, "nameEs", "nameEn", "nombre_vitrina")
        estilo.check_pair(r, where, c, "rewardEs", "rewardEn", "ficha")
        if c.get("phase") not in PHASES:
            r.error(f"{where}: phase {c.get('phase')!r}")
        for d in c.get("displays", []):
            if d not in pieces:
                r.error(f"{where}: el mueble «{d}» no está en building_pieces.json")
            elif d not in museum_pieces:
                r.error(f"{where}: el mueble «{d}» no es de la categoría museo")
            shown.add(d)
        if not c.get("displays"):
            r.error(f"{where}: sin muebles donde exponerla")
        if "derivedFrom" in c:
            base = collections.get(c["derivedFrom"])
            art_doc = data.get("artifacts.json", {})
            rarities = art_doc.get("treasureRarities", [])
            known = {x.get("id") for x in art_doc.get("rarities", [])}
            if base is None or base.get("source") != "artifacts.json":
                r.error(f"{where}: derivedFrom debe ser la colección de artifacts.json")
            if not rarities or not set(rarities) <= known:
                r.error(f"{where}: artifacts.json → treasureRarities {rarities} vacío o con rarezas desconocidas")
            count = sum(1 for a in art_doc.get("artifacts", []) if a.get("rarity") in rarities)
            if c.get("pieces") != count:
                r.error(f"{where}: pieces={c.get('pieces')} pero artifacts.json tiene {count} tesoros")
            continue
        source = c.get("source")
        src = data.get(source)
        if src is None:
            r.error(f"{where}: el fichero {source!r} no existe en Content/Data")
            continue
        items = [x for key in c.get("lists", []) for x in (src.get(key) or [])]
        if not c.get("lists") or not all(isinstance(src.get(k), list) for k in c["lists"]):
            r.error(f"{where}: lists {c.get('lists')} no son listas de {source}")
        if c.get("pieces") != len(items):
            r.error(f"{where}: pieces={c.get('pieces')} pero {source} tiene {len(items)}")
        for x in items:
            xid = x.get("id")
            if not x.get("nameEs") or not x.get("nameEn"):
                r.error(f"{where}: «{xid}» de {source} sin nameEs/nameEn (lo necesita la etiqueta de vitrina)")
            if xid in all_piece_ids:
                r.error(f"{where}: «{xid}» también está en la colección «{all_piece_ids[xid]}»")
            all_piece_ids[xid] = cid
        if source in MUSEUM_FILES:
            expected, obtain = MUSEUM_FILES[source]
            if expected != cid or src.get("collection") != cid:
                r.error(f"{where}: {source} es la colección «{expected}» (su campo collection: {src.get('collection')!r})")
            if src.get("display") not in c.get("displays", []):
                r.error(f"{where}: {source} → display {src.get('display')!r} no está en displays")
            _ids(r, source, src.get("pieces", []))
            for p in src.get("pieces", []):
                _check_museum_piece(ctx, f"{source} «{p.get('id')}»", p, c.get("phase"), obtain)
    for f in MUSEUM_FILES:
        if f in data and f not in {c.get("source") for c in collections.values()}:
            r.error(f"museum_collections.json: ninguna colección lee {f}")
    for pid in sorted(museum_pieces - shown):
        r.error(f"building_pieces.json «{pid}»: mueble de museo que ninguna colección usa")


def check_content(ds, r) -> None:
    ctx = _Ctx(ds, r)
    check_halden(ctx)
    check_journal(ctx)
    check_clues(ctx)
    check_ruin_assignment(ctx)
    check_museum(ctx)
    halden = len(ds.data.get("halden_diaries.json", {}).get("entries", []))
    journal = len(ds.data.get("journal_entries.json", {}).get("entries", []))
    clues = len(ds.data.get("map_clues.json", {}).get("clues", []))
    museum = sum(len(ds.data.get(f, {}).get("pieces", [])) for f in MUSEUM_FILES)
    r.info.append(f"contenido H4: {halden} cuadernos Halden, {journal} entradas de diario, {clues} pistas, "
                  f"{museum} piezas de museo nuevas")
