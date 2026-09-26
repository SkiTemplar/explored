"""Comprobaciones de Content/Data/*.json. Cada check añade mensajes a un Report."""

from __future__ import annotations

import copy
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from . import crafting

REPO_ROOT = Path(__file__).resolve().parents[4]

# Biblia de contenido §2.1 (claves sin tildes, como las usa el C++).
PROPERTIES = {
    "Filo", "Punta", "Largo", "Rigido", "Flexible", "Contundente", "Ata", "Adhesivo",
    "Inflamable", "Combustible", "Calor", "Recipiente", "Estanco", "Abrasivo", "Moldeable",
    "Fibroso", "Aislante", "Impermeable", "Flota", "Brillante", "Sonoro", "Nutritivo",
    "Medicinal", "Toxico", "Pesado",
}
SIZES = {"Pequeno", "Mediano", "Grande", "DosManos"}
# Verbos que exige ItemsSpec.cpp (biblia §2.2).
REQUIRED_VERBS = {"Golpear", "Tallar", "Atar", "Pegar", "Afilar", "Trenzar", "Machacar", "Raspar"}
SEASONS = {"seca", "primeras_lluvias", "monzon", "ciclones"}
BASIC_SHAPES = re.compile(r"^/Engine/BasicShapes/(Cube|Sphere|Cylinder|Cone|Plane)\.\1$")
GENERATED_MESH = re.compile(r"^/Game/Generated/Meshes/[A-Za-z0-9_/]+/(SM_[A-Za-z0-9_]+)\.\1$")

# GDD §10 y §12: sin fauna terrestre ni narrativa de personajes eliminada.
FORBIDDEN_TERMS = [
    "jabali", "jabalí", "cerdo", "cabra", "rata", "murcielago", "murciélago", "serpiente",
    "lagarto", "iguana", "perro", "canela", "almudena_", "rodrigo", "ines", "inés",
    "diario halden", "pagina halden", "página halden", "haldenpage", "beacon", "baliza",
    "rescate", "mascota", "caza terrestre",
]

DATA_FILES = [
    "items.json", "templates.json", "verbs.json", "story_es.json", "plants.json",
    "building_pieces.json", "survival_needs.json", "meshes_pendientes.json",
]


@dataclass
class Report:
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    info: list[str] = field(default_factory=list)

    def error(self, msg: str) -> None:
        self.errors.append(msg)

    def warn(self, msg: str) -> None:
        self.warnings.append(msg)

    @property
    def ok(self) -> bool:
        return not self.errors


@dataclass
class DataSet:
    data: dict[str, object]
    repo_root: Path = REPO_ROOT

    @classmethod
    def load(cls, repo_root: Path = REPO_ROOT) -> "DataSet":
        data_dir = repo_root / "Content" / "Data"
        data = {}
        for name in DATA_FILES:
            path = data_dir / name
            if path.exists():
                data[name] = json.loads(path.read_text(encoding="utf-8"))
        return cls(data, repo_root)

    def copy(self) -> "DataSet":
        return DataSet(copy.deepcopy(self.data), self.repo_root)

    @property
    def items(self) -> list[dict]:
        return self.data.get("items.json", [])

    @property
    def templates(self) -> list[dict]:
        return self.data.get("templates.json", [])

    @property
    def verbs(self) -> list[dict]:
        return self.data.get("verbs.json", [])

    @property
    def plants(self) -> list[dict]:
        return self.data.get("plants.json", {}).get("plants", [])

    @property
    def building(self) -> dict:
        return self.data.get("building_pieces.json", {"tiers": [], "pieces": []})

    @property
    def item_ids(self) -> set[str]:
        return {i.get("id") for i in self.items}


# --------------------------------------------------------------------------- esquemas


def _type_ok(value, types) -> bool:
    if isinstance(value, bool) and bool not in types:
        return False
    return isinstance(value, types)


def check_files_present(ds: DataSet, r: Report) -> None:
    for name in DATA_FILES:
        if name not in ds.data:
            r.error(f"Falta Content/Data/{name}")


def check_items_schema(ds: DataSet, r: Report) -> None:
    seen: set[str] = set()
    if len(ds.items) < 60:
        r.error(f"items.json: {len(ds.items)} objetos; ItemsSpec.cpp exige al menos 60")
    for item in ds.items:
        iid = item.get("id")
        if not isinstance(iid, str) or not re.fullmatch(r"[a-z0-9_]+", iid or ""):
            r.error(f"items.json: id inválido {iid!r} (minúsculas, dígitos y _)")
            continue
        if iid in seen:
            r.error(f"items.json: id duplicado «{iid}»")
        seen.add(iid)
        for key in ("nameEs", "nameEn", "meshPath", "size"):
            if not isinstance(item.get(key), str) or not item.get(key):
                r.error(f"items.json «{iid}»: falta {key}")
        if item.get("size") not in SIZES:
            r.error(f"items.json «{iid}»: size {item.get('size')!r} no es {sorted(SIZES)}")
        for key in ("weightKg", "volumeLiters"):
            v = item.get(key)
            if not _type_ok(v, (int, float)) or v < 0 or v > 200:
                r.error(f"items.json «{iid}»: {key}={v!r} fuera de [0, 200]")
        interno = "interno" in item.get("tags", [])
        if not interno and item.get("weightKg", 0) <= 0:
            r.error(f"items.json «{iid}»: weightKg debe ser > 0 en objetos reales")
        if item.get("size") == "Pequeno" and item.get("weightKg", 0) > 3:
            r.warn(f"items.json «{iid}»: Pequeno con {item['weightKg']} kg (¿cabe en un bolsillo?)")
        if "maxDurability" in item and (not _type_ok(item["maxDurability"], (int, float)) or item["maxDurability"] < 0):
            r.error(f"items.json «{iid}»: maxDurability negativa o no numérica")
        for key in ("nutritionEnergy", "nutritionProtein", "nutritionVitamins"):
            if key in item:
                v = item[key]
                if not _type_ok(v, (int, float)) or not 0 <= v <= 5:
                    r.error(f"items.json «{iid}»: {key}={v!r} fuera de la escala 0-5")
        tags = item.get("tags", [])
        if not isinstance(tags, list) or not all(isinstance(t, str) for t in tags):
            r.error(f"items.json «{iid}»: tags debe ser lista de cadenas")
        names = set()
        for prop in item.get("properties", []):
            name, value = prop.get("name"), prop.get("value")
            if name not in PROPERTIES:
                r.error(f"items.json «{iid}»: propiedad desconocida {name!r} (biblia §2.1)")
            if name in names:
                r.error(f"items.json «{iid}»: propiedad {name} repetida")
            names.add(name)
            if not _type_ok(value, (int, float)) or not 0 <= value <= 5:
                r.error(f"items.json «{iid}».{name}={value!r} fuera de [0, 5]")
        if "comida" in tags and "Nutritivo" not in names:
            r.warn(f"items.json «{iid}»: etiqueta comida sin propiedad Nutritivo")


def check_verbs(ds: DataSet, r: Report) -> None:
    ids = [v.get("id") for v in ds.verbs]
    for vid in set(ids):
        if ids.count(vid) > 1:
            r.error(f"verbs.json: verbo duplicado «{vid}»")
    for missing in sorted(REQUIRED_VERBS - set(ids)):
        r.error(f"verbs.json: falta el verbo «{missing}» (biblia §2.2)")
    for v in ds.verbs:
        if not v.get("nameEs") or not v.get("description"):
            r.error(f"verbs.json «{v.get('id')}»: falta nameEs o description")


def check_templates(ds: DataSet, r: Report) -> None:
    verb_ids = {v.get("id") for v in ds.verbs}
    item_ids = ds.item_ids
    seen: set[str] = set()
    if len(ds.templates) < 15:
        r.error(f"templates.json: {len(ds.templates)} plantillas; ItemsSpec.cpp exige al menos 15")
    for t in ds.templates:
        tid = t.get("id")
        if not tid:
            r.error("templates.json: plantilla sin id")
            continue
        if tid in seen:
            r.error(f"templates.json: id duplicado «{tid}»")
        seen.add(tid)
        if t.get("resultDefinitionId") not in item_ids:
            r.error(f"templates.json «{tid}»: produce «{t.get('resultDefinitionId')}», que no está en items.json")
        if not t.get("verbs"):
            r.error(f"templates.json «{tid}»: sin verbos")
        for verb in t.get("verbs", []):
            if verb not in verb_ids:
                r.error(f"templates.json «{tid}»: verbo «{verb}» no está en verbs.json")
        slots = t.get("slots", [])
        if not slots:
            r.error(f"templates.json «{tid}»: sin slots")
        roles = [s.get("role") for s in slots]
        if len(set(roles)) != len(roles):
            r.error(f"templates.json «{tid}»: roles repetidos {roles}")
        for s in slots:
            for req in s.get("requirements", []):
                if req.get("property") not in PROPERTIES:
                    r.error(f"templates.json «{tid}»/{s.get('role')}: propiedad desconocida {req.get('property')!r}")
                m = req.get("min")
                if not _type_ok(m, (int, float)) or not 0 <= m <= 5:
                    r.error(f"templates.json «{tid}»/{s.get('role')}: min={m!r} fuera de [0, 5]")
            for tag in s.get("tags", []):
                if not any(tag in i.get("tags", []) for i in ds.items):
                    r.error(f"templates.json «{tid}»/{s.get('role')}: ningún objeto tiene la etiqueta «{tag}»")
        placeholders = {int(n) for n in re.findall(r"\{(\d+)\}", t.get("nameTemplate", ""))}
        if placeholders and max(placeholders) >= len(slots):
            r.error(f"templates.json «{tid}»: nameTemplate usa {{{max(placeholders)}}} con {len(slots)} slots")
        if t.get("isSharpen") and t.get("verbs") != ["Afilar"]:
            r.warn(f"templates.json «{tid}»: isSharpen con verbos distintos de Afilar")


# --------------------------------------------------------------------------- progresión


def check_crafting_reachability(ds: DataSet, r: Report, max_depth: int = 2) -> crafting.Reachability:
    reach = crafting.simulate(ds.items, ds.templates, max_depth=max_depth)
    for t in ds.templates:
        if t["id"] not in reach.reached_templates:
            r.error(
                f"templates.json «{t['id']}»: inalcanzable en {max_depth} pasos desde materiales en bruto "
                "(requisitos imposibles o siempre gana otra plantilla con más slots)"
            )
    for verb, winner, shadowed in sorted(reach.ties):
        r.info.append(f"con «{verb}», «{winner}» gana a «{shadowed}» por orden del fichero (empate en slots)")
    for tid, hits in sorted(crafting.single_piece_templates(ds.items, ds.templates).items()):
        r.info.append(f"«{tid}» se dispara con una sola pieza + cualquier otra: {', '.join(hits)}")
    produced = {t["resultDefinitionId"] for t in ds.templates}
    for item in ds.items:
        if item["id"] in produced and item["id"] not in reach.reached_items:
            r.error(f"items.json «{item['id']}»: solo se obtiene fabricando y ninguna cadena lo produce")
    return reach


def _obtainable(ds: DataSet, reach: crafting.Reachability) -> set[str]:
    return set(reach.reached_items)


def check_plants(ds: DataSet, r: Report, obtainable: set[str]) -> None:
    doc = ds.data.get("plants.json", {})
    piece_ids = {p.get("id") for p in ds.building.get("pieces", [])}
    seen = set()
    has_lemon_tree = False
    for plant in doc.get("plants", []):
        pid = plant.get("id")
        if pid in seen:
            r.error(f"plants.json: id duplicado «{pid}»")
        seen.add(pid)
        for key in ("plantedFrom",):
            ref = plant.get(key)
            if ref not in ds.item_ids:
                r.error(f"plants.json «{pid}»: {key} «{ref}» no está en items.json")
            elif ref not in obtainable:
                r.error(f"plants.json «{pid}»: {key} «{ref}» no es obtenible")
        if plant.get("requiresPiece") not in piece_ids:
            r.error(f"plants.json «{pid}»: requiresPiece «{plant.get('requiresPiece')}» no está en building_pieces.json")
        bad_seasons = set(plant.get("seasons", [])) - SEASONS
        if bad_seasons or not plant.get("seasons"):
            r.error(f"plants.json «{pid}»: estaciones inválidas {sorted(bad_seasons) or '[]'}")
        stages = plant.get("stages", [])
        if len(stages) < 2:
            r.error(f"plants.json «{pid}»: necesita al menos 2 etapas")
        stage_ids = [s.get("id") for s in stages]
        if len(set(stage_ids)) != len(stage_ids):
            r.error(f"plants.json «{pid}»: etapas repetidas")
        for s in stages[:-1]:
            d = s.get("days")
            if not _type_ok(d, (int, float)) or not 1 <= d <= 32:
                r.error(f"plants.json «{pid}».{s.get('id')}: days={d!r} fuera de [1, 32] (un año = 32 días)")
        if stages and stages[-1].get("days") != 0:
            r.error(f"plants.json «{pid}»: la última etapa es la final y debe tener days=0")
        total = sum(s.get("days", 0) for s in stages)
        if total > 32:
            r.warn(f"plants.json «{pid}»: {total} días hasta cosechar, más de un año de juego")
        h = plant.get("harvest", {})
        if h.get("item") not in ds.item_ids:
            r.error(f"plants.json «{pid}»: cosecha «{h.get('item')}» no está en items.json")
        if not (isinstance(h.get("min"), int) and isinstance(h.get("max"), int) and 1 <= h["min"] <= h["max"] <= 20):
            r.error(f"plants.json «{pid}»: cosecha min/max inválidos ({h.get('min')}, {h.get('max')})")
        if not _type_ok(h.get("everyDays"), (int, float)) or h.get("everyDays") < 0:
            r.error(f"plants.json «{pid}»: everyDays inválido")
        if pid == "limonero":
            has_lemon_tree = True
            if not plant.get("neverRemoved"):
                r.error("plants.json «limonero»: debe tener neverRemoved=true (GDD §8.7)")
            if h.get("item") != "limon":
                r.error("plants.json «limonero»: debe cosechar «limon» (cura el escorbuto)")
    if doc and not has_lemon_tree:
        r.error("plants.json: falta el limonero (GDD §8.7)")


def check_building(ds: DataSet, r: Report, obtainable: set[str]) -> None:
    doc = ds.building
    tiers = {t.get("id"): t for t in doc.get("tiers", [])}
    orders = sorted(t.get("order") for t in tiers.values())
    if orders != list(range(len(orders))):
        r.error(f"building_pieces.json: órdenes de tier no consecutivos {orders}")
    expected = ["palma", "bambu", "madera", "piedra"]
    if [t for t, _ in sorted(tiers.items(), key=lambda kv: kv[1].get("order", 0))] != expected:
        r.error(f"building_pieces.json: los tiers deben ser {expected} en ese orden (GDD §8.6)")
    for t in tiers.values():
        for tool in t.get("requiresTools", []):
            if tool not in obtainable:
                r.error(f"building_pieces.json tier «{t['id']}»: herramienta «{tool}» no obtenible")
    pieces = {p.get("id"): p for p in doc.get("pieces", [])}
    if len(pieces) != len(doc.get("pieces", [])):
        r.error("building_pieces.json: ids de pieza duplicados")
    for pid, p in pieces.items():
        if p.get("tier") not in tiers:
            r.error(f"building_pieces.json «{pid}»: tier «{p.get('tier')}» desconocido")
        if not p.get("cost"):
            r.error(f"building_pieces.json «{pid}»: sin coste")
        for c in p.get("cost", []):
            if c.get("item") not in ds.item_ids:
                r.error(f"building_pieces.json «{pid}»: ingrediente «{c.get('item')}» no está en items.json")
            elif c["item"] not in obtainable:
                r.error(f"building_pieces.json «{pid}»: ingrediente «{c['item']}» no obtenible")
            if not isinstance(c.get("count"), int) or not 1 <= c["count"] <= 50:
                r.error(f"building_pieces.json «{pid}»: count {c.get('count')!r} fuera de [1, 50]")
        for tool in p.get("tools", []):
            if tool not in ds.item_ids:
                r.error(f"building_pieces.json «{pid}»: herramienta «{tool}» no está en items.json")
            elif tool not in obtainable:
                r.error(f"building_pieces.json «{pid}»: herramienta «{tool}» no obtenible")
        for req in p.get("requiresPieces", []):
            if req not in pieces:
                r.error(f"building_pieces.json «{pid}»: requiere «{req}», que no existe")
        if not _type_ok(p.get("buildMinutes"), (int, float)) or not 1 <= p["buildMinutes"] <= 600:
            r.error(f"building_pieces.json «{pid}»: buildMinutes fuera de [1, 600]")
        if not isinstance(p.get("integrity"), int) or not 1 <= p["integrity"] <= 100:
            r.error(f"building_pieces.json «{pid}»: integrity fuera de [1, 100]")
        if p.get("maxCycloneCategory") not in (0, 1, 2, 3):
            r.error(f"building_pieces.json «{pid}»: maxCycloneCategory fuera de 0-3")
    # Tier de una pieza estructural nunca por debajo de lo que aguanta: piedra ≥ madera ≥ ...
    by_tier: dict[str, list[int]] = {}
    for p in pieces.values():
        if p.get("category") == "estructura" and p.get("tier") in tiers:
            by_tier.setdefault(p["tier"], []).append(p.get("integrity", 0))
    prev = None
    for tier in expected:
        if tier not in by_tier:
            continue
        avg = sum(by_tier[tier]) / len(by_tier[tier])
        if prev is not None and avg <= prev[1]:
            r.error(f"building_pieces.json: integridad media de «{tier}» ({avg:.0f}) no supera a «{prev[0]}» ({prev[1]:.0f})")
        prev = (tier, avg)
    _check_piece_cycles(pieces, r)


def _check_piece_cycles(pieces: dict[str, dict], r: Report) -> None:
    state: dict[str, int] = {}

    def visit(pid: str, path: list[str]) -> None:
        if state.get(pid) == 2 or pid not in pieces:
            return
        if state.get(pid) == 1:
            cycle = path[path.index(pid):] + [pid]
            r.error(f"building_pieces.json: ciclo de requisitos {' → '.join(cycle)}")
            return
        state[pid] = 1
        for req in pieces[pid].get("requiresPieces", []):
            visit(req, path + [pid])
        state[pid] = 2

    for pid in pieces:
        visit(pid, [])


# --------------------------------------------------------------------------- mallas


def blender_mesh_names(repo_root: Path) -> set[str]:
    names: set[str] = set()
    props = repo_root / "Tools" / "Blender" / "props"
    for py in props.glob("*.py"):
        src = py.read_text(encoding="utf-8")
        for m in re.finditer(r"dict\(name='([A-Za-z0-9_]+)'", src):
            names.add("SM_" + m.group(1))
        # kit modular (kit_construccion.py): los nombres se generan como
        # name=f'Kit_{_mat}_{_key}' cruzando MATERIALS x PIECES
        if "name=f'Kit_{_mat}_{_key}'" in src:
            mats = re.search(r"^MATERIALS = \[([^\]]*)\]", src, re.M)
            pieces = re.search(r"^PIECES = \[(.*?)^\]", src, re.M | re.S)
            if mats and pieces:
                for mat in re.findall(r"'(\w+)'", mats.group(1)):
                    for key in re.findall(r"^\s*\('(\w+)',", pieces.group(1), re.M):
                        names.add(f"SM_Kit_{mat}_{key}")
    return names


def pending_expected(ds: DataSet) -> dict[str, set[str]]:
    items = {i["id"] for i in ds.items if BASIC_SHAPES.match(i.get("meshPath", "")) and "interno" not in i.get("tags", [])}
    pieces = {p["id"] for p in ds.building.get("pieces", []) if p.get("mesh") is None}
    stages = {f"{pl['id']}.{s['id']}" for pl in ds.plants for s in pl.get("stages", []) if s.get("mesh") is None}
    return {"items": items, "buildingPieces": pieces, "plantStages": stages}


def check_meshes(ds: DataSet, r: Report) -> None:
    known = blender_mesh_names(ds.repo_root)
    for item in ds.items:
        path = item.get("meshPath", "")
        m = GENERATED_MESH.match(path)
        if BASIC_SHAPES.match(path):
            continue
        if not m:
            r.error(f"items.json «{item['id']}»: meshPath {path!r} no es /Engine/BasicShapes ni /Game/Generated/Meshes/.../SM_*")
        elif m.group(1) not in known:
            r.error(f"items.json «{item['id']}»: malla {m.group(1)} no la genera ningún script de Tools/Blender/props")
    for p in ds.building.get("pieces", []):
        if p.get("mesh") is not None and p["mesh"] not in known:
            r.error(f"building_pieces.json «{p['id']}»: malla {p['mesh']} no existe en Tools/Blender/props")
    for pl in ds.plants:
        for s in pl.get("stages", []):
            if s.get("mesh") is not None and s["mesh"] not in known:
                r.error(f"plants.json «{pl['id']}.{s['id']}»: malla {s['mesh']} no existe en Tools/Blender/props")

    pending = ds.data.get("meshes_pendientes.json", {})
    for group, expected in pending_expected(ds).items():
        listed = {e.get("id") if isinstance(e, dict) else e for e in pending.get(group, [])}
        for missing in sorted(expected - listed):
            r.error(f"meshes_pendientes.json/{group}: falta «{missing}» (usa marcador o mesh null)")
        for stale in sorted(listed - expected):
            r.error(f"meshes_pendientes.json/{group}: «{stale}» ya tiene malla o no existe; quítalo")


# --------------------------------------------------------------------------- supervivencia


def _cpp_float(text: str, pattern: str) -> float | None:
    m = re.search(pattern, text)
    return float(m.group(1)) if m else None


def _function_body(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        return ""
    end = text.find("\n\t}\n", start)
    return text[start:end if end > 0 else len(text)]


def check_survival(ds: DataSet, r: Report) -> None:
    doc = ds.data.get("survival_needs.json")
    if not doc:
        return
    src = ds.repo_root / "Source" / "Explored" / "Survival"
    header = (src / "SurvivalModel.h").read_text(encoding="utf-8") if (src / "SurvivalModel.h").exists() else ""
    cpp = (src / "SurvivalModel.cpp").read_text(encoding="utf-8") if (src / "SurvivalModel.cpp").exists() else ""
    if not header or not cpp:
        r.warn("survival_needs.json: no se encuentra SurvivalModel.{h,cpp}; no se compara con el C++")
    state_start = header.find("struct EXPLORED_API FSurvivalState")
    header = header[state_start:header.find("};", state_start)] if state_start >= 0 else ""
    all_tags = {t for i in ds.items for t in i.get("tags", [])}
    seen = set()
    for need in doc.get("needs", []):
        nid = need.get("id")
        if nid in seen:
            r.error(f"survival_needs.json: necesidad duplicada «{nid}»")
        seen.add(nid)
        init = need.get("initial")
        if not _type_ok(init, (int, float)) or not 0 <= init <= 100:
            r.error(f"survival_needs.json «{nid}»: initial fuera de [0, 100]")
        for tag in need.get("recoversWith", []):
            if tag not in all_tags:
                r.error(f"survival_needs.json «{nid}»: ningún objeto tiene la etiqueta «{tag}»")
        if header and need.get("state"):
            v = _cpp_float(header, rf"float {need['state']} = ([0-9.]+)f;")
            if v is None:
                r.error(f"survival_needs.json «{nid}»: FSurvivalState no tiene el campo {need['state']}")
            elif v != init:
                r.error(f"survival_needs.json «{nid}»: initial={init} pero SurvivalModel.h dice {v}")
        if cpp and need.get("cppConstant"):
            v = _cpp_float(cpp, rf"constexpr float {need['cppConstant']} = ([0-9.]+)f;")
            if v is None:
                r.error(f"survival_needs.json «{nid}»: no existe la constante {need['cppConstant']} en SurvivalModel.cpp")
            elif v != need.get("hoursToEmpty"):
                r.error(f"survival_needs.json «{nid}»: hoursToEmpty={need.get('hoursToEmpty')} pero {need['cppConstant']}={v}")
    for required in ("hambre", "sed", "vitamina_c", "animo"):
        if required not in seen:
            r.error(f"survival_needs.json: falta la necesidad «{required}» (GDD §8.3)")
    if not cpp:
        return
    sleep = doc.get("sleepRecoveryHours", {})
    v = _cpp_float(cpp, rf"constexpr float {sleep.get('cppConstant')} = ([0-9.]+)f;")
    if v is not None and v != sleep.get("value"):
        r.error(f"survival_needs.json: sleepRecoveryHours={sleep.get('value')} pero C++ dice {v}")
    mode_body = _function_body(cpp, "float ModeScale(")
    for mode, cpp_name in (("Explorer", "Explorer"), ("Castaway", "Castaway")):
        v = _cpp_float(mode_body, rf"ESurvivalMode::{cpp_name}: return ([0-9.]+)f;")
        if v is not None and v != doc.get("modeScale", {}).get(mode):
            r.error(f"survival_needs.json: modeScale.{mode}={doc['modeScale'].get(mode)} pero C++ dice {v}")
    act_body = _function_body(cpp, "float ActivityMetabolism(")
    for act, value in doc.get("activityMetabolism", {}).items():
        v = _cpp_float(act_body, rf"EActivity::{act}: return ([0-9.]+)f;")
        if v is None and act == "Walking":
            v = _cpp_float(act_body, r"default: return ([0-9.]+)f;")
        if v is not None and v != value:
            r.error(f"survival_needs.json: activityMetabolism.{act}={value} pero C++ dice {v}")
    bt = doc.get("bodyTemperature", {})
    for key, pat in (("hypothermiaBelow", r"BodyTemperature < ([0-9.]+)f\)"), ("heatstrokeAbove", r"BodyTemperature > ([0-9.]+)f\)")):
        v = _cpp_float(cpp, pat)
        if v is not None and v != bt.get(key):
            r.error(f"survival_needs.json: bodyTemperature.{key}={bt.get(key)} pero C++ dice {v}")


# --------------------------------------------------------------------------- story y reglas


def check_story(ds: DataSet, r: Report) -> None:
    doc = ds.data.get("story_es.json", {})
    themes = doc.get("petroglyph_themes", [])
    if len(themes) != 30:
        r.error(f"story_es.json: {len(themes)} temas de petroglifo; el GDD §6.1 fija 30")
    if len(set(themes)) != len(themes):
        r.error("story_es.json: temas de petroglifo repetidos")
    for group in ("map_marks", "artifact_kinds"):
        ids = [e.get("id") for e in doc.get(group, [])]
        if len(set(ids)) != len(ids):
            r.error(f"story_es.json/{group}: ids repetidos")
        for e in doc.get(group, []):
            label = e.get("label", "")
            if not label or len(label) > 60:
                r.error(f"story_es.json/{group} «{e.get('id')}»: etiqueta vacía o de más de 60 caracteres (GDD §3: una línea)")
    for required in ("water", "cave", "danger", "resource"):
        if required not in {e.get("id") for e in doc.get("map_marks", [])}:
            r.error(f"story_es.json: falta el sello de mapa «{required}» (GDD §5.4)")
    if len(doc.get("artifact_kinds", [])) < 6:
        r.error("story_es.json: el GDD §7 enumera 6 tipos de artefacto")


def check_forbidden_terms(ds: DataSet, r: Report) -> None:
    for name, content in ds.data.items():
        text = json.dumps(content, ensure_ascii=False).lower()
        for term in FORBIDDEN_TERMS:
            if re.search(rf"(?<![a-záéíóúñ]){re.escape(term)}(?![a-záéíóúñ])", text):
                r.error(f"{name}: contiene «{term}», eliminado por el GDD §10/§12")


# --------------------------------------------------------------------------- entrada


def run_all(ds: DataSet) -> Report:
    r = Report()
    check_files_present(ds, r)
    check_items_schema(ds, r)
    check_verbs(ds, r)
    check_templates(ds, r)
    reach = check_crafting_reachability(ds, r)
    obtainable = _obtainable(ds, reach)
    check_plants(ds, r, obtainable)
    check_building(ds, r, obtainable)
    check_meshes(ds, r)
    check_survival(ds, r)
    check_story(ds, r)
    check_forbidden_terms(ds, r)
    return r
