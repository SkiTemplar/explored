"""Créditos del tráiler y texto para publicar, a partir de Tools/Audio/music_sources.json.

Solo se citan las piezas que suenan en el montaje (`music.music_plan`); las CC BY 4.0 llevan
la atribución completa (título, autor, fuente, licencia con enlace y cambios).
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

from .music import MusicCut

CC_BY = "CC-BY-4.0"
CHANGES_ES = "fragmentos recortados por frases, encadenados con fundidos cruzados y mezclados con efectos"
CHANGES_EN = "excerpts trimmed at phrase boundaries, joined with crossfades and mixed with sound effects"

SUMMARY_ES = (
    "Iria Valdés apaga faros para ganarse la vida. Esta vez es ella quien naufraga en la Isla de Sal y "
    "tiene tres días para volver a encender su faro —la lente del carguero varado, aceite de pescado y "
    "una mecha— antes de que el correo «Mistral» pase de noche. Baliza Siete es una aventura de "
    "supervivencia corta en primera persona (45-75 minutos), sin combate, para PC (Windows)."
)
SUMMARY_EN = (
    "Iria Valdés switches off lighthouses for a living. This time she is the one stranded on the Isla de Sal, "
    "with three days to light her lighthouse again (the lens from the grounded freighter, fish oil and a wick) "
    "before the mail boat \"Mistral\" passes at night. Baliza Siete is a short first-person survival game "
    "(45-75 minutes) with no combat, for PC (Windows)."
)


@dataclass(frozen=True)
class Piece:
    piece_id: str
    title: str
    composer: str
    performer: str
    source_url: str
    license: str
    license_url: str


def load_pieces(sources_json: Path, ids: list[str]) -> list[Piece]:
    data = json.loads(sources_json.read_text(encoding="utf-8"))
    by_id = {p["id"]: p for p in data["pieces"]}
    missing = [i for i in ids if i not in by_id]
    if missing:
        raise KeyError(f"piezas sin registrar en music_sources.json: {missing}")
    return [
        Piece(i, by_id[i]["title"], by_id[i]["composer"], by_id[i]["performer"], by_id[i]["source_url"],
              by_id[i]["license"], by_id[i]["license_url"])
        for i in ids
    ]


def used_piece_ids(cuts: tuple[MusicCut, ...]) -> list[str]:
    seen: list[str] = []
    for cut in cuts:
        if cut.piece_id not in seen:
            seen.append(cut.piece_id)
    return seen


def _composer_note(piece: Piece, lang: str) -> str:
    """En los arreglos (Canon de Pachelbel) el autor del crédito CC BY es el intérprete-arreglista."""
    if piece.composer == piece.performer:
        return ""
    return f", sobre la obra de {piece.composer}" if lang == "es" else f", after {piece.composer}"


def attribution(piece: Piece, lang: str) -> str:
    if piece.license == CC_BY:
        if lang == "es":
            return (f"«{piece.title}» {piece.performer} (incompetech.com){_composer_note(piece, 'es')}. Licencia: "
                    f"Creative Commons Reconocimiento 4.0 Internacional (CC BY 4.0), {piece.license_url}. "
                    f"Fuente: {piece.source_url}. Cambios: {CHANGES_ES}.")
        return (f"\"{piece.title}\" {piece.performer} (incompetech.com){_composer_note(piece, 'en')}. Licensed under "
                f"Creative Commons Attribution 4.0 International (CC BY 4.0), {piece.license_url}. "
                f"Source: {piece.source_url}. Changes: {CHANGES_EN}.")
    if lang == "es":
        return (f"«{piece.title}», {piece.composer}; interpretación de {piece.performer}. "
                f"Dominio público / CC0 1.0 ({piece.source_url}).")
    return f"\"{piece.title}\", {piece.composer}; performed by {piece.performer}. Public domain / CC0 1.0 ({piece.source_url})."


def credits_markdown(pieces: list[Piece]) -> str:
    lines = [
        "# Créditos del tráiler / Trailer credits",
        "",
        "> Generado por `cd Tools/Trailer && uv run python assemble.py credits` a partir de",
        "> `Tools/Audio/music_sources.json`. No se edita a mano.",
        "",
        "## Música / Music",
        "",
        *[f"- {attribution(p, 'es')}" for p in pieces],
        "",
        *[f"- {attribution(p, 'en')}" for p in pieces],
        "",
        "## Sonido, imagen y tipografía / Sound, picture and type",
        "",
        "- Ambientes y efectos: síntesis procedural propia de `Tools/Audio` (sin muestras de terceros).",
        "- Imagen: capturada del juego (Unreal Engine 5.6).",
        "- Tipografía: Lora (The Lora Project Authors), SIL Open Font License 1.1 (`Content/Slate/Fonts/OFL.txt`).",
        "",
    ]
    return "\n".join(lines)


def description_markdown(pieces: list[Piece], duration: float) -> str:
    cc_by = [p for p in pieces if p.license == CC_BY]
    lines = [
        "# Baliza Siete — tráiler",
        "",
        f"Duración {duration:.1f} s · 1920×1080 · 30 fps · H.264 + AAC estéreo.".replace(".", ",", 1),
        "",
        "## Texto para publicar (ES)",
        "",
        SUMMARY_ES,
        "",
        "## Text to publish (EN)",
        "",
        SUMMARY_EN,
        "",
        "## Atribución obligatoria (CC BY 4.0) / Required attribution",
        "",
        "Copiar en la descripción del vídeo / Paste in the video description:",
        "",
        "```text",
        *[attribution(p, "en") for p in cc_by],
        "```",
        "",
        "## Toda la música / All music",
        "",
        *[f"- {attribution(p, 'es')}" for p in pieces],
        "",
        "Ambientes y efectos propios (Tools/Audio). Tipografía Lora (SIL OFL 1.1).",
        "",
    ]
    return "\n".join(lines)


def write_credits(sources_json: Path, cuts: tuple[MusicCut, ...], docs_path: Path, description_path: Path,
                  duration: float) -> tuple[Path, Path]:
    pieces = load_pieces(sources_json, used_piece_ids(cuts))
    docs_path.parent.mkdir(parents=True, exist_ok=True)
    description_path.parent.mkdir(parents=True, exist_ok=True)
    docs_path.write_text(credits_markdown(pieces), encoding="utf-8")
    description_path.write_text(description_markdown(pieces, duration), encoding="utf-8")
    return docs_path, description_path
