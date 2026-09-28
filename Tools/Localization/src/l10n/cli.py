"""CLI: ``uv run l10n [check|export] [--strict] [--quiet] [--check] [--utf16]``.

- ``check`` (por defecto): construye el catálogo y muestra errores, avisos y literales.
- ``export``: escribe el manifiesto y los archivos de Unreal, el catálogo y el informe.
  Con ``--check`` no escribe: falla si lo versionado no está al día.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from . import report, unreal
from .catalogue import REPO_ROOT, Catalogue, Sources, build


def generated_files(cat: Catalogue, utf16: bool = False) -> dict[str, bytes]:
    files = {path: unreal.encode(text, utf16) for path, text in unreal.outputs(cat.exported).items()}
    files[report.CATALOGUE_PATH] = report.catalogue_json(cat).encode("utf-8")
    files[report.REPORT_PATH] = report.markdown(cat).encode("utf-8")
    return files


_LINE_REF = re.compile(r"(\.(?:cpp|h|ini))(?::\d+(?:-\d+)?| - line \d+)")


def _normalized(content: bytes) -> str:
    """Sin números de línea: mover código sin cambiar textos no desfasa lo generado."""
    text = content.decode("utf-16") if content.startswith(b"\xff\xfe") else content.decode("utf-8")
    return _LINE_REF.sub(r"\1", text)


def stale_files(cat: Catalogue, repo_root: Path = REPO_ROOT) -> list[str]:
    stale = []
    for rel, content in generated_files(cat).items():
        path = repo_root / rel
        if not path.exists() or _normalized(path.read_bytes()) != _normalized(content):
            stale.append(rel)
    return stale


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", nargs="?", choices=["check", "export"], default="check")
    parser.add_argument("--strict", action="store_true", help="los avisos (y los literales) también fallan")
    parser.add_argument("--quiet", action="store_true", help="solo errores y resumen")
    parser.add_argument("--check", action="store_true", help="export: no escribe; falla si algo está desfasado")
    parser.add_argument("--utf16", action="store_true", help="export: manifiesto y archivos en UTF-16 LE con BOM, como el editor")
    args = parser.parse_args(argv)

    try:
        cat = build(Sources.load(REPO_ROOT))
    except ValueError as exc:
        # Fuentes ilegibles (JSON mal formado, LOCTEXT sin espacio de nombres...): sin traza de Python.
        print(f"ERROR  {exc}")
        return 2
    r = cat.report

    if args.command == "export":
        if args.check:
            stale = stale_files(cat, REPO_ROOT)
            for rel in stale:
                print(f"DESFASADO  {rel}  (ejecuta: cd Tools/Localization && uv run l10n export)")
            if stale:
                return 1
        else:
            for rel, content in generated_files(cat, args.utf16).items():
                path = REPO_ROOT / rel
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)
                print(f"Escrito {rel}")

    if not args.quiet:
        for msg in r.info:
            print(f"INFO   {msg}")
        for msg in r.warnings:
            print(f"AVISO  {msg}")
        for lit in cat.literals:
            if lit.category != "literal":
                print(f"{lit.category.upper():<11}{lit.path}:{lit.line}: «{lit.text}»")
    for msg in r.errors:
        print(f"ERROR  {msg}")
    s = cat.stats()
    print(f"\n{s['total']} textos ({s['cpp']} C++/ini, {s['datos']} datos), {s['sin_en']} sin inglés, "
          f"{s['pendientes']} pendientes de integrar, {s['literales']} literales sin localizar; "
          f"{len(r.errors)} errores, {len(r.warnings)} avisos")
    failed = bool(r.errors) or (args.strict and bool(r.warnings))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
