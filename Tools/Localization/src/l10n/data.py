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


def _path(*keys: str) -> Callable[[object], Iterator[tuple[str, object]]]:
    """Lista anidada en objetos: ``_path("trade", "wants")`` → doc["trade"]["wants"]."""

    def records(doc: object) -> Iterator[tuple[str, object]]:
        node = doc
        for key in keys:
            node = node.get(key, {}) if isinstance(node, dict) else {}
        for i, rec in enumerate(_items(node)):
            yield _id(rec, i), rec

    return records


def _keyed(name: str) -> Callable[[object], Iterator[tuple[str, object]]]:
    """Objeto indexado por id: ``{"derrumbe": {...}, "oscuridad": {...}}`` (las notas sueltas se saltan)."""

    def records(doc: object) -> Iterator[tuple[str, object]]:
        node = doc.get(name, {}) if isinstance(doc, dict) else {}
        for key, rec in (node.items() if isinstance(node, dict) else []):
            if isinstance(rec, dict):
                yield str(key), rec

    return records


FIELDS: list[FieldSpec] = [
    FieldSpec("items.json", "items", _list, "nameEs", "nameEn", "nombre del objeto (mano, suelo, mochila)"),
        FieldSpec("verbs.json", "verbs", _list, "nameEs", "nameEn", "verbo de fabricación"),
    FieldSpec("verbs.json", "verbs", _list, "description", "descriptionEn", "ayuda del verbo"),
    FieldSpec("survival_needs.json", "needs", _sub("needs"), "nameEs", "nameEn", "barra del cuerpo (HUD)"),
    FieldSpec("boats.json", "boats", _sub("boats"), "nameEs", "nameEn", "embarcación del astillero"),
    FieldSpec("fauna.json", "species", _sub("species"), "nameEs", "nameEn", "especie marina"),
    FieldSpec("fauna.json", "lod", _path("lod", "tiers"), "nameEs", "nameEn", "nivel de detalle de fauna (ajustes)"),
    FieldSpec("fauna_terrestre.json", "species", _sub("species"), "nameEs", "nameEn", "especie terrestre"),
    FieldSpec("recipes.json", "recipes", _sub("recipes"), "nameEs", "nameEn", "receta (objeto + verbo, GDD v3 §5.6)"),
    FieldSpec("notebook.json", "pages", _sub("pages"), "textEs", "textEn", "página del cuaderno de Roig (GDD v3 §2.1)"),
    FieldSpec("notebook.json", "pages", _sub("pages"), "iriaEs", "iriaEn", "nota de Iria en la página (GDD v3 §2.1)"),
    FieldSpec("notebook.json", "cards", _sub("cards"), "textEs", "textEn", "cartel de inicio (GDD v3 §2)"),
    FieldSpec("notebook.json", "epilogue", _sub("epilogue"), "textEs", "textEn", "línea del epílogo (GDD v3 §2)"),
    FieldSpec("objectives.json", "objectives", _sub("objectives"), "textEs", "textEn", "objetivo del HUD (GDD v3 §3)"),
    FieldSpec("thoughts.json", "thoughts", _sub("thoughts"), "textEs", "textEn", "pensamiento de Iria (GDD v3 §2.2)"),
]

# Campos «…Es» que no ve el jugador: notas de diseño y documentación interna.
DESIGN_NOTES: set[tuple[str, str]] = {
    ("*", "noteEs"),
    ("*", "redNotaEs"),
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
                            and not _is_note(file, key)):
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
            if not isinstance(rec, dict):
                errors.append(f"Content/Data/{spec.file}: {spec.collection} {rid} no es un objeto")
                continue
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
        if not isinstance(es_list, list) or not (en_list is None or isinstance(en_list, list)):
            errors.append(f"{file}: {es_key} y {en_key} deben ser listas")
            continue
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
