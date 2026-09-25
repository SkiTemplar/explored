"""CLI `explored-audio`: por ahora un unico subcomando, `build`, que genera
todo el catalogo. Se deja como subcomando (y no como script plano) para poder
añadir mas tarde `spectrograms` u otras tandas sin romper la interfaz."""

from __future__ import annotations

import argparse
import sys

from .build import build_all, default_output_root


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="explored-audio", description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    build_parser = sub.add_parser("build", help="genera todo el catalogo de audio en Art/Export/Audio")
    build_parser.add_argument("--out", type=str, default=None, help="ruta de salida (por defecto: Art/Export/Audio del repo)")
    build_parser.add_argument("--quiet", action="store_true", help="no imprime el progreso por sonido")

    args = parser.parse_args(argv)

    if args.command == "build":
        out = args.out or default_output_root()
        build_all(output_root=out, verbose=not args.quiet)
        return 0

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
