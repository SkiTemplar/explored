"""Comprobaciones sobre el audio real de la cache (Tools/Audio/.cache/music).

El audio no se versiona, asi que estos tests se saltan si la cache no
existe. Para exigirlos (tras `uv run explored-music fetch`):

    EXPLORED_MUSIC_REQUIRE_CACHE=1 uv run pytest tests/test_recorded_cache.py
"""

from __future__ import annotations

import os

import numpy as np
import pytest
import soundfile as sf

from explored_audio.constants import SAMPLE_RATE
from explored_audio.recorded.fetch import VERSIONING_LIMIT_BYTES, ogg_path, original_path, sha256_file
from explored_audio.recorded.loudness import integrated_lufs, sample_peak_dbfs
from explored_audio.recorded.sources import default_cache_root, load_sources

SOURCES = load_sources()
CACHE = default_cache_root()
REQUIRE = os.environ.get("EXPLORED_MUSIC_REQUIRE_CACHE") == "1"
IDS = [p.id for p in SOURCES.pieces]
# El codificador Vorbis puede subir el pico unas decimas sobre el techo al
# que se limito antes de codificar: se admite 1 dB de margen, nunca 0 dBFS.
OGG_PEAK_MARGIN_DB = 1.0


def _need(path):
    if not path.exists():
        if REQUIRE:
            pytest.fail(f"falta {path}: ejecuta `uv run explored-music fetch`")
        pytest.skip(f"sin cache de musica ({path.name}); ver docstring del modulo")


@pytest.fixture(scope="module", params=SOURCES.pieces, ids=IDS)
def decoded(request):
    piece = request.param
    path = ogg_path(CACHE, piece)
    _need(path)
    audio, fs = sf.read(path, dtype="float64", always_2d=True)
    return piece, audio, fs


@pytest.mark.parametrize("piece", SOURCES.pieces, ids=IDS)
def test_hash_del_original_cuadra(piece):
    path = original_path(CACHE, piece)
    _need(path)
    assert sha256_file(path) == piece.sha256


def test_formato_48k_estereo(decoded):
    piece, audio, fs = decoded
    assert fs == SAMPLE_RATE, piece.id
    assert audio.shape[1] == 2, piece.id
    assert audio.shape[0] > 20 * fs, f"{piece.id}: menos de 20 s tras recortar"


def test_sin_clipping(decoded):
    piece, audio, _ = decoded
    assert np.all(np.isfinite(audio)), piece.id
    peak = sample_peak_dbfs(audio)
    assert peak < 0.0, f"{piece.id}: satura ({peak:.2f} dBFS)"
    assert peak <= SOURCES.peak_ceiling_dbfs + OGG_PEAK_MARGIN_DB, f"{piece.id}: pico {peak:.2f} dBFS"
    # Ni una racha de muestras pegadas al pico (firma de un recorte duro).
    at_peak = np.abs(audio) >= 10 ** (peak / 20) * 0.9999
    assert at_peak.sum() < 8, f"{piece.id}: {at_peak.sum()} muestras en el pico"


def test_lufs_en_objetivo(decoded):
    piece, audio, fs = decoded
    lufs = integrated_lufs(audio, fs)
    assert abs(lufs - SOURCES.target_lufs) <= SOURCES.lufs_tolerance, f"{piece.id}: {lufs:.2f} LUFS"


def test_silencios_recortados(decoded):
    piece, audio, fs = decoded
    win = int(0.5 * fs)
    head = np.max(np.abs(audio[:win]))
    # Tras el recorte, el primer medio segundo ya tiene sonido; el final
    # cae con un fundido, asi que se mira el segundo anterior a ese fundido.
    tail = np.max(np.abs(audio[-2 * fs : -fs]))
    assert head > 10 ** (-60 / 20), f"{piece.id}: empieza con silencio"
    assert tail > 10 ** (-60 / 20), f"{piece.id}: acaba con silencio"


def test_regla_de_los_10_mb():
    paths = [ogg_path(CACHE, p) for p in SOURCES.pieces]
    missing = [p for p in paths if not p.exists()]
    if missing:
        _need(missing[0])
    total = sum(p.stat().st_size for p in paths)
    if total > VERSIONING_LIMIT_BYTES:
        # Si pasan de 10 MB no se versionan: la cache tiene que quedar fuera de git.
        gitignore = (CACHE.parents[3] / ".gitignore").read_text(encoding="utf-8")
        assert ".cache/" in gitignore.splitlines()
