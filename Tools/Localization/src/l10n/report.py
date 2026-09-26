"""Informe en Markdown (docs/tecnico/localizacion-informe.md) y catálogo en JSON."""

from __future__ import annotations

import json

from .catalogue import Catalogue

REPORT_PATH = "docs/tecnico/localizacion-informe.md"
CATALOGUE_PATH = "Content/Localization/catalogo.json"

_CATEGORY_TEXT = {
    "literal": "Literales con letras que llegan a la pantalla: pasar a `NSLOCTEXT`/`LOCTEXT` (o `FText::Format`).",
    "invariante": "Sin letras (números, símbolos, separadores): `FText::AsCultureInvariant` o `FText::AsNumber`, sin traducir.",
    "revisar": "Parecen prosa pero no se ve cómo llegan a la UI: comprobar a mano.",
}


def _cell(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", "\\n")


def markdown(cat: Catalogue) -> str:
    s = cat.stats()
    lines = [
        "# Informe de localización",
        "",
        "Generado por `Tools/Localization` (`uv run l10n export`); no se edita a mano.",
        "La guía está en [`localizacion.md`](localizacion.md).",
        "",
        "## Recuento",
        "",
        "| Concepto | Número |",
        "|---|---|",
        f"| Textos en el catálogo | {s['total']} |",
        f"| … del C++ y los .ini (van al manifiesto de Unreal) | {s['cpp']} |",
        f"| … de `Content/Data` (campos bilingües) | {s['datos']} |",
        f"| Textos sin inglés | {s['sin_en']} |",
        f"| Claves propuestas pendientes de integrar | {s['pendientes']} |",
        f"| Literales sin localizar | {s['literales']} |",
        f"| Literales invariantes | {s['invariantes']} |",
        f"| Literales para revisar | {s['revisar']} |",
        f"| Errores / avisos | {len(cat.report.errors)} / {len(cat.report.warnings)} |",
        "",
        "## Literales del C++",
        "",
    ]
    for category in ("literal", "invariante", "revisar"):
        lits = [lit for lit in cat.literals if lit.category == category]
        lines += [f"### {category.capitalize()} ({len(lits)})", "", _CATEGORY_TEXT[category], ""]
        if lits:
            lines += ["| Fichero:línea | Texto | Código |", "|---|---|---|"]
            lines += [f"| `{lit.path}:{lit.line}` | `{_cell(lit.text)}` | `{_cell(lit.context)}` |" for lit in lits]
            lines.append("")
    pending = [e for e in cat.entries if e.origin == "pendiente"]
    lines += ["## Claves propuestas pendientes de integrar", "",
              "Ya traducidas en `Tools/Localization/translations/en.json`; el paquete que toque el fichero",
              "las usa con exactamente este espacio de nombres, clave y texto.", ""]
    if pending:
        lines += ["| Espacio,clave | ES | EN | Dónde |", "|---|---|---|---|"]
        lines += [f"| `{e.id}` | {_cell(e.es or '')} | {_cell(e.en or '')} | {_cell(e.locations[0])} |" for e in pending]
        lines.append("")
    for title, msgs in (("Errores", cat.report.errors), ("Avisos", cat.report.warnings)):
        lines += [f"## {title} ({len(msgs)})", ""]
        lines += [f"- {_cell(m)}" for m in msgs] or ["Ninguno."]
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def catalogue_json(cat: Catalogue) -> str:
    doc = {
        "version": 1,
        "note": "Catálogo generado por Tools/Localization (uv run l10n export). Fuente de verdad: el código "
        "(español), Tools/Localization/translations/en.json (inglés del C++) y Content/Data (campos bilingües).",
        "stats": cat.stats(),
        "entries": [e.to_json() for e in sorted(cat.entries, key=lambda e: (e.origin, e.namespace, e.key))],
    }
    return json.dumps(doc, ensure_ascii=False, indent="\t") + "\n"
