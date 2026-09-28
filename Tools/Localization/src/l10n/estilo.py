"""Guía de estilo anti-IA (biblia 07 §1.2–§1.5) aplicada a los textos del catálogo.

Solo se comprueba lo que una máquina puede ver sin equivocarse casi nunca: palabras y
construcciones de la lista negra, emoji, exclamaciones fuera de sitio, «fishes» y los
máximos de longitud de los logros y del museo. El resto del checklist (tríadas, calcos,
leerlo en voz alta) sigue siendo trabajo de quien escribe.
"""

from __future__ import annotations

import re

# Lista negra de §1.2. Palabras completas, sin distinguir mayúsculas.
BLACKLIST_ES: list[tuple[str, str]] = [
    (r"\bépic[oa]s?\b", "adjetivo vacío"),
    (r"\bincreíbles?\b", "adjetivo vacío"),
    (r"\basombros[oa]s?\b", "adjetivo vacío"),
    (r"\bmajestuos[oa]s?\b", "adjetivo vacío"),
    (r"\bimpresionantes?\b", "adjetivo vacío"),
    (r"\bfascinantes?\b", "adjetivo vacío"),
    (r"\búnic[oa] en su especie\b", "adjetivo vacío"),
    (r"\bdefinitiv[oa]s?\b", "adjetivo vacío"),
    (r"\bsumérgete\b", "metáfora de inmersión"),
    (r"\bembárcate\b", "metáfora de viaje"),
    (r"\badéntrate\b", "metáfora de viaje"),
    (r"\bdesata tu\b", "metáfora vacía"),
    (r"\bun viaje de descubrimiento\b", "metáfora de viaje"),
    (r"\bno es (?:solo|sólo|simplemente) .{1,60}?, es\b", "falsa disyuntiva «no es X, es Y»"),
    (r"\ben definitiva\b", "muletilla de resumen"),
    (r"\ben resumen\b", "muletilla de resumen"),
    (r"\bcabe destacar\b", "muletilla de resumen"),
    (r"\bno cabe duda\b", "muletilla de resumen"),
    (r"\bsin lugar a dudas\b", "muletilla de resumen"),
    (r"¿\s*alguna vez\b", "pregunta retórica de gancho"),
    (r"¿\s*estás list[oa]\b", "pregunta retórica de gancho"),
    (r"\bprimerísim[oa]\b", "superlativo hinchado"),
]
BLACKLIST_EN: list[tuple[str, str]] = [
    (r"\bunleash", "hype verb"),
    (r"\belevate", "hype verb"),
    (r"\bunlock your\b", "hype verb"),
    (r"\bembark on\b", "hype verb"),
    (r"\bdive into\b", "hype verb"),
    (r"\bdiscover a world\b", "hype verb"),
    (r"\bseamless", "vacío corporativo"),
    (r"\bgame-changing\b", "vacío corporativo"),
    (r"\ba testament to\b", "vacío corporativo"),
    (r"\bboundless\b", "vacío corporativo"),
    (r"\bimmersive\b", "vacío corporativo"),
    (r"\bepic\b", "adjetivo vacío"),
    (r"\bincredible\b", "adjetivo vacío"),
    (r"\bamazing\b", "adjetivo vacío"),
    (r"\bawesome\b", "adjetivo vacío"),
    (r"\bbreathtaking\b", "adjetivo vacío"),
    (r"\bmajestic\b", "adjetivo vacío"),
    (r"\bfascinating\b", "adjetivo vacío"),
    (r"\bultimate\b", "adjetivo vacío"),
    (r"\bit'?s not just\b", "falsa disyuntiva"),
    (r"\bit'?s worth noting\b", "muletilla"),
    (r"\bin today'?s world\b", "muletilla"),
    (r"\bat the end of the day\b", "muletilla"),
    (r"\bneedless to say\b", "muletilla"),
    (r"\bhave you ever wondered\b", "pregunta retórica de gancho"),
    (r"^ready to\b[^.]*\?", "pregunta retórica de gancho"),
    (r"\bvery first\b", "superlativo hinchado"),
    (r"\bfishes\b", "«fish» es invariable en plural (biblia 07 §5.3)"),
]
_ES = [(re.compile(p, re.I), why) for p, why in BLACKLIST_ES]
_EN = [(re.compile(p, re.I | re.M), why) for p, why in BLACKLIST_EN]

# Emoji y pictogramas: prohibidos siempre (biblia 07 §1.4 regla 5).
EMOJI_RE = re.compile("[\U0001F000-\U0001FAFF☀-➿⬀-⯿️]")

# Textos donde la exclamación nunca marca un peligro de la ficción: logros y museo.
NO_EXCLAMATION = ("Data.achievements.", "Data.artifacts.")

# Máximos de §1.3 (palabras de nombre; caracteres de descripción).
ACHIEVEMENT_NAME_WORDS = 4
ACHIEVEMENT_DESC_CHARS = {"es": 90, "en": 110}
MUSEUM_NAME_WORDS = 6


def _words(text: str) -> int:
    return len([w for w in re.split(r"\s+", text.strip()) if w])


def _sentences(text: str) -> int:
    return len([s for s in re.split(r"(?<=[.?!])\s+", text.strip()) if s])


def blacklist(text: str, culture: str) -> list[str]:
    pats = _ES if culture == "es" else _EN
    out = []
    for pat, why in pats:
        m = pat.search(text)
        if m:
            out.append(f"«{m.group(0).strip()}» ({why}, biblia 07 §1.2)")
    return out


def check(namespace: str, key: str, es: str, en: str) -> tuple[list[str], list[str]]:
    """Errores y avisos de estilo de un texto con sus dos idiomas."""
    errors: list[str] = []
    warnings: list[str] = []
    for culture, text in (("es", es), ("en", en)):
        tag = culture.upper()
        if EMOJI_RE.search(text):
            errors.append(f"{tag} lleva emoji; están prohibidos en todo el juego (biblia 07 §1.4)")
        for hit in blacklist(text, culture):
            warnings.append(f"{tag} {hit}")
        if namespace.startswith(NO_EXCLAMATION) and ("!" in text or "¡" in text):
            warnings.append(f"{tag} lleva exclamación en un texto sin peligro de la ficción (biblia 07 §1.5)")
        if text.count("!") > 1:
            warnings.append(f"{tag} lleva más de un signo de exclamación (biblia 07 §1.5)")

    if namespace == "Data.achievements.achievements":
        if key.endswith(".nameEs"):
            for tag, text in (("ES", es), ("EN", en)):
                if _words(text) > ACHIEVEMENT_NAME_WORDS:
                    warnings.append(f"nombre de logro {tag} con {_words(text)} palabras (máximo {ACHIEVEMENT_NAME_WORDS}, §1.3)")
                if text.rstrip()[-1:] in ".…:;":
                    warnings.append(f"nombre de logro {tag} con puntuación final (§1.3)")
        elif key.endswith(".descriptionEs"):
            for culture, text in (("es", es), ("en", en)):
                limit = ACHIEVEMENT_DESC_CHARS[culture]
                if len(text) > limit:
                    warnings.append(f"descripción de logro {culture.upper()} con {len(text)} caracteres (máximo {limit}, §1.3)")
                if _sentences(text) > 1:
                    warnings.append(f"descripción de logro {culture.upper()} con más de una frase (§1.3)")
                if "…" in text or "..." in text:
                    warnings.append(f"descripción de logro {culture.upper()} cortada con «…»; se corta, no se abrevia (§1.5)")
    elif namespace == "Data.artifacts.artifacts" and key.endswith(".nameEs"):
        for tag, text in (("ES", es), ("EN", en)):
            if _words(text) > MUSEUM_NAME_WORDS:
                warnings.append(f"etiqueta de vitrina {tag} con {_words(text)} palabras (máximo {MUSEUM_NAME_WORDS}, §1.3)")
    return errors, warnings
