"""Glosario ES/EN (docs/tecnico/glosario.md, biblia 07 §5.2) como regla comprobable.

Cada fila con comprobación dice: si el español casa con esta expresión, el inglés tiene que
llevar esta otra. Así un nombre propio («Isla del Humo» → «Smoke Island») o una decisión de
traducción («galería» → «tunnel») no se traduce de dos formas en dos sitios.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

GLOSSARY = Path("docs") / "tecnico" / "glosario.md"
_ROW = re.compile(r"^\|(.+)\|\s*$")
_RULE = re.compile(r"^`(.+)`\s*→\s*`(.+)`$")


@dataclass(frozen=True)
class Term:
    es: str
    en: str
    note: str
    es_pattern: re.Pattern | None = None
    en_required: str | None = None


def parse(text: str) -> tuple[list[Term], list[str]]:
    """Filas de la tabla de términos y errores de formato."""
    terms: list[Term] = []
    errors: list[str] = []
    in_table = False
    for n, line in enumerate(text.splitlines(), 1):
        m = _ROW.match(line.strip())
        if not m:
            in_table = False
            continue
        cells = [c.strip() for c in m.group(1).split("|")]
        if cells[:2] == ["Español", "Inglés"]:
            in_table = True
            continue
        if not in_table or set("".join(cells)) <= set("-: "):
            continue
        if len(cells) != 4:
            errors.append(f"{GLOSSARY.as_posix()}:{n}: la fila tiene {len(cells)} columnas y deben ser 4")
            continue
        es, en, note, rule = cells
        if not es or not en:
            errors.append(f"{GLOSSARY.as_posix()}:{n}: término sin español o sin inglés")
            continue
        pattern = required = None
        if rule not in ("—", "-", ""):
            rm = _RULE.match(rule)
            if not rm:
                errors.append(f"{GLOSSARY.as_posix()}:{n}: comprobación «{rule}» no tiene la forma `es` → `en`")
            else:
                try:
                    pattern = re.compile(rm.group(1))
                    required = rm.group(2)
                except re.error as exc:
                    errors.append(f"{GLOSSARY.as_posix()}:{n}: expresión «{rm.group(1)}» no válida ({exc})")
        terms.append(Term(es, en, note, pattern, required))
    return terms, errors


def load(repo_root: Path) -> tuple[list[Term], list[str]]:
    path = repo_root / GLOSSARY
    if not path.exists():
        return [], [f"Falta {GLOSSARY.as_posix()} (glosario de biblia 07 §5.2)"]
    return parse(path.read_text(encoding="utf-8"))


def check(terms: list[Term], es: str, en: str) -> list[str]:
    """Términos del glosario presentes en el español que el inglés no respeta."""
    out = []
    for t in terms:
        if t.es_pattern is None or t.en_required is None:
            continue
        m = t.es_pattern.search(es)
        if m and t.en_required.lower() not in en.lower():
            out.append(f"«{m.group(0)}» se traduce «{t.en_required}» según el glosario ({t.es} → {t.en})")
    return out
