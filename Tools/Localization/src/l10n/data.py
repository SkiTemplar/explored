"""Campos bilingües de Content/Data/*.json.

Cada texto de datos que ve el jugador tiene su campo en español (fuente) y su pareja en
inglés al lado (``nameEs``/``nameEn``, ``label``/``labelEn``...). El juego elige uno u
otro según la cultura activa (``ExploredLocalization::Pick`` en C++).
"""

from __future__ import annotations

from collections.abc import Callable, Iterator
from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class DataText:
    file: str
    namespace: str  # "Data.<fichero sin .json>.<colección>"
    key: str  # "<id>.<campo>"
    es: Any  # str o None en datos sanos; el catálogo marca como error cualquier otro tipo
    en: Any
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
    records: Callable[[object], Iterator[tuple[str, object]]]
    es_field: str
    en_field: str
    purpose: str  # para el informe


def _id(rec: object, index: int) -> str:
    # Un registro que no es un objeto no tiene «id»: se nombra por su posición para el error.
    return str(rec.get("id")) if isinstance(rec, dict) else f"#{index}"


def _items(doc: object, name: str | None = None) -> list[object]:
    if name is not None:
        doc = doc.get(name, []) if isinstance(doc, dict) else []
    return doc if isinstance(doc, list) else []


def _list(doc: object) -> Iterator[tuple[str, object]]:
    for i, rec in enumerate(_items(doc)):
        yield _id(rec, i), rec


def _sub(name: str) -> Callable[[object], Iterator[tuple[str, object]]]:
    def records(doc: object) -> Iterator[tuple[str, object]]:
        for i, rec in enumerate(_items(doc, name)):
            yield _id(rec, i), rec

    return records


def _stages(doc: object) -> Iterator[tuple[str, object]]:
    for i, plant in enumerate(_items(doc, "plants")):
        if not isinstance(plant, dict):
            yield _id(plant, i), plant
            continue
        for j, stage in enumerate(_items(plant, "stages")):
            yield f"{plant.get('id')}.{_id(stage, j)}", stage


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
    FieldSpec("ruins.json", "techniques", _sub("techniques"), "nameEs", "nameEn", "técnica de wayfinding"),
    FieldSpec("ruins.json", "techniques", _sub("techniques"), "revealsEs", "revealsEn", "qué revela la técnica"),
    FieldSpec("ruins.json", "elements", _sub("elements"), "nameEs", "nameEn", "elemento de ruina"),
    FieldSpec("ruins.json", "sites", _sub("sites"), "nameEs", "nameEn", "nombre de la ruina"),
    FieldSpec("fish.json", "species", _sub("species"), "nameEs", "nameEn", "pez (pecera del museo)"),
    FieldSpec("fish.json", "legendary", _sub("legendary"), "nameEs", "nameEn", "captura legendaria"),
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

# Listas paralelas: la inglesa va en otra clave del mismo objeto, en el mismo orden.
PARALLEL_LISTS: list[tuple[str, str, str, str]] = [
    ("story_es.json", "petroglyph_themes", "petroglyph_themes_en", "motivo de petroglifo"),
]


def _ns(file: str, collection: str) -> str:
    return f"Data.{file.removesuffix('.json')}.{collection}"


def extract(data: dict[str, object]) -> tuple[list[DataText], list[str]]:
    """Textos bilingües de los datos y errores estructurales (p. ej. listas desparejas)."""
    texts: list[DataText] = []
    errors: list[str] = []
    for spec in FIELDS:
        doc = data.get(spec.file)
        if doc is None:
            continue
        for rid, rec in spec.records(doc):
            if not isinstance(rec, dict):
                errors.append(f"Content/Data/{spec.file}: {spec.collection} {rid} no es un objeto")
                continue
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
        if not isinstance(es_list, list) or not (en_list is None or isinstance(en_list, list)):
            errors.append(f"{file}: {es_key} y {en_key} deben ser listas")
            continue
        if en_list is not None and len(en_list) != len(es_list):
            errors.append(f"{file}: {en_key} tiene {len(en_list)} entradas y {es_key} {len(es_list)}")
        for i, es in enumerate(es_list):
            en = en_list[i] if en_list is not None and i < len(en_list) else None
            texts.append(DataText(file, _ns(file, es_key), f"{i:02d}", es, en, es_key, en_key, f"{i}"))
    return texts, errors
