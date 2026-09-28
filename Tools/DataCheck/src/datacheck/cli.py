"""CLI: ``uv run datacheck [--strict] [--write-pending] [--write-cooking] [--quiet] [--root RUTA]``."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import cooking
from .checks import REPO_ROOT, DataSet, pending_expected, run_all


def write_pending(ds: DataSet) -> None:
    path = ds.repo_root / "Content" / "Data" / "meshes_pendientes.json"
    groups = pending_expected(ds)
    doc = {
        "version": 1,
        "note": "Objetos, piezas y etapas que usan un marcador de /Engine/BasicShapes o mesh null. "
        "Lo mantiene Tools/DataCheck (uv run datacheck --write-pending); quita la entrada al añadir su script en Tools/Blender.",
        **{group: sorted(ids) for group, ids in groups.items()},
    }
    path.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    ds.data["meshes_pendientes.json"] = doc
    print(f"Escrito {path.relative_to(ds.repo_root)}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--strict", action="store_true", help="los avisos también fallan")
    parser.add_argument("--write-pending", action="store_true", help="regenera meshes_pendientes.json")
    parser.add_argument("--write-cooking", action="store_true",
                        help="regenera FireData.inl y CookingData.inl desde fuels.json, recipes.json e items.json")
    parser.add_argument("--quiet", action="store_true", help="no muestra la información de diseño")
    parser.add_argument("--root", type=Path, default=REPO_ROOT,
                        help="raíz del repositorio a validar (por defecto, la de este checkout)")
    args = parser.parse_args(argv)

    ds = DataSet.load(args.root)
    if args.write_pending:
        write_pending(ds)
    if args.write_cooking:
        for rel in cooking.write_generated(ds.repo_root, ds.data):
            print(f"Escrito {rel.as_posix()}")
    report = run_all(ds)

    if not args.quiet:
        for msg in report.info:
            print(f"INFO   {msg}")
    for msg in report.warnings:
        print(f"AVISO  {msg}")
    for msg in report.errors:
        print(f"ERROR  {msg}")
    print(f"\n{len(report.errors)} errores, {len(report.warnings)} avisos, {len(report.info)} notas")
    failed = bool(report.errors) or (args.strict and bool(report.warnings))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
