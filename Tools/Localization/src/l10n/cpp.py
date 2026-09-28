"""Lectura del C++ (y de los .ini): textos LOCTEXT/NSLOCTEXT y literales sin localizar.

No es un compilador: tokeniza lo justo (comentarios, cadenas) para no confundir un
``//`` dentro de una cadena con un comentario, y trabaja por sentencias para decidir si un
``TEXT("...")`` acaba en pantalla o solo en el registro.
"""

from __future__ import annotations

import re
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path

# Cadena de C++ sin prefijo (NSLOCTEXT y LOCTEXT usan literales normales).
_STR = r'"((?:[^"\\\n]|\\.)*)"'
NSLOCTEXT_RE = re.compile(rf"\bNSLOCTEXT\s*\(\s*{_STR}\s*,\s*{_STR}\s*,\s*{_STR}\s*\)")
LOCTEXT_RE = re.compile(rf"(?<![A-Za-z_])LOCTEXT\s*\(\s*{_STR}\s*,\s*{_STR}\s*\)")
NAMESPACE_DEFINE_RE = re.compile(rf"#\s*define\s+LOCTEXT_NAMESPACE\s+{_STR}")
NAMESPACE_UNDEF_RE = re.compile(r"#\s*undef\s+LOCTEXT_NAMESPACE\b")
TEXT_RE = re.compile(rf"\bTEXT\s*\(\s*{_STR}\s*\)")
# Cualquier invocación de las macros, para detectar las que las expresiones de arriba no saben leer.
ANY_LOC_MACRO_RE = re.compile(r"(?<![A-Za-z_])(?:NS)?LOCTEXT\s*\(")
_HEX = re.compile(r"[0-9A-Fa-f]+")

# Sentencias cuyo texto no llega al jugador (registro, asserts, errores internos, rutas).
EXCLUDED_TOKENS = (
    "UE_LOG", "UE_CLOG", "checkf", "ensureMsgf", "ensureAlwaysMsgf", "OutError", "LogTemp",
    "FSoftObjectPath", "CreateDefaultSubobject", "FindConsoleVariable", "FParse::",
    "SetCollisionProfileName", "MakeAction", "NewObject", "AddError", "AddWarning", "AddInfo",
)
# Sentencias que llevan el texto a la pantalla.
UI_SINKS = (
    "FText::FromString", "FText::AsCultureInvariant", "DrawText", "Prompt", ".Text(", "SetText",
    "ToolTip", "Label", "OutVerbs", "Title", "Message",
)
IGNORE_MARKER = "loc: ignorar"


@dataclass(frozen=True)
class LocText:
    namespace: str
    key: str
    source: str
    path: str  # relativo a la raíz del repo
    line: int


@dataclass(frozen=True)
class Literal:
    path: str
    line: int
    text: str
    category: str  # "literal" (a convertir), "invariante" (sin letras) o "revisar"
    context: str


def unescape(s: str) -> str:
    """Deshace los escapes de un literal de C++ (los que usan los textos del juego)."""
    out: list[str] = []
    i = 0
    simple = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "'": "'", "\\": "\\", "0": "\0"}
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            nxt = s[i + 1]
            if nxt in simple:
                out.append(simple[nxt])
                i += 2
                continue
            if nxt in "uU":
                width = 4 if nxt == "u" else 8
                digits = s[i + 2 : i + 2 + width]
                if len(digits) == width and _HEX.fullmatch(digits):
                    out.append(chr(int(digits, 16)))
                    i += 2 + width
                    continue
            if nxt == "x":
                # En C++ \x se come todos los dígitos hexadecimales que siguen.
                m = _HEX.match(s, i + 2)
                if m:
                    out.append(chr(int(m.group(), 16)))
                    i = m.end()
                    continue
        out.append(c)
        i += 1
    return "".join(out)


def strip_comments(code: str) -> str:
    """Sustituye los comentarios por espacios conservando saltos de línea y posiciones."""
    out = list(code)
    i, n = 0, len(code)
    while i < n:
        c = code[i]
        if c == '"' or c == "'":
            quote = c
            i += 1
            while i < n and code[i] != quote and code[i] != "\n":
                i += 2 if code[i] == "\\" else 1
            i += 1
        elif code.startswith("//", i):
            while i < n and code[i] != "\n":
                out[i] = " "
                i += 1
        elif code.startswith("/*", i):
            while i < n and not code.startswith("*/", i):
                if code[i] != "\n":
                    out[i] = " "
                i += 1
            for j in range(i, min(i + 2, n)):
                out[j] = " "
            i += 2
        else:
            i += 1
    return "".join(out)


def _line_of(code: str, offset: int) -> int:
    return code.count("\n", 0, offset) + 1


def _mask_strings(code: str) -> str:
    """Rellena el contenido de las cadenas con 'x' para buscar delimitadores fuera de ellas."""
    return re.sub(_STR, lambda m: '"' + "x" * len(m.group(1)) + '"', code)


def extract_loctext(code: str, rel_path: str) -> list[LocText]:
    """Textos de NSLOCTEXT y LOCTEXT (con el LOCTEXT_NAMESPACE vigente en cada punto)."""
    clean = strip_comments(code)
    found: list[LocText] = []
    # Una macro que no encaja (literales concatenados, una constante en vez de un literal...)
    # desaparecería del catálogo sin avisar: mejor fallar con la línea.
    parsed = {m.start() for m in NSLOCTEXT_RE.finditer(clean)} | {m.start() for m in LOCTEXT_RE.finditer(clean)}
    for m in ANY_LOC_MACRO_RE.finditer(_mask_strings(clean)):
        if m.start() not in parsed:
            raise ValueError(f"{rel_path}:{_line_of(clean, m.start())}: no se puede leer este LOCTEXT/NSLOCTEXT "
                             "(usa un único literal por argumento)")
    for m in NSLOCTEXT_RE.finditer(clean):
        found.append(LocText(unescape(m.group(1)), unescape(m.group(2)), unescape(m.group(3)),
                             rel_path, _line_of(clean, m.start())))
    # LOCTEXT: el espacio de nombres lo fija el #define anterior más cercano no anulado.
    marks: list[tuple[int, str | None]] = []
    for m in NAMESPACE_DEFINE_RE.finditer(clean):
        marks.append((m.start(), unescape(m.group(1))))
    for m in NAMESPACE_UNDEF_RE.finditer(clean):
        marks.append((m.start(), None))
    marks.sort()
    for m in LOCTEXT_RE.finditer(clean):
        ns = None
        for pos, value in marks:
            if pos < m.start():
                ns = value
        line = _line_of(clean, m.start())
        if ns is None:
            raise ValueError(f"{rel_path}:{line}: LOCTEXT sin LOCTEXT_NAMESPACE definido")
        found.append(LocText(ns, unescape(m.group(1)), unescape(m.group(2)), rel_path, line))
    found.sort(key=lambda t: t.line)
    return found


def _statement_bounds(masked: str, start: int, end: int) -> tuple[int, int]:
    """Límites de la sentencia que contiene [start, end): entre ; { } fuera de cadenas."""
    lo = max(masked.rfind(";", 0, start), masked.rfind("{", 0, start), masked.rfind("}", 0, start)) + 1
    candidates = [p for p in (masked.find(";", end), masked.find("{", end), masked.find("}", end)) if p != -1]
    hi = min(candidates) if candidates else len(masked)
    return lo, hi


_PRINTF_SPEC = re.compile(r"%[-+ #0]*\d*(?:\.\d+)?(?:ll|l|h)?[a-zA-Z]")
_TIMES = re.compile(r"(?<=\s)x(?=\s)")


def _has_words(text: str) -> bool:
    """¿Hay letras que traducir? Sin contar especificadores de printf ni el «x» de «%d x %d»."""
    rest = _TIMES.sub("", _PRINTF_SPEC.sub("", text))
    return any(ch.isalpha() for ch in rest)


_IDENTIFIER_LIKE = re.compile(r"^[A-Za-z0-9_./:*%\-]*$")


def scan_literals(code: str, rel_path: str) -> list[Literal]:
    """``TEXT("...")`` que pueden acabar en pantalla sin pasar por LOCTEXT/NSLOCTEXT."""
    clean = strip_comments(code)
    masked = _mask_strings(clean)
    raw_lines = code.splitlines()
    found: list[Literal] = []
    for m in TEXT_RE.finditer(clean):
        text = unescape(m.group(1))
        line = _line_of(clean, m.start())
        if IGNORE_MARKER in raw_lines[line - 1]:
            continue
        lo, hi = _statement_bounds(masked, m.start(), m.end())
        statement = clean[lo:hi]
        if any(tok in statement for tok in EXCLUDED_TOKENS):
            continue
        has_letters = _has_words(text)
        in_sink = any(tok in statement for tok in UI_SINKS)
        context = " ".join(raw_lines[line - 1].split())
        if in_sink:
            found.append(Literal(rel_path, line, text, "literal" if has_letters else "invariante", context))
        elif has_letters and (not _IDENTIFIER_LIKE.match(text) or not text.isascii()):
            # Parece prosa (espacios o letras con tilde) pero no vemos cómo llega a la UI.
            found.append(Literal(rel_path, line, text, "revisar", context))
    return found


def iter_sources(
    repo_root: Path, roots: list[str], suffixes: tuple[str, ...], exclude: tuple[str, ...] = ()
) -> Iterator[tuple[Path, str]]:
    for root in roots:
        base = repo_root / root
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            rel = path.relative_to(repo_root).as_posix()
            if path.suffix in suffixes and not any(rel.startswith(e) for e in exclude):
                yield path, rel
