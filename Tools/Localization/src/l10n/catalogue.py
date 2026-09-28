"""Catálogo único de textos ES/EN y sus comprobaciones."""

from __future__ import annotations

import copy
import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from . import cpp, data

REPO_ROOT = Path(__file__).resolve().parents[4]
TOOL_ROOT = Path(__file__).resolve().parents[2]
TRANSLATIONS = TOOL_ROOT / "translations" / "en.json"

# Dónde busca textos el C++ (lo mismo que Config/Localization/Game_Gather.ini).
CPP_ROOTS = ["Source/Explored"]
CPP_EXCLUDE = ("Source/Explored/Tests/",)
INI_ROOTS = ["Config"]
INI_EXCLUDE = ("Config/Localization/",)

DATA_FILES = [
    "items.json", "templates.json", "verbs.json", "story_es.json", "plants.json",
    "building_pieces.json", "survival_needs.json", "ruins.json",
    "fish.json", "halden_diaries.json", "journal_entries.json", "map_clues.json", "museum_collections.json",
    "shells.json", "herbarium.json", "insects.json", "fossils.json", "minerals.json",
]

# El inglés suele ser más corto que el español; si sale bastante más largo, puede no caber
# en un botón o una fila de ajustes pensados para el texto español.
LENGTH_RATIO = 1.3
LENGTH_MIN = 12
DEDICATION = "Para Almudena, mi Limón"
PLACEHOLDER_RE = re.compile(r"\{([A-Za-z0-9_]+)\}")


@dataclass
class Entry:
    namespace: str
    key: str
    es: str | None
    en: str | None
    origin: str  # "cpp", "ini", "datos" o "pendiente"
    locations: list[str] = field(default_factory=list)
    note: str = ""

    @property
    def id(self) -> str:
        return f"{self.namespace},{self.key}"

    def to_json(self) -> dict:
        doc = {"namespace": self.namespace, "key": self.key, "es": self.es, "en": self.en,
               "origin": self.origin, "locations": self.locations}
        if self.note:
            doc["note"] = self.note
        return doc


@dataclass
class Report:
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    info: list[str] = field(default_factory=list)


@dataclass
class Sources:
    """Todo lo que lee la herramienta; los tests lo copian y lo modifican."""

    loctexts: list[cpp.LocText]
    literals: list[cpp.Literal]
    data: dict[str, Any]  # JSON tal cual: la estructura se comprueba al construir el catálogo
    translations: dict[str, Any]
    repo_root: Path = REPO_ROOT
    problems: list[str] = field(default_factory=list)  # p. ej. claves repetidas en un JSON

    @classmethod
    def load(cls, repo_root: Path = REPO_ROOT, translations_path: Path | None = None) -> Sources:
        # Se resuelve al llamar (no al definir) para que los tests puedan apuntar a otro en.json.
        translations_path = translations_path or TRANSLATIONS
        problems: list[str] = []
        loctexts: list[cpp.LocText] = []
        literals: list[cpp.Literal] = []
        for path, rel in cpp.iter_sources(repo_root, CPP_ROOTS, (".h", ".cpp"), CPP_EXCLUDE):
            code = path.read_text(encoding="utf-8")
            loctexts += cpp.extract_loctext(code, rel)
            literals += cpp.scan_literals(code, rel)
        for path, rel in cpp.iter_sources(repo_root, INI_ROOTS, (".ini",), INI_EXCLUDE):
            loctexts += cpp.extract_loctext(path.read_text(encoding="utf-8"), rel)
        docs: dict[str, Any] = {}
        for name in DATA_FILES:
            p = repo_root / "Content" / "Data" / name
            if p.exists():
                docs[name] = load_json(p, f"Content/Data/{name}", problems)
        translations: Any = {}
        if translations_path.exists():
            translations = load_json(translations_path, "translations/en.json", problems)
        if not isinstance(translations, dict):
            raise ValueError("translations/en.json: la raíz debe ser un objeto {espacio: {clave: {es, en}}}")
        return cls(loctexts, literals, docs, translations, repo_root, problems)

    def copy(self) -> Sources:
        return copy.deepcopy(self)


@dataclass
class Catalogue:
    entries: list[Entry]
    literals: list[cpp.Literal]
    report: Report

    @property
    def exported(self) -> list[Entry]:
        """Lo que va al manifiesto de Unreal: los textos del C++ y de los .ini."""
        return [e for e in self.entries if e.origin in ("cpp", "ini")]

    def stats(self) -> dict[str, int]:
        live = [e for e in self.entries if e.origin != "pendiente"]
        return {
            "total": len(live),
            "cpp": sum(e.origin in ("cpp", "ini") for e in live),
            "datos": sum(e.origin == "datos" for e in live),
            "pendientes": sum(e.origin == "pendiente" for e in self.entries),
            "sin_en": sum(not (e.en or "").strip() for e in live),
            "literales": sum(lit.category == "literal" for lit in self.literals),
            "invariantes": sum(lit.category == "invariante" for lit in self.literals),
            "revisar": sum(lit.category == "revisar" for lit in self.literals),
        }


def load_json(path: Path, label: str, problems: list[str]) -> Any:
    """Lee un JSON avisando de las claves repetidas (``json`` se queda con la última sin decir nada)."""

    def pairs(items: list[tuple[str, Any]]) -> dict[str, Any]:
        doc: dict[str, Any] = {}
        for key, value in items:
            if key in doc:
                problems.append(f"{label}: clave «{key}» repetida en el mismo objeto; solo cuenta la última")
            doc[key] = value
        return doc

    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=pairs)
    except json.JSONDecodeError as exc:
        raise ValueError(f"{label}: JSON mal formado ({exc})") from exc


def _valid_translations(raw: dict[str, Any], r: Report) -> dict[str, dict[str, dict[str, str]]]:
    """Las entradas de translations/en.json bien formadas; las demás, como errores."""
    out: dict[str, dict[str, dict[str, str]]] = {}
    for ns, keys in raw.items():
        if not isinstance(keys, dict):
            r.errors.append(f"translations/en.json: {ns} debe ser un objeto {{clave: {{es, en}}}}")
            continue
        out[ns] = {}
        for key, tr in keys.items():
            if not isinstance(tr, dict):
                r.errors.append(f"translations/en.json: {ns},{key} debe ser un objeto {{es, en}}")
                continue
            bad = sorted(f for f, v in tr.items() if not isinstance(v, str))
            if bad:
                r.errors.append(f"translations/en.json: {ns},{key}: {', '.join(bad)} debe ser texto")
                continue
            out[ns][key] = tr
    return out


def placeholders(text: str | None) -> set[str]:
    return set(PLACEHOLDER_RE.findall(text or ""))


def build(src: Sources) -> Catalogue:
    r = Report()
    entries: list[Entry] = []
    by_id: dict[str, Entry] = {}
    r.errors += src.problems
    translations = _valid_translations(src.translations, r)

    # --- C++ e .ini: el español está en el código; el inglés, en translations/en.json.
    for t in src.loctexts:
        eid = f"{t.namespace},{t.key}"
        where = f"{t.path}:{t.line}"
        if eid in by_id:
            prev = by_id[eid]
            if prev.es != t.source:
                r.errors.append(f"{where}: la clave {eid} ya tiene otro texto en {prev.locations[0]} "
                                f"(«{prev.es}» frente a «{t.source}»); Unreal la trataría como conflicto")
            prev.locations.append(where)
            continue
        origin = "ini" if t.path.endswith(".ini") else "cpp"
        tr = translations.get(t.namespace, {}).get(t.key)
        en = None
        if tr is not None:
            en = tr.get("en")
            if tr.get("es") != t.source:
                r.errors.append(f"{where}: traducción desfasada de {eid}: el código dice «{t.source}» "
                                f"y translations/en.json traduce «{tr.get('es')}»")
            if tr.get("pendiente"):
                r.warnings.append(f"{eid}: ya está en el código ({where}); quita «pendiente» de translations/en.json")
        e = Entry(t.namespace, t.key, t.source, en, origin, [where], (tr or {}).get("nota", ""))
        by_id[eid] = e
        entries.append(e)

    # --- Traducciones sin código: pendientes de integrar (documentadas) o huérfanas.
    for ns, keys in sorted(translations.items()):
        for key, tr in sorted(keys.items()):
            eid = f"{ns},{key}"
            if eid in by_id:
                continue
            if tr.get("pendiente"):
                entries.append(Entry(ns, key, tr.get("es"), tr.get("en"), "pendiente", [tr["pendiente"]], tr.get("nota", "")))
            else:
                r.warnings.append(f"translations/en.json: {eid} no aparece en el código (¿clave renombrada o borrada?)")

    # --- Datos: los dos idiomas están en el propio JSON.
    texts, errors = data.extract(src.data)
    r.errors += errors
    for t in texts:
        e = Entry(t.namespace, t.key, t.es, t.en, "datos", [t.where])
        if e.id in by_id:
            r.errors.append(f"{e.locations[0]}: id repetido {e.id}")
            continue
        by_id[e.id] = e
        entries.append(e)

    for e in entries:
        _check_entry(e, r)

    lits = src.literals
    for lit in lits:
        if lit.category == "literal":
            r.warnings.append(f"{lit.path}:{lit.line}: literal sin localizar «{lit.text}»")
    r.info.append(f"{len(entries)} textos ({sum(e.origin == 'pendiente' for e in entries)} pendientes de integrar)")
    return Catalogue(entries, lits, r)


def _check_entry(e: Entry, r: Report) -> None:
    where = e.locations[0] if e.locations else e.id
    # Los datos son JSON sin esquema: un número o una lista en lugar de texto no debe romper la herramienta.
    for lang, value in (("es", e.es), ("en", e.en)):
        if value is not None and not isinstance(value, str):
            r.errors.append(f"{where}: {e.id}: el texto «{lang}» debe ser una cadena, no {type(value).__name__}")
            return
    es, en = e.es or "", e.en or ""
    if not es.strip():
        r.errors.append(f"{where}: {e.id} sin texto en español")
        return
    if not en.strip():
        r.errors.append(f"{where}: {e.id} sin inglés («{es}»)")
        return
    if placeholders(es) != placeholders(en):
        r.errors.append(f"{where}: {e.id}: marcadores distintos {sorted(placeholders(es))} (ES) "
                        f"y {sorted(placeholders(en))} (EN)")
    if DEDICATION in es and en != es:
        r.errors.append(f"{where}: {e.id}: la dedicatoria «{DEDICATION}» no se traduce")
    edges_es = (es[:1].isspace(), es[-1:].isspace())
    edges_en = (en[:1].isspace(), en[-1:].isspace())
    if edges_es != edges_en:
        r.warnings.append(f"{where}: {e.id}: espacios al principio o al final distintos entre ES y EN")
    if len(en) >= LENGTH_MIN and len(en) > LENGTH_RATIO * len(es):
        r.warnings.append(f"{where}: {e.id}: el inglés ({len(en)} car.) es más de {LENGTH_RATIO:.1f}× "
                          f"el español ({len(es)}); comprueba que cabe")
