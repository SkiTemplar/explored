"""Descarga, verificacion de hash y exportacion a OGG de la banda sonora
grabada. Los ficheros van a la cache ignorada por git:

    Tools/Audio/.cache/music/src/<id><ext>   originales tal cual se bajaron
    Tools/Audio/.cache/music/ogg/<id>.ogg    tratados (48 kHz, -16 LUFS)
    Tools/Audio/.cache/music/manifest.json   medidas de cada OGG

Un original cuyo SHA-256 no coincide con el de music_sources.json se borra
y la pieza falla: nunca se procesa un fichero distinto del que se reviso.
"""

from __future__ import annotations

import hashlib
import json
import os
import time
import urllib.error
import urllib.request
from dataclasses import asdict
from pathlib import Path

import numpy as np
import soundfile as sf

from ..constants import SAMPLE_RATE
from .loudness import integrated_lufs, sample_peak_dbfs
from .process import process
from .sources import Piece, SourceList

# Wikimedia pide un User-Agent que identifique la herramienta y como
# contactar (https://meta.wikimedia.org/wiki/User-Agent_policy).
USER_AGENT = "ExploredSoundtrackFetcher/1.0 (+https://github.com/SkiTemplar/explored)"
# Calidad Vorbis de libsndfile: 0 es la mejor, 1 la peor. 0,6 ronda q3
# (unos 110 kbps en estereo), de sobra para piano y cuerdas de fondo.
VORBIS_COMPRESSION = 0.6
OGG_WRITE_BLOCK = 1 << 15
# Por encima de este total los OGG no se versionan (encargo 21): el script
# los regenera. Ver `versionable()`.
VERSIONING_LIMIT_BYTES = 10 * 1024 * 1024
# Pausa entre descargas para no disparar el limite de ritmo de Wikimedia.
POLITE_PAUSE_S = 3.0


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path, retries: int = 6) -> None:
    """Descarga con reintentos. Ante un 429 (Wikimedia limita el ritmo) se
    respeta `Retry-After` si llega y, si no, se espera cada vez el doble."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + ".part")
    delay = 5.0
    for attempt in range(retries + 1):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=120) as r, open(tmp, "wb") as f:
                while chunk := r.read(1 << 20):
                    f.write(chunk)
            os.replace(tmp, dest)
            time.sleep(POLITE_PAUSE_S)
            return
        except OSError as e:
            tmp.unlink(missing_ok=True)
            if attempt == retries:
                raise
            wait = delay
            if isinstance(e, urllib.error.HTTPError):
                if e.code not in (429, 500, 502, 503, 504):
                    raise
                retry_after = e.headers.get("Retry-After", "") if e.headers else ""
                if retry_after.isdigit():
                    wait = max(wait, float(retry_after))
            time.sleep(wait)
            delay *= 2


def original_path(cache: Path, piece: Piece) -> Path:
    return cache / "src" / f"{piece.id}{piece.extension}"


def ogg_path(cache: Path, piece: Piece) -> Path:
    return cache / "ogg" / f"{piece.id}.ogg"


def ensure_original(cache: Path, piece: Piece, pin: bool = False) -> tuple[Path, str]:
    """Descarga el original si falta y comprueba su SHA-256.

    Con `pin=True` no se exige hash: se devuelve el calculado para que el
    llamante lo escriba en music_sources.json (solo al anadir piezas)."""
    path = original_path(cache, piece)
    if path.exists() and not pin and sha256_file(path) != piece.sha256:
        path.unlink()  # cache corrupta o de otra version: se vuelve a bajar
    if not path.exists():
        download(piece.download_url, path)
    digest = sha256_file(path)
    if not pin and digest != piece.sha256:
        path.unlink()
        raise ValueError(f"{piece.id}: SHA-256 {digest} no coincide con el esperado {piece.sha256}")
    return path, digest


def export_ogg(path: Path, audio: np.ndarray, piece: Piece) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp.ogg")
    with sf.SoundFile(
        tmp, "w", samplerate=SAMPLE_RATE, channels=2, format="OGG", subtype="VORBIS",
        compression_level=VORBIS_COMPRESSION,
    ) as f:
        f.title = piece.title
        f.artist = f"{piece.performer} ({piece.composer})"
        f.copyright = f"{piece.license} {piece.license_url}"
        f.comment = piece.source_url
        # Por bloques: libsndfile 1.2 revienta (segfault) si recibe minutos
        # de audio Vorbis en una sola llamada a write.
        data = np.ascontiguousarray(audio, dtype=np.float32)
        for i in range(0, len(data), OGG_WRITE_BLOCK):
            f.write(data[i : i + OGG_WRITE_BLOCK])
    os.replace(tmp, path)


def measure_ogg(path: Path) -> dict[str, float]:
    audio, fs = sf.read(path, dtype="float64", always_2d=True)
    return {
        "seconds": round(len(audio) / fs, 3),
        "lufs": round(integrated_lufs(audio, fs), 2),
        "peak_dbfs": round(sample_peak_dbfs(audio), 2),
        "bytes": path.stat().st_size,
    }


def build_piece(cache: Path, sources: SourceList, piece: Piece, force: bool = False) -> dict:
    src, _ = ensure_original(cache, piece)
    out = ogg_path(cache, piece)
    stamp = out.with_suffix(".sha256")
    # Se reprocesa si cambia el original, los parametros o el propio OGG.
    key = f"{piece.sha256}:{sources.target_lufs}:{sources.peak_ceiling_dbfs}:{VORBIS_COMPRESSION}"
    if not force and out.exists() and stamp.exists() and stamp.read_text().strip() == key:
        return {"id": piece.id, "cached": True, **measure_ogg(out)}
    audio, fs = sf.read(src, dtype="float64", always_2d=True)
    processed, report = process(audio, fs, sources.target_lufs, sources.peak_ceiling_dbfs)
    export_ogg(out, processed, piece)
    stamp.write_text(key + "\n")
    return {"id": piece.id, "cached": False, "process": asdict(report), **measure_ogg(out)}


def versionable(total_bytes: int) -> bool:
    return total_bytes <= VERSIONING_LIMIT_BYTES


def build_all(cache: Path, sources: SourceList, only: set[str] | None = None, force: bool = False, log=print) -> dict:
    results = []
    failures = []
    for piece in sources.pieces:
        if only and piece.id not in only:
            continue
        try:
            r = build_piece(cache, sources, piece, force=force)
            results.append(r)
            log(f"{piece.id}: {r['seconds']:.1f} s, {r['lufs']:.1f} LUFS, pico {r['peak_dbfs']:.1f} dBFS"
                + (" (cache)" if r["cached"] else ""))
        except Exception as e:  # noqa: BLE001 - se informa y se sigue con el resto
            failures.append({"id": piece.id, "error": str(e)})
            log(f"{piece.id}: ERROR {e}")
    total = sum(r["bytes"] for r in results)
    manifest = {
        "target_lufs": sources.target_lufs,
        "peak_ceiling_dbfs": sources.peak_ceiling_dbfs,
        "total_bytes": total,
        "versionable": versionable(total),
        "pieces": results,
        "failures": failures,
    }
    cache.mkdir(parents=True, exist_ok=True)
    (cache / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest
