"""Exporta a WAV las piezas grabadas que usa el juego (Content/Data/soundtrack.json)
para importarlas en Unreal con Tools/Unreal/import_audio.py.

Lee los OGG tratados de la caché de `explored-music fetch` y escribe
`Art/Export/Audio/Grabada/<id>.wav` (48 kHz, 16 bits) más
`Art/Export/Audio/manifest_soundtrack.json`, con el mismo formato que el
manifiesto de `explored-audio build` y la carpeta de destino de cada pieza
(`content_root`).

Uso (desde Tools/Audio):
    uv run explored-music fetch
    uv run python scripts/export_soundtrack.py
    EXPLORED_AUDIO_MANIFEST=Art/Export/Audio/manifest_soundtrack.json \
        powershell -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_audio.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import soundfile as sf

REPO = Path(__file__).resolve().parents[3]
SOUNDTRACK = REPO / "Content" / "Data" / "soundtrack.json"
CACHE = REPO / "Tools" / "Audio" / ".cache" / "music" / "ogg"
OUT = REPO / "Art" / "Export" / "Audio"


def main() -> int:
    data = json.loads(SOUNDTRACK.read_text(encoding="utf-8"))
    root = data["assetRoot"]
    entries: list[dict] = []
    missing: list[str] = []
    for piece in data["pieces"]:
        source = CACHE / f"{piece['id']}.ogg"
        if not source.exists():
            missing.append(piece["id"])
            continue
        audio, rate = sf.read(source, always_2d=True)
        dest = OUT / "Grabada" / f"{piece['id']}.wav"
        dest.parent.mkdir(parents=True, exist_ok=True)
        sf.write(dest, audio, rate, subtype="PCM_16")
        entries.append({
            "name": piece["id"],
            "category": "Musica",
            "loop": False,
            "channels": int(audio.shape[1]),
            "duration_s": round(audio.shape[0] / rate, 3),
            "file": f"Grabada/{piece['id']}.wav",
            "content_root": root,
        })
        print(f"  {piece['id']:<40} {audio.shape[0] / rate:7.1f} s")
    if missing:
        print("Faltan en la caché (ejecuta `uv run explored-music fetch`): " + ", ".join(missing), file=sys.stderr)
        return 1
    manifest = {"version": 1, "sample_rate": 48000, "bit_depth": 16, "count": len(entries), "sounds": entries}
    (OUT / "manifest_soundtrack.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"{len(entries)} piezas en {OUT / 'Grabada'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
