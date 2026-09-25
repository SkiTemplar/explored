"""Escritura de WAV a 48 kHz / 16 bits."""

from __future__ import annotations

from pathlib import Path

import numpy as np
import soundfile as sf

from .constants import SAMPLE_RATE


def to_wav_bytes(audio: np.ndarray, sr: int = SAMPLE_RATE) -> bytes:
    """Serializa a bytes WAV PCM16 en memoria (para comprobar determinismo)."""
    import io

    data = audio.T if audio.ndim == 2 else audio
    buf = io.BytesIO()
    sf.write(buf, data, sr, format="WAV", subtype="PCM_16")
    return buf.getvalue()


def write_wav(path: Path, audio: np.ndarray, sr: int = SAMPLE_RATE) -> None:
    """`audio`: mono (N,) o estereo (2, N). Se guarda como PCM 16 bits."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    data = audio.T if audio.ndim == 2 else audio
    sf.write(str(path), data, sr, subtype="PCM_16")
