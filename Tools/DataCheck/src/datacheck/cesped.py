"""grass.json (césped del kit K6), island_decor.json (vegetación decorativa) y ground_field.bin.

grass.json (versión 2, césped continuo): forma y rangos, las 11 mallas del kit K6 (que existan en
Art/Export/K6_Cesped/manifest.json) sin sombras, las 5 mezclas por sitio, zonas que existan en island.json,
el adelgazamiento («thinning», FGrassThinning) y el espejo con FGrassConfig (GrassFieldModel.h) y
FGrassThinning (GrassThinning.h).
island_decor.json: mallas de /Game/Art que existan en algún manifest de Art/Export y tope de instancias
de FIslandDecor (IslandDecorModel.h). ground_field.bin: que exista y tenga la cabecera EXGF.
"""

from __future__ import annotations

import json
import re
from collections.abc import Callable
from pathlib import Path
from typing import Any

GRASS_CPP = "WorldGen/GrassFieldModel.h"
THINNING_CPP = "WorldGen/GrassThinning.h"
GRASS_VERSION = 2
DECOR_CPP = "WorldGen/IslandDecorModel.h"
KINDS = ("tuft_a", "tuft_b", "tuft_c", "tall", "flower_a", "flower_b", "fern", "clover", "patch_a", "patch_b",
         "patch_c")
CONTEXTS = ("common", "edge", "clearing", "palm", "dirt")
K6_MESH = re.compile(r"^/Game/Art/K6_Cesped/(SM_[A-Za-z_]+_01)$")
ART_MESH = re.compile(r"^/Game/Art/(K\d_[A-Za-z]+)/(SM_[A-Za-z_]+_01)$")
MATERIAL = re.compile(r"^/Game/Art/Materials/M_[A-Za-z]+$")
COLLECTION = re.compile(r"^/Game/Art/Materials/MPC_[A-Za-z]+$")
# (clave de primer nivel, mínimo, máximo, constante C++ o None)
TOP = [
    ("cellSizeM", 2.0, 32.0, "DefaultCellSizeM"),
    ("workPerFrame", 50, 20000, "DefaultWorkPerFrame"),
    ("maxSlopeDeg", 1.0, 89.0, "DefaultMaxSlopeDeg"),
    ("clearanceM", 0.0, 10.0, "DefaultClearanceM"),
    ("pathMarginM", 0.0, 5.0, "DefaultPathMarginM"),
    ("pointClearanceM", 0.0, 20.0, None),
    ("sinkM", 0.0, 0.5, None),
    ("alignToNormal", 0.0, 1.0, None),
    ("edgeBandM", 0.0, 10.0, None),
    ("seed", 0, 2**32 - 1, None),
]
ZONE_MIRRORS = {
    "palmar": "ZonePalmarDensity", "colina_faro": "ZoneColinaDensity", "bosque_bambu": "ZoneBambuDensity",
    "cala_varada": "ZoneCalaDensity", "playa_norte": "ZonePlayaDensity", "rocas_pescador": "ZoneRocasDensity",
    "isla_carguero": "ZoneCargueroDensity",
}
# (clave de «thinning», mínimo, máximo, constante C++ de GrassThinning.h o None)
THINNING = [
    ("denseM", 1.0, 200.0, "DefaultDenseM"),
    ("endM", 1.0, 200.0, "DefaultEndM"),
    ("endFadeM", 0.0, 200.0, None),
    ("exponent", 0.5, 4.0, None),
    ("maxGrow", 1.0, 4.0, None),
    ("growZ", 0.0, 1.0, None),
    ("band", 0.001, 1.0, None),
    ("levelRatio", 0.25, 0.9, None),
    ("minLevel", 0.001, 0.5, None),
    ("marginM", 0.0, 20.0, None),
    ("hysteresisM", 0.0, 20.0, None),
]
MAX_DENSITY = 16.0

Report = Callable[[str], None]


def _obj(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def _num(value: Any) -> float | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return float(value)


def _cpp_constant(root: Path, rel: str, name: str) -> float | None:
    path = root / "Source" / "Explored" / rel
    if not path.exists():
        return None
    m = re.search(rf"constexpr\s+(?:float|int32)\s+{name}\s*=\s*(-?[0-9.]+)f?\s*;", path.read_text(encoding="utf-8"))
    return float(m.group(1)) if m else None


def _mirror(root: Path, where: str, value: float, name: str, error: Report, rel: str = GRASS_CPP) -> None:
    cpp = _cpp_constant(root, rel, name)
    if cpp is None:
        error(f"grass.json {where}: no existe la constante {name} en {rel}")
    elif value != cpp:
        error(f"grass.json {where}={value:g} pero {name}={cpp:g} en {rel}")


def _manifest_meshes(root: Path) -> dict[str, set[str]]:
    """Kit → mallas de cada Art/Export/K*/manifest.json."""
    out: dict[str, set[str]] = {}
    for path in sorted((root / "Art" / "Export").glob("K*/manifest.json")):
        try:
            doc = json.loads(path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError):
            continue
        out[path.parent.name] = {str(m.get("malla")) for m in doc.get("mallas", []) if isinstance(m, dict)}
    return out


def _range(doc: dict[str, Any], key: str, lo: float, hi: float, where: str, error: Report) -> float | None:
    value = _num(doc.get(key))
    if value is None or not lo <= value <= hi:
        error(f"grass.json {where}{key}: debe ser un número entre {lo:g} y {hi:g}")
        return None
    return value


def _check_top(doc: dict[str, Any], root: Path, error: Report) -> None:
    for key, lo, hi, constant in TOP:
        value = _range(doc, key, lo, hi, "", error)
        if value is not None and constant:
            _mirror(root, key, value, constant, error)
    scale = doc.get("scale")
    if not (isinstance(scale, list) and len(scale) == 2 and all(_num(s) is not None for s in scale)
            and 0 < scale[0] <= scale[1] <= 5):
        error("grass.json scale: debe ser [mínimo, máximo] con 0 < mínimo ≤ máximo ≤ 5")


def _check_thinning(doc: dict[str, Any], root: Path, error: Report) -> None:
    thin = doc.get("thinning")
    if not isinstance(thin, dict):
        error("grass.json thinning: falta el objeto")
        return
    values: dict[str, float] = {}
    for key, lo, hi, constant in THINNING:
        value = _range(thin, key, lo, hi, "thinning.", error)
        if value is None:
            continue
        values[key] = value
        if constant:
            _mirror(root, f"thinning.{key}", value, constant, error, THINNING_CPP)
    dense, end, fade = values.get("denseM"), values.get("endM"), values.get("endFadeM")
    if dense is not None and end is not None and not dense < end:
        error("grass.json thinning: denseM debe ser menor que endM")
    elif dense is not None and end is not None and fade is not None and fade > end - dense:
        error("grass.json thinning.endFadeM: no puede pasar de endM - denseM")
    wind = _obj(doc.get("wind"))
    start, stop = _num(wind.get("fadeStartM")), _num(wind.get("fadeEndM"))
    if start is None or stop is None or not 0 <= start < stop:
        error("grass.json wind: debe ser 0 ≤ fadeStartM < fadeEndM")
    elif end is not None and stop > end:
        error("grass.json wind.fadeEndM: no puede pasar de thinning.endM (más allá no hay césped)")


def _check_density(doc: dict[str, Any], root: Path, zone_ids: set[str], error: Report) -> None:
    density = doc.get("density")
    if not isinstance(density, dict):
        error("grass.json density: falta el objeto")
        return
    for key, constant in (("default", "DefaultDensity"), ("dirt", "DefaultDirtDensity")):
        value = _range(density, key, 0.0, MAX_DENSITY, "density.", error)
        if value is not None:
            _mirror(root, f"density.{key}", value, constant, error)
    _range(density, "zoneFeatherM", 0.0, 50.0, "density.", error)
    zones = density.get("zones")
    if not isinstance(zones, dict):
        error("grass.json density.zones: falta el objeto")
        return
    for zid, value in zones.items():
        if zone_ids and zid not in zone_ids:
            error(f"grass.json density.zones.{zid}: no es una zona de island.json")
        number = _num(value)
        if number is None or not 0 <= number <= MAX_DENSITY:
            error(f"grass.json density.zones.{zid}: debe estar entre 0 y {MAX_DENSITY:g}")
        elif zid in ZONE_MIRRORS:
            _mirror(root, f"density.zones.{zid}", number, ZONE_MIRRORS[zid], error)
    for zid in sorted(set(ZONE_MIRRORS) - set(zones)):
        error(f"grass.json density.zones: falta {zid} (FGrassConfig::Defaults la usa)")


def _check_assets(doc: dict[str, Any], manifests: dict[str, set[str]], error: Report) -> None:
    meshes = doc.get("meshes")
    if not isinstance(meshes, dict) or set(meshes) != set(KINDS):
        error(f"grass.json meshes: debe tener exactamente {', '.join(KINDS)}")
        meshes = meshes if isinstance(meshes, dict) else {}
    for kind, entry in meshes.items():
        if not isinstance(entry, dict):
            error(f"grass.json meshes.{kind}: debe ser un objeto")
            continue
        m = K6_MESH.match(str(entry.get("mesh", "")))
        if not m:
            error(f"grass.json meshes.{kind}.mesh: debe ser /Game/Art/K6_Cesped/SM_<Nombre>_01")
        elif manifests and m.group(1) not in manifests.get("K6_Cesped", set()):
            error(f"grass.json meshes.{kind}.mesh: {m.group(1)} no está en Art/Export/K6_Cesped/manifest.json")
        _range(entry, "scale", 0.05, 5.0, f"meshes.{kind}.", error)
        _range(entry, "footprintM", 0.0, 2.0, f"meshes.{kind}.", error)
        if entry.get("castShadow") is not False:
            error(f"grass.json meshes.{kind}.castShadow: debe ser false (el césped no proyecta sombras)")
    for key in ("material", "fallbackMaterial"):
        if not MATERIAL.match(str(doc.get(key, ""))):
            error(f"grass.json {key}: debe ser /Game/Art/Materials/M_<Nombre>")
    if not COLLECTION.match(str(doc.get("parameterCollection", ""))):
        error("grass.json parameterCollection: debe ser /Game/Art/Materials/MPC_<Nombre>")
    fallback = ART_MESH.match(str(doc.get("fallbackMesh", "")))
    if not fallback:
        error("grass.json fallbackMesh: debe ser /Game/Art/<Kit>/SM_<Nombre>_01")
    elif manifests and fallback.group(2) not in manifests.get(fallback.group(1), set()):
        error(f"grass.json fallbackMesh: {fallback.group(2)} no está en el manifest de {fallback.group(1)}")


def _check_mixes(doc: dict[str, Any], error: Report) -> None:
    mixes = doc.get("mixes")
    if not isinstance(mixes, dict) or set(mixes) != set(CONTEXTS):
        error(f"grass.json mixes: debe tener exactamente {', '.join(CONTEXTS)}")
        mixes = mixes if isinstance(mixes, dict) else {}
    for context, weights in mixes.items():
        if not isinstance(weights, dict) or not set(weights) <= set(KINDS):
            error(f"grass.json mixes.{context}: pesos de mallas desconocidas")
            continue
        values = [_num(w) for w in weights.values()]
        if any(v is None or v < 0 for v in values):
            error(f"grass.json mixes.{context}: los pesos deben ser números ≥ 0")
        elif context == "common" and sum(v for v in values if v is not None) <= 0:
            error("grass.json mixes.common: no puede estar vacía")
    for key in ("clearings", "palms"):
        if not isinstance(doc.get(key), dict):
            error(f"grass.json {key}: falta el objeto")
    clearings = _obj(doc.get("clearings"))
    _range(clearings, "cellM", 1.0, 200.0, "clearings.", error)
    _range(clearings, "density", 0.0, 1.0, "clearings.", error)
    palms = _obj(doc.get("palms"))
    trunk, ring = _num(palms.get("trunkM")), _num(palms.get("radiusM"))
    if trunk is None or ring is None or not 0 <= trunk < ring:
        error("grass.json palms: debe ser 0 ≤ trunkM < radiusM")


def check_grass(doc: Any, root: Path, zone_ids: set[str], error: Report) -> None:
    if not isinstance(doc, dict):
        error("grass.json: debe ser un objeto")
        return
    if doc.get("version") != GRASS_VERSION:
        error(f"grass.json version: debe ser {GRASS_VERSION}")
    manifests = _manifest_meshes(root)
    _check_top(doc, root, error)
    _check_thinning(doc, root, error)
    _check_density(doc, root, zone_ids, error)
    _check_assets(doc, manifests, error)
    _check_mixes(doc, error)


def check_decor(doc: Any, root: Path, error: Report) -> None:
    if not isinstance(doc, dict) or not isinstance(doc.get("meshes"), list) or not isinstance(doc.get("instances"), list):
        error("island_decor.json: faltan «meshes» o «instances»")
        return
    manifests = _manifest_meshes(root)
    for path in doc["meshes"]:
        m = ART_MESH.match(str(path))
        if not m:
            error(f"island_decor.json: malla {path} fuera de /Game/Art/<Kit>/SM_<Nombre>_01")
        elif manifests and m.group(2) not in manifests.get(m.group(1), set()):
            error(f"island_decor.json: {m.group(2)} no está en Art/Export/{m.group(1)}/manifest.json")
    cap = _cpp_constant(root, DECOR_CPP, "MaxInstances")
    if cap is not None and len(doc["instances"]) > cap:
        error(f"island_decor.json: {len(doc['instances'])} instancias y FIslandDecor admite {cap:g}")
    count = len(doc["meshes"])
    for i, inst in enumerate(doc["instances"]):
        if not (isinstance(inst, list) and len(inst) == 6 and all(_num(v) is not None for v in inst)
                and 0 <= inst[0] < count and 0 < inst[5] <= 10):
            error(f"island_decor.json: instancia {i} no válida")
            return


def check_ground_field(root: Path, error: Report) -> None:
    path = root / "Content" / "Data" / "ground_field.bin"
    if not path.exists():
        error("Falta Content/Data/ground_field.bin (uv run island-groundfield en Tools/Island)")
    elif path.read_bytes()[:4] != b"EXGF":
        error("ground_field.bin: cabecera distinta de EXGF")


def check_vegetation(data: dict[str, Any], root: Path, error: Report) -> None:
    island = data.get("island.json")
    zones = island.get("zones", []) if isinstance(island, dict) else []
    zone_ids = {str(z.get("id")) for z in zones if isinstance(z, dict)}
    if "grass.json" in data:
        check_grass(data["grass.json"], root, zone_ids, error)
    if "island_decor.json" in data:
        check_decor(data["island_decor.json"], root, error)
    check_ground_field(root, error)
