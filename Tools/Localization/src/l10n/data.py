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
    return texts, errors
