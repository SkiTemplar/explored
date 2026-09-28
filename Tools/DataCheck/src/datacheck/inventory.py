"""Reglas del inventario que dependen de los datos: pilas y tabla de ids de red.

- Pilas (biblia 03 §1.3): espejo de ``FInventoryModel::ComputeMaxStack``. Apilan hasta
  ``MaxStackSize`` los objetos sin durabilidad ni líquido propio, que no sean DosManos ni
  lleven una etiqueta de ``NonStackableTagNames`` (InventoryModel.cpp). El equipo que
  reconoce ``FindEquipmentSpec`` por su id tiene que llevar su etiqueta para no apilar.
- Tabla de ids de red (biblia 08 §2.4): espejo de ``FContentIdTableModel``. Cada fichero
  de ``CONTENT_TABLES`` numera sus ids por orden de bytes en un ``uint16``; los ids tienen
  que ser ``[a-z0-9_]`` y únicos para que el orden no dependa de mayúsculas ni del idioma.
- Hash de contenido del saludo de conexión: FNV-1a de 64 bits de ``Content/Data/*.json``
  con el mismo formato que ``FContentIdTableModel::ComputeContentHash``.
"""

from __future__ import annotations

import re
from pathlib import Path

INVENTORY_H = Path("Source") / "Explored" / "Carry" / "InventoryModel.h"
INVENTORY_CPP = Path("Source") / "Explored" / "Carry" / "InventoryModel.cpp"
CONTENT_IDS_CPP = Path("Source") / "Explored" / "Items" / "ContentIdTableModel.cpp"

# Biblia 03 §1.3: «apilan hasta 10 unidades por hueco».
BIBLIA_STACK = 10
# Límite duro de carga sin mochila (FInventoryModel::BaseComfortableKg × MaxLoadRatio).
BODY_LIMIT_KG = 15.0 * 2.0
# Mismo orden que EContentKind (ContentIdTableModel.h).
CONTENT_TABLES = [
    "items.json", "templates.json", "building_pieces.json", "plants.json", "boats.json", "achievements.json",
]
UINT16_IDS = 0xFFFF  # 0..65534; 0xFFFF es «ninguno».
CONTENT_ID = re.compile(r"^[a-z0-9_]+$")

FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211
MASK64 = (1 << 64) - 1


# --------------------------------------------------------------------------- hash

def fnv1a64(data: bytes, h: int = FNV_OFFSET) -> int:
    for byte in data:
        h = ((h ^ byte) * FNV_PRIME) & MASK64
    return h


def content_hash(files: dict[str, bytes]) -> int:
    """Nombre, byte 0, longitud en 8 bytes little-endian y contenido, por orden de nombre."""
    h = FNV_OFFSET
    for name in sorted(files, key=lambda n: n.encode("ascii")):
        data = files[name]
        h = fnv1a64(name.encode("ascii") + b"\0" + len(data).to_bytes(8, "little") + data, h)
    return h


def content_hash_of_repo(repo_root: Path) -> int:
    data_dir = repo_root / "Content" / "Data"
    return content_hash({p.name: p.read_bytes() for p in data_dir.glob("*.json")})


# --------------------------------------------------------------------------- espejos del C++

def _read(repo_root: Path, rel: Path) -> str | None:
    path = repo_root / rel
    return path.read_text(encoding="utf-8") if path.exists() else None


def cpp_max_stack(repo_root: Path) -> int | None:
    text = _read(repo_root, INVENTORY_H)
    m = re.search(r"static constexpr int32 MaxStackSize = (\d+);", text or "")
    return int(m.group(1)) if m else None


def cpp_non_stackable_tags(repo_root: Path) -> list[str] | None:
    text = _read(repo_root, INVENTORY_CPP)
    m = re.search(r"NonStackableTagNames\[\]\s*=\s*\{(.*?)\};", text or "", re.S)
    return re.findall(r'TEXT\("([^"]+)"\)', m.group(1)) if m else None


def cpp_equipment_ids(repo_root: Path) -> dict[str, str]:
    """Id → etiqueta que FindEquipmentSpec también acepta (``Id == ... || Item.HasTag(...)``)."""
    text = _read(repo_root, INVENTORY_CPP) or ""
    m = re.search(r"FInventoryModel::FindEquipmentSpec\(.*?\n\}", text, re.S)
    body = m.group(0) if m else ""
    out: dict[str, str] = {}
    pattern = r'if \(Id == FName\(TEXT\("([^"]+)"\)\)(?: \|\| Item\.HasTag\(Tag\(TEXT\("([^"]+)"\)\)\))?\)'
    for iid, tag in re.findall(pattern, body):
        out[iid] = tag
    return out


def cpp_content_tables(repo_root: Path) -> list[str] | None:
    text = _read(repo_root, CONTENT_IDS_CPP)
    m = re.search(r"LexToString\(EContentKind Kind\)\s*\{(.*?)\n\}", text or "", re.S)
    return re.findall(r'case EContentKind::\w+: return TEXT\("([^"]+)"\);', m.group(1)) if m else None


# --------------------------------------------------------------------------- pilas

def _recipiente(item: dict) -> float:
    return next((p.get("value", 0) for p in item.get("properties", []) if p.get("name") == "Recipiente"), 0)


def liquid_capacity(item: dict) -> float:
    """Espejo de UCarryComponent::MakeRecord + LiquidCapacityFromRecipiente."""
    tags = item.get("tags", [])
    if "recipiente" in tags or "cantimplora" in tags:
        return max(0.0, _recipiente(item)) * 0.25
    return 0.0


def max_stack(item: dict, non_stackable_tags: list[str], stack_size: int = BIBLIA_STACK) -> int:
    """Espejo de FInventoryModel::ComputeMaxStack para una definición de items.json."""
    if (item.get("maxDurability") or 0) > 0 or liquid_capacity(item) > 0 or item.get("size") == "DosManos":
        return 1
    if any(t in item.get("tags", []) for t in non_stackable_tags):
        return 1
    return stack_size


def _ids_of(ds, name: str) -> list:
    doc = ds.data.get(name)
    if name == "items.json" or name == "templates.json":
        entries = doc if isinstance(doc, list) else []
    elif name == "building_pieces.json":
        entries = (doc or {}).get("pieces", [])
    elif name == "plants.json":
        entries = (doc or {}).get("plants", [])
    elif name == "boats.json":
        entries = (doc or {}).get("boats", [])
    else:
        entries = (doc or {}).get("achievements", [])
    return [e.get("id") if isinstance(e, dict) else None for e in entries]


def check_inventory(ds, r) -> None:
    root = ds.repo_root

    # 1) Espejos del C++.
    stack = cpp_max_stack(root)
    if stack is None:
        r.error(f"{INVENTORY_H.as_posix()}: no encuentro MaxStackSize")
        stack = BIBLIA_STACK
    elif stack != BIBLIA_STACK:
        r.error(f"FInventoryModel::MaxStackSize={stack} y la biblia 03 §1.3 dice {BIBLIA_STACK}")
    tags = cpp_non_stackable_tags(root)
    if tags is None:
        r.error(f"{INVENTORY_CPP.as_posix()}: no encuentro NonStackableTagNames")
        tags = []
    tables = cpp_content_tables(root)
    if tables is None:
        r.error(f"{CONTENT_IDS_CPP.as_posix()}: no encuentro LexToString(EContentKind)")
    elif tables != CONTENT_TABLES:
        r.error(f"EContentKind {tables} y datacheck.inventory.CONTENT_TABLES {CONTENT_TABLES} no coinciden")

    # 2) Pilas de items.json.
    items = {i.get("id"): i for i in ds.items if isinstance(i, dict)}
    for iid, tag in sorted(cpp_equipment_ids(root).items()):
        item = items.get(iid)
        if item is None:
            continue
        if tag and tag not in item.get("tags", []):
            r.error(f"items.json «{iid}»: FindEquipmentSpec lo trata como equipo pero le falta la etiqueta «{tag}»")
        if max_stack(item, tags, stack) > 1:
            r.error(f"items.json «{iid}»: es equipo y apilaría (añade una etiqueta de NonStackableTagNames)")
    stackable = [i for i in items.values() if max_stack(i, tags, stack) > 1]
    heavy = sorted(i["id"] for i in stackable if stack * float(i.get("weightKg", 0)) > BODY_LIMIT_KG)
    r.info.append(f"Pilas: {len(stackable)} de {len(items)} objetos apilan hasta {stack}")
    if heavy:
        r.info.append(f"Pilas: una pila llena de {', '.join(heavy)} pasa de {BODY_LIMIT_KG:g} kg y solo cabe partida")
    tools = sorted(i["id"] for i in stackable if "herramienta" in i.get("tags", []) or "arma" in i.get("tags", []))
    if tools:
        r.info.append(f"Pilas: herramientas o armas sin durabilidad que apilan: {', '.join(tools)}")

    # 3) Tabla de ids de red.
    sizes = []
    for name in CONTENT_TABLES:
        if name not in ds.data:
            continue
        ids = _ids_of(ds, name)
        seen: set[str] = set()
        for iid in ids:
            if not isinstance(iid, str) or not CONTENT_ID.fullmatch(iid):
                r.error(f"{name}: id {iid!r} no vale para la tabla de red (solo a-z, 0-9 y _)")
                continue
            if iid in seen:
                r.error(f"{name}: id «{iid}» repetido; la tabla de red necesita ids únicos")
            seen.add(iid)
        if len(ids) > UINT16_IDS:
            r.error(f"{name}: {len(ids)} ids no caben en un uint16 (máximo {UINT16_IDS})")
        sizes.append(f"{name.removesuffix('.json')} {len(seen)}")
    r.info.append(f"Tabla de ids de red: {', '.join(sizes)}")
    r.info.append(f"Hash de contenido (saludo de conexión): 0x{content_hash_of_repo(root):016x}")
