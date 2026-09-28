"""CLI `explored-audio`: `build` genera todo el catalogo, `music` renderiza
solo la musica (una capa por pieza de `Content/Data/music_layers.json`) y
`music-layers` reescribe ese JSON (la descripcion de la musica adaptativa que
lee el juego, sin renderizar audio)."""

from __future__ import annotations

import argparse
import sys

from .build import build_all, build_music_layers, default_output_root


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="explored-audio", description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    build_parser = sub.add_parser("build", help="genera todo el catalogo de audio en Art/Export/Audio")
    build_parser.add_argument("--out", type=str, default=None, help="ruta de salida (por defecto: Art/Export/Audio del repo)")
    build_parser.add_argument("--quiet", action="store_true", help="no imprime el progreso por sonido")

    music_parser = sub.add_parser("music", help="renderiza las capas de musica que declara Content/Data/music_layers.json")
    music_parser.add_argument("--layers", type=str, default=None, help="JSON de capas (por defecto: Content/Data/music_layers.json)")
    music_parser.add_argument("--out", type=str, default=None, help="ruta de salida (por defecto: Art/Export/Audio del repo)")
    music_parser.add_argument("--quiet", action="store_true", help="no imprime el progreso por pieza")

    layers_parser = sub.add_parser("music-layers", help="escribe Content/Data/music_layers.json para el director de musica")
    layers_parser.add_argument("--out", type=str, default=None, help="ruta del JSON (por defecto: Content/Data del repo)")

    args = parser.parse_args(argv)

    if args.command == "build":
        out = args.out or default_output_root()
        build_all(output_root=out, verbose=not args.quiet)
        return 0

    if args.command == "music":
        build_music_layers(args.layers, args.out, verbose=not args.quiet)
        return 0

    if args.command == "music-layers":
        from .music.layers import write_music_layers

        print(write_music_layers(args.out))
        return 0

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
