"""Descarga (y verifica por SHA256) el soundfont acustico y, en Windows, el
binario de FluidSynth, a `Tools/Audio/.cache/` -ignorado por git, igual que
`Tools/Textures/.cache/`.

No hace falta ejecutarlo a mano para generar musica: `explored_audio.music.
soundfont.ensure_soundfont()/ensure_fluidsynth()` descargan solas la primera
vez que hace falta renderizar. Este script existe para dejar la cache lista de
antemano (por ejemplo, antes de un `pytest` sin red disponible despues) y para
poder inspeccionar de un vistazo que se ha descargado y con que licencia.

Uso: `uv run python scripts/fetch_soundfont.py` desde `Tools/Audio`.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from explored_audio.music import soundfont  # noqa: E402


def main() -> int:
    print(f"Soundfont: {soundfont.SOUNDFONT_URL}")
    sf_path = soundfont.ensure_soundfont()
    print(f"  -> {sf_path} ({sf_path.stat().st_size / 1_000_000:.1f} MB, SHA256 verificado)")
    print("  Licencia: MIT -- ver Tools/Audio/THIRD_PARTY_SOUNDFONT.md")

    print(f"FluidSynth: {soundfont.FLUIDSYNTH_URL}")
    fs_path = soundfont.ensure_fluidsynth()
    print(f"  -> {fs_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
