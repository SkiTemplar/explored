"""Genera `docs/creditos-musica.md` a partir de music_sources.json.

El fichero no se edita a mano: si cambia la lista, se regenera con
`uv run explored-music credits`, y un test comprueba que el documento
versionado coincide con lo que sale de aqui.
"""

from __future__ import annotations

from pathlib import Path

from .sources import SourceList, repo_root

SOURCE_LABELS = {
    "wikimedia_commons": "Wikimedia Commons",
    "musopen": "Musopen",
    "freepd": "FreePD",
    "incompetech": "incompetech.com",
    "opengameart": "OpenGameArt",
}

LICENSE_NAMES = {
    "CC-BY-3.0": {
        "es": "Creative Commons Reconocimiento 3.0 (CC BY 3.0)",
        "en": "Creative Commons Attribution 3.0 (CC BY 3.0)",
    },
    "CC-BY-4.0": {
        "es": "Creative Commons Reconocimiento 4.0 Internacional (CC BY 4.0)",
        "en": "Creative Commons Attribution 4.0 International (CC BY 4.0)",
    },
    "CC0-1.0": {"es": "CC0 1.0 (dominio público)", "en": "CC0 1.0 (public domain)"},
    "PD": {"es": "Dominio público", "en": "Public domain"},
}

MOMENT_NAMES = {
    "dia": ("Día", "Day"),
    "noche": ("Noche", "Night"),
    "lluvia": ("Lluvia", "Rain"),
    "mar": ("Mar", "Sea"),
    "cueva": ("Cueva", "Cave"),
    "ruinas": ("Ruinas", "Ruins"),
}

CHANGES = {
    "es": "Cambios: volumen normalizado y silencios del principio y del final recortados.",
    "en": "Changes: loudness normalized, leading and trailing silence trimmed.",
}


def default_credits_path() -> Path:
    return repo_root() / "docs" / "creditos-musica.md"


def credit_lines(piece, lang: str) -> list[str]:
    """Las tres o cuatro lineas de credito de una pieza CC-BY, en el formato
    que pide incompetech: titulo e intérprete, licencia, enlace, cambios."""
    label = SOURCE_LABELS[piece.source]
    title = f"«{piece.title}»" if lang == "es" else f'"{piece.title}"'
    return [
        f"{title} {piece.performer} ({label})",
        ("Licencia: " if lang == "es" else "Licensed under ") + LICENSE_NAMES[piece.license][lang],
        piece.license_url,
        CHANGES[lang],
    ]


def render_credits(sources: SourceList) -> str:
    credited = [p for p in sources.pieces if p.needs_credit]
    free = [p for p in sources.pieces if not p.needs_credit]
    out: list[str] = []
    w = out.append
    w("# Créditos de música / Music credits")
    w("")
    w("> Generado por `cd Tools/Audio && uv run explored-music credits` a partir de")
    w("> `Tools/Audio/music_sources.json`. No se edita a mano.")
    w("")
    w("La banda sonora de Explored mezcla la música compuesta por código del propio juego")
    w("con grabaciones de licencia abierta. Las piezas CC BY exigen crédito; aquí está listo")
    w("para copiar en la pantalla de créditos y en la página de Steam.")
    w("")

    for lang, heading, intro in (
        ("es", "## Español", "Música adicional:"),
        ("en", "## English", "Additional music:"),
    ):
        w(heading)
        w("")
        w(intro)
        w("")
        for p in credited:
            lines = credit_lines(p, lang)
            w(f"- {lines[0]}  ")
            for extra in lines[1:-1]:
                w(f"  {extra}  ")
            w(f"  {lines[-1]}")
        w("")

    w("## Steam")
    w("")
    w("Texto plano para la descripción de la tienda (Steam no admite Markdown aquí):")
    w("")
    w("```text")
    w("Additional music / Música adicional:")
    for p in credited:
        w("")
        for line in credit_lines(p, "en"):
            w(line)
    w("")
    w("Recordings in the public domain or under CC0 from Wikimedia Commons and Musopen.")
    w("Grabaciones de dominio público o CC0 de Wikimedia Commons y Musopen.")
    w("```")
    w("")

    w("## Grabaciones de dominio público y CC0 / Public domain and CC0 recordings")
    w("")
    w("No exigen crédito. Se listan igualmente para dejar constancia del origen de cada")
    w("fichero y de quién lo tocó.")
    w("")
    w("| Pieza / Piece | Compositor / Composer | Intérprete / Performer | Licencia / License | Origen / Source |")
    w("|---|---|---|---|---|")
    for p in free:
        w(
            f"| {p.title} | {p.composer} | {p.performer} | {LICENSE_NAMES[p.license]['en']} "
            f"(`{p.license_evidence}`) | [{SOURCE_LABELS[p.source]}]({p.source_url}) |"
        )
    w("")

    w("## Reparto por momento de juego / Pieces by game moment")
    w("")
    w("| Momento / Moment | Piezas / Pieces |")
    w("|---|---|")
    for key, (es, en) in MOMENT_NAMES.items():
        titles = [p.title for p in sources.pieces if p.moment == key]
        if titles:
            w(f"| {es} / {en} | {'; '.join(titles)} |")
    w("")
    return "\n".join(out)


def write_credits(sources: SourceList, path: Path | None = None) -> Path:
    path = path or default_credits_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(render_credits(sources), encoding="utf-8")
    return path
