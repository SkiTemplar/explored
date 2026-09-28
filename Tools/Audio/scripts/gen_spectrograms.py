"""Genera un espectrograma PNG de cada ambiente para revision visual.

No es parte del pipeline de build (matplotlib es una dependencia de
desarrollo, no de runtime): se ejecuta a mano con
`uv run python scripts/gen_spectrograms.py`.
"""

from __future__ import annotations

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from explored_audio.build import default_output_root, render_by_name  # noqa: E402
from explored_audio.catalog import build_catalog  # noqa: E402
from explored_audio.constants import SAMPLE_RATE  # noqa: E402


def plot_spectrogram(audio: np.ndarray, name: str, out_path: Path) -> None:
    # Un solo canal (no la mezcla L+R): sumar a mono un par decorrelado
    # introduce un filtro en peine que no existe en la señal real en estereo.
    mono = audio if audio.ndim == 1 else audio[0]
    fig, ax = plt.subplots(figsize=(10, 4))
    _spec, _freqs, _t, im = ax.specgram(mono, NFFT=2048, Fs=SAMPLE_RATE, noverlap=1024, cmap="magma")
    ax.set_ylim(0, 12000)
    ax.set_xlabel("tiempo (s)")
    ax.set_ylabel("frecuencia (Hz)")
    ax.set_title(name)
    fig.colorbar(im, ax=ax, label="dB")
    fig.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=110)
    plt.close(fig)


def main() -> int:
    out_dir = default_output_root() / "_spectrograms"
    for spec in build_catalog():
        if spec.category != "Ambiente":
            continue
        audio = render_by_name(spec.name)
        plot_spectrogram(audio, spec.name, out_dir / f"{spec.name}.png")
        print(f"  {spec.name} -> {out_dir / (spec.name + '.png')}")
    print(f"\nEspectrogramas en {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
