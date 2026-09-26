"""Huerto, cartografia y construccion ampliada: cada sonido tiene el
caracter espectral y temporal que lo distingue (el peso de la pala en
graves, la pluma en agudos, golpes separados al clavar...)."""

from __future__ import annotations

import numpy as np
from scipy import signal

from explored_audio.constants import SAMPLE_RATE


def _band_share(audio: np.ndarray, low_hz: float, high_hz: float) -> float:
    spectrum = np.abs(np.fft.rfft(audio)) ** 2
    freqs = np.fft.rfftfreq(len(audio), 1.0 / SAMPLE_RATE)
    band = (freqs >= low_hz) & (freqs < high_hz)
    return float(spectrum[band].sum() / spectrum.sum())


def _onsets(audio: np.ndarray, min_gap_s: float = 0.15) -> list[int]:
    """Arranques: la envolvente (5 ms) cruza hacia arriba el 40 % de su maximo."""
    win = int(0.005 * SAMPLE_RATE)
    env = np.convolve(np.abs(audio), np.ones(win) / win, mode="same")
    above = env > 0.4 * env.max()
    rises = np.nonzero(above[1:] & ~above[:-1])[0] + 1
    if above[0]:
        rises = np.concatenate([[0], rises])
    onsets: list[int] = []
    for r in rises:
        if not onsets or r - onsets[-1] > min_gap_s * SAMPLE_RATE:
            onsets.append(int(r))
    return onsets


def test_cavar_tiene_peso_grave(rendered):
    for i in (1, 2, 3):
        audio = rendered[f"sfx_garden_dig_{i:02d}"]
        assert _band_share(audio, 0.0, 400.0) >= 0.4


def test_regar_es_continuo_y_de_medios(rendered):
    """Un chorro, no golpes: la envolvente no cae a silencio en la parte
    central, y el grueso de la energia esta en 300 Hz - 4 kHz."""
    audio = rendered["sfx_garden_water"]
    assert 1.5 <= len(audio) / SAMPLE_RATE <= 3.0
    win = int(0.05 * SAMPLE_RATE)
    env = np.sqrt(np.convolve(audio ** 2, np.ones(win) / win, mode="valid"))
    mid = env[len(env) // 4 : 3 * len(env) // 4]
    assert mid.min() >= 0.25 * mid.max()
    assert _band_share(audio, 300.0, 4000.0) >= 0.5


def test_pluma_es_aguda_y_discreta(rendered):
    """Friccion del plumin: casi toda la energia sobre 2 kHz, sin siseo por
    encima de 10 kHz dominante, y mas baja que un golpe de herramienta."""
    from explored_audio.levels import lufs_approx

    for i in (1, 2):
        audio = rendered[f"sfx_map_pen_scratch_{i:02d}"]
        assert _band_share(audio, 2000.0, 24000.0) >= 0.8
        assert _band_share(audio, 10000.0, 24000.0) <= 0.3
        assert lufs_approx(audio) < lufs_approx(rendered["sfx_stone_hit_01"]) - 3.0


def test_clavar_son_varios_golpes_que_suben_de_tono(rendered):
    sos = signal.butter(4, [150.0, 1200.0], btype="bandpass", fs=SAMPLE_RATE, output="sos")
    for i in (1, 2):
        audio = rendered[f"sfx_build_hammer_{i:02d}"]
        onsets = _onsets(audio)
        assert 3 <= len(onsets) <= 4, f"sfx_build_hammer_{i:02d}: {len(onsets)} golpes"
        body = signal.sosfiltfilt(sos, audio)
        peaks = []
        for pos in onsets:
            seg = body[pos : pos + int(0.12 * SAMPLE_RATE)]
            spectrum = np.abs(np.fft.rfft(seg * np.hanning(len(seg))))
            freqs = np.fft.rfftfreq(len(seg), 1.0 / SAMPLE_RATE)
            peaks.append(freqs[np.argmax(spectrum)])
        assert peaks[-1] > peaks[0], f"el tono no sube: {peaks}"


def test_sello_y_desmontar_tienen_golpe_grave(rendered):
    assert _band_share(rendered["sfx_map_stamp"], 0.0, 400.0) >= 0.5
    assert _band_share(rendered["sfx_build_dismantle"], 0.0, 400.0) >= 0.3


def test_desplegar_mapa_es_crujido_de_papel(rendered):
    assert _band_share(rendered["sfx_map_unfold"], 1500.0, 12000.0) >= 0.5
