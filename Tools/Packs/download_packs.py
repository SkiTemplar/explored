"""Descarga los packs CC0 usados en la vegetacion low poly de Landing.

Uso:
    uv run python Tools/Packs/download_packs.py [--pack <id> ...] [--force]

Descarga cada pack de PACKS a Tools/Packs/.cache/<id>/ (cache ignorada por git,
cubierta por la regla generica ".cache/" del .gitignore del repo) y lo
descomprime ahi mismo. No versiona los zips crudos.

Packs que requieren login (Quaternius vía Google Drive, KayKit vía itch.io) no
se pueden descargar por script sin credenciales: se listan en BLOCKED_PACKS
con instrucciones para descarga manual. Ver Tools/Packs/packs.json para el
manifiesto completo (licencia, version, que se uso de cada uno).
"""
from __future__ import annotations

import argparse
import io
import sys
import urllib.request
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CACHE_DIR = Path(__file__).resolve().parent / ".cache"

# Packs descargables por script: URL directa y verificable (probada con curl -I
# el 2026-09-27; devuelve 200 y Content-Type: application/zip).
PACKS: dict[str, dict] = {
    "kenney-nature-kit": {
        "url": "https://kenney.nl/media/pages/assets/nature-kit/37ac38a37b-1677698939/kenney_nature-kit.zip",
        "license": "CC0 1.0",
        "source": "https://kenney.nl/assets/nature-kit",
    },
    "kenney-survival-kit": {
        "url": "https://kenney.nl/media/pages/assets/survival-kit/4065a8185b-1712149243/kenney_survival-kit.zip",
        "license": "CC0 1.0",
        "source": "https://kenney.nl/assets/survival-kit",
    },
}

# Packs que el director pidió pero que no se pueden bajar sin sesion (Quaternius
# reparte por carpeta de Google Drive; KayKit por itch.io con login/claim). Se
# documentan para que Rodrigo los baje a mano si hacen falta mas variantes.
BLOCKED_PACKS: dict[str, str] = {
    "quaternius-ultimate-stylized-nature": (
        "Google Drive (requiere sesion para bajar la carpeta entera): "
        "https://drive.google.com/drive/folders/1IV3bXHzkNvuNWFHPi4KPx-G4ghuxIuT-"
    ),
    "quaternius-ultimate-nature": (
        "Google Drive (requiere sesion): "
        "https://drive.google.com/drive/folders/1-Kl0L_Jg8awbh0S5T-z3zxh4mVlnxTpa"
    ),
    "quaternius-stylized-nature-megakit": (
        "itch.io (requiere cuenta/login para reclamar el pack gratuito): "
        "https://quaternius.itch.io/stylized-nature-megakit"
    ),
    "kaykit": (
        "itch.io (requiere cuenta/login): https://kaykit.itch.io"
    ),
}


def download_and_extract(pack_id: str, info: dict, force: bool) -> None:
    dest = CACHE_DIR / pack_id
    if dest.exists() and not force:
        print(f"[skip] {pack_id}: ya esta en cache ({dest})")
        return
    print(f"[get ] {pack_id}: {info['url']}")
    req = urllib.request.Request(info["url"], headers={"User-Agent": "Explored-Tools/1.0"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        data = resp.read()
    dest.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        zf.extractall(dest)
    print(f"[ok  ] {pack_id}: {len(data) / 1024:.0f} KB -> {dest}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pack", action="append", dest="packs", help="Descargar solo estos ids (repetible)")
    parser.add_argument("--force", action="store_true", help="Re-descargar aunque ya este en cache")
    args = parser.parse_args()

    targets = args.packs or list(PACKS)
    CACHE_DIR.mkdir(parents=True, exist_ok=True)

    failures = []
    for pack_id in targets:
        if pack_id in BLOCKED_PACKS:
            print(f"[skip] {pack_id}: necesita login -> {BLOCKED_PACKS[pack_id]}")
            continue
        info = PACKS.get(pack_id)
        if not info:
            print(f"[warn] pack desconocido: {pack_id}")
            continue
        try:
            download_and_extract(pack_id, info, args.force)
        except Exception as exc:  # noqa: BLE001 - se reporta y se sigue con el resto
            print(f"[fail] {pack_id}: {exc}")
            failures.append(pack_id)

    print()
    print("Packs bloqueados por login (descarga manual, ver Tools/Packs/packs.json):")
    for pack_id, note in BLOCKED_PACKS.items():
        print(f"  - {pack_id}: {note}")

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
