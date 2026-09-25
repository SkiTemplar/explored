"""Orquesta la generacion de todo el catalogo: genera cada sonido, aplica el
postproceso comun (retirada de DC, techo de pico de seguridad), lo exporta a
WAV 48 kHz/16 bits y escribe `manifest.json`.

Punto de entrada: `uv run explored-audio build` o `uv run python -m
explored_audio.build`.
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from .catalog import SoundSpec, build_catalog
from .constants import SAMPLE_RATE
from .io_utils import write_wav
from .levels import enforce_peak_ceiling, linear_safety_clamp, lufs_approx, match_lufs, remove_dc
from .manifest import build_entry, write_manifest

# Los colchones de "Ambiente" son bucles de fondo: se normalizan a una
# sonoridad de referencia consistente (a diferencia de un efecto puntual,
# donde interesa conservar el factor de cresta natural del diseño).
AMBIENCE_TARGET_LUFS = -23.0


def default_output_root() -> Path:
    """Raiz `Art/Export/Audio` del repo, deducida de la ubicacion de este fichero
    (Tools/Audio/src/explored_audio/build.py -> repo/Art/Export/Audio)."""
    repo_root = Path(__file__).resolve().parents[4]
    return repo_root / "Art" / "Export" / "Audio"


def finalize(audio: np.ndarray, category: str) -> np.ndarray:
    """Postproceso comun: sin continua, sonoridad consistente en Ambiente, sin clipping."""
    audio = remove_dc(audio)
    if category == "Ambiente":
        audio = match_lufs(audio, AMBIENCE_TARGET_LUFS)
    audio = enforce_peak_ceiling(audio)
    # El limitador de picos es una saturacion no lineal (tanh): si la señal
    # de origen no era perfectamente simetrica puede desplazar la media una
    # pizca. Se retira otra vez, ya despues de todo el postproceso (el
    # desplazamiento que queda tras el limitador es minimo y no vuelve a
    # acercar el pico al techo).
    audio = remove_dc(audio)
    audio = linear_safety_clamp(audio)
    return audio


def render_sound(spec: SoundSpec) -> np.ndarray:
    """Genera y normaliza un sonido del catalogo por su spec. Determinista."""
    raw = spec.generate(spec.name)
    return finalize(np.asarray(raw, dtype=np.float64), spec.category)


def render_by_name(name: str) -> np.ndarray:
    for spec in build_catalog():
        if spec.name == name:
            return render_sound(spec)
    raise KeyError(f"sonido no encontrado en el catalogo: {name}")


def build_all(output_root: Path | None = None, verbose: bool = True) -> list[dict]:
    output_root = Path(output_root) if output_root is not None else default_output_root()
    entries: list[dict] = []

    for spec in build_catalog():
        audio = render_sound(spec)
        channels = 1 if audio.ndim == 1 else audio.shape[0]
        duration_s = audio.shape[-1] / SAMPLE_RATE
        lufs = lufs_approx(audio)

        wav_path = output_root / spec.category / f"{spec.name}.wav"
        write_wav(wav_path, audio, SAMPLE_RATE)
        entries.append(build_entry(spec.name, spec.category, spec.is_loop, channels, duration_s, lufs))

        if verbose:
            kind = "bucle" if spec.is_loop else "one-shot"
            ch = "estereo" if channels == 2 else "mono"
            print(f"  {spec.name:<28} [{spec.category:<9}] {ch:<8} {kind:<8} {duration_s:6.2f}s  {lufs:6.1f} LUFS")

    write_manifest(entries, output_root / "manifest.json")
    if verbose:
        print(f"\n{len(entries)} sonidos exportados en {output_root}")
    return entries


def main(argv: list[str] | None = None) -> int:
    build_all()
    return 0


if __name__ == "__main__":
    sys.exit(main())
