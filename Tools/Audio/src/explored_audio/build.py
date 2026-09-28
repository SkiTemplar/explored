"""Orquesta la generacion de todo el catalogo: genera cada sonido, aplica el
postproceso comun (retirada de DC, techo de pico de seguridad), lo exporta a
WAV 48 kHz/16 bits y escribe `manifest.json`.

Punto de entrada: `uv run explored-audio build` o `uv run python -m
explored_audio.build`.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

from .catalog import SoundSpec, build_catalog
from .constants import SAMPLE_RATE
from .io_utils import write_wav
from .levels import enforce_peak_ceiling, linear_safety_clamp, lufs_approx, master_to_lufs, match_lufs, remove_dc
from .manifest import build_entry, write_manifest

# Los colchones de "Ambiente" son bucles de fondo: se normalizan a una
# sonoridad de referencia consistente (a diferencia de un efecto puntual,
# donde interesa conservar el factor de cresta natural del diseño).
AMBIENCE_TARGET_LUFS = -23.0
# La musica se mezcla mas alta que un colchon de ambiente (es la protagonista
# cuando suena) pero deja margen bajo el techo de pico para las capas de
# efectos y ambiente que conviven con ella en el juego. Es sonoridad
# INTEGRADA con puertas (BS.1770, `levels.integrated_lufs`), no la
# aproximacion `lufs_approx` del resto del catalogo.
MUSIC_TARGET_LUFS = -16.0


def default_output_root() -> Path:
    """Raiz `Art/Export/Audio` del repo, deducida de la ubicacion de este fichero
    (Tools/Audio/src/explored_audio/build.py -> repo/Art/Export/Audio)."""
    repo_root = Path(__file__).resolve().parents[4]
    return repo_root / "Art" / "Export" / "Audio"


def finalize(audio: np.ndarray, category: str, is_loop: bool = False) -> np.ndarray:
    """Postproceso comun: sin continua, sonoridad consistente en Ambiente y
    Musica, sin clipping.

    La musica pasa por un master propio (`levels.master_to_lufs`): ganancia a
    -16 LUFS integrados y limitador de pico verdadero con anticipacion, que
    no satura como el `tanh` de seguridad. En los bucles el limitador trata
    la pieza como circular, para que la ganancia no salte en la union."""
    if not np.all(np.isfinite(audio)):
        # Un NaN atraviesa el limitador (`tanh(nan)`) y acabaria en el WAV.
        raise ValueError("el generador ha producido valores no finitos (NaN/inf)")
    audio = remove_dc(audio)
    if category == "Ambiente":
        audio = match_lufs(audio, AMBIENCE_TARGET_LUFS)
    elif category == "Musica":
        audio = master_to_lufs(audio, MUSIC_TARGET_LUFS, sr=SAMPLE_RATE, circular=is_loop)
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
    try:
        return finalize(np.asarray(raw, dtype=np.float64), spec.category, spec.is_loop)
    except ValueError as exc:
        raise ValueError(f"{spec.name}: {exc}") from exc


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


# Tolerancia al comparar la duracion renderizada con `music_layers.json`: el
# JSON redondea a 6 decimales y el render se corta a muestras enteras.
LAYER_DURATION_TOLERANCE_S = 0.05


def build_music_layers(
    layers_path: Path | None = None,
    output_root: Path | None = None,
    verbose: bool = True,
) -> list[dict]:
    """Renderiza la musica segun `Content/Data/music_layers.json`: una capa
    (WAV) por cada pieza que declara el JSON, mas la muestra de flauta, en
    `<output_root>/Musica` (la flauta, en `Efectos`). Es el mismo JSON que
    lee el director de musica del juego, asi que lo que se genera es
    exactamente lo que el juego espera encontrar.

    Falla (ValueError) si el JSON nombra una pieza que el catalogo no sabe
    generar, o si la duracion renderizada no cuadra con la declarada: un
    bucle debe durar lo mismo (el director cuenta vueltas por compases) y una
    pieza sin bucle nunca menos."""
    from .levels import integrated_lufs
    from .music.layers import default_output_path

    layers_path = Path(layers_path) if layers_path is not None else default_output_path()
    output_root = Path(output_root) if output_root is not None else default_output_root()
    payload = json.loads(layers_path.read_text(encoding="utf-8"))
    pieces = payload.get("pieces")
    if not isinstance(pieces, list) or not pieces:
        raise ValueError(f"{layers_path}: no declara ninguna pieza en 'pieces'")

    specs = {spec.name: spec for spec in build_catalog()}
    entries: list[dict] = []
    for piece in pieces:
        piece_id = piece.get("id") if isinstance(piece, dict) else None
        spec = specs.get(piece_id) if isinstance(piece_id, str) else None
        if spec is None or spec.category != "Musica":
            raise ValueError(f"{layers_path}: pieza desconocida en el catalogo de musica: {piece_id!r}")
        audio = render_sound(spec)
        duration_s = audio.shape[-1] / SAMPLE_RATE
        expected_s = float(piece["duration_s"])
        if bool(piece["loop"]) != spec.is_loop:
            raise ValueError(f"{piece_id}: el JSON dice loop={piece['loop']} y la partitura {spec.is_loop}")
        if spec.is_loop and abs(duration_s - expected_s) > LAYER_DURATION_TOLERANCE_S:
            raise ValueError(f"{piece_id}: dura {duration_s:.3f} s y el JSON declara {expected_s:.3f} s")
        if not spec.is_loop and duration_s < expected_s - LAYER_DURATION_TOLERANCE_S:
            raise ValueError(f"{piece_id}: dura {duration_s:.3f} s, menos que los {expected_s:.3f} s del JSON")
        write_wav(output_root / spec.category / f"{spec.name}.wav", audio, SAMPLE_RATE)
        lufs = integrated_lufs(audio, SAMPLE_RATE)
        entries.append(build_entry(spec.name, spec.category, spec.is_loop, audio.shape[0], duration_s, lufs))
        if verbose:
            print(f"  {spec.name:<28} {duration_s:7.2f}s  {lufs:6.1f} LUFS")

    flute_name = (payload.get("flute") or {}).get("sample")
    if isinstance(flute_name, str) and flute_name in specs:
        flute_spec = specs[flute_name]
        flute = render_sound(flute_spec)
        write_wav(output_root / flute_spec.category / f"{flute_name}.wav", flute, SAMPLE_RATE)
        entries.append(build_entry(flute_name, flute_spec.category, flute_spec.is_loop, 1, flute.shape[-1] / SAMPLE_RATE, lufs_approx(flute)))
        if verbose:
            print(f"  {flute_name:<28} (muestra de la flauta)")
    return entries


def main(argv: list[str] | None = None) -> int:
    build_all()
    return 0


if __name__ == "__main__":
    sys.exit(main())
