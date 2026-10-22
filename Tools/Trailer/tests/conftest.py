from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import pytest
import soundfile as sf

from explored_trailer.audio_io import SAMPLE_RATE

REPO = Path(__file__).resolve().parents[3]


def tone(seconds: float, freq: float = 440.0, amp: float = 0.3, channels: int = 2, seed: int = 0) -> np.ndarray:
    t = np.arange(int(seconds * SAMPLE_RATE)) / SAMPLE_RATE
    rng = np.random.default_rng(seed)
    mono = amp * np.sin(2 * np.pi * freq * t) + 0.02 * rng.standard_normal(len(t))
    return np.stack([mono] * channels, axis=1)


def write_wav(path: Path, audio: np.ndarray) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(path), audio, SAMPLE_RATE, subtype="PCM_16")
    return path


@pytest.fixture()
def fake_audio_root(tmp_path: Path) -> Path:
    """Árbol mínimo con los WAV que usa la mezcla (tonos cortos en lugar de los reales)."""
    from explored_trailer.layers import HORN, build_layers
    from explored_trailer.music import music_plan
    from explored_trailer.timeline import nominal_timeline

    root = tmp_path / "Audio"
    tl = nominal_timeline()
    for layer in (*build_layers(tl), HORN):
        target = root / layer.relpath
        if not target.exists():
            write_wav(target, tone(4.0 if layer.loop else 1.5, 220.0 + len(layer.name) * 7, 0.2, channels=1))
    lengths = {"mus_rec_gymnopedie_1": 40.0, "mus_rec_cello_sonata_largo": 40.0, "mus_rec_macleod_canon_in_d": 342.6}
    for cut in music_plan(tl):
        target = root / "Grabada" / f"{cut.piece_id}.wav"
        if not target.exists():
            write_wav(target, tone(lengths[cut.piece_id], 330.0, 0.25))
    return root


@pytest.fixture()
def trailer_json(tmp_path: Path) -> Path:
    data = {
        "fps": 30,
        "frames": 2250,
        "shots": [{"index": i, "name": n, "start": s, "end": e} for i, n, s, e in [
            (1, "mar", 0.0, 4.2), (2, "cala", 4.2, 12.0), (3, "montaje", 12.0, 22.3), (4, "faro", 22.3, 32.0),
            (5, "ciclon", 32.0, 41.6), (6, "carguero", 41.6, 50.4), (7, "final", 50.4, 68.0), (8, "titulo", 68.0, 75.0),
        ]],
        "events": {"ignite": 50.9, "mistral": 53.9, "bocina": 63.5},
    }
    path = tmp_path / "trailer.json"
    path.write_text(json.dumps(data), encoding="utf-8")
    return path
