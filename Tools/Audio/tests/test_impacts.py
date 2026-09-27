"""Golpes de herramienta (`sfx_wood_chop_*`, `sfx_stone_hit_*`): un golpe
seco y de banda ancha, no una nota que resuena (antes sonaban a marimba y a
campana: el 95 % de la energia en una banda y 300 ms hasta -30 dB)."""

from __future__ import annotations

import numpy as np
from scipy import signal

from explored_audio.constants import SAMPLE_RATE

SR = SAMPLE_RATE
CHOPS = [f"sfx_wood_chop_{i:02d}" for i in (1, 2, 3)]
HITS = [f"sfx_stone_hit_{i:02d}" for i in (1, 2, 3)]


def _psd(audio: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    return signal.welch(audio, SR, nperseg=2048)


def _share(audio: np.ndarray, lo: float, hi: float) -> float:
    f, p = _psd(audio)
    return float(p[(f >= lo) & (f < hi)].sum() / p.sum())


def _flatness(audio: np.ndarray, lo: float = 200.0, hi: float = 4000.0) -> float:
    f, p = _psd(audio)
    band = p[(f >= lo) & (f < hi)] + 1e-20
    return float(np.exp(np.mean(np.log(band))) / np.mean(band))


def _t30_ms(audio: np.ndarray) -> float:
    env = np.sqrt(np.convolve(audio**2, np.ones(240) / 240, "same"))
    peak = int(np.argmax(env))
    below = np.nonzero(env[peak:] < env[peak] * 10 ** (-30 / 20))[0]
    return 1000.0 * below[0] / SR if below.size else 1000.0 * (audio.size - peak) / SR


def _centroid(audio: np.ndarray) -> float:
    f, p = _psd(audio)
    return float((f * p).sum() / p.sum())


def test_hachazo_es_un_golpe_seco_no_una_nota(rendered):
    for name in CHOPS:
        audio = rendered[name]
        assert _flatness(audio) > 0.03, f"{name}: demasiado tonal"
        assert _t30_ms(audio) < 200.0, f"{name}: resuena demasiado"
        # Golpe sordo del tronco y fibras que se rasgan, no solo el "toc" medio.
        assert _share(audio, 20.0, 200.0) > 0.08, f"{name}: sin golpe grave"
        assert _share(audio, 2000.0, 8000.0) > 0.01, f"{name}: sin chasquido ni fibras"
        assert _share(audio, 200.0, 800.0) < 0.85, f"{name}: todo en una banda"


def test_golpe_de_piedra_es_seco_y_suelta_arenilla(rendered):
    for name in HITS:
        audio = rendered[name]
        assert _flatness(audio) > 0.03, f"{name}: demasiado tonal"
        assert _t30_ms(audio) < 120.0, f"{name}: suena a campana"
        # Arenilla: energia aguda despues del golpe (60-250 ms).
        tail = audio[int(0.06 * SR) : int(0.25 * SR)]
        sos = signal.butter(4, 3000.0, btype="highpass", fs=SR, output="sos")
        hf = signal.sosfiltfilt(sos, tail)
        assert np.sqrt(np.mean(hf**2)) > 1e-3, f"{name}: sin arenilla"


def test_piedra_mas_aguda_que_madera_y_mas_grave_que_la_lasca(rendered):
    chop = np.mean([_centroid(rendered[n]) for n in CHOPS])
    hit = np.mean([_centroid(rendered[n]) for n in HITS])
    knap = np.mean([_centroid(rendered[f"sfx_stone_knap_{i:02d}"]) for i in (1, 2)])
    assert chop < hit < knap


def test_golpes_empiezan_y_acaban_en_silencio(rendered):
    # -50 dBFS: la continua que retira build.finalize deja un escalon
    # inaudible; un chasquido de verdad estaria decenas de dB por encima.
    for name in CHOPS + HITS:
        audio = rendered[name]
        assert abs(audio[0]) < 3e-3, f"{name}: no empieza en silencio"
        assert np.max(np.abs(audio[-48:])) < 3e-3, f"{name}: no acaba en silencio"
