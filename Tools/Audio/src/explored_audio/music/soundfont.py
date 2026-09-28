"""Adquisicion y uso del soundfont acustico y de FluidSynth (el renderizador),
para que `sequencer.py` pueda sostener la misma partitura (compose.py, sin
cambios) con muestras reales en vez de sintesis por numpy.

Ni el soundfont ni el binario de FluidSynth se versionan: se descargan una
vez a `Tools/Audio/.cache/` (ya cubierto por el `.cache/` del `.gitignore`
raiz, el mismo patron que usa `Tools/Textures` para sus originales de
Poly Haven) y se verifican por SHA256 antes de usarse. `scripts/fetch_soundfont.py`
hace la descarga explicita; las funciones de aqui la disparan tambien solas la
primera vez que hace falta renderizar, para que `pytest` funcione sin un paso
manual previo (necesita red la primera vez; las siguientes usan la cache).

Soundfont elegido: **FluidR3Mono_GM.sf3** (version mono, comprimida, de
FluidR3 GM), tal como lo distribuye MuseScore en su propio repositorio -
fuente oficial y verificable, con el texto de la licencia MIT copiado en
`Tools/Audio/THIRD_PARTY_SOUNDFONT.md`. Se prefiere a la version estereo
original por pesar una tercera parte (~24 MB) para el mismo set General MIDI.
"""

from __future__ import annotations

import hashlib
import platform
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile
from pathlib import Path

import numpy as np
import soundfile

# Fuente: repositorio oficial de MuseScore, fijado a un commit concreto (la
# rama `main` puede cambiar el fichero y romper el hash sin aviso).
# Licencia: MIT (Frank Wen 2000-2002, conversion mono Michael Cowgill 2014-17;
# texto completo en THIRD_PARTY_SOUNDFONT.md, copiado de FluidR3Mono_License.md
# del mismo commit, que tambien se descarga y se verifica junto al soundfont).
MUSESCORE_COMMIT = "894e82c1b12937021eb024305ef21c64335e21ce"
_MUSESCORE_RAW = f"https://raw.githubusercontent.com/musescore/MuseScore/{MUSESCORE_COMMIT}/share/sound"
SOUNDFONT_URL = f"{_MUSESCORE_RAW}/FluidR3Mono_GM.sf3"
SOUNDFONT_SHA256 = "2aacd036d7058d40a371846ef2f5dc5f130d648ab3837fe2626591ba49a71254"
SOUNDFONT_FILENAME = "FluidR3Mono_GM.sf3"
SOUNDFONT_LICENSE_URL = f"{_MUSESCORE_RAW}/FluidR3Mono_License.md"
SOUNDFONT_LICENSE_SHA256 = "0fa7d85b3114adb91cebd42fe955e22df6b19917e9c8e19c401080c075975636"
SOUNDFONT_LICENSE_FILENAME = "FluidR3Mono_License.md"
SOUNDFONT_LICENSE = "MIT"
# Por encima de este tamaño un recurso de terceros no se versiona nunca (ni
# siquiera con LFS): se descarga a la cache. El soundfont ronda los 24 MB,
# pero tampoco se versiona: descargarlo y verificarlo cuesta segundos.
MAX_VERSIONED_BYTES = 50 * 1024 * 1024

# Fuente: https://github.com/FluidSynth/fluidsynth/releases/tag/v2.6.1 (build oficial
# de la propia organizacion FluidSynth, licencia LGPL). Solo se usa como
# herramienta de build (no se distribuye ni se versiona), asi que la LGPL no
# afecta a la licencia del propio repositorio.
FLUIDSYNTH_URL = "https://github.com/FluidSynth/fluidsynth/releases/download/v2.6.1/fluidsynth-v2.6.1-win10-x64-cpp11.zip"
FLUIDSYNTH_SHA256 = "fab7a2e4b85675b66970f97a39bbc239729c5e0f237198b5922a6a73cbc8677c"
FLUIDSYNTH_EXE_RELATIVE = Path("fluidsynth-v2.6.1-win10-x64-cpp11") / "bin" / "fluidsynth.exe"


def _cache_dir() -> Path:
    """`Tools/Audio/.cache` (deducido de la ubicacion de este fichero, igual
    que `music/layers.py` deduce la raiz del repo)."""
    return Path(__file__).resolve().parents[3] / ".cache"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _download_verified(url: str, dest: Path, expected_sha256: str) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_name(dest.name + ".part")
    try:
        urllib.request.urlretrieve(url, tmp)  # noqa: S310 - URL fija a una fuente oficial fijada arriba
    except OSError as error:
        raise RuntimeError(
            f"No se pudo descargar {url}: {error}. Reintenta con conexion a "
            f"internet, o coloca el fichero a mano en {dest}."
        ) from error
    digest = _sha256(tmp)
    if digest != expected_sha256:
        tmp.unlink(missing_ok=True)
        raise RuntimeError(
            f"SHA256 inesperado para {url}: se obtuvo {digest}, se esperaba "
            f"{expected_sha256}. Descarga corrupta o el fichero remoto cambio; no se usa."
        )
    tmp.replace(dest)


# Ficheros de la cache cuyo hash ya se comprobo en este proceso (hashear
# 24 MB en cada pieza renderizada seria tiempo perdido).
_verified: set[Path] = set()


def _ensure_cached(url: str, dest: Path, expected_sha256: str) -> Path:
    """Devuelve `dest` verificado por SHA256. Si falta, lo descarga; si esta
    pero no coincide (descarga a medias de otra version, fichero tocado a
    mano), lo descarta y lo vuelve a descargar en vez de usarlo."""
    if dest in _verified and dest.exists():
        return dest
    if dest.exists() and _sha256(dest) != expected_sha256:
        dest.unlink()
    if not dest.exists():
        _download_verified(url, dest, expected_sha256)
    _verified.add(dest)
    return dest


def ensure_soundfont() -> Path:
    """Descarga (si hace falta) y devuelve la ruta local al soundfont, ya
    verificado por SHA256, junto con el texto de su licencia."""
    folder = _cache_dir() / "soundfont"
    _ensure_cached(SOUNDFONT_LICENSE_URL, folder / SOUNDFONT_LICENSE_FILENAME, SOUNDFONT_LICENSE_SHA256)
    return _ensure_cached(SOUNDFONT_URL, folder / SOUNDFONT_FILENAME, SOUNDFONT_SHA256)


def ensure_fluidsynth() -> Path:
    """Devuelve la ruta al ejecutable de FluidSynth: el del PATH del sistema
    si ya hay uno instalado, o si no (y estamos en Windows) el binario oficial
    cacheado, descargandolo la primera vez."""
    found = shutil.which("fluidsynth") or shutil.which("fluidsynth.exe")
    if found:
        return Path(found)

    if platform.system() != "Windows":
        raise RuntimeError(
            "No se encontro 'fluidsynth' en el PATH y la descarga automatica solo "
            "cubre Windows. Instalalo con el gestor de paquetes del sistema "
            "(por ejemplo 'apt install fluidsynth' o 'brew install fluid-synth')."
        )

    extract_dir = _cache_dir() / "fluidsynth"
    exe = extract_dir / FLUIDSYNTH_EXE_RELATIVE
    if exe.exists():
        return exe

    zip_path = _cache_dir() / "fluidsynth-download" / "fluidsynth-win10-x64.zip"
    _download_verified(FLUIDSYNTH_URL, zip_path, FLUIDSYNTH_SHA256)
    with zipfile.ZipFile(zip_path) as zf:
        zf.extractall(extract_dir)
    if not exe.exists():
        raise RuntimeError(f"fluidsynth.exe no aparecio tras extraer {zip_path} en {extract_dir}")
    return exe


def render_midi_to_stereo(midi_path: Path, sr: int) -> np.ndarray:
    """Renderiza un fichero MIDI con el soundfont acustico a un array estereo
    `(2, N)` (misma convencion canal-primero que el resto del modulo `music`).

    El reverb y el chorus internos de FluidSynth se desactivan (`-R 0 -C 0`):
    la reverberacion de sala/placa la aplica `reverb.schroeder_reverb` despues,
    igual que hacia la version sintetizada, para conservar un unico punto de
    control sobre el espacio de cada pieza."""
    exe = ensure_fluidsynth()
    sf2 = ensure_soundfont()
    with tempfile.TemporaryDirectory(prefix="explored_audio_fluidsynth_") as tmp:
        out_wav = Path(tmp) / "render.wav"
        cmd = [
            str(exe), "-ni", "-R", "0", "-C", "0", "-g", "1.0",
            "-r", str(sr), "-F", str(out_wav), str(sf2), str(midi_path),
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, check=False)
        if result.returncode != 0 or not out_wav.exists():
            raise RuntimeError(
                f"fluidsynth fallo renderizando {midi_path} (codigo {result.returncode}):\n"
                f"{result.stdout}\n{result.stderr}"
            )
        data, file_sr = soundfile.read(str(out_wav), dtype="float64", always_2d=True)
    if file_sr != sr:
        raise RuntimeError(f"fluidsynth devolvio {file_sr} Hz, se esperaban {sr} Hz")
    return data.T
