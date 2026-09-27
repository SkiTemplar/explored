"""Descarga texturas CC0 de Poly Haven (https://polyhaven.com) a una caché local.

La caché NO se versiona (`Tools/Textures/.cache/`, cubierta por el `.cache/` de
`.gitignore`). Los materiales fotobasheados (VolcanicRock, Limestone; ver
`texgen/photobash.py`) leen de aquí — si la caché está vacía, `gen_textures.py`
la rellena solo la primera vez.

Uso (desde la raíz del repositorio, sin dependencias extra: solo stdlib):
    uv run python Tools/Textures/fetch_polyhaven.py dark_rock marble_cliff_04

API pública de Poly Haven: `GET /files/<id>` da, por mapa y resolución, la URL de
descarga (png/jpg/exr). Todos los assets de Poly Haven son CC0 (dominio público);
la atribución de cada uno usado aquí está en `docs/art/texturas.md`.
"""

from __future__ import annotations

import json
import sys
import urllib.request
from pathlib import Path

CACHE = Path(__file__).resolve().parent / ".cache" / "polyhaven"
RES = "2k"
# Nombre corto -> clave del JSON de /files/<id>.
MAP_KEYS = {"diff": "Diffuse", "disp": "Displacement", "ao": "AO", "rough": "Rough"}
# api.polyhaven.com devuelve 403 sin User-Agent (bloquea el genérico de urllib).
_HEADERS = {"User-Agent": "Explored-texgen/1.0 (+https://github.com/SkiTemplar/explored)"}


def _get(url: str, timeout: int = 30):
    return urllib.request.urlopen(urllib.request.Request(url, headers=_HEADERS), timeout=timeout)


def fetch(asset: str, cache: Path = CACHE) -> Path:
    """Descarga (si falta) diffuse/displacement/AO/roughness a 2k PNG para `asset`."""
    out_dir = cache / asset
    out_dir.mkdir(parents=True, exist_ok=True)
    missing = [short for short in MAP_KEYS if not (out_dir / f"{short}_{RES}.png").exists()]
    if not missing:
        print(f"[polyhaven] {asset}: ya en caché ({out_dir})")
        return out_dir

    with _get(f"https://api.polyhaven.com/files/{asset}") as resp:
        files = json.load(resp)

    for short in missing:
        key = MAP_KEYS[short]
        dest = out_dir / f"{short}_{RES}.png"
        file_url = files[key][RES]["png"]["url"]
        print(f"[polyhaven] {asset}/{short}: {file_url}")
        with _get(file_url) as resp, open(dest, "wb") as fh:
            fh.write(resp.read())
    return out_dir


def main(argv: list[str] | None = None) -> None:
    assets = sys.argv[1:] if argv is None else argv
    if not assets:
        print(__doc__)
        raise SystemExit(1)
    for asset in assets:
        fetch(asset)


if __name__ == "__main__":
    main()
