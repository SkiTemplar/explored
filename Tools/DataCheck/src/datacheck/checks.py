"""Comprobaciones de Content/Data/*.json. Cada check añade mensajes a un Report."""

from __future__ import annotations

import copy
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from . import achievements, cooking, crafting, fases, fauna, mining, music, packs

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
# Encajes de building_pieces.json: espejo de EBuildSocket (Source/Explored/Building/BuildingTypes.h).
BUILDING_SOCKETS = {"pilar", "suelo", "pared", "puerta", "techo", "escalera", "mueble", "terreno"}
# Pieza que protege de las aves los cultivos con birdsEat (FFarmModel::ScarecrowRadius, GDD §8.7).
SCARECROW_PIECE = "espantapajaros"
# Compost del huerto (biblia de contenido §7.2, biblia 02 §10.1): sus efectos copian FFarmModel.
FARM_MODEL_H = "Farming/FarmModel.h"
BASIC_SHAPES = re.compile(r"^/Engine/BasicShapes/(Cube|Sphere|Cylinder|Cone|Plane)\.\1$")
GENERATED_MESH = re.compile(r"^/Game/Generated/Meshes/[A-Za-z0-9_/]+/(SM_[A-Za-z0-9_]+)\.\1$")

# GDD §12: sin narrativa de personajes eliminada. El GDD v2 §3.6-3.7 reintroduce la
# fauna terrestre (cerdo salvaje, cabra, aves que se posan y animales de granja, en
# fauna.json y fauna_terrestre.json), así que solo quedan prohibidas las especies que
# ningún documento vigente contempla y el perro del prólogo eliminado.
FORBIDDEN_TERMS = [
    "rata", "murcielago", "murciélago", "serpiente",
    "lagarto", "iguana", "perro", "canela", "almudena_", "rodrigo", "ines", "inés",
    "diario halden", "pagina halden", "página halden", "haldenpage", "beacon", "baliza",
    "rescate", "mascota",
]

DATA_FILES = [
    "items.json", "templates.json", "verbs.json", "story_es.json", "plants.json",
    "building_pieces.json", "survival_needs.json", "meshes_pendientes.json", "achievements.json",
    "artifacts.json", "ruins.json", "fuels.json", "recipes.json", "boats.json",
    "fish.json", "music_layers.json", "packs_catalogo.json", "mining.json", "fauna.json",
    "fases_futuras.json", "fauna_terrestre.json",
]
ASCII_ID = re.compile(r"^[a-z0-9_]+$")
# Objetos rescatados del Albatros (biblia §3.3): el barco «Limón» debe usar alguno (GDD §4.3, §8.10).
ALBATROS_ITEMS = {"chapa_fuselaje", "tubo_aluminio", "cable_electrico", "cinta_americana"}


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
    def boats(self) -> list[dict]:
        return self.data.get("boats.json", {}).get("boats", [])

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
    # «station» todavía no lo lee el C++: fija en datos la estación que exige la plantilla (biblia 03 §1.4).
    stations = {p.get("id") for p in ds.data.get("building_pieces.json", {}).get("pieces", [])
                if p.get("category") == "produccion"}
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
        station = t.get("station")
        if station is not None and station not in stations:
            r.error(f"templates.json «{tid}»: estación «{station}» no es una pieza de producción de building_pieces.json")
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
        if "birdsEat" in plant and not isinstance(plant["birdsEat"], bool):
            r.error(f"plants.json «{pid}»: birdsEat debe ser true o false")
        elif plant.get("birdsEat") and SCARECROW_PIECE not in piece_ids:
            r.error(f"plants.json «{pid}»: birdsEat sin pieza «{SCARECROW_PIECE}» en building_pieces.json (GDD §8.7)")
        if pid == "limonero":
            has_lemon_tree = True
            if not plant.get("neverRemoved"):
                r.error("plants.json «limonero»: debe tener neverRemoved=true (GDD §8.7)")
            if h.get("item") != "limon":
                r.error("plants.json «limonero»: debe cosechar «limon» (cura el escorbuto)")
    if doc and not has_lemon_tree:
        r.error("plants.json: falta el limonero (GDD §8.7)")


def check_compost(ds: DataSet, r: Report, obtainable: set[str]) -> None:
    """Pila de compost: restos obtenibles, pieza del primer bancal y efectos iguales a FFarmModel."""
    doc = ds.data.get("plants.json", {})
    if not doc:
        return
    c = doc.get("compost")
    if not isinstance(c, dict):
        r.error("plants.json: falta el bloque «compost» (GDD v2 §3.6, biblia §7.2)")
        return
    pieces = {p.get("id"): p for p in ds.building.get("pieces", [])}
    tiers = [t.get("id") for t in ds.building.get("tiers", [])]
    piece = pieces.get(c.get("piece"))
    if piece is None:
        r.error(f"plants.json/compost: piece «{c.get('piece')}» no está en building_pieces.json")
    else:
        bed = pieces.get("bancal")
        if bed and piece.get("tier") in tiers and bed.get("tier") in tiers \
                and tiers.index(piece["tier"]) > tiers.index(bed["tier"]):
            r.error(f"plants.json/compost: la pieza «{piece['id']}» llega en un tier posterior al bancal")
    result = next((i for i in ds.items if i.get("id") == c.get("result")), None)
    if result is None:
        r.error(f"plants.json/compost: result «{c.get('result')}» no está en items.json")
    elif "comida" in result.get("tags", []):
        r.error(f"plants.json/compost: «{result['id']}» no puede ser comida (se echaría a sí mismo a la pila)")
    per = c.get("inputsPerResult")
    cap = c.get("capacity")
    if not (isinstance(per, int) and 1 <= per <= 10):
        r.error(f"plants.json/compost: inputsPerResult={per!r} fuera de [1, 10]")
    elif not (isinstance(cap, int) and cap >= per and cap % per == 0):
        r.error(f"plants.json/compost: capacity={cap!r} debe ser múltiplo de inputsPerResult ({per})")
    days = c.get("daysToMature")
    if not (isinstance(days, int) and 1 <= days <= 8):
        r.error(f"plants.json/compost: daysToMature={days!r} fuera de [1, 8]")
    tags = {t for i in ds.items for t in i.get("tags", [])}
    usable = False
    for entry in c.get("inputs", []):
        if "item" in entry:
            ref = entry["item"]
            if ref not in ds.item_ids:
                r.error(f"plants.json/compost: input «{ref}» no está en items.json")
            elif ref == c.get("result"):
                r.error("plants.json/compost: el compost no puede ser su propio resto")
            else:
                usable |= ref in obtainable
        elif "tag" in entry:
            if entry["tag"] not in tags:
                r.error(f"plants.json/compost: ninguna entrada de items.json tiene la etiqueta «{entry['tag']}»")
            else:
                usable |= any(entry["tag"] in i.get("tags", []) and i["id"] in obtainable for i in ds.items)
        else:
            r.error(f"plants.json/compost: input sin «item» ni «tag»: {entry!r}")
    if not usable:
        r.error("plants.json/compost: ningún resto aceptado es obtenible")
    header = _read_source(ds, FARM_MODEL_H)
    if header:
        growth = _cpp_float(header, r"CompostGrowth = ([0-9.]+)f;")
        cpp_days = _cpp_int(ds.repo_root, f"Source/Explored/{FARM_MODEL_H}", "CompostDays")
        if growth is None or cpp_days is None:
            r.error("plants.json/compost: no encuentro CompostGrowth o CompostDays en FarmModel.h")
        if growth is not None and c.get("growthMultiplier") != growth:
            r.error(f"plants.json/compost: growthMultiplier={c.get('growthMultiplier')} y FFarmModel::CompostGrowth={growth}")
        if cpp_days is not None and c.get("durationDays") != cpp_days:
            r.error(f"plants.json/compost: durationDays={c.get('durationDays')} y FFarmModel::CompostDays={cpp_days}")


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
        # Encaje en la rejilla: lo lee FBuildingModel (ParseBuildSocket) y sin él la pieza no se coloca.
        if p.get("socket") not in BUILDING_SOCKETS:
            r.error(f"building_pieces.json «{pid}»: socket {p.get('socket')!r} no es uno de {sorted(BUILDING_SOCKETS)}")
        if "respawnPoint" in p and not isinstance(p["respawnPoint"], bool):
            r.error(f"building_pieces.json «{pid}»: respawnPoint debe ser true o false")
    if not any(p.get("respawnPoint") is True for p in pieces.values()):
        r.error("building_pieces.json: ninguna pieza es punto de reaparición (GDD §8.6: las fogatas encendidas)")
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


# --------------------------------------------------------------------------- embarcaciones


def _cpp_enum(text: str, name: str) -> list[str]:
    m = re.search(rf"enum class {name}\s*:\s*uint8\s*\{{(.*?)\}};", text, re.S)
    if not m:
        return []
    body = re.sub(r"//[^\n]*", "", m.group(1))
    names = [re.split(r"[\s=]", v.strip())[0] for v in body.split(",")]
    return [n for n in names if n and n != "Count"]


def _read_source(ds: DataSet, rel: str) -> str:
    path = ds.repo_root / "Source" / "Explored" / rel
    return path.read_text(encoding="utf-8") if path.exists() else ""


def check_boats(ds: DataSet, r: Report, obtainable: set[str]) -> None:
    doc = ds.data.get("boats.json")
    if not doc:
        return
    boats = {b.get("id"): b for b in ds.boats}
    if len(boats) != len(ds.boats):
        r.error("boats.json: ids de embarcación duplicados")
    pieces = {p.get("id") for p in ds.building.get("pieces", [])}

    types_h = _read_source(ds, "Boats/BoatTypes.h")
    model_cpp = _read_source(ds, "Boats/BoatModel.cpp")
    progress_h = _read_source(ds, "Narrative/ExploredProgress.h")
    cpp_types = _cpp_enum(types_h, "EBoatType")
    ship_parts = set(_cpp_enum(progress_h, "EShipPart"))
    if not cpp_types:
        r.warn("boats.json: no se encuentra EBoatType en Boats/BoatTypes.h; no se compara con el C++")
    cpp_meshes = {
        m.group(1): m.group(2)
        for m in re.finditer(r"case EBoatType::(\w+):(?:(?!break;).)*?D\.MeshName = TEXT\(\"([^\"]*)\"\)", model_cpp, re.S)
    }

    orders = sorted(b.get("order") for b in boats.values())
    if orders != list(range(len(orders))):
        r.error(f"boats.json: órdenes de progresión no consecutivos {orders}")
    if cpp_types and sorted(b.get("type") for b in boats.values()) != sorted(cpp_types):
        r.error(f"boats.json: los tipos deben ser exactamente los de EBoatType {cpp_types}")

    for bid, b in boats.items():
        if not isinstance(bid, str) or not re.fullmatch(r"[a-z0-9_]+", bid or ""):
            r.error(f"boats.json: id inválido {bid!r} (minúsculas, dígitos y _)")
            continue
        for key in ("nameEs", "nameEn"):
            if not isinstance(b.get(key), str) or not b[key]:
                r.error(f"boats.json «{bid}»: falta {key}")
        if b.get("station") not in pieces:
            r.error(f"boats.json «{bid}»: estación «{b.get('station')}» no está en building_pieces.json")
        if not b.get("cost"):
            r.error(f"boats.json «{bid}»: sin coste")
        for c in b.get("cost", []):
            if c.get("item") not in ds.item_ids:
                r.error(f"boats.json «{bid}»: ingrediente «{c.get('item')}» no está en items.json")
            elif c["item"] not in obtainable:
                r.error(f"boats.json «{bid}»: ingrediente «{c['item']}» no obtenible")
            if not isinstance(c.get("count"), int) or not 1 <= c["count"] <= 50:
                r.error(f"boats.json «{bid}»: count {c.get('count')!r} fuera de [1, 50]")
        for tool in b.get("tools", []):
            if tool not in ds.item_ids:
                r.error(f"boats.json «{bid}»: herramienta «{tool}» no está en items.json")
            elif tool not in obtainable:
                r.error(f"boats.json «{bid}»: herramienta «{tool}» no obtenible")
        req = b.get("requiresBoat")
        if req is not None:
            if req not in boats:
                r.error(f"boats.json «{bid}»: requiere la embarcación «{req}», que no existe")
            elif boats[req].get("order", 0) >= b.get("order", 0):
                r.error(f"boats.json «{bid}»: requiere «{req}», que no va antes en la progresión")
        elif b.get("consumesRequiredBoat"):
            r.error(f"boats.json «{bid}»: consumesRequiredBoat sin requiresBoat")
        bad_parts = set(b.get("requiresShipParts", [])) - ship_parts
        if ship_parts and bad_parts:
            r.error(f"boats.json «{bid}»: piezas del Albatros desconocidas {sorted(bad_parts)} (EShipPart)")
        if not _type_ok(b.get("buildMinutes"), (int, float)) or not 1 <= b["buildMinutes"] <= 600:
            r.error(f"boats.json «{bid}»: buildMinutes fuera de [1, 600]")
        btype = b.get("type")
        if btype in cpp_meshes and (cpp_meshes[btype] or None) != b.get("mesh"):
            r.error(f"boats.json «{bid}»: mesh={b.get('mesh')!r} pero FBoatDefinition::MeshName dice {cpp_meshes[btype]!r}")

    limon = next((b for b in boats.values() if b.get("type") == "Limon"), None)
    if limon is None:
        r.error("boats.json: falta el barco «Limón» (GDD §8.10)")
    else:
        if ship_parts and set(limon.get("requiresShipParts", [])) != ship_parts:
            r.error("boats.json: el «Limón» debe exigir las cuatro piezas del Albatros (GDD §4.3)")
        if not {c.get("item") for c in limon.get("cost", [])} & ALBATROS_ITEMS:
            r.error("boats.json: el «Limón» debe usar material rescatado del Albatros (GDD §8.10)")
    first = next((b for b in boats.values() if b.get("order") == 0), None)
    if first is not None and first.get("type") != "Raft":
        r.error("boats.json: la progresión empieza por la balsa (GDD §8.10)")


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
    displays = {d["id"] for d in ds.data.get("artifacts.json", {}).get("displays", []) if d.get("mesh") is None}
    boats = {b["id"] for b in ds.boats if b.get("mesh") is None}
    animals = {s["id"] for s in ds.data.get("fauna.json", {}).get("species", []) if s.get("mesh") is None}
    return {"items": items, "buildingPieces": pieces, "plantStages": stages, "museumDisplays": displays, "boats": boats,
            "fauna": animals}


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
    for b in ds.boats:
        if b.get("mesh") is not None and b["mesh"] not in known:
            r.error(f"boats.json «{b.get('id')}»: malla {b['mesh']} no existe en Tools/Blender/props")

    # Mobiliario de la base ya modelado (SM_Base_*) que ninguna pieza usa: es
    # contenido listo que el jugador no puede construir. Nota, no error: puede
    # faltar un sistema (p. ej. el banco de chatarra) antes de darlo de alta.
    used = {p.get("mesh") for p in ds.building.get("pieces", [])}
    used |= {d.get("mesh") for d in ds.data.get("artifacts.json", {}).get("displays", [])}
    idle = sorted(n for n in known if n.startswith("SM_Base_") and n not in used)
    if idle:
        r.info.append(f"building_pieces.json: mallas de base sin pieza construible: {', '.join(idle)}")

    pending = ds.data.get("meshes_pendientes.json", {})
    for group, expected in pending_expected(ds).items():
        listed = {e.get("id") if isinstance(e, dict) else e for e in pending.get(group, [])}
        for missing in sorted(expected - listed):
            r.error(f"meshes_pendientes.json/{group}: falta «{missing}» (usa marcador o mesh null)")
        for stale in sorted(listed - expected):
            r.error(f"meshes_pendientes.json/{group}: «{stale}» ya tiene malla o no existe; quítalo")


# --------------------------------------------------------------------------- ruinas y museo

RUINS_CPP = "Source/Explored/Ruins/RuinsModel.cpp"
RUINS_H = "Source/Explored/Ruins/RuinsModel.h"
MUSEUM_CPP = "Source/Explored/Ruins/MuseumModel.cpp"
MUSEUM_H = "Source/Explored/Ruins/MuseumModel.h"
ARCHIPELAGO_CPP = "Source/Explored/WorldGen/ArchipelagoLayout.cpp"
SIZE_ORDER = {"Pequeno": 0, "Mediano": 1, "Grande": 2}


def _cpp_lex_ids(repo_root: Path, rel: str, enum: str) -> set[str]:
    """Ids que devuelve LexToString(<enum>) en un .cpp (``case E::X: return TEXT("id");``)."""
    path = repo_root / rel
    if not path.exists():
        return set()
    text = path.read_text(encoding="utf-8")
    return set(re.findall(rf'case {enum}::\w+: return TEXT\("([^"]+)"\);', text))


def _cpp_int(repo_root: Path, rel: str, name: str) -> int | None:
    path = repo_root / rel
    if not path.exists():
        return None
    m = re.search(rf"static constexpr int32 {name} = (\d+);", path.read_text(encoding="utf-8"))
    return int(m.group(1)) if m else None


def _check_names(r: Report, where: str, entry: dict, keys: tuple[str, ...] = ("nameEs", "nameEn")) -> None:
    for key in keys:
        v = entry.get(key)
        if not isinstance(v, str) or not v or len(v) > 60:
            r.error(f"{where} «{entry.get('id')}»: {key} vacío o de más de 60 caracteres (GDD §3: una línea)")


def _check_ids(r: Report, where: str, entries: list) -> list[str]:
    ids = [e.get("id") for e in entries]
    for e in entries:
        if not isinstance(e.get("id"), str) or not ASCII_ID.match(e["id"]):
            r.error(f"{where}: id inválido {e.get('id')!r} (minúsculas ASCII, dígitos y _)")
    for dup in sorted({i for i in ids if isinstance(i, str) and ids.count(i) > 1}):
        r.error(f"{where}: id duplicado «{dup}»")
    return ids


def check_ruins(ds: DataSet, r: Report) -> None:
    doc = ds.data.get("ruins.json")
    if not doc:
        return
    techniques = _check_ids(r, "ruins.json/techniques", doc.get("techniques", []))
    elements = _check_ids(r, "ruins.json/elements", doc.get("elements", []))
    sites = _check_ids(r, "ruins.json/sites", doc.get("sites", []))
    for group in ("techniques", "elements", "sites"):
        for e in doc.get(group, []):
            _check_names(r, f"ruins.json/{group}", e)
    for e in doc.get("techniques", []):
        _check_names(r, "ruins.json/techniques", e, ("revealsEs", "revealsEn"))
    if len(techniques) != 5:
        r.error(f"ruins.json: {len(techniques)} técnicas; el GDD §6.2 fija 5")

    # Espejo del C++: mismos ids que LexToString y mismas constantes.
    root = ds.repo_root
    cpp_techniques = _cpp_lex_ids(root, RUINS_CPP, "EWayfindingTechnique")
    if not cpp_techniques:
        r.warn("ruins.json: no se encuentra RuinsModel.cpp; no se compara con el C++")
        return
    if set(techniques) != cpp_techniques:
        r.error(f"ruins.json/techniques {sorted(techniques)} no coincide con RuinsModel.cpp {sorted(cpp_techniques)}")
    cpp_elements = _cpp_lex_ids(root, RUINS_CPP, "ERuinElementKind")
    if set(elements) != cpp_elements:
        r.error(f"ruins.json/elements {sorted(elements)} no coincide con RuinsModel.cpp {sorted(cpp_elements)}")
    archetypes = _cpp_lex_ids(root, ARCHIPELAGO_CPP, "EIslandArchetype")
    expected_sites = {f"ruin_{a.lower()}" for a in archetypes} | {"ruin_compass"}
    if archetypes and set(sites) != expected_sites:
        r.error(f"ruins.json/sites {sorted(sites)} no coincide con las islas del C++ {sorted(expected_sites)}")
    for key, name in (("requiredStarPaths", "RequiredStarPaths"), ("petroglyphsPerSite", "PetroglyphsPerSite")):
        v = _cpp_int(root, RUINS_H, name)
        if v is None:
            r.error(f"ruins.json: no existe la constante {name} en RuinsModel.h")
        elif doc.get(key) != v:
            r.error(f"ruins.json: {key}={doc.get(key)!r} pero RuinsModel.h dice {name}={v}")
    # Una ruina de isla por cada técnica que no es camino de estrellas; el resto y la brújula enseñan caminos.
    star_paths = (len(expected_sites) - 1) - (len(techniques) - 1) + 1
    if isinstance(doc.get("requiredStarPaths"), int) and doc["requiredStarPaths"] > star_paths:
        r.error(f"ruins.json: requiredStarPaths={doc['requiredStarPaths']} pero solo hay {star_paths} caminos de estrellas")


def check_artifacts(ds: DataSet, r: Report) -> None:
    doc = ds.data.get("artifacts.json")
    if not doc:
        return
    root = ds.repo_root
    kinds = {k.get("id") for k in ds.data.get("story_es.json", {}).get("artifact_kinds", [])}
    sizes = _cpp_lex_ids(root, MUSEUM_CPP, "EArtifactSize") or set(SIZE_ORDER)
    provenances = _check_ids(r, "artifacts.json/provenances", doc.get("provenances", []))
    rarities = _check_ids(r, "artifacts.json/rarities", doc.get("rarities", []))
    for group in ("provenances", "rarities"):
        for e in doc.get(group, []):
            _check_names(r, f"artifacts.json/{group}", e)
    for enum, ids, group in (("EArtifactProvenance", provenances, "provenances"), ("EArtifactRarity", rarities, "rarities")):
        cpp = _cpp_lex_ids(root, MUSEUM_CPP, enum)
        if cpp and set(ids) != cpp:
            r.error(f"artifacts.json/{group} {sorted(ids)} no coincide con MuseumModel.cpp {sorted(cpp)}")

    known_meshes = blender_mesh_names(root)
    pieces = {p.get("id"): p for p in ds.building.get("pieces", [])}
    displays = doc.get("displays", [])
    _check_ids(r, "artifacts.json/displays", displays)
    largest_slot = -1
    for d in displays:
        did = d.get("id")
        _check_names(r, "artifacts.json/displays", d)
        piece_id = d.get("piece")
        if piece_id is None:
            r.info.append(f"artifacts.json: el mueble «{did}» aún no tiene pieza en building_pieces.json")
        elif piece_id not in pieces:
            r.error(f"artifacts.json/displays «{did}»: pieza «{piece_id}» no está en building_pieces.json")
        elif pieces[piece_id].get("category") != "museo":
            r.error(f"artifacts.json/displays «{did}»: la pieza «{piece_id}» no es de la categoría museo")
        if d.get("mesh") is not None and d["mesh"] not in known_meshes:
            r.error(f"artifacts.json/displays «{did}»: malla {d['mesh']} no existe en Tools/Blender/props")
        slots = d.get("slots", [])
        if not slots:
            r.error(f"artifacts.json/displays «{did}»: sin huecos")
        for i, slot in enumerate(slots):
            if slot.get("maxSize") not in sizes:
                r.error(f"artifacts.json/displays «{did}»[{i}]: maxSize {slot.get('maxSize')!r} no es {sorted(sizes)}")
            elif piece_id in pieces:
                # solo cuentan los muebles que el jugador puede construir
                largest_slot = max(largest_slot, SIZE_ORDER.get(slot["maxSize"], -1))
            off = slot.get("offsetCm")
            if not (isinstance(off, list) and len(off) == 3 and all(_type_ok(v, (int, float)) and abs(v) <= 400 for v in off)):
                r.error(f"artifacts.json/displays «{did}»[{i}]: offsetCm debe ser [x, y, z] en cm dentro de ±400")

    artifacts = doc.get("artifacts", [])
    _check_ids(r, "artifacts.json/artifacts", artifacts)
    threshold = _cpp_int(root, MUSEUM_H, "CollectorThreshold")
    if threshold is not None and len(artifacts) < threshold:
        r.error(f"artifacts.json: {len(artifacts)} tesoros; el logro «Coleccionista» pide {threshold} expuestos")
    for a in artifacts:
        aid = a.get("id")
        _check_names(r, "artifacts.json/artifacts", a)
        if a.get("kind") not in kinds:
            r.error(f"artifacts.json «{aid}»: kind «{a.get('kind')}» no está en story_es.json/artifact_kinds")
        if a.get("provenance") not in provenances:
            r.error(f"artifacts.json «{aid}»: procedencia «{a.get('provenance')}» desconocida")
        if a.get("rarity") not in rarities:
            r.error(f"artifacts.json «{aid}»: rareza «{a.get('rarity')}» desconocida")
        if a.get("size") not in sizes:
            r.error(f"artifacts.json «{aid}»: size {a.get('size')!r} no es {sorted(sizes)}")
        elif SIZE_ORDER.get(a["size"], 99) > largest_slot:
            r.error(f"artifacts.json «{aid}»: tamaño {a['size']} sin ningún hueco construible donde exponerlo")
        if a.get("mesh") not in known_meshes:
            r.error(f"artifacts.json «{aid}»: malla {a.get('mesh')!r} no existe en Tools/Blender/props")
    for missing in sorted(kinds - {a.get("kind") for a in artifacts}):
        r.error(f"artifacts.json: ningún tesoro del tipo «{missing}» (story_es.json/artifact_kinds)")


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
    check_survival_body(doc, src, r)


def _body_constants(node, path: str = "body"):
    """Recorre «body» y devuelve (ruta, valor, cppConstant) de cada constante reflejada."""
    if isinstance(node, dict):
        if "cppConstant" in node and "value" in node:
            yield path, node["value"], node["cppConstant"]
            return
        for key, child in node.items():
            yield from _body_constants(child, f"{path}.{key}")


def check_survival_body(doc: dict, src, r: Report) -> None:
    body = doc.get("body")
    if body is None:
        return
    path = src / "BodyModel.cpp"
    if not path.exists():
        r.warn("survival_needs.json: no se encuentra BodyModel.cpp; no se compara «body» con el C++")
        return
    cpp = path.read_text(encoding="utf-8")
    for where, value, name in _body_constants(body):
        if not _type_ok(value, (int, float)):
            r.error(f"survival_needs.json {where}: value debe ser un número")
            continue
        v = _cpp_float(cpp, rf"constexpr float {name} = ([0-9.]+)f;")
        if v is None:
            r.error(f"survival_needs.json {where}: no existe la constante {name} en BodyModel.cpp")
        elif v != value:
            r.error(f"survival_needs.json {where}: value={value} pero {name}={v} en BodyModel.cpp")
    events_body = _function_body(cpp, "float FBodyModel::MoraleEventDelta(")
    for event, value in body.get("moraleEvents", {}).items():
        m = re.search(rf"EMoraleEvent::{event}: return (-?[0-9.]+)f;", events_body)
        if m is None:
            r.error(f"survival_needs.json body.moraleEvents: EMoraleEvent::{event} no existe en BodyModel.cpp")
        elif float(m.group(1)) != value:
            r.error(f"survival_needs.json body.moraleEvents.{event}={value} pero BodyModel.cpp dice {m.group(1)}")


# --------------------------------------------------------------------------- fuego y cocina


def check_cooking(ds: DataSet, r: Report) -> None:
    piece_ids = {p.get("id") for p in ds.building.get("pieces", [])}
    if "fuels.json" in ds.data:
        cooking.check_fuels(ds.data["fuels.json"], ds.items, piece_ids, r.error)
    if "recipes.json" in ds.data:
        cooking.check_recipes(ds.data["recipes.json"], ds.items, piece_ids, r.error)
    cooking.check_generated(ds.repo_root, ds.data, r.error)


# --------------------------------------------------------------------------- pesca

FISH_HABITATS = {"orilla", "laguna", "arrecife", "talud", "profundo"}
FISH_METHODS = {"cana", "arpon", "red", "trampa", "mano"}
FISH_TRAPS = {"nasa", "trampa_cangrejos", "corral_piedras"}
FISH_CPP = Path("Source") / "Explored" / "Fishing" / "FishingModel.cpp"
FISH_TRAP_KINDS = {"Nasa": "nasa", "CrabTrap": "trampa_cangrejos", "StoneCorral": "corral_piedras"}


def _cpp_calls(text: str, fn: str) -> dict[str, list[str]]:
    """Argumentos de cada llamada «fn(TEXT("id"), ...)» del C++, por id."""
    calls = {}
    for m in re.finditer(rf'{fn}\(TEXT\("(\w+)"\),(.*?)\);', text, re.S):
        calls[m.group(1)] = [a.strip() for a in m.group(2).split(",")]
    return calls


def _cpp_num(arg: str) -> float:
    return float(arg.rstrip("f"))


def _check_fish_species(doc: dict, ids: set[str], items: dict[str, dict], r: Report) -> None:
    bait_keys = {"sin_cebo"} | set(doc.get("baits", {}))
    species = doc.get("species", [])
    rod_fish = [s for s in species if "cana" in s.get("methods", [])]
    if len(rod_fish) != 11:
        r.error(f"fish.json: {len(rod_fish)} peces de caña; el GDD §8.8 fija 11")
    if "langosta" not in {s.get("id") for s in species}:
        r.error("fish.json: falta la langosta de arrecife (GDD §8.8)")
    for s in species:
        sid = s.get("id")
        if sid not in ids:
            r.error(f"fish.json «{sid}»: la captura no está en items.json")
        elif "comida" not in items[sid].get("tags", []):
            r.error(f"fish.json «{sid}»: la captura debe tener la etiqueta comida")
        if not set(s.get("habitats", [])) or set(s.get("habitats", [])) - FISH_HABITATS:
            r.error(f"fish.json «{sid}»: hábitats inválidos {s.get('habitats')}")
        if not set(s.get("methods", [])) or set(s.get("methods", [])) - FISH_METHODS:
            r.error(f"fish.json «{sid}»: métodos inválidos {s.get('methods')}")
        for key in ("depthM", "weightKg"):
            lo, hi = (list(s.get(key) or []) + [0, 0])[:2]
            if not (_type_ok(lo, (int, float)) and _type_ok(hi, (int, float)) and 0 < lo < hi):
                r.error(f"fish.json «{sid}»: {key} {s.get(key)} no es un rango [min < max] positivo")
        for group in ("periods", "tide", "moon", "baits"):
            for k, v in s.get(group, {}).items():
                if not _type_ok(v, (int, float)) or not 0 <= v <= 3:
                    r.error(f"fish.json «{sid}».{group}.{k}={v!r} fuera de [0, 3]")
        for k in s.get("baits", {}):
            if k not in bait_keys:
                r.error(f"fish.json «{sid}»: cebo desconocido «{k}»")


def _check_fish_mirror(ds: DataSet, doc: dict, r: Report) -> None:
    """Espejo del C++: ids y números de las tablas de FFishingModel."""
    cpp_path = ds.repo_root / FISH_CPP
    if not cpp_path.exists():
        r.warn("fish.json: no se encuentra FishingModel.cpp; no se compara con el C++")
        return
    cpp = cpp_path.read_text(encoding="utf-8")
    cpp_species = _cpp_calls(cpp, "MakeSpecies")
    json_species = {s.get("id"): s for s in doc.get("species", [])}
    for sid in sorted(set(cpp_species) ^ set(json_species)):
        r.error(f"fish.json: la especie «{sid}» no coincide entre fish.json y FishingModel.cpp")
    for sid in sorted(set(cpp_species) & set(json_species)):
        n = [_cpp_num(a) for a in cpp_species[sid][-9:]]
        s = json_species[sid]
        expected = [*s.get("depthM", []), s.get("bitesPerMinute"), *s.get("weightKg", []),
                    s.get("strengthKgf"), s.get("staminaSeconds"), s.get("aggression"), s.get("wariness")]
        if n != expected:
            r.error(f"fish.json «{sid}»: números distintos de FishingModel.cpp ({expected} frente a {n})")
    cpp_legends = _cpp_calls(cpp, "MakeLegend")
    json_legends = {leg.get("id") for leg in doc.get("legendary", [])}
    for lid in sorted(set(cpp_legends) ^ json_legends):
        r.error(f"fish.json: la legendaria «{lid}» no coincide entre fish.json y FishingModel.cpp")
    cpp_traps = {(FISH_TRAP_KINDS.get(m.group(1)), m.group(2), float(m.group(3)))
                 for m in re.finditer(r'\{ ETrapKind::(\w+), TEXT\("(\w+)"\), [^{}]*?, ([\d.]+)f, [\d.]+f, EFishBait', cpp)}
    json_traps = {(k, c.get("item"), c.get("perHour"))
                  for k, t in doc.get("traps", {}).items() for c in t.get("catches", [])}
    for kind, item_id, rate in sorted(cpp_traps ^ json_traps, key=str):
        r.error(f"fish.json: trampa «{kind}» «{item_id}» ({rate}/h) no coincide con FishingModel.cpp")


def check_fish(ds: DataSet, r: Report) -> None:
    doc = ds.data.get("fish.json")
    if not doc:
        return
    ids = ds.item_ids
    items = {i["id"]: i for i in ds.items}
    for key, item_id in doc.get("baits", {}).items():
        if item_id not in ids:
            r.error(f"fish.json baits.{key}: «{item_id}» no está en items.json")
    _check_fish_species(doc, ids, items, r)

    legendary = doc.get("legendary", [])
    if len(legendary) != 5:
        r.error(f"fish.json: {len(legendary)} legendarias; la biblia §4.6 fija 5")
    for leg in legendary:
        if not leg.get("rewards"):
            r.error(f"fish.json legendaria «{leg.get('id')}»: sin recompensa")
        for reward in leg.get("rewards", []):
            if reward not in ids:
                r.error(f"fish.json legendaria «{leg.get('id')}»: recompensa «{reward}» no está en items.json")
        if not leg.get("spot"):
            r.error(f"fish.json legendaria «{leg.get('id')}»: sin sitio")

    traps = doc.get("traps", {})
    if set(traps) != FISH_TRAPS:
        r.error(f"fish.json: trampas {sorted(traps)}; se esperan {sorted(FISH_TRAPS)}")
    for kind, trap in traps.items():
        if not isinstance(trap.get("capacity"), int) or trap["capacity"] < 1:
            r.error(f"fish.json trampa «{kind}»: capacidad inválida")
        for c in trap.get("catches", []):
            if c.get("item") not in ids:
                r.error(f"fish.json trampa «{kind}»: «{c.get('item')}» no está en items.json")
            if not _type_ok(c.get("perHour"), (int, float)) or not 0 < c["perHour"] <= 1:
                r.error(f"fish.json trampa «{kind}» «{c.get('item')}»: perHour fuera de (0, 1]")
            bait = c.get("favouriteBait")
            if bait is not None and bait not in ids:
                r.error(f"fish.json trampa «{kind}»: cebo «{bait}» no está en items.json")
    for c in doc.get("tidePool", {}).get("catches", []):
        if c.get("item") not in ids:
            r.error(f"fish.json poza: «{c.get('item')}» no está en items.json")
    for y in doc.get("butchery", {}).get("yields", []):
        if y not in ids:
            r.error(f"fish.json despiece: «{y}» no está en items.json")
    _check_fish_mirror(ds, doc, r)


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
                r.error(f"{name}: contiene «{term}», eliminado por el GDD §12 (y no recuperado por el GDD v2)")


# GDD §8.8 (recolección y mar): nombre del GDD -> ids de items.json que lo cubren.
GDD_FOOD = {
    "coco": ("coco_verde", "coco_maduro"), "plátano": ("platano",), "mango": ("mango_fruta",),
    "papaya": ("papaya",), "carambola": ("carambola",), "guayaba": ("guayaba",),
    "fruta del pan": ("fruta_pan",), "taro": ("taro",), "yuca": ("yuca",), "batata": ("batata",),
    "miel": ("miel",), "huevos de gaviota": ("huevo",), "algas": ("alga_comestible",),
    "cangrejos": ("cangrejo",), "lapas": ("lapa",), "erizos": ("erizo",), "pulpo": ("pulpo",),
    "langosta de arrecife": ("langosta",),
}
# GDD §8.8: setas (2 comestibles, 2 tóxicas, 1 alucinógena).
GDD_MUSHROOMS = {"comestible": 2, "toxica": 2, "alucinogena": 1}


def _mushroom_kind(item: dict) -> str:
    if "alucinogena" in item.get("tags", []):
        return "alucinogena"
    toxic = any(p["name"] == "Toxico" and p["value"] > 0 for p in item.get("properties", []))
    return "toxica" if toxic else "comestible"


def check_gdd_food_coverage(ds: DataSet, r: Report) -> None:
    """Comida del GDD §8.8 que aún falta en items.json (nota, no error).

    Añadir una comida obliga a darla de alta en recipes.json/foods y a regenerar
    CookingData.inl, así que el hueco se deja visible en vez de bloquear.
    """
    ids = {i["id"] for i in ds.items}
    missing = [name for name, cands in GDD_FOOD.items() if not ids.intersection(cands)]
    if missing:
        r.info.append(f"items.json: falta comida del GDD §8.8: {', '.join(missing)}")
    counts = {k: 0 for k in GDD_MUSHROOMS}
    for item in ds.items:
        if "seta" in item.get("tags", []):
            counts[_mushroom_kind(item)] += 1
    short = [f"{k} {counts[k]}/{n}" for k, n in GDD_MUSHROOMS.items() if counts[k] < n]
    if short:
        r.info.append(f"items.json: setas por debajo del GDD §8.8: {', '.join(short)}")


# GDD v2 §3.4 (estratos de minería): nombre del GDD -> ids de items.json que lo cubren.
GDD_MINING = {
    "tierra y arena": ("arena",), "arcilla": ("arcilla_roja",), "caliza": ("caliza",),
    "basalto": ("basalto",), "obsidiana": ("obsidiana",), "veta de cobre": ("mineral_cobre",),
    "hierro de meteorito": ("hierro_meteorito",), "azufre": ("azufre",),
    "cristal": ("cristal_cuarzo",),
}
# GDD v2 §3.4: herramientas mínimas por dureza (1 pala tosca, 2 pico de piedra, 3 tallado, 4 obsidiana).
MINING_TOOLS = ("pala", "pico")


def check_gdd_mining(ds: DataSet, r: Report) -> None:
    """Cada estrato del GDD v2 §3.4 deja un objeto en items.json (error si falta).

    Las herramientas, los estratos por isla y la progresión de picos los valida
    ``mining.check_mining`` sobre mining.json.
    """
    ids = {i["id"] for i in ds.items}
    for name, cands in GDD_MINING.items():
        if not ids.intersection(cands):
            r.error(f"items.json: falta el estrato «{name}» del GDD v2 §3.4 ({' o '.join(cands)})")
    templates = {t.get("resultDefinitionId") for t in ds.templates}
    missing = [t for t in MINING_TOOLS if t not in templates]
    if missing:
        r.info.append(f"templates.json: sin plantilla para herramientas de minería del GDD v2 §3.4: {', '.join(missing)}")


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
    check_compost(ds, r, obtainable)
    check_building(ds, r, obtainable)
    check_boats(ds, r, obtainable)
    check_meshes(ds, r)
    check_survival(ds, r)
    check_cooking(ds, r)
    check_story(ds, r)
    achievements.check_achievements(ds, r)
    check_ruins(ds, r)
    check_artifacts(ds, r)
    check_fish(ds, r)
    music.check_music(ds, r)
    check_forbidden_terms(ds, r)
    check_gdd_food_coverage(ds, r)
    check_gdd_mining(ds, r)
    mining.check_mining(ds, r)
    fauna.check_fauna(ds, r, PROPERTIES)
    fases.check_future_phases(ds, r, BUILDING_SOCKETS)
    packs.check_catalog(ds.repo_root, ds.data, r.error)
    return r
