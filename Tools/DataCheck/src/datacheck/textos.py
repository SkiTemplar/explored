"""Guía de estilo anti-IA de biblia 07 §1 aplicada a los textos de datos que ve el jugador.

Tres comprobaciones, todas mecánicas (la lectura en voz alta sigue siendo cosa de una persona):

- **Lista negra** (§1.2): adjetivos vacíos, metáforas de viaje, muletillas y ganchos, en
  español y en inglés, como palabra entera y sin distinguir mayúsculas.
- **Signos** (§1.2 y §1.4): ni exclamaciones editoriales ni emoji en nombres y descripciones.
- **Longitud de los logros** (§1.3): nombre de 4 palabras como mucho y sin puntuación final;
  descripción de una frase, 90 caracteres en español y 110 en inglés.
"""

from __future__ import annotations

import re
from collections.abc import Iterator

# §1.2, español. Palabras y giros prohibidos en cualquier texto que vea el jugador.
BLACKLIST_ES = (
    "épico", "épica", "épicos", "épicas", "increíble", "increíbles", "asombroso", "asombrosa",
    "majestuoso", "majestuosa", "impresionante", "impresionantes", "definitivo", "definitiva",
    "perfecto", "perfecta", "único en su especie", "única en su especie",
    "sumérgete", "embárcate", "desata tu", "un viaje de descubrimiento", "adéntrate",
    "un mundo que te espera", "los límites de lo posible", "una experiencia que nunca olvidarás",
    "cambia las reglas del juego", "en definitiva", "en resumen", "cabe destacar",
    "no cabe duda", "sin lugar a dudas", "alguna vez soñaste", "estás listo", "estás lista",
    "no es solo", "no es sólo", "no es simplemente",
)
# §1.2, inglés.
BLACKLIST_EN = (
    "unleash", "elevate", "unlock your potential", "embark on", "dive into", "discover a world",
    "seamless", "game-changing", "a testament to", "boundless", "immersive", "it's not just",
    "it's worth noting", "in today's world", "at the end of the day", "needless to say",
    "have you ever wondered", "whether you're a",
)

ACHIEVEMENT_NAME_WORDS = 4
ACHIEVEMENT_DESC_MAX = {"descriptionEs": 90, "descriptionEn": 110}
EMOJI = re.compile("[\U0001F000-\U0001FAFF☀-➿️]")
FINAL_PUNCT = re.compile(r"[.,;:!?¡¿…]$")
SENTENCE_END = re.compile(r"[.!?…](?=\s|$)")


def _phrase_re(phrases: tuple[str, ...]) -> re.Pattern[str]:
    alternatives = "|".join(re.escape(p) for p in sorted(phrases, key=len, reverse=True))
    return re.compile(rf"(?<![\w])(?:{alternatives})(?![\w])", re.IGNORECASE)


_BLACK = {"es": _phrase_re(BLACKLIST_ES), "en": _phrase_re(BLACKLIST_EN)}


def problems(text: str, lang: str) -> list[str]:
    """Rasgos de la lista negra, exclamaciones y emoji de un texto (vacío = limpio)."""
    found = []
    m = _BLACK[lang].search(text)
    if m:
        found.append(f"usa «{m.group(0)}», de la lista negra de biblia 07 §1.2")
    if "!" in text or "¡" in text:
        found.append("lleva exclamación (biblia 07 §1.2: solo para un peligro real, nunca en datos)")
    if EMOJI.search(text):
        found.append("lleva emoji (biblia 07 §1.4)")
    return found


def achievement_length_problems(ach: dict) -> list[str]:
    """Límites de biblia 07 §1.3 para un logro."""
    found = []
    for key in ("nameEs", "nameEn"):
        name = ach.get(key)
        if not isinstance(name, str):
            continue
        words = len(name.split())
        if words > ACHIEVEMENT_NAME_WORDS:
            found.append(f"{key} tiene {words} palabras (máximo {ACHIEVEMENT_NAME_WORDS})")
        if FINAL_PUNCT.search(name.strip()):
            found.append(f"{key} acaba en signo de puntuación")
    for key, limit in ACHIEVEMENT_DESC_MAX.items():
        desc = ach.get(key)
        if not isinstance(desc, str):
            continue
        if len(desc) > limit:
            found.append(f"{key} tiene {len(desc)} caracteres (máximo {limit})")
        if len(SENTENCE_END.findall(desc.strip())) > 1:
            found.append(f"{key} tiene más de una frase")
    return found


# Campos de datos que ve el jugador: (fichero, cómo sacar los registros, campo ES, campo EN).
def _records(doc, key: str | None) -> Iterator[dict]:
    seq = doc if key is None else (doc.get(key, []) if isinstance(doc, dict) else [])
    for rec in seq if isinstance(seq, list) else []:
        if isinstance(rec, dict):
            yield rec


PLAYER_FIELDS = (
    ("items.json", None, "nameEs", "nameEn"),
    ("templates.json", None, "nameEs", "nameEn"),
    ("templates.json", None, "nameTemplate", "nameTemplateEn"),
    ("building_pieces.json", "pieces", "nameEs", "nameEn"),
    ("recipes_smithing.json", "recipes", "nameEs", "nameEn"),
    ("achievements.json", "achievements", "nameEs", "nameEn"),
    ("achievements.json", "achievements", "descriptionEs", "descriptionEn"),
)


def check_player_texts(data: dict[str, object], error) -> None:
    """Lista negra, exclamaciones y emoji en todos los campos de PLAYER_FIELDS."""
    for file, key, es_field, en_field in PLAYER_FIELDS:
        doc = data.get(file)
        if doc is None:
            continue
        for rec in _records(doc, key):
            for field, lang in ((es_field, "es"), (en_field, "en")):
                text = rec.get(field)
                if isinstance(text, str):
                    for p in problems(text, lang):
                        error(f"{file} «{rec.get('id')}».{field}: {p}")
