"""Formato fuente de localización de Unreal: ``Game.manifest`` y ``<cultura>/Game.archive``.

Supuestos (UE 4.14+ hasta 5.6; ver docs/tecnico/localizacion.md):

- Ambos son JSON con tabulaciones. Raíz: ``FormatVersion``, ``Namespace`` (vacío),
  ``Children`` (textos sin espacio de nombres) y ``Subnamespaces`` (uno por espacio de
  nombres, con su ``Namespace`` y sus ``Children``).
- Manifiesto (``FormatVersion`` 1): un hijo por texto fuente distinto dentro del espacio de
  nombres, ``{"Source": {"Text": ...}, "Keys": [{"Key": ..., "Path": ...}]}``; si varias
  claves comparten texto, van juntas en ``Keys``.
- Archivo (``FormatVersion`` 2): un hijo por clave,
  ``{"Source": {"Text": ...}, "Translation": {"Text": ...}, "Key": ...}``. En la cultura
  nativa la traducción es el propio texto fuente.
- Los espacios de nombres no llevan puntos (Unreal no los anida); los del juego son
  ``Explored`` y ``ExploredUI``.
- Unreal escribe estos ficheros en UTF-16 LE con BOM, pero lee también UTF-8; aquí se
  escriben en UTF-8 para que el diff de git sea legible (``--utf16`` imita al editor).
"""

from __future__ import annotations

import json
from collections import defaultdict
from collections.abc import Callable

from .catalogue import Entry

TARGET = "Game"
NATIVE_CULTURE = "es"
CULTURES = ["es", "en"]
MANIFEST_VERSION = 1
ARCHIVE_VERSION = 2


def _path_of(location: str) -> str:
    # "Source/.../X.cpp:22" -> "Source/.../X.cpp - line 22" (como lo escribe GatherText).
    path, _, line = location.rpartition(":")
    return f"{path} - line {line}" if line.isdigit() else location


def _by_namespace(entries: list[Entry]) -> dict[str, list[Entry]]:
    groups: dict[str, list[Entry]] = defaultdict(list)
    for e in entries:
        if "." in e.namespace:
            raise ValueError(f"{e.id}: los espacios de nombres exportados no llevan puntos")
        groups[e.namespace].append(e)
    return groups


def _root(version: int, entries: list[Entry], children_of: Callable[[list[Entry]], list[dict]]) -> dict:
    groups_children = {ns: children_of(group) for ns, group in _by_namespace(entries).items()}
    return {
        "FormatVersion": version,
        "Namespace": "",
        "Children": groups_children.pop("", []),
        "Subnamespaces": [{"Namespace": ns, "Children": groups_children[ns]} for ns in sorted(groups_children)],
    }


def manifest(entries: list[Entry]) -> dict:
    def children(ns_entries: list[Entry]) -> list[dict]:
        by_source: dict[str, list[Entry]] = defaultdict(list)
        for e in ns_entries:
            by_source[e.es].append(e)
        out = []
        for source in sorted(by_source):
            keys = sorted(by_source[source], key=lambda e: e.key)
            out.append({"Source": {"Text": source},
                        "Keys": [{"Key": e.key, "Path": _path_of(e.locations[0])} for e in keys]})
        return out

    return _root(MANIFEST_VERSION, entries, children)


def archive(entries: list[Entry], culture: str) -> dict:
    def children(ns_entries: list[Entry]) -> list[dict]:
        out = []
        for e in sorted(ns_entries, key=lambda e: e.key):
            translation = e.es if culture == NATIVE_CULTURE else (e.en or "")
            out.append({"Source": {"Text": e.es}, "Translation": {"Text": translation}, "Key": e.key})
        return out

    return _root(ARCHIVE_VERSION, entries, children)


def dumps(doc: dict) -> str:
    return json.dumps(doc, ensure_ascii=False, indent="\t") + "\n"


def encode(text: str, utf16: bool) -> bytes:
    return ("﻿" + text).encode("utf-16-le") if utf16 else text.encode("utf-8")


def outputs(entries: list[Entry]) -> dict[str, str]:
    """Ficheros relativos a la raíz del repo -> contenido."""
    base = f"Content/Localization/{TARGET}"
    files = {f"{base}/{TARGET}.manifest": dumps(manifest(entries))}
    for culture in CULTURES:
        files[f"{base}/{culture}/{TARGET}.archive"] = dumps(archive(entries, culture))
    return files
