"""CLI `explored-music`: banda sonora grabada de licencia abierta.

    uv run explored-music check      valida music_sources.json (sin red)
    uv run explored-music fetch      descarga, verifica, normaliza y exporta OGG
    uv run explored-music credits    reescribe docs/creditos-musica.md
    uv run explored-music pin ID...  calcula el SHA-256 de piezas nuevas
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .credits import write_credits
from .fetch import build_all, ensure_original
from .sources import (
    default_cache_root,
    default_sources_path,
    load_layer_roles,
    load_sources,
    safe_to_fetch,
    validate,
)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="explored-music", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sources", type=Path, default=None, help="ruta de music_sources.json")
    parser.add_argument("--cache", type=Path, default=None, help="cache de audio (por defecto Tools/Audio/.cache/music)")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("check", help="valida la lista sin descargar nada")
    fetch_p = sub.add_parser("fetch", help="descarga y exporta los OGG a la cache")
    fetch_p.add_argument("ids", nargs="*", help="solo estas piezas (por defecto, todas)")
    fetch_p.add_argument("--force", action="store_true", help="reprocesa aunque el OGG este al dia")
    credits_p = sub.add_parser("credits", help="escribe docs/creditos-musica.md")
    credits_p.add_argument("--out", type=Path, default=None)
    pin_p = sub.add_parser("pin", help="descarga y escribe el SHA-256 de las piezas indicadas")
    pin_p.add_argument("ids", nargs="+")
    args = parser.parse_args(argv)

    sources_path = args.sources or default_sources_path()
    cache = args.cache or default_cache_root()

    if args.command == "pin":
        return _pin(sources_path, cache, set(args.ids))

    sources = load_sources(sources_path)
    errors = validate(sources, load_layer_roles())
    if errors:
        for e in errors:
            print(f"ERROR {e}", file=sys.stderr)
        return 1

    if args.command == "check":
        print(f"{len(sources.pieces)} piezas correctas")
        return 0
    if args.command == "credits":
        print(write_credits(sources, args.out))
        return 0
    if args.command == "fetch":
        manifest = build_all(cache, sources, only=set(args.ids) or None, force=args.force)
        mb = manifest["total_bytes"] / (1024 * 1024)
        state = "cabe en el repo" if manifest["versionable"] else "no se versiona: pasa de 10 MB, se regenera con este script"
        print(f"{len(manifest['pieces'])} OGG, {mb:.1f} MB en {cache / 'ogg'} ({state})")
        return 1 if manifest["failures"] else 0
    return 1


def _pin(sources_path: Path, cache: Path, ids: set[str]) -> int:
    """Rellena el sha256 de las piezas indicadas. Solo para anadir piezas:
    despues hay que escuchar el fichero y revisar su licencia a mano."""
    raw = json.loads(sources_path.read_text(encoding="utf-8"))
    sources = load_sources(sources_path)
    by_id = {p.id: p for p in sources.pieces}
    unknown = ids - set(by_id)
    if unknown:
        print(f"ERROR ids desconocidos: {sorted(unknown)}", file=sys.stderr)
        return 1
    unsafe = sorted(i for i in ids if not safe_to_fetch(by_id[i]))
    if unsafe:
        print(f"ERROR id o download_url no validos (se descargaria fuera de la cache o de un dominio no admitido): {unsafe}",
              file=sys.stderr)
        return 1
    failed = False
    for entry in raw["pieces"]:
        if entry["id"] in ids:
            try:
                _, digest = ensure_original(cache, by_id[entry["id"]], pin=True)
            except OSError as e:
                print(f"ERROR {entry['id']}: {e}", file=sys.stderr)
                failed = True
                continue
            entry["sha256"] = digest
            print(f"{entry['id']}: {digest}")
            # Se guarda tras cada pieza: un corte a medias no pierde lo ya hecho.
            sources_path.write_text(json.dumps(raw, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
