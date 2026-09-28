"""Barcos por piezas (biblia 02 §8): ``Content/Data/boat_pieces.json`` y los planos de ``boats.json``.

- Catálogo: espejo exacto de ``EBoatPieceType`` y de la tabla ``FSpecTable`` de
  ``FBoatPiecesModel`` (id, masa, litros de flotación, carga, vela e integridad); tier,
  estación, coste y herramientas válidos y obtenibles; nombres en ES y EN.
- Planos: cada embarcación de ``boats.json`` lista sus piezas (``pieces``) y el recuento
  coincide fila a fila con las tablas ``<Tipo>Rows`` de ``BoatPiecesModel.cpp``; todo plano
  tiene quilla, cuadernas y tablones; el «Limón» exige las cuatro piezas del Albatros.
"""

from __future__ import annotations

import re

PIECES_CPP = "Source/Explored/Boats/BoatPiecesModel.cpp"
PIECES_H = "Source/Explored/Boats/BoatPiecesModel.h"
ID_RE = re.compile(r"^[a-z0-9_]+$")
# Toda pieza del catálogo de la biblia 02 §8.1, por su tipo del C++.
REQUIRED_TYPES = {
    "Keel", "Frame", "HullPlank", "Deck", "Mast", "Sail", "Outrigger", "Rudder", "RowingBench", "Mooring",
}
# Sin estas tres no hay casco (EBoatHullIssue::NoKeel, NoFrames, NoPlanks).
HULL_MINIMUM = ("Keel", "Frame", "HullPlank")
NUMERIC_FIELDS = ("massKg", "buoyancyLiters", "cargoKg", "sailAreaM2")
SPEC_RE = re.compile(
    r"Set\(T::(\w+),\s*TEXT\(\"([a-z0-9_]+)\"\),\s*FVector\([^)]*\),\s*"
    r"([\d.]+)f,\s*([\d.]+)f,\s*([\d.]+)f,\s*([\d.]+)f,\s*(\d+)\)"
)
ROWS_RE = re.compile(r"const FBlueprintRow (\w+)Rows\[\] = \{(.*?)\};", re.S)


def _read(ds, rel: str) -> str:
    path = ds.repo_root / rel
    return path.read_text(encoding="utf-8") if path.exists() else ""


def cpp_enum(header: str) -> list[str]:
    m = re.search(r"enum class EBoatPieceType\s*:\s*uint8\s*\{(.*?)\};", header, re.S)
    if not m:
        return []
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    names = [v.strip() for v in body.split(",")]
    return [n for n in names if n and n != "Count"]


def cpp_specs(source: str) -> dict[str, dict]:
    """Tipo del C++ -> {id, massKg, buoyancyLiters, cargoKg, sailAreaM2, integrity}."""
    specs = {}
    for m in SPEC_RE.finditer(source):
        specs[m.group(1)] = {
            "id": m.group(2), "massKg": float(m.group(3)), "buoyancyLiters": float(m.group(4)),
            "cargoKg": float(m.group(5)), "sailAreaM2": float(m.group(6)), "integrity": int(m.group(7)),
        }
    return specs


def cpp_blueprints(source: str) -> dict[str, dict[str, int]]:
    """EBoatType -> {EBoatPieceType: filas} de las tablas de planos canónicos."""
    plans = {}
    for m in ROWS_RE.finditer(source):
        counts: dict[str, int] = {}
        for t in re.findall(r"\{T::(\w+),", m.group(2)):
            counts[t] = counts.get(t, 0) + 1
        plans[m.group(1)] = counts
    return plans


def check_boat_pieces(ds, r, obtainable: set[str], *, header: str | None = None, source: str | None = None) -> None:
    doc = ds.data.get("boat_pieces.json")
    if not doc:
        return
    header = _read(ds, PIECES_H) if header is None else header
    source = _read(ds, PIECES_CPP) if source is None else source
    enum = cpp_enum(header)
    specs = cpp_specs(source)
    if not enum:
        r.warn("boat_pieces.json: no se encuentra EBoatPieceType en BoatPiecesModel.h; no se compara con el C++")
    elif set(enum) != REQUIRED_TYPES:
        r.error(f"BoatPiecesModel.h: EBoatPieceType debe tener las diez piezas de la biblia 02 §8.1 {sorted(REQUIRED_TYPES)}")

    items = set(ds.item_ids)
    stations = {p.get("id") for p in ds.building.get("pieces", [])}
    tiers = {t.get("id") for t in ds.building.get("tiers", [])}
    pieces = doc.get("pieces", [])
    by_id: dict[str, dict] = {}
    by_type: dict[str, str] = {}
    for p in pieces:
        pid = p.get("id")
        where = f"boat_pieces.json «{pid}»"
        if not isinstance(pid, str) or not ID_RE.match(pid):
            r.error(f"boat_pieces.json: id inválido {pid!r} (minúsculas, dígitos y _)")
            continue
        if pid in by_id:
            r.error(f"boat_pieces.json: id duplicado «{pid}»")
        if pid in items:
            r.error(f"{where}: el id choca con un objeto de items.json")
        by_id[pid] = p
        ptype = p.get("type")
        if enum and ptype not in enum:
            r.error(f"{where}: tipo «{ptype}» no está en EBoatPieceType")
        if ptype in by_type:
            r.error(f"{where}: el tipo «{ptype}» ya es de «{by_type[ptype]}»")
        by_type[ptype] = pid
        for key in ("nameEs", "nameEn"):
            if not isinstance(p.get(key), str) or not p[key].strip():
                r.error(f"{where}: falta {key}")
        if p.get("tier") not in tiers:
            r.error(f"{where}: tier «{p.get('tier')}» no está en building_pieces.json")
        if p.get("station") not in stations:
            r.error(f"{where}: estación «{p.get('station')}» no está en building_pieces.json")
        if not p.get("cost"):
            r.error(f"{where}: sin coste")
        for c in p.get("cost", []):
            if c.get("item") not in items:
                r.error(f"{where}: ingrediente «{c.get('item')}» no está en items.json")
            elif c["item"] not in obtainable:
                r.error(f"{where}: ingrediente «{c['item']}» no obtenible")
            if not isinstance(c.get("count"), int) or not 1 <= c["count"] <= 50:
                r.error(f"{where}: count {c.get('count')!r} fuera de [1, 50]")
        for tool in p.get("tools", []):
            if tool not in items:
                r.error(f"{where}: herramienta «{tool}» no está en items.json")
            elif tool not in obtainable:
                r.error(f"{where}: herramienta «{tool}» no obtenible")
        if not isinstance(p.get("buildMinutes"), int) or not 1 <= p["buildMinutes"] <= 240:
            r.error(f"{where}: buildMinutes fuera de [1, 240]")
        integrity = p.get("integrity")
        if not isinstance(integrity, int) or not 1 <= integrity <= 100:
            r.error(f"{where}: integrity {integrity!r} fuera de [1, 100] (biblia 02 §8.3)")
        for key in NUMERIC_FIELDS:
            v = p.get(key)
            if isinstance(v, bool) or not isinstance(v, (int, float)) or v < 0:
                r.error(f"{where}: {key} debe ser un número ≥ 0")
        # Espejo del C++: la física manda allí; el JSON es lo que lee el editor y el jugador.
        spec = specs.get(ptype)
        if specs and spec is None:
            r.error(f"{where}: el tipo «{ptype}» no tiene fila en FSpecTable ({PIECES_CPP})")
        elif spec:
            if spec["id"] != pid:
                r.error(f"{where}: FSpecTable dice que {ptype} es «{spec['id']}»")
            for key in NUMERIC_FIELDS + ("integrity",):
                v = p.get(key)
                if isinstance(v, (int, float)) and not isinstance(v, bool) and abs(v - spec[key]) > 1e-6:
                    r.error(f"{where}: {key}={v} pero FBoatPieceSpec dice {spec[key]:g}")
    missing = REQUIRED_TYPES - set(by_type)
    if missing:
        r.error(f"boat_pieces.json: faltan piezas de la biblia 02 §8.1: {sorted(missing)}")

    _check_blueprints(ds, r, by_id, cpp_blueprints(source))


def _check_blueprints(ds, r, by_id: dict[str, dict], plans: dict[str, dict[str, int]]) -> None:
    for b in ds.boats:
        bid = b.get("id")
        where = f"boats.json «{bid}»"
        listed = b.get("pieces")
        if not isinstance(listed, list) or not listed:
            r.error(f"{where}: el plano no lista sus piezas (biblia 02 §8.4)")
            continue
        counts: dict[str, int] = {}
        for entry in listed:
            pid = entry.get("piece")
            count = entry.get("count")
            if pid not in by_id:
                r.error(f"{where}: pieza «{pid}» no está en boat_pieces.json")
                continue
            if isinstance(count, bool) or not isinstance(count, int) or not 1 <= count <= 64:
                r.error(f"{where}: count {count!r} de «{pid}» fuera de [1, 64]")
                continue
            ptype = by_id[pid].get("type")
            if ptype in counts:
                r.error(f"{where}: «{pid}» aparece dos veces")
            counts[ptype] = counts.get(ptype, 0) + count
        for need in HULL_MINIMUM:
            if need not in counts:
                r.error(f"{where}: el plano no tiene {need}: no hay casco (EBoatHullIssue)")
        if "Sail" in counts and "Mast" not in counts:
            r.error(f"{where}: vela sin mástil")
        cpp = plans.get(b.get("type"))
        if plans and cpp is None:
            r.error(f"{where}: no hay tabla {b.get('type')}Rows en {PIECES_CPP}")
        elif cpp is not None and cpp != counts:
            r.error(f"{where}: las piezas no coinciden con FBoatPiecesModel::Blueprint ({dict(sorted(cpp.items()))})")
