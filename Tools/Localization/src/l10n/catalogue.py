"""Catálogo único de textos ES/EN y sus comprobaciones."""

from __future__ import annotations

import copy
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

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
    "building_pieces.json", "survival_needs.json",
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
    data: dict[str, object]
    translations: dict[str, dict[str, dict]]
    repo_root: Path = REPO_ROOT

    @classmethod
    def load(cls, repo_root: Path = REPO_ROOT, translations_path: Path = TRANSLATIONS) -> "Sources":
        loctexts: list[cpp.LocText] = []
        literals: list[cpp.Literal] = []
        for path, rel in cpp.iter_sources(repo_root, CPP_ROOTS, (".h", ".cpp"), CPP_EXCLUDE):
            code = path.read_text(encoding="utf-8")
            loctexts += cpp.extract_loctext(code, rel)
            literals += cpp.scan_literals(code, rel)
        for path, rel in cpp.iter_sources(repo_root, INI_ROOTS, (".ini",), INI_EXCLUDE):
            loctexts += cpp.extract_loctext(path.read_text(encoding="utf-8"), rel)
        docs = {}
        for name in DATA_FILES:
            p = repo_root / "Content" / "Data" / name
            if p.exists():
                docs[name] = json.loads(p.read_text(encoding="utf-8"))
        translations = json.loads(translations_path.read_text(encoding="utf-8")) if translations_path.exists() else {}
        return cls(loctexts, literals, docs, translations, repo_root)

    def copy(self) -> "Sources":
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


def placeholders(text: str | None) -> set[str]:
    return set(PLACEHOLDER_RE.findall(text or ""))


def build(src: Sources) -> Catalogue:
    r = Report()
    entries: list[Entry] = []
    by_id: dict[str, Entry] = {}

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
        tr = src.translations.get(t.namespace, {}).get(t.key)
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
    for ns, keys in sorted(src.translations.items()):
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
    if not (e.es or "").strip():
        r.errors.append(f"{where}: {e.id} sin texto en español")
        return
    if not (e.en or "").strip():
        r.errors.append(f"{where}: {e.id} sin inglés («{e.es}»)")
        return
    if placeholders(e.es) != placeholders(e.en):
        r.errors.append(f"{where}: {e.id}: marcadores distintos {sorted(placeholders(e.es))} (ES) "
                        f"y {sorted(placeholders(e.en))} (EN)")
    if DEDICATION in e.es and e.en != e.es:
        r.errors.append(f"{where}: {e.id}: la dedicatoria «{DEDICATION}» no se traduce")
    if e.es != e.es.strip() or e.en != e.en.strip():
        if (e.es[:1].isspace(), e.es[-1:].isspace()) != (e.en[:1].isspace(), e.en[-1:].isspace()):
            r.warnings.append(f"{where}: {e.id}: espacios al principio o al final distintos entre ES y EN")
    if len(e.en) >= LENGTH_MIN and len(e.en) > LENGTH_RATIO * len(e.es):
        r.warnings.append(f"{where}: {e.id}: el inglés ({len(e.en)} car.) es más de {LENGTH_RATIO:.1f}× "
                          f"el español ({len(e.es)}); comprueba que cabe")
