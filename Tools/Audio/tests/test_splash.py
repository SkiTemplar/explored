"""Chapoteos (`sfx_splash_small`, `sfx_splash_big`): burbujas y gotas, no siseo."""

from __future__ import annotations

import numpy as np
import pytest
from scipy import signal
from scipy.ndimage import median_filter

from explored_audio.build import render_by_name
from explored_audio.constants import SAMPLE_RATE
from explored_audio.levels import k_weighted_momentary_max


@pytest.mark.parametrize(("name", "target"), [("sfx_splash_small", -15.0), ("sfx_splash_big", -13.0)])
def test_chapoteo_suena_a_burbujas_y_gotas(name: str, target: float) -> None:
    """Como el paso en agua somera: al menos un 15 % de la energia de
    200-6000 Hz en picos espectrales 12 dB sobre la mediana local (la
    burbuja de la cavidad y las de las gotas) y al menos 4 repuntes de 6 dB
    en la envolvente de 10 ms (las gotas que vuelven a caer). La version de
    ruido en banda daba 0,1-0,9 % de energia tonal y un centroide de ~4,5 kHz.
    La sonoridad momentanea se mantiene en la de antes (±1 dB)."""
    audio = render_by_name(name)
    freqs, _, z = signal.stft(audio, SAMPLE_RATE, nperseg=512, noverlap=384)
    power = np.abs(z[(freqs >= 200.0) & (freqs < 6000.0)]) ** 2 + 1e-18
    local = median_filter(power, size=(15, 1), mode="nearest")
    tonal = power[power > local * 10.0**1.2].sum() / power.sum()
    assert tonal >= 0.15, f"{name}: solo {tonal:.1%} de energia tonal (burbujas)"

    win = int(0.01 * SAMPLE_RATE)
    env = np.sqrt(np.convolve(audio**2, np.ones(win) / win, mode="same"))[:: win // 2]
    peaks, _ = signal.find_peaks(20.0 * np.log10(env + 1e-9), prominence=6.0)
    assert len(peaks) >= 4, f"{name}: {len(peaks)} repuntes, no se oyen las gotas"

    f, p = signal.welch(audio, SAMPLE_RATE, nperseg=2048)
    centroid = float((f * p).sum() / p.sum())
    assert centroid < 2500.0, f"{name}: centroide {centroid:.0f} Hz, suena a siseo"
    assert abs(k_weighted_momentary_max(audio) - target) <= 1.0


def test_chapoteo_grande_es_mas_grave_y_largo_que_el_pequeno() -> None:
    small = render_by_name("sfx_splash_small")
    big = render_by_name("sfx_splash_big")
    assert len(big) > len(small)

    def low_share(x: np.ndarray) -> float:
        f, p = signal.welch(x, SAMPLE_RATE, nperseg=2048)
        return float(p[f < 300.0].sum() / p.sum())

    assert low_share(big) > low_share(small)
