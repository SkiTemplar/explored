"""Descarga de texturas CC0 de Poly Haven (https://polyhaven.com) a una caché local.

La caché no se versiona (`Tools/Textures/.cache/`). Los materiales fotobasheados
(`texgen/photobash.py`) la rellenan solos la primera vez que la necesitan.

API pública: `GET https://api.polyhaven.com/files/<id>` da, por mapa y resolución, la URL
de descarga. Todos los assets de Poly Haven son CC0; la atribución de los usados está en
`docs/art/texturas.md`.
"""

from __future__ import annotations

import json
import re
import urllib.parse
import urllib.request
from pathlib import Path

CACHE = Path(__file__).resolve().parent.parent / ".cache" / "polyhaven"
RES = "2k"
# Nombre corto -> clave del JSON de /files/<id>.
MAP_KEYS = {"diff": "Diffuse", "disp": "Displacement", "ao": "AO", "rough": "Rough"}
# api.polyhaven.com devuelve 403 sin User-Agent (bloquea el genérico de urllib).
_HEADERS = {"User-Agent": "Explored-texgen/1.0 (+https://github.com/SkiTemplar/explored)"}
# Identificadores de Poly Haven: minúsculas, dígitos y guion bajo. Evita rutas fuera de la caché.
_ASSET_ID = re.compile(r"^[a-z0-9_]{1,64}$")
# Solo se descargan ficheros del CDN oficial y por HTTPS.
_ALLOWED_HOSTS = {"dl.polyhaven.org"}
_MAX_BYTES = 64 * 1024 * 1024


def _get(url: str, timeout: int = 30):
    return urllib.request.urlopen(urllib.request.Request(url, headers=_HEADERS), timeout=timeout)


def _check_download_url(url: object) -> str:
    if not isinstance(url, str):
        raise ValueError(f"URL de descarga no válida: {url!r}")
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme != "https" or parsed.hostname not in _ALLOWED_HOSTS:
        raise ValueError(f"URL de descarga fuera de {sorted(_ALLOWED_HOSTS)}: {url}")
    return url


def asset_dir(asset: str, cache: Path = CACHE) -> Path:
    if not _ASSET_ID.fullmatch(asset):
        raise ValueError(f"Identificador de Poly Haven no válido: {asset!r}")
    return cache / asset


def fetch(asset: str, cache: Path = CACHE) -> Path:
    """Descarga (si falta) diffuse/displacement/AO/roughness a 2k PNG para `asset`."""
    out_dir = asset_dir(asset, cache)
    missing = [short for short in MAP_KEYS if not (out_dir / f"{short}_{RES}.png").exists()]
    if not missing:
        return out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    with _get(f"https://api.polyhaven.com/files/{asset}") as resp:
        files = json.load(resp)

    for short in missing:
        file_url = _check_download_url(files[MAP_KEYS[short]][RES]["png"]["url"])
        print(f"[polyhaven] {asset}/{short}: {file_url}")
        with _get(file_url) as resp:
            data = resp.read(_MAX_BYTES + 1)
        if len(data) > _MAX_BYTES:
            raise ValueError(f"{file_url} supera {_MAX_BYTES} bytes")
        dest = out_dir / f"{short}_{RES}.png"
        tmp = dest.with_suffix(".part")
        tmp.write_bytes(data)
        tmp.replace(dest)
    return out_dir
