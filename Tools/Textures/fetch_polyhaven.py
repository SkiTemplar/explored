"""CLI: descarga texturas CC0 de Poly Haven a la caché local (ver `texgen/polyhaven.py`).

Uso (desde la raíz del repositorio):
    uv run python Tools/Textures/fetch_polyhaven.py dark_rock marble_cliff_04
"""

from __future__ import annotations

import sys

from texgen.polyhaven import fetch


def main(argv: list[str] | None = None) -> None:
    assets = sys.argv[1:] if argv is None else argv
    if not assets:
        print(__doc__)
        raise SystemExit(1)
    for asset in assets:
        print(f"[polyhaven] {asset}: {fetch(asset)}")


if __name__ == "__main__":
    main()
