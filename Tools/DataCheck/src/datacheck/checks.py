"""Comprobaciones de Content/Data/*.json de Baliza Siete (GDD v3). Cada check añade mensajes a un Report.

Lista cerrada: 17 objetos (§7), 6 verbos y 10 recetas (§5.6), fuego (§5.2), cuerpo y consumibles
(§5.1), pesca (§5.4) y barril (§5.5). Los números que también viven en C++ (valores por defecto de los
modelos puros) se comparan con el código para que el JSON y el C++ no se separen.
"""

from __future__ import annotations

import copy
import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from . import historia, music, textos

REPO_ROOT = Path(__file__).resolve().parents[4]

SIZES = {"Pequeno", "Mediano", "Grande", "DosManos"}
ASCII_ID = re.compile(r"^[a-z0-9_]+$")
# GDD §11.1, kit K3: /Game/Art/K3_Objetos/SM_<Nombre>_01.SM_<Nombre>_01
K3_MESH = re.compile(r"^/Game/Art/K3_Objetos/(SM_[A-Za-z]+_01)\.\1$")

# GDD v3 §7: id → (pila, pesado).
GDD_ITEMS: dict[str, tuple[int, bool]] = {
    "cuchillo": (1, False), "cantimplora": (1, False), "barometro": (1, False), "coco": (4, False),
    "coco_vacio": (4, False), "lena": (6, False), "fibra": (6, False), "cuerda": (4, False),
    "cana_bambu": (2, False), "cana_pescar": (1, False), "tronco": (1, True), "remo": (1, False),
    "pez_crudo": (3, False), "pez_asado": (3, False), "tarro_aceite": (2, False), "mecha": (1, False),
    "lente": (1, True),
}
GDD_VERBS = ["encender", "trenzar", "atar", "cocinar", "hervir", "montar"]
# GDD v3 §5.6: id → (verbo, lugar, segundos).
GDD_RECIPES: dict[str, tuple[str, str, float]] = {
    "hoguera": ("encender", "suelo", 3), "cuerda": ("trenzar", "cualquier", 4),
    "cana_pescar": ("atar", "cualquier", 3), "pez_asado": ("cocinar", "hoguera", 20),
    "mecha": ("trenzar", "cualquier", 6), "tarro_aceite": ("hervir", "hoguera", 40),
    "remo": ("atar", "cualquier", 3), "balsa": ("atar", "sitio_balsa", 20),
    "linterna": ("montar", "linterna", 3), "baliza": ("encender", "linterna", 3),
}
PLACES = {"cualquier", "suelo", "hoguera", "sitio_balsa", "linterna"}
WORLD_OUTPUTS = {"hoguera", "balsa", "linterna_montada", "final"}
NEEDS = ["sed", "hambre", "energia", "calor"]

# Espejos JSON → constante C++ (fichero relativo a Source/Explored, nombre de la constante).
CAMPFIRE_CPP = "Cooking/CampfireModel.h"
FISH_CPP = "Fishing/CastFishingModel.h"
CONSUMABLES_CPP = "Items/IslandConsumablesModel.h"
BACKPACK_CPP = "Items/BackpackModel.h"
BARREL_CPP = "Weather/RainBarrelModel.h"

# GDD §12 (v1): términos de narrativa eliminada que no deben volver a los datos.
FORBIDDEN_TERMS = [
    "rata", "murcielago", "murciélago", "serpiente",
    "lagarto", "iguana", "perro", "canela", "rodrigo", "ines", "inés",
    "rescate", "mascota",
]

DATA_FILES = [
    "items.json", "verbs.json", "recipes.json", "fuels.json", "survival_needs.json", "fish.json",
    "music_layers.json", "notebook.json", "objectives.json", "thoughts.json",
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


def _dicts(value: Any) -> list[dict[str, Any]]:
    """Entradas de una lista JSON que son objetos (lo demás lo informa su check de esquema)."""
    if not isinstance(value, list):
        return []
    return [v for v in value if isinstance(v, dict)]


@dataclass
class DataSet:
    # JSON tal cual se lee: cada check comprueba los tipos que usa.
    data: dict[str, Any]
    repo_root: Path = REPO_ROOT
    # Ficheros que existen pero no son JSON válido: nombre -> mensaje del parser.
    load_errors: dict[str, str] = field(default_factory=dict)

    @classmethod
    def load(cls, repo_root: Path = REPO_ROOT) -> "DataSet":
        data_dir = repo_root / "Content" / "Data"
        data: dict[str, Any] = {}
        load_errors: dict[str, str] = {}
        for name in DATA_FILES:
            path = data_dir / name
            if path.exists():
                try:
                    data[name] = json.loads(path.read_text(encoding="utf-8"))
                except (json.JSONDecodeError, UnicodeDecodeError) as exc:
                    load_errors[name] = str(exc)
        return cls(data, repo_root, load_errors)

    def copy(self) -> "DataSet":
        return DataSet(copy.deepcopy(self.data), self.repo_root, dict(self.load_errors))

    def doc(self, name: str) -> dict[str, Any]:
        value = self.data.get(name)
        return value if isinstance(value, dict) else {}

    @property
    def items(self) -> list[dict[str, Any]]:
        return _dicts(self.data.get("items.json"))

    @property
    def verbs(self) -> list[dict[str, Any]]:
        return _dicts(self.data.get("verbs.json"))

    @property
    def recipes(self) -> list[dict[str, Any]]:
        return _dicts(self.doc("recipes.json").get("recipes"))

    @property
    def item_ids(self) -> set[str]:
        return {i["id"] for i in self.items if isinstance(i.get("id"), str)}


def _type_ok(value: Any, types: type | tuple[type, ...]) -> bool:
    types = types if isinstance(types, tuple) else (types,)
    if isinstance(value, bool) and bool not in types:
        return False
    return isinstance(value, types)


def _number(value: Any) -> bool:
    return _type_ok(value, (int, float))


def _text(value: Any) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _read_cpp(ds: DataSet, rel: str) -> str:
    path = ds.repo_root / "Source" / "Explored" / rel
    return path.read_text(encoding="utf-8") if path.exists() else ""


def _cpp_constant(text: str, name: str) -> float | None:
    m = re.search(rf"constexpr\s+(?:float|int32|double)\s+{name}\s*=\s*(-?[0-9.]+)f?\s*;", text)
    return float(m.group(1)) if m else None


def _mirror(ds: DataSet, r: Report, where: str, value: Any, rel: str, constant: str) -> None:
    """El valor del JSON debe coincidir con la constante por defecto del modelo C++."""
    text = _read_cpp(ds, rel)
    if not text:
        r.warn(f"{where}: no se encuentra Source/Explored/{rel}; no se compara con el C++")
        return
    cpp = _cpp_constant(text, constant)
    if cpp is None:
        r.error(f"{where}: no existe la constante {constant} en {rel}")
    elif not _number(value) or float(value) != cpp:
        r.error(f"{where}={value} pero {constant}={cpp:g} en {rel}")


# --------------------------------------------------------------------------- ficheros


def check_files_present(ds: DataSet, r: Report) -> None:
    for name in DATA_FILES:
        if name in ds.load_errors:
            r.error(f"{name}: JSON mal formado ({ds.load_errors[name]})")
        elif name not in ds.data:
            r.error(f"Falta Content/Data/{name}")


# --------------------------------------------------------------------------- objetos y verbos


def _check_item(item: dict[str, Any], r: Report) -> None:
    iid = item.get("id")
    for key in ("nameEs", "nameEn"):
        if not _text(item.get(key)):
            r.error(f"items.json «{iid}»: falta {key}")
    mesh = item.get("meshPath")
    if not isinstance(mesh, str) or not K3_MESH.match(mesh):
        r.error(f"items.json «{iid}»: meshPath «{mesh}» no es /Game/Art/K3_Objetos/SM_<Nombre>_01 (GDD §11.1)")
    for key in ("weightKg", "volumeLiters"):
        if not _number(item.get(key)) or item[key] <= 0:
            r.error(f"items.json «{iid}»: {key} debe ser un número > 0")
    if item.get("size") not in SIZES:
        r.error(f"items.json «{iid}»: size «{item.get('size')}» desconocido")
    tags = item.get("tags", [])
    if not isinstance(tags, list) or not all(_text(t) for t in tags):
        r.error(f"items.json «{iid}»: tags debe ser una lista de textos")
    expected = GDD_ITEMS.get(iid) if isinstance(iid, str) else None
    if expected is None:
        return
    stack, heavy = expected
    if item.get("stack") != stack or not _type_ok(item.get("stack"), int):
        r.error(f"items.json «{iid}»: stack={item.get('stack')} pero el GDD §5.3/§7 dice {stack}")
    if item.get("heavy") is not heavy:
        r.error(f"items.json «{iid}»: heavy={item.get('heavy')} pero el GDD §7 dice {heavy}")
    if heavy and item.get("size") != "DosManos":
        r.error(f"items.json «{iid}»: un objeto pesado se lleva con dos manos (size DosManos)")


def check_items(ds: DataSet, r: Report) -> None:
    raw = ds.data.get("items.json")
    if raw is None:
        return
    if not isinstance(raw, list):
        r.error("items.json: la raíz debe ser una lista")
        return
    seen: list[str] = []
    for entry in raw:
        if not isinstance(entry, dict):
            r.error("items.json: hay una entrada que no es un objeto")
            continue
        iid = entry.get("id")
        if not isinstance(iid, str) or not ASCII_ID.match(iid):
            r.error(f"items.json: id «{iid}» no es ASCII en minúsculas")
            continue
        if iid in seen:
            r.error(f"items.json: id duplicado «{iid}»")
        seen.append(iid)
        _check_item(entry, r)
    missing = sorted(set(GDD_ITEMS) - set(seen))
    extra = sorted(set(seen) - set(GDD_ITEMS))
    if missing:
        r.error(f"items.json: faltan objetos del GDD §7: {', '.join(missing)}")
    if extra:
        r.error(f"items.json: objetos fuera de la lista cerrada del GDD §7: {', '.join(extra)}")


def check_verbs(ds: DataSet, r: Report) -> None:
    raw = ds.data.get("verbs.json")
    if raw is None:
        return
    if not isinstance(raw, list):
        r.error("verbs.json: la raíz debe ser una lista")
        return
    ids = [v.get("id") for v in _dicts(raw)]
    if ids != GDD_VERBS:
        r.error(f"verbs.json: {ids} no es la lista cerrada del GDD §5.6 {GDD_VERBS}")
    for verb in _dicts(raw):
        for key in ("nameEs", "nameEn", "description", "descriptionEn"):
            if not _text(verb.get(key)):
                r.error(f"verbs.json «{verb.get('id')}»: falta {key}")


# --------------------------------------------------------------------------- recetas


def _amounts(value: Any, where: str, r: Report, known: set[str]) -> dict[str, int]:
    out: dict[str, int] = {}
    if not isinstance(value, list):
        r.error(f"{where}: debe ser una lista de {{item, count}}")
        return out
    for entry in value:
        if not isinstance(entry, dict):
            r.error(f"{where}: entrada que no es un objeto")
            continue
        item, count = entry.get("item"), entry.get("count")
        if item not in known:
            r.error(f"{where}: «{item}» no está en items.json ni es un objeto del mundo")
        if not isinstance(count, int) or isinstance(count, bool) or count <= 0:
            r.error(f"{where}: cantidad «{count}» de «{item}» no es un entero positivo")
            continue
        if isinstance(item, str):
            out[item] = out.get(item, 0) + count
    return out


def _check_recipe(rec: dict[str, Any], ds: DataSet, r: Report) -> None:
    rid = rec.get("id")
    where = f"recipes.json «{rid}»"
    items = ds.item_ids
    for key in ("nameEs", "nameEn"):
        if not _text(rec.get(key)):
            r.error(f"{where}: falta {key}")
    verbs = {v.get("id") for v in ds.verbs}
    if rec.get("verb") not in verbs:
        r.error(f"{where}: verbo «{rec.get('verb')}» no está en verbs.json")
    if rec.get("place") not in PLACES:
        r.error(f"{where}: lugar «{rec.get('place')}» desconocido ({', '.join(sorted(PLACES))})")
    _amounts(rec.get("inputs"), f"{where}.inputs", r, items)
    _amounts(rec.get("stationNeeds", []), f"{where}.stationNeeds", r, items | WORLD_OUTPUTS)
    tools = rec.get("tools", [])
    if not isinstance(tools, list) or any(t not in items for t in tools):
        r.error(f"{where}: tools debe listar objetos de items.json")
    if not _number(rec.get("seconds")) or rec["seconds"] <= 0:
        r.error(f"{where}: seconds debe ser > 0")
    steps = rec.get("steps", 1)
    if not _type_ok(steps, int) or steps < 1:
        r.error(f"{where}: steps debe ser un entero ≥ 1")
    out = rec.get("output")
    if not isinstance(out, dict) or ("item" in out) == ("world" in out):
        r.error(f"{where}: output necesita exactamente «item» o «world»")
    elif "item" in out:
        if out["item"] not in items:
            r.error(f"{where}: la salida «{out['item']}» no está en items.json")
        if not _type_ok(out.get("count"), int) or out["count"] <= 0:
            r.error(f"{where}: la salida necesita un count entero positivo")
    elif out["world"] not in WORLD_OUTPUTS:
        r.error(f"{where}: salida de mundo «{out['world']}» desconocida")
    expected = GDD_RECIPES.get(rid) if isinstance(rid, str) else None
    if expected and (rec.get("verb"), rec.get("place"), rec.get("seconds")) != expected:
        r.error(f"{where}: (verbo, lugar, segundos)=({rec.get('verb')}, {rec.get('place')}, {rec.get('seconds')}) "
                f"pero el GDD §5.6 dice {expected}")


def check_recipes(ds: DataSet, r: Report) -> None:
    if "recipes.json" not in ds.data:
        return
    if not isinstance(ds.doc("recipes.json").get("recipes"), list):
        r.error("recipes.json: falta la lista «recipes»")
        return
    ids = [rec.get("id") for rec in ds.recipes]
    for dup in sorted({i for i in ids if isinstance(i, str) and ids.count(i) > 1}):
        r.error(f"recipes.json: receta duplicada «{dup}»")
    if sorted(i for i in ids if isinstance(i, str)) != sorted(GDD_RECIPES):
        r.error(f"recipes.json: {ids} no es la lista cerrada de 10 recetas del GDD §5.6")
    for rec in ds.recipes:
        _check_recipe(rec, ds, r)
    produced = {rec.get("output", {}).get("item") for rec in ds.recipes if isinstance(rec.get("output"), dict)}
    raw = {"coco", "lena", "fibra", "cana_bambu", "tronco", "pez_crudo", "lente", "cuchillo", "cantimplora", "barometro"}
    for item in sorted(ds.item_ids - produced - raw - {"coco_vacio"}):
        r.error(f"recipes.json: «{item}» no sale de ninguna receta ni se recoge en la isla")


# --------------------------------------------------------------------------- fuego, cuerpo, pesca


def check_fuels(ds: DataSet, r: Report) -> None:
    doc = ds.doc("fuels.json")
    if not doc:
        return
    fire = doc.get("campfire")
    if not isinstance(fire, dict):
        r.error("fuels.json: falta «campfire»")
        return
    for key, constant in (("burnSeconds", "DefaultBurnSeconds"), ("maxBurnSeconds", "DefaultMaxBurnSeconds"),
                          ("heatRadiusMeters", "DefaultHeatRadiusMeters"), ("warmthPerMinute", "DefaultWarmthPerMinute")):
        _mirror(ds, r, f"fuels.json campfire.{key}", fire.get(key), CAMPFIRE_CPP, constant)
    fuel_constants = {"lena": "DefaultWoodSeconds", "tarro_aceite": "DefaultOilJarSeconds"}
    fuels = _dicts(doc.get("fuels"))
    for fuel in fuels:
        item = fuel.get("item")
        if item not in ds.item_ids:
            r.error(f"fuels.json: combustible «{item}» no está en items.json")
        elif item in fuel_constants:
            _mirror(ds, r, f"fuels.json «{item}».secondsPerFuel", fuel.get("secondsPerFuel"), CAMPFIRE_CPP,
                    fuel_constants[item])
        else:
            r.error(f"fuels.json: «{item}» no es combustible en el GDD §5.2 (leña o tarro de aceite)")
    if {f.get("item") for f in fuels} != set(fuel_constants):
        r.error("fuels.json: los combustibles deben ser exactamente leña y tarro de aceite (GDD §5.2)")


def _check_consumables(ds: DataSet, doc: dict[str, Any], r: Report) -> None:
    mirrors = {
        ("coco", "thirst"): "CoconutThirst", ("coco", "hunger"): "CoconutHunger",
        ("cantimplora", "thirst"): "CanteenSipThirst", ("pez_crudo", "hunger"): "RawFishHunger",
        ("pez_crudo", "energy"): "RawFishEnergy", ("pez_asado", "hunger"): "GrilledFishHunger",
    }
    seen = set()
    for entry in _dicts(doc.get("consumables")):
        item = entry.get("item")
        seen.add(item)
        if item not in ds.item_ids:
            r.error(f"survival_needs.json consumables: «{item}» no está en items.json")
        leaves = entry.get("leaves")
        if leaves is not None and leaves not in ds.item_ids:
            r.error(f"survival_needs.json consumables «{item}»: deja «{leaves}», que no está en items.json")
        for key in ("thirst", "hunger", "energy"):
            constant = mirrors.get((str(item), key))
            if constant:
                _mirror(ds, r, f"survival_needs.json consumables «{item}».{key}", entry.get(key), CONSUMABLES_CPP, constant)
            elif not _number(entry.get(key, 0)):
                r.error(f"survival_needs.json consumables «{item}».{key} debe ser un número")
        if item == "cantimplora":
            _mirror(ds, r, "survival_needs.json consumables «cantimplora».sips", entry.get("sips"), BACKPACK_CPP,
                    "CanteenMaxSips")
    if seen != {"coco", "cantimplora", "pez_crudo", "pez_asado"}:
        r.error("survival_needs.json consumables: deben ser coco, cantimplora, pez crudo y pez asado (GDD §5.1)")


def check_survival(ds: DataSet, r: Report) -> None:
    doc = ds.doc("survival_needs.json")
    if not doc:
        return
    needs = _dicts(doc.get("needs"))
    if [n.get("id") for n in needs] != NEEDS:
        r.error(f"survival_needs.json: las barras deben ser {NEEDS} (GDD §5.1)")
    for need in needs:
        nid = need.get("id")
        for key in ("nameEs", "nameEn"):
            if not _text(need.get(key)):
                r.error(f"survival_needs.json «{nid}»: falta {key}")
        if not _number(need.get("initial")) or not 0 <= need["initial"] <= 100:
            r.error(f"survival_needs.json «{nid}»: initial fuera de [0, 100]")
        if not _number(need.get("lossPerMinute")) or need["lossPerMinute"] < 0:
            r.error(f"survival_needs.json «{nid}»: lossPerMinute debe ser un número ≥ 0")
    _check_consumables(ds, doc, r)
    kit = {k.get("item"): k for k in _dicts(doc.get("starterKit"))}
    if set(kit) != {"cuchillo", "cantimplora", "barometro"}:
        r.error("survival_needs.json starterKit: debe ser cuchillo, cantimplora y barómetro (GDD §5.3)")
    _mirror(ds, r, "survival_needs.json starterKit «cantimplora».sips", kit.get("cantimplora", {}).get("sips"),
            BACKPACK_CPP, "StarterCanteenSips")
    _mirror(ds, r, "survival_needs.json backpackSlots", doc.get("backpackSlots"), BACKPACK_CPP, "DefaultSlots")
    barrel = doc.get("rainBarrel", {})
    barrel = barrel if isinstance(barrel, dict) else {}
    _mirror(ds, r, "survival_needs.json rainBarrel.charges", barrel.get("charges"), BARREL_CPP, "MaxCharges")
    _mirror(ds, r, "survival_needs.json rainBarrel.showerCharges", barrel.get("showerCharges"), BARREL_CPP, "ShowerCharges")


def check_fish(ds: DataSet, r: Report) -> None:
    doc = ds.doc("fish.json")
    if not doc:
        return
    for key, constant in (("meanBiteSeconds", "DefaultMeanBiteSeconds"), ("minBiteSeconds", "DefaultMinBiteSeconds"),
                          ("biteWindowSeconds", "DefaultBiteWindowSeconds"), ("goldenHourFactor", "DefaultGoldenHourFactor")):
        _mirror(ds, r, f"fish.json {key}", doc.get(key), FISH_CPP, constant)
    for key in ("rod", "catch"):
        if doc.get(key) not in ds.item_ids:
            r.error(f"fish.json {key}: «{doc.get(key)}» no está en items.json")
    for key in ("dawn", "dusk", "night"):
        span = doc.get(key)
        if not isinstance(span, list) or len(span) != 2 or not all(_number(h) and 0 <= h < 24 for h in span):
            r.error(f"fish.json {key}: debe ser [desde, hasta] en horas 0-24")


# --------------------------------------------------------------------------- reglas generales


def check_forbidden_terms(ds: DataSet, r: Report) -> None:
    for name, content in ds.data.items():
        text = json.dumps(content, ensure_ascii=False).lower()
        for term in FORBIDDEN_TERMS:
            if re.search(rf"(?<![a-záéíóúñ]){re.escape(term)}(?![a-záéíóúñ])", text):
                r.error(f"{name}: contiene «{term}», eliminado del diseño")


def run_all(ds: DataSet) -> Report:
    r = Report()
    check_files_present(ds, r)
    check_items(ds, r)
    check_verbs(ds, r)
    check_recipes(ds, r)
    check_fuels(ds, r)
    check_survival(ds, r)
    check_fish(ds, r)
    textos.check_player_texts(ds.data, r.error)
    music.check_music(ds, r)
    historia.check_story(ds.data, r.error)
    check_forbidden_terms(ds, r)
    return r
