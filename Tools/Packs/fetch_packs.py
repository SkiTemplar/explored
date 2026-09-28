"""Descarga y verifica los packs CC0 de ``packs.json`` en una caché fuera del repo.

Uso (desde ``Tools/Packs``):
    uv run python fetch_packs.py                 # todos los packs
    uv run python fetch_packs.py kenney_survival_kit kaykit_resource_bits
    uv run python fetch_packs.py --list          # packs, versión y licencia
    uv run python fetch_packs.py --update-sha    # rellena sha256 vacíos (solo al añadir un pack)

Caché: ``Art/Packs/`` (en ``.gitignore``) o ``$EXPLORED_PACKS_CACHE``. Cada pack queda como
``<caché>/<id>.zip`` (el original, verificado con sha256) y ``<caché>/<id>/`` (extraído).
Nunca se versionan los ficheros de los packs: solo este script, el manifiesto y el catálogo.

Fuentes admitidas:
- ``direct``: URL fija al zip (Kenney publica cada versión con su propio hash en la ruta).
- ``itch``: página de itch.io con precio libre («name your own price», mínimo 0). Se usa el
  mismo flujo que el botón «No thanks, just take me to the downloads»: POST a
  ``<página>/file/<upload_id>`` devuelve una URL firmada de corta duración. ``upload_id``
  fija el fichero concreto; si el autor lo sustituye, cambia el id o el sha256 y falla.
"""

from __future__ import annotations

import argparse
import hashlib
import http.cookiejar
import json
import os
import re
import shutil
import sys
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parent.parent
MANIFEST = HERE / "packs.json"
USER_AGENT = "Mozilla/5.0 (Explored fetch_packs.py)"

CSRF_RE = re.compile(r'name="csrf_token" (?:value|content)="([^"]+)"')


def cache_dir() -> Path:
    env = os.environ.get("EXPLORED_PACKS_CACHE")
    return Path(env) if env else REPO_ROOT / "Art" / "Packs"


def load_manifest(path: Path = MANIFEST) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _opener() -> urllib.request.OpenerDirector:
    jar = http.cookiejar.CookieJar()
    op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))
    op.addheaders = [("User-Agent", USER_AGENT)]
    return op


def _read(op: urllib.request.OpenerDirector, url: str, data: dict | None = None) -> bytes:
    body = urllib.parse.urlencode(data).encode() if data is not None else None
    with op.open(urllib.request.Request(url, data=body), timeout=60) as r:
        return r.read()


def itch_file_url(op: urllib.request.OpenerDirector, page: str, upload_id: int) -> str:
    """URL firmada del upload ``upload_id`` de una página itch.io de precio libre."""
    html = _read(op, page).decode("utf-8", "replace")
    m = CSRF_RE.search(html)
    token = m.group(1) if m else ""
    raw = _read(op, f"{page}/file/{upload_id}?source=game_download", {"csrf_token": token})
    reply = json.loads(raw)
    if "url" not in reply:
        raise RuntimeError(f"itch.io no dio URL para {page} upload {upload_id}: {reply}")
    return reply["url"]


def download(pack: dict, dest: Path) -> None:
    op = _opener()
    src = pack["source"]
    if src["kind"] == "direct":
        url = src["url"]
    elif src["kind"] == "itch":
        url = itch_file_url(op, src["page"], src["uploadId"])
    else:
        raise ValueError(f"fuente desconocida: {src['kind']}")
    tmp = dest.with_suffix(".part")
    with op.open(urllib.request.Request(url), timeout=300) as r, tmp.open("wb") as f:
        shutil.copyfileobj(r, f)
    tmp.replace(dest)


def fetch(pack: dict, root: Path, *, update_sha: bool = False) -> Path:
    """Descarga (si falta), verifica sha256 y extrae. Devuelve la carpeta extraída.

    ``pack["archive"]`` (opcional, por defecto ``"zip"``) da la extensión real del fichero
    descargado. Con ``"zip"`` se descomprime como siempre; con cualquier otra extensión
    (p. ej. ``"glb"``) el fichero se copia tal cual dentro de ``out/`` sin descomprimir, para
    los modelos sueltos de Poly Pizza (CC0 o CC-BY, un único ``.glb`` sin zip).
    """
    root.mkdir(parents=True, exist_ok=True)
    ext = pack.get("archive", "zip")
    archive = root / f"{pack['id']}.{ext}"
    out = root / pack["id"]
    expected = pack.get("sha256") or ""
    if archive.exists() and expected and sha256_of(archive) != expected:
        archive.unlink()
    if not archive.exists():
        download(pack, archive)
    got = sha256_of(archive)
    if not expected:
        if not update_sha:
            raise RuntimeError(f"{pack['id']}: sin sha256 en packs.json (usa --update-sha al añadirlo)")
        pack["sha256"] = got
    elif got != expected:
        archive.unlink()
        raise RuntimeError(f"{pack['id']}: sha256 {got} != {expected} (el autor cambió el fichero)")
    stamp = out / ".sha256"
    if not (stamp.exists() and stamp.read_text().strip() == got):
        if out.exists():
            shutil.rmtree(out)
        if ext == "zip":
            with zipfile.ZipFile(archive) as z:
                for info in z.infolist():
                    target = (out / info.filename).resolve()
                    if not target.is_relative_to(out.resolve()):
                        raise RuntimeError(f"{pack['id']}: ruta fuera de la carpeta en el zip: {info.filename}")
                z.extractall(out)
        else:
            out.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(archive, out / archive.name)
        stamp.write_text(got + "\n")
    return out


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("ids", nargs="*", help="ids del manifiesto (por defecto, todos)")
    parser.add_argument("--list", action="store_true", help="lista los packs sin descargar")
    parser.add_argument("--update-sha", action="store_true", help="escribe en el manifiesto los sha256 vacíos")
    parser.add_argument("--manifest", type=Path, default=MANIFEST,
        help="manifiesto a usar (por defecto packs.json; packs_cc_by.json para los CC-BY con crédito)")
    args = parser.parse_args(argv)

    manifest = load_manifest(args.manifest)
    packs = {p["id"]: p for p in manifest["packs"]}
    if args.list:
        for p in packs.values():
            print(f"{p['id']:34} {p['author']:10} {p['version']:8} {p['license']['spdx']}")
        return 0
    unknown = [i for i in args.ids if i not in packs]
    if unknown:
        print(f"Packs desconocidos: {', '.join(unknown)}", file=sys.stderr)
        return 2
    root = cache_dir()
    failed = 0
    for pid in args.ids or list(packs):
        try:
            out = fetch(packs[pid], root, update_sha=args.update_sha)
            print(f"[packs] {pid}: {out}")
        except Exception as e:  # noqa: BLE001 - se informa y se sigue con el resto
            failed += 1
            print(f"[packs] {pid}: ERROR {e}", file=sys.stderr)
    if args.update_sha:
        args.manifest.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
