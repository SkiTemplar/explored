"""Textos de la historia de Baliza Siete (GDD v3 §2, §2.1, §2.2 y §3).

- ``notebook.json``: las 8 páginas del cuaderno (una por número, ids ``cuaderno_pN``), solo la 1 y
  la 8 críticas, texto de Roig de 70 palabras como mucho y nota de Iria de 20; cartel de inicio y
  las tres líneas del epílogo.
- ``objectives.json``: los ids que devuelve ``FBeaconProgressModel::CurrentObjective``, una línea
  de 60 caracteres como mucho y una etapa de ``EBeaconStage``.
- ``thoughts.json``: los 12 pensamientos de ``EIriaThought`` con su id, de 20 palabras como mucho.

Todos los textos llevan español e inglés.
"""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

PAGES = 8
CRITICAL_PAGES = {1, 8}
PAGE_MAX_WORDS = 70
NOTE_MAX_WORDS = 20
OBJECTIVE_MAX_CHARS = 60

STAGES = {
    "Prologo", "Orilla", "Faro", "Balsa", "Carguero", "LenteEnMano", "Ciclon", "Preparacion",
    "LinternaMontada", "Final",
}
# Ids que puede devolver FBeaconProgressModel::CurrentObjective (Narrative/BeaconProgressModel.cpp).
OBJECTIVES = {
    "obj_bebe_coco", "obj_haz_fuego", "obj_pesca_cocina", "obj_sube_faro", "obj_lee_cuaderno",
    "obj_duerme_refugio", "obj_prepara_balsa", "obj_cruza_bajo", "obj_encuentra_lente",
    "obj_vuelve_antes_cierre", "obj_refugiate_faro", "obj_sobrevive_noche", "obj_recupera_lente",
    "obj_prepara_aceite", "obj_trenza_mecha", "obj_monta_linterna", "obj_enciende_baliza",
}
# EIriaThought → id de FBeaconDirectorModel::ThoughtTextId (Narrative/BeaconDirectorModel.cpp).
THOUGHTS = {
    "FirstMove": "pensamiento_primer_movimiento",
    "SeeLighthouse": "pensamiento_ver_faro",
    "FirstFire": "pensamiento_primera_hoguera",
    "EnterHouse": "pensamiento_entrar_casa",
    "FirstNight": "pensamiento_primera_noche",
    "BoardRaft": "pensamiento_subir_balsa",
    "SeeFreighter": "pensamiento_ver_carguero",
    "TakeLens": "pensamiento_coger_lente",
    "BarometerLow": "pensamiento_barometro_bajo",
    "CycloneEye": "pensamiento_ojo_ciclon",
    "MountLantern": "pensamiento_montar_linterna",
    "LightBeacon": "pensamiento_encender",
}
EPILOGUE = {"epilogo_completo", "epilogo_parcial", "epilogo_comun"}
CARDS = {"cartel_inicio"}

Error = Callable[[str], None]


def _records(doc: Any, key: str) -> list[dict[str, Any]]:
    seq = doc.get(key) if isinstance(doc, dict) else None
    return [r for r in seq if isinstance(r, dict)] if isinstance(seq, list) else []


def _words(text: Any) -> int:
    return len(text.split()) if isinstance(text, str) else 0


def _bilingual(rec: dict[str, Any], where: str, es: str, en: str, error: Error) -> None:
    for field in (es, en):
        value = rec.get(field)
        if not isinstance(value, str) or not value.strip():
            error(f"{where}: falta «{field}»")


def _ids(records: list[dict[str, Any]], file: str, expected: set[str], error: Error) -> None:
    ids = [r.get("id") for r in records]
    for iid in sorted({i for i in ids if isinstance(i, str) and ids.count(i) > 1}):
        error(f"{file}: id «{iid}» repetido")
    present = {i for i in ids if isinstance(i, str)}
    for iid in sorted(expected - present):
        error(f"{file}: falta «{iid}»")
    for iid in sorted(present - expected):
        error(f"{file}: «{iid}» no lo usa el juego")


def _check_page(rec: dict[str, Any], error: Error) -> int | None:
    page = rec.get("page")
    if not isinstance(page, int) or isinstance(page, bool) or not 1 <= page <= PAGES:
        error(f"notebook.json «{rec.get('id')}»: «page» debe ser 1-{PAGES}")
        return None
    where = f"notebook.json página {page}"
    if rec.get("id") != f"cuaderno_p{page}":
        error(f"{where}: el id debe ser «cuaderno_p{page}»")
    if rec.get("critical") is not (page in CRITICAL_PAGES):
        error(f"{where}: solo las páginas 1 y 8 son críticas")
    _bilingual(rec, where, "textEs", "textEn", error)
    _bilingual(rec, where, "iriaEs", "iriaEn", error)
    if _words(rec.get("textEs")) > PAGE_MAX_WORDS:
        error(f"{where}: el texto de Roig pasa de {PAGE_MAX_WORDS} palabras")
    if _words(rec.get("iriaEs")) > NOTE_MAX_WORDS:
        error(f"{where}: la nota de Iria pasa de {NOTE_MAX_WORDS} palabras")
    return page


def check_notebook(doc: Any, error: Error) -> None:
    pages = _records(doc, "pages")
    numbers = [n for n in (_check_page(rec, error) for rec in pages) if n is not None]
    if sorted(numbers) != list(range(1, PAGES + 1)):
        error(f"notebook.json: tiene que haber una página por número del 1 al {PAGES}")
    for key, expected in (("epilogue", EPILOGUE), ("cards", CARDS)):
        records = _records(doc, key)
        _ids(records, f"notebook.json/{key}", expected, error)
        for rec in records:
            _bilingual(rec, f"notebook.json «{rec.get('id')}»", "textEs", "textEn", error)


def check_objectives(doc: Any, error: Error) -> None:
    records = _records(doc, "objectives")
    _ids(records, "objectives.json", OBJECTIVES, error)
    for rec in records:
        where = f"objectives.json «{rec.get('id')}»"
        _bilingual(rec, where, "textEs", "textEn", error)
        if rec.get("stage") not in STAGES:
            error(f"{where}: etapa «{rec.get('stage')}» desconocida")
        for field in ("textEs", "textEn"):
            text = rec.get(field)
            if isinstance(text, str) and len(text) > OBJECTIVE_MAX_CHARS:
                error(f"{where}: {field} pasa de {OBJECTIVE_MAX_CHARS} caracteres")


def check_thoughts(doc: Any, error: Error) -> None:
    records = _records(doc, "thoughts")
    _ids(records, "thoughts.json", set(THOUGHTS.values()), error)
    seconds = doc.get("subtitleSeconds") if isinstance(doc, dict) else None
    if not isinstance(seconds, (int, float)) or isinstance(seconds, bool) or seconds <= 0:
        error("thoughts.json: «subtitleSeconds» debe ser un número positivo")
    for rec in records:
        where = f"thoughts.json «{rec.get('id')}»"
        trigger = rec.get("trigger")
        if not isinstance(trigger, str) or THOUGHTS.get(trigger) != rec.get("id"):
            error(f"{where}: el disparador «{trigger}» no corresponde a este id")
        _bilingual(rec, where, "textEs", "textEn", error)
        if _words(rec.get("textEs")) > NOTE_MAX_WORDS:
            error(f"{where}: pasa de {NOTE_MAX_WORDS} palabras")


def check_story(data: dict[str, Any], error: Error) -> None:
    """Los tres ficheros de la historia (los que falten los informa check_files_present)."""
    if "notebook.json" in data:
        check_notebook(data["notebook.json"], error)
    if "objectives.json" in data:
        check_objectives(data["objectives.json"], error)
    if "thoughts.json" in data:
        check_thoughts(data["thoughts.json"], error)
