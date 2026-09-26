"""El pipeline de build escribe WAV + manifest.json coherente, en una carpeta
temporal (nunca se escribe en Art/Export/Audio desde los tests)."""

from __future__ import annotations

import json

import soundfile as sf

from explored_audio.build import build_all
from explored_audio.constants import SAMPLE_RATE


def test_build_all_escribe_wav_y_manifest_coherente(tmp_path):
    entries = build_all(output_root=tmp_path, verbose=False)
    assert len(entries) == 131

    manifest_path = tmp_path / "manifest.json"
    assert manifest_path.exists()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert manifest["count"] == 131
    assert manifest["sample_rate"] == SAMPLE_RATE
    assert manifest["bit_depth"] == 16

    by_name = {s["name"]: s for s in manifest["sounds"]}
    assert set(by_name) == {e["name"] for e in entries}

    # Comprobacion cruzada con un par de ficheros: el WAV en disco coincide
    # con lo que dice el manifiesto (duracion, canales, ruta).
    for name in ("amb_ocean_calm", "sfx_ui_click", "mus_theme"):
        entry = by_name[name]
        wav_path = tmp_path / entry["file"]
        assert wav_path.exists()
        data, sr = sf.read(str(wav_path))
        assert sr == SAMPLE_RATE
        channels = 1 if data.ndim == 1 else data.shape[1]
        assert channels == entry["channels"]
        duration_s = data.shape[0] / sr
        assert abs(duration_s - entry["duration_s"]) < 0.01
