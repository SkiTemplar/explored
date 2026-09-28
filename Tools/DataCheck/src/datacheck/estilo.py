"""Guía anti-IA de biblia 07 §1 aplicada a los textos de datos que ve el jugador.

Lo que se puede comprobar sin leer como una persona: lista negra (§1.2), longitud por tipo de
texto (§1.3), exclamaciones (§1.5), emoji (§1.4.5), puntos suspensivos en vez de cortar (§1.5)
y rayas como muletilla (§1.2). Las tríadas y el calco siguen necesitando una revisión humana.
"""

from __future__ import annotations

import re

# §1.3: (máx. ES, máx. EN) en caracteres, o en palabras si la clave termina en «_palabras».
LIMITS = {
    "diario": {"chars": (280, 340), "sentences": (2, 4)},
    "pista": {"chars": (280, 340), "sentences": (1, 3)},
    "ficha": {"chars": (160, 200), "sentences": (1, 2)},
    "nombre_vitrina": {"words": (6, 6)},
    "nombre_logro": {"words": (4, 4)},
    "descripcion_logro": {"chars": (90, 110), "sentences": (1, 1)},
}

BLACKLIST_ES = [
    r"[ée]pic[oa]s?", r"incre[íi]bles?", r"asombros[oa]s?", r"majestuos[oa]s?", r"impresionantes?",
    r"definitiv[oa]s?", r"perfect[oa]s?", r"fascinantes?", r"sum[ée]rgete", r"emb[áa]rcate", r"desata tu",
    r"ad[ée]ntrate", r"un viaje de", r"en definitiva", r"en resumen", r"cabe destacar", r"no cabe duda",
    r"sin lugar a dudas", r"¿alguna vez", r"¿est[áa]s listo", r"no es solo", r"no es simplemente",
    r"los l[íi]mites de lo posible", r"nunca olvidar[áa]s",
]
BLACKLIST_EN = [
    r"unleash\w*", r"elevate\w*", r"embark\w*", r"dive into", r"discover a world", r"seamless\w*",
    r"game-changing", r"a testament to", r"boundless", r"immersive", r"it'?s not just", r"it'?s worth noting",
    r"in today'?s world", r"at the end of the day", r"needless to say", r"have you ever wondered",
    r"ready to [^.]*\?", r"unlock your", r"epic", r"amazing", r"breathtaking", r"awe-inspiring",
]
_ES = [re.compile(rf"(?<![\wáéíóúñ]){p}(?![\wáéíóúñ])", re.I) for p in BLACKLIST_ES]
_EN = [re.compile(rf"(?<!\w){p}(?!\w)", re.I) for p in BLACKLIST_EN]
EMOJI = re.compile("[\U0001F000-\U0001FAFF☀-➿️]")
# Fin de frase: punto, interrogación o exclamación seguidos de espacio o del final. «{Day}.»
# y las abreviaturas no aparecen en estos textos.
SENTENCE_END = re.compile(r"[.?!](?=\s|$)")


def sentences(text: str) -> int:
    return max(1, len(SENTENCE_END.findall(text.strip())))


def lint(text: str, lang: str, kind: str) -> list[str]:
    """Problemas de estilo de un texto (lista vacía si cumple)."""
    problems: list[str] = []
    for rx in _ES if lang == "es" else _EN:
        m = rx.search(text)
        if m:
            problems.append(f"lista negra de biblia 07 §1.2: «{m.group(0)}»")
    if "!" in text or "¡" in text:
        problems.append("exclamación (biblia 07 §1.5: solo ante un peligro real de la ficción)")
    if EMOJI.search(text):
        problems.append("emoji (biblia 07 §1.4)")
    if "…" in text or "..." in text:
        problems.append("puntos suspensivos: se corta, no se abrevia (biblia 07 §1.5)")
    for sentence in re.split(r"(?<=[.?!])\s+", text):
        if sentence.count("—") > 1:
            problems.append("más de una raya en una frase (biblia 07 §1.2)")
            break
    if text != text.strip() or "  " in text:
        problems.append("espacios de más")
    limits = LIMITS.get(kind, {})
    i = 0 if lang == "es" else 1
    if "chars" in limits and len(text) > limits["chars"][i]:
        problems.append(f"{len(text)} caracteres; el máximo para «{kind}» es {limits['chars'][i]} (biblia 07 §1.3)")
    if "words" in limits and len(text.split()) > limits["words"][i]:
        problems.append(f"{len(text.split())} palabras; el máximo para «{kind}» es {limits['words'][i]} (biblia 07 §1.3)")
    if "sentences" in limits:
        lo, hi = limits["sentences"]
        n = sentences(text)
        if not lo <= n <= hi:
            problems.append(f"{n} frases; «{kind}» admite de {lo} a {hi} (biblia 07 §1.3)")
    return problems


def check_pair(r, where: str, rec: dict, es_key: str, en_key: str, kind: str) -> None:
    """Los dos textos existen, cumplen la guía y el inglés no es más de 1,3× el español (l10n)."""
    es, en = rec.get(es_key), rec.get(en_key)
    for key, text, lang in ((es_key, es, "es"), (en_key, en, "en")):
        if not isinstance(text, str) or not text.strip():
            r.error(f"{where}: falta {key}")
            continue
        for problem in lint(text, lang, kind):
            r.error(f"{where}: {key}: {problem}")
    if isinstance(es, str) and isinstance(en, str) and len(en) >= 12 and len(en) > 1.3 * len(es):
        r.error(f"{where}: {en_key} ({len(en)} car.) es más de 1,3× {es_key} ({len(es)}); "
                "no pasaría l10n --strict")
