"""Modificadores de argumento de ``FText::Format`` (``{Count}|plural(one=pez,other=peces)``).

Unreal admite cuatro modificadores detrás de un marcador:

- ``plural(zero=…,one=…,two=…,few=…,many=…,other=…)``: forma según el número (CLDR).
- ``ordinal(…)``: igual, con las categorías ordinales (1st, 2nd, 3rd…).
- ``gender(masculino,femenino[,neutro])``: forma según el género de un argumento.
- ``hpp(a,b)``: partícula postposicional coreana; en español e inglés no tiene sentido.

Los valores pueden ir entre comillas dobles para llevar comas o paréntesis, y la tilde
invertida (`) escapa el carácter siguiente, igual que en el propio formato de Unreal.

El chequeo de marcadores (``{Count}`` en los dos idiomas) no ve lo que va detrás de la
barra: aquí se valida la sintaxis y las categorías de cada idioma, y que los dos idiomas
usen los mismos modificadores numéricos sobre los mismos argumentos (biblia 07 §5.3).
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

MODIFIERS = ("plural", "ordinal", "gender", "hpp")
CATEGORIES = ("zero", "one", "two", "few", "many", "other")

# Categorías CLDR que usa cada idioma: (admitidas, obligatorias). Una categoría fuera de
# las admitidas no se elige nunca (p. ej. «zero» en inglés: el 0 cae en «other»).
PLURAL_RULES: dict[str, dict[str, tuple[frozenset[str], frozenset[str]]]] = {
    "es": {
        "plural": (frozenset({"one", "many", "other"}), frozenset({"one", "other"})),
        "ordinal": (frozenset({"other"}), frozenset({"other"})),
    },
    "en": {
        "plural": (frozenset({"one", "other"}), frozenset({"one", "other"})),
        "ordinal": (frozenset({"one", "two", "few", "other"}), frozenset({"one", "two", "few", "other"})),
    },
}

_NAME_RE = re.compile(r"[A-Za-z]+")
_KEY_RE = re.compile(r"^\s*([A-Za-z]+)\s*=(.*)$", re.S)
_STRAY_RE = re.compile(r"\|(" + "|".join(MODIFIERS) + r")\s*\(")


@dataclass
class Modifier:
    argument: str
    name: str
    raw: str
    values: list[str] = field(default_factory=list)  # sin procesar, en orden

    @property
    def numeric(self) -> bool:
        return self.name in ("plural", "ordinal")


@dataclass
class Parsed:
    modifiers: list[Modifier]
    errors: list[str]


def _split_args(body: str) -> tuple[list[str], str | None]:
    """Parte «a=1,b="x, y"» por comas fuera de comillas y llaves. Devuelve (partes, error)."""
    parts: list[str] = []
    cur: list[str] = []
    quoted = False
    depth = 0
    i = 0
    while i < len(body):
        c = body[i]
        if c == "`" and i + 1 < len(body):
            cur.append(body[i:i + 2])
            i += 2
            continue
        if c == '"':
            quoted = not quoted
        elif not quoted and c == "{":
            depth += 1
        elif not quoted and c == "}":
            depth -= 1
            if depth < 0:
                return parts, "llave de cierre sin abrir"
        elif not quoted and depth == 0 and c == ",":
            parts.append("".join(cur))
            cur = []
            i += 1
            continue
        cur.append(c)
        i += 1
    if quoted:
        return parts, "comillas sin cerrar"
    if depth:
        return parts, "llave sin cerrar"
    parts.append("".join(cur))
    return parts, None


def _find_close(text: str, start: int) -> int:
    """Índice del «)» que cierra el paréntesis abierto justo antes de start; -1 si no se
    cierra, -2 si el texto acaba dentro de unas comillas."""
    quoted = False
    depth = 0
    i = start
    while i < len(text):
        c = text[i]
        if c == "`":
            i += 2
            continue
        if c == '"':
            quoted = not quoted
        elif not quoted:
            if c == "(":
                depth += 1
            elif c == ")":
                if depth == 0:
                    return i
                depth -= 1
        i += 1
    return -2 if quoted else -1


def parse(text: str | None) -> Parsed:
    """Modificadores de un texto y errores de sintaxis."""
    text = text or ""
    mods: list[Modifier] = []
    errors: list[str] = []
    consumed: set[int] = set()  # posiciones de «|» ya asignadas a un marcador
    i = 0
    while i < len(text):
        c = text[i]
        if c == "`":
            i += 2
            continue
        if c != "{":
            i += 1
            continue
        end = text.find("}", i + 1)
        if end < 0:
            break
        arg = text[i + 1:end]
        i = end + 1
        if not re.fullmatch(r"[A-Za-z0-9_]+", arg) or i >= len(text) or text[i] != "|":
            continue
        consumed.add(i)
        m = _NAME_RE.match(text, i + 1)
        if not m:
            errors.append(f"«{{{arg}}}|» sin modificador detrás de la barra")
            continue
        name = m.group(0)
        after = m.end()
        if name not in MODIFIERS:
            errors.append(f"«{{{arg}}}|{name}»: modificador desconocido (admite {', '.join(MODIFIERS)})")
            i = after
            continue
        if after >= len(text) or text[after] != "(":
            errors.append(f"«{{{arg}}}|{name}» sin paréntesis de argumentos")
            i = after
            continue
        close = _find_close(text, after + 1)
        if close < 0:
            what = "comillas sin cerrar" if close == -2 else "paréntesis sin cerrar"
            errors.append(f"«{{{arg}}}|{name}(»: {what}")
            break
        body = text[after + 1:close]
        values, err = _split_args(body)
        raw = text[i:close + 1]
        i = close + 1
        if err:
            errors.append(f"«{{{arg}}}{raw}»: {err}")
            continue
        mods.append(Modifier(arg, name, raw, values))
    for m in _STRAY_RE.finditer(text):
        if m.start() not in consumed:
            errors.append(f"«{m.group(0)}» no va pegado a un marcador «{{Arg}}»; se vería tal cual en pantalla")
    return Parsed(mods, errors)


def check(text: str | None, culture: str) -> tuple[list[str], list[str]]:
    """Errores y avisos de los modificadores de un texto en su idioma."""
    parsed = parse(text)
    errors = list(parsed.errors)
    warnings: list[str] = []
    rules = PLURAL_RULES.get(culture, {})
    for mod in parsed.modifiers:
        label = f"«{{{mod.argument}}}{mod.raw}»"
        if mod.name == "hpp":
            warnings.append(f"{label}: «hpp» es la partícula del coreano; en {culture} no hace nada")
            if len(mod.values) != 2:
                errors.append(f"{label}: «hpp» lleva exactamente dos formas")
            continue
        if mod.name == "gender":
            if len(mod.values) not in (2, 3) or any(not v.strip() for v in mod.values):
                errors.append(f"{label}: «gender» lleva dos o tres formas no vacías (masculino, femenino[, neutro])")
            elif any((m := _KEY_RE.match(v)) and m.group(1) in CATEGORIES for v in mod.values):
                errors.append(f"{label}: «gender» va por posición, sin «clave=»")
            continue
        seen: dict[str, str] = {}
        for value in mod.values:
            km = _KEY_RE.match(value)
            if not km:
                errors.append(f"{label}: «{value.strip()}» no tiene la forma categoría=texto")
                continue
            key = km.group(1)
            if key not in CATEGORIES:
                errors.append(f"{label}: categoría «{key}» desconocida (admite {', '.join(CATEGORIES)})")
                continue
            if key in seen:
                errors.append(f"{label}: categoría «{key}» repetida")
            seen[key] = km.group(2)
        if "other" not in seen and not any(e.startswith(label) for e in errors):
            errors.append(f"{label}: falta la categoría «other», la que Unreal usa por defecto")
        allowed, required = rules.get(mod.name, (frozenset(CATEGORIES), frozenset({"other"})))
        missing = sorted((required - set(seen)) - {"other"})
        if missing:
            errors.append(f"{label}: en {culture} falta {', '.join(missing)}; ese número caería en «other»")
        unused = sorted(set(seen) - allowed)
        if unused:
            warnings.append(f"{label}: {', '.join(unused)} no existe en {culture} y no se elige nunca")
    return errors, warnings


def numeric_signature(text: str | None) -> set[tuple[str, str]]:
    """Pares (argumento, modificador) numéricos: lo que tiene que coincidir entre idiomas."""
    return {(m.argument, m.name) for m in parse(text).modifiers if m.numeric}


def compare(es: str | None, en: str | None) -> list[str]:
    """Diferencias de modificadores numéricos entre el español y el inglés."""
    a, b = numeric_signature(es), numeric_signature(en)
    if a == b:
        return []
    fmt = lambda s: ", ".join(f"{{{arg}}}|{name}" for arg, name in sorted(s)) or "ninguno"  # noqa: E731
    return [f"modificadores de plural distintos: {fmt(a)} (ES) y {fmt(b)} (EN)"]
