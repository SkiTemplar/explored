"""Genera un espectrograma PNG de cada ambiente (y, por separado, de cada
pista/capa de musica) para revision visual sin poder escuchar el resultado.

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


def _repo_root() -> Path:
    # Tools/Audio/scripts/gen_spectrograms.py -> raiz del repo.
    return Path(__file__).resolve().parents[3]


def _audio_preview_dir() -> Path:
    """`Saved/AudioPreview` del repo: ya cubierto por el `Saved/` del
    .gitignore raiz (convencion de Unreal), asi que estas previsualizaciones
    nunca se versionan."""
    return _repo_root() / "Saved" / "AudioPreview"


def plot_spectrogram(audio: np.ndarray, name: str, out_path: Path) -> None:
    # Un solo canal (no la mezcla L+R): sumar a mono un par decorrelado
    # introduce un filtro en peine que no existe en la señal real en estereo.
    mono = audio if audio.ndim == 1 else audio[0]
    fig, ax = plt.subplots(figsize=(10, 4))
    spec, freqs, t, im = ax.specgram(mono, NFFT=2048, Fs=SAMPLE_RATE, noverlap=1024, cmap="magma")
    ax.set_ylim(0, 12000)
    ax.set_xlabel("tiempo (s)")
    ax.set_ylabel("frecuencia (Hz)")
    ax.set_title(name)
    fig.colorbar(im, ax=ax, label="dB")
    fig.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=110)
    plt.close(fig)


def main(argv: list[str] | None = None) -> int:
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--musica", action="store_true", help="solo la musica (y la muestra de flauta)")
    args = parser.parse_args(argv)

    out_dir = default_output_root() / "_spectrograms"
    for spec in build_catalog():
        if args.musica or spec.category != "Ambiente":
            continue
        audio = render_by_name(spec.name)
        plot_spectrogram(audio, spec.name, out_dir / f"{spec.name}.png")
        print(f"  {spec.name} -> {out_dir / (spec.name + '.png')}")
    if not args.musica:
        print(f"\nEspectrogramas de ambiente en {out_dir}")

    # Musica: una imagen por pista/capa (mas la muestra de flauta diegetica,
    # que comparte instrumento y renderizador), en Saved/AudioPreview -para
    # revisar el resultado del soundfont acustico sin poder escucharlo.
    preview_dir = _audio_preview_dir()
    music_names = [spec.name for spec in build_catalog() if spec.category == "Musica"]
    music_names.append("sfx_flute_note")
    for name in music_names:
        audio = render_by_name(name)
        plot_spectrogram(audio, name, preview_dir / f"{name}.png")
        print(f"  {name} -> {preview_dir / (name + '.png')}")
    print(f"\nEspectrogramas de musica en {preview_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
