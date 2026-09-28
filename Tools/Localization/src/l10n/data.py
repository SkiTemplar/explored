"""Campos bilingües de Content/Data/*.json.

Cada texto de datos que ve el jugador tiene su campo en español (fuente) y su pareja en
inglés al lado (``nameEs``/``nameEn``, ``label``/``labelEn``...). El juego elige uno u
otro según la cultura activa (``ExploredLocalization::Pick`` en C++).
"""

from __future__ import annotations

from collections.abc import Callable, Iterator
from dataclasses import dataclass


@dataclass(frozen=True)
class DataText:
    file: str
    namespace: str  # "Data.<fichero sin .json>.<colección>"
    key: str  # "<id>.<campo>"
    es: str | None
    en: str | None
    es_field: str
    en_field: str
    record: str = ""

    @property
    def where(self) -> str:
        return f"Content/Data/{self.file} «{self.record}» {self.es_field}/{self.en_field}"


@dataclass(frozen=True)
class FieldSpec:
    file: str
    collection: str
    records: Callable[[object], Iterator[tuple[str, dict]]]
    es_field: str
    en_field: str
    purpose: str  # para el informe


def _list(doc: object) -> Iterator[tuple[str, dict]]:
    for rec in doc if isinstance(doc, list) else []:
        yield str(rec.get("id")), rec


def _sub(name: str) -> Callable[[object], Iterator[tuple[str, dict]]]:
    def records(doc: object) -> Iterator[tuple[str, dict]]:
        for rec in (doc.get(name, []) if isinstance(doc, dict) else []):
            yield str(rec.get("id")), rec

    return records


def _path(*keys: str) -> Callable[[object], Iterator[tuple[str, dict]]]:
    """Lista anidada en objetos: ``_path("trade", "wants")`` → doc["trade"]["wants"]."""

    def records(doc: object) -> Iterator[tuple[str, dict]]:
        node = doc
        for key in keys:
            node = node.get(key, {}) if isinstance(node, dict) else {}
        for rec in node if isinstance(node, list) else []:
            if isinstance(rec, dict):
                yield str(rec.get("id")), rec

    return records


def _keyed(name: str) -> Callable[[object], Iterator[tuple[str, dict]]]:
    """Objeto indexado por id: ``{"derrumbe": {...}, "oscuridad": {...}}`` (las notas sueltas se saltan)."""

    def records(doc: object) -> Iterator[tuple[str, dict]]:
        node = doc.get(name, {}) if isinstance(doc, dict) else {}
        for key, rec in (node.items() if isinstance(node, dict) else []):
            if isinstance(rec, dict):
                yield str(key), rec

    return records


def _landmarks(doc: object) -> Iterator[tuple[str, dict]]:
    for island in (doc.get("islands", []) if isinstance(doc, dict) else []):
        for mark in island.get("landmarks", []):
            yield str(mark.get("id")), mark


def _stages(doc: object) -> Iterator[tuple[str, dict]]:
    for plant in (doc.get("plants", []) if isinstance(doc, dict) else []):
        for stage in plant.get("stages", []):
            yield f"{plant.get('id')}.{stage.get('id')}", stage


FIELDS: list[FieldSpec] = [
    FieldSpec("items.json", "items", _list, "nameEs", "nameEn", "nombre del objeto (mano, suelo, mochila)"),
    FieldSpec("templates.json", "templates", _list, "nameEs", "nameEn", "nombre de la receta"),
    FieldSpec("templates.json", "templates", _list, "nameTemplate", "nameTemplateEn",
              "nombre generado del objeto fabricado ({0}, {1}... = pieza de cada hueco)"),
    FieldSpec("verbs.json", "verbs", _list, "nameEs", "nameEn", "verbo de fabricación"),
    FieldSpec("verbs.json", "verbs", _list, "description", "descriptionEn", "ayuda del verbo"),
    FieldSpec("plants.json", "plants", _sub("plants"), "nameEs", "nameEn", "nombre de la planta"),
    FieldSpec("plants.json", "stages", _stages, "nameEs", "nameEn", "etapa de crecimiento"),
    FieldSpec("building_pieces.json", "tiers", _sub("tiers"), "nameEs", "nameEn", "nivel de construcción"),
    FieldSpec("building_pieces.json", "pieces", _sub("pieces"), "nameEs", "nameEn", "pieza de construcción"),
    FieldSpec("survival_needs.json", "needs", _sub("needs"), "nameEs", "nameEn", "necesidad (señal corporal del HUD)"),
    FieldSpec("story_es.json", "map_marks", _sub("map_marks"), "label", "labelEn", "sello del mapa"),
    FieldSpec("story_es.json", "artifact_kinds", _sub("artifact_kinds"), "label", "labelEn", "tipo de tesoro del museo"),
    FieldSpec("achievements.json", "achievements", _sub("achievements"), "nameEs", "nameEn", "nombre del logro (Steam y menú)"),
    FieldSpec("achievements.json", "achievements", _sub("achievements"), "descriptionEs", "descriptionEn",
              "descripción del logro (Steam y menú)"),
    FieldSpec("artifacts.json", "artifacts", _sub("artifacts"), "nameEs", "nameEn", "etiqueta de vitrina del museo"),
    FieldSpec("artifacts.json", "displays", _sub("displays"), "nameEs", "nameEn", "mueble del museo"),
    FieldSpec("artifacts.json", "provenances", _sub("provenances"), "nameEs", "nameEn", "procedencia de un tesoro"),
    FieldSpec("artifacts.json", "rarities", _sub("rarities"), "nameEs", "nameEn", "rareza de un tesoro"),
    FieldSpec("boats.json", "boats", _sub("boats"), "nameEs", "nameEn", "embarcación del astillero"),
    FieldSpec("exploration.json", "kinds", _sub("kinds"), "nameEs", "nameEn", "tipo de lugar del mapa"),
    FieldSpec("exploration.json", "landmarks", _landmarks, "nameEs", "nameEn", "lugar con nombre del mapa"),
    FieldSpec("fauna.json", "species", _sub("species"), "nameEs", "nameEn", "especie marina"),
    FieldSpec("fauna.json", "lod", _path("lod", "tiers"), "nameEs", "nameEn", "nivel de detalle de fauna (ajustes)"),
    FieldSpec("fauna_terrestre.json", "species", _sub("species"), "nameEs", "nameEn", "especie terrestre"),
    FieldSpec("fish.json", "species", _sub("species"), "nameEs", "nameEn", "pez (pesca y pecera del museo)"),
    FieldSpec("fish.json", "legendary", _sub("legendary"), "nameEs", "nameEn", "captura legendaria (trofeo)"),
    FieldSpec("fuels.json", "levels", _sub("levels"), "nameEs", "nameEn", "nivel de fuego"),
    FieldSpec("fuels.json", "ignition", _sub("ignition"), "nameEs", "nameEn", "forma de encender"),
    FieldSpec("fuels.json", "smeltingLevels", _sub("smeltingLevels"), "nameEs", "nameEn", "horno de fundición"),
    FieldSpec("recipes_smithing.json", "recipes", _sub("recipes"), "nameEs", "nameEn",
              "receta de fundición, chatarra o forja"),
    FieldSpec("mining.json", "layers", _sub("layers"), "nameEs", "nameEn", "capa del subsuelo"),
    FieldSpec("mining.json", "strata", _sub("strata"), "nameEs", "nameEn", "estrato minable"),
    FieldSpec("mining.json", "tools", _sub("tools"), "nameEs", "nameEn", "herramienta de excavación"),
    FieldSpec("mining.json", "places", _sub("places"), "nameEs", "nameEn", "lugar subterráneo"),
    FieldSpec("mining.json", "hazards", _keyed("hazards"), "nameEs", "nameEn", "peligro de la mina"),
    FieldSpec("combat.json", "creatures", _sub("creatures"), "nameEs", "nameEn", "criatura con la que se combate"),
    FieldSpec("recipes.json", "vessels", _sub("vessels"), "nameEs", "nameEn", "recipiente de cocina"),
    FieldSpec("recipes.json", "recipes", _sub("recipes"), "nameEs", "nameEn", "receta de cocina"),
    FieldSpec("ruins.json", "elements", _sub("elements"), "nameEs", "nameEn", "elemento de una ruina"),
    FieldSpec("ruins.json", "sites", _sub("sites"), "nameEs", "nameEn", "tipo de ruina"),
    FieldSpec("ruins.json", "techniques", _sub("techniques"), "nameEs", "nameEn", "técnica de wayfinding"),
    FieldSpec("ruins.json", "techniques", _sub("techniques"), "revealsEs", "revealsEn", "lo que revela una técnica"),
    FieldSpec("fases_futuras.json", "pendingItems", _sub("pendingItems"), "nameEs", "nameEn", "objeto de fase 2/3"),
    FieldSpec("fases_futuras.json", "tramway", _path("tramway", "pieces"), "nameEs", "nameEn", "pieza de raíles [F2]"),
    FieldSpec("fases_futuras.json", "livestock.pieces", _path("livestock", "pieces"), "nameEs", "nameEn", "pieza de granja [F2]"),
    FieldSpec("fases_futuras.json", "livestock.species", _path("livestock", "species"), "nameEs", "nameEn",
              "animal doméstico [F2]"),
    FieldSpec("fases_futuras.json", "defenses.pieces", _path("defenses", "pieces"), "nameEs", "nameEn", "muralla [F2]"),
    FieldSpec("fases_futuras.json", "defenses.traps", _path("defenses", "traps"), "nameEs", "nameEn", "trampa [F2]"),
    FieldSpec("fases_futuras.json", "trade.tiers", _path("trade", "tiers"), "nameEs", "nameEn", "reputación del pueblo [F3]"),
    FieldSpec("fases_futuras.json", "trade.wants", _path("trade", "wants"), "nameEs", "nameEn", "lo que pide el pueblo [F3]"),
    FieldSpec("fases_futuras.json", "trade.favors", _path("trade", "favors"), "nameEs", "nameEn", "favor del pueblo [F3]"),
    FieldSpec("fases_futuras.json", "trade.reputationActions", _path("trade", "reputationActions"), "nameEs", "nameEn",
              "acción que mueve la reputación [F3]"),
    FieldSpec("halden_diaries.json", "entries", _sub("entries"), "textEs", "textEn", "cuaderno de la expedición Halden"),
    FieldSpec("journal_entries.json", "entries", _sub("entries"), "textEs", "textEn",
              "entrada del diario del náufrago ({Day} = día de la partida)"),
    FieldSpec("map_clues.json", "clues", _sub("clues"), "clueEs", "clueEn", "pista en prosa de un tesoro"),
    FieldSpec("museum_collections.json", "collections", _sub("collections"), "nameEs", "nameEn", "colección del museo"),
    FieldSpec("museum_collections.json", "collections", _sub("collections"), "rewardEs", "rewardEn",
              "recompensa por completar la colección"),
    *(FieldSpec(f, "pieces", _sub("pieces"), es, en, purpose)
      for f in ("shells.json", "herbarium.json", "insects.json", "fossils.json", "minerals.json")
      for es, en, purpose in (("nameEs", "nameEn", "etiqueta de vitrina"),
                              ("descriptionEs", "descriptionEn", "ficha de vitrina"))),
]

# Campos «…Es» que no ve el jugador: notas de diseño y documentación interna.
DESIGN_NOTES: set[tuple[str, str]] = {
    ("*", "noteEs"),
    ("*", "redNotaEs"),
    ("exploration.json", "accessEs"),  # guion de diseño de cada lugar, con referencias al GDD
    ("exploration.json", "rewardEs"),
    ("journal_entries.json", "descriptionEs"),  # catálogo de sucesos que disparan el diario
}

# Listas paralelas: la inglesa va en otra clave del mismo objeto, en el mismo orden.
PARALLEL_LISTS: list[tuple[str, str, str, str]] = [
    ("story_es.json", "petroglyph_themes", "petroglyph_themes_en", "motivo de petroglifo"),
]


def _ns(file: str, collection: str) -> str:
    return f"Data.{file.removesuffix('.json')}.{collection}"


def _is_note(file: str, key: str) -> bool:
    return ("*", key) in DESIGN_NOTES or (file, key) in DESIGN_NOTES


def _unregistered(file: str, doc: object, covered: set[tuple[int, str]]) -> Iterator[str]:
    """Campos «…Es» con texto que ninguna FieldSpec declara (quedarían sin comprobar)."""

    def walk(node: object, path: str) -> Iterator[str]:
        if isinstance(node, dict):
            for key, value in node.items():
                if isinstance(value, str):
                    if (key.endswith("Es") and len(key) > 2 and value.strip() and (id(node), key) not in covered
                            and not _is_note(file, key)
                            and not (file == "achievements.json" and path == ".stats[]")):  # documentación del catálogo
                        yield f"{path}.{key}" + (f" «{node.get('id')}»" if node.get("id") else "")
                else:
                    yield from walk(value, f"{path}.{key}")
        elif isinstance(node, list):
            for item in node:
                yield from walk(item, f"{path}[]")

    yield from walk(doc, "")


def extract(data: dict[str, object]) -> tuple[list[DataText], list[str]]:
    """Textos bilingües de los datos y errores estructurales (p. ej. listas desparejas)."""
    texts: list[DataText] = []
    errors: list[str] = []
    covered: set[tuple[int, str]] = set()
    for spec in FIELDS:
        doc = data.get(spec.file)
        if doc is None:
            continue
        for rid, rec in spec.records(doc):
            covered.add((id(rec), spec.es_field))
            es, en = rec.get(spec.es_field), rec.get(spec.en_field)
            if es is None and en is None:
                continue
            if es == "" and not en:
                # Plantilla sin nombre propio: usa el del objeto resultante.
                continue
            texts.append(DataText(spec.file, _ns(spec.file, spec.collection), f"{rid}.{spec.es_field}",
                                  es, en, spec.es_field, spec.en_field, rid))
    for file, es_key, en_key, _purpose in PARALLEL_LISTS:
        doc = data.get(file)
        if not isinstance(doc, dict) or es_key not in doc:
            continue
        es_list, en_list = doc.get(es_key) or [], doc.get(en_key)
        if en_list is not None and len(en_list) != len(es_list):
            errors.append(f"{file}: {en_key} tiene {len(en_list)} entradas y {es_key} {len(es_list)}")
        for i, es in enumerate(es_list):
            en = en_list[i] if en_list is not None and i < len(en_list) else None
            texts.append(DataText(file, _ns(file, es_key), f"{i:02d}", es, en, es_key, en_key, f"{i}"))
    for file, doc in sorted(data.items()):
        for where in _unregistered(file, doc, covered):
            errors.append(f"Content/Data/{file}: {where} no está registrado en Tools/Localization/src/l10n/data.py "
                          "(añade su FieldSpec o, si no lo ve el jugador, a DESIGN_NOTES)")
    return texts, errors
