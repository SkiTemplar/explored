"""CLI: ``uv run datacheck [--strict] [--quiet] [--root RUTA]``."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .checks import REPO_ROOT, DataSet, run_all


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--strict", action="store_true", help="los avisos también fallan")
    parser.add_argument("--quiet", action="store_true", help="no muestra la información de diseño")
    parser.add_argument("--root", type=Path, default=REPO_ROOT,
                        help="raíz del repositorio a validar (por defecto, la de este checkout)")
    args = parser.parse_args(argv)

    report = run_all(DataSet.load(args.root))
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
