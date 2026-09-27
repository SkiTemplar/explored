"""Herramientas: sierra, talla de piedra y atar con cuerda.

La sierra suena por los dientes que muerden y el tronco que resuena, con la
ida mas fuerte que la vuelta (antes era ruido de 1,5-8 kHz modulado por un
seno, sin graves). La talla es un golpe vitreo corto con la lasca cayendo.
La cuerda son tirones con crujido de fibra, no un siseo continuo."""

from __future__ import annotations

import numpy as np

from explored_audio.constants import SAMPLE_RATE
from explored_audio.filters import static_filter
from explored_audio.levels import k_weighted_momentary_max

SR = SAMPLE_RATE


def _mono(audio: np.ndarray) -> np.ndarray:
    return audio if audio.ndim == 1 else audio.mean(axis=0)


def _envelope_db(x: np.ndarray, win_s: float) -> np.ndarray:
    win = int(win_s * SR)
    frames = x[: len(x) // win * win].reshape(-1, win)
    return 20.0 * np.log10(np.sqrt(np.mean(frames**2, axis=1)) + 1e-9)


def _band_share(x: np.ndarray, low_hz: float, high_hz: float) -> float:
    spectrum = np.abs(np.fft.rfft(x)) ** 2
    freqs = np.fft.rfftfreq(len(x), 1.0 / SR)
    return float(spectrum[(freqs >= low_hz) & (freqs < high_hz)].sum() / spectrum.sum())


def test_la_sierra_tiene_cuerpo_de_tronco_y_no_sisea(rendered):
    x = _mono(rendered["sfx_wood_saw"])
    assert _band_share(x, 150, 500) >= 0.25
    assert _band_share(x, 8000, 24000) <= 0.05


def test_la_sierra_va_y_viene_con_la_ida_mas_fuerte(rendered):
    # Ventanas de 50 ms en la parte sostenida: las idas (arriba) y las
    # vueltas (en medio) se separan claramente, y en cada cambio de sentido
    # la hoja casi se para.
    env = _envelope_db(_mono(rendered["sfx_wood_saw"]), 0.05)[3:-3]
    assert np.percentile(env, 90) - np.percentile(env, 50) >= 4.0
    assert np.percentile(env, 90) - np.percentile(env, 10) >= 10.0


def test_la_sierra_zumba_al_ritmo_de_los_dientes(rendered):
    # La envolvente del raspado (banda de 2,5 kHz) esta modulada por el paso
    # de los dientes, ~80-200 Hz; un ruido estacionario no tendria ese pico.
    x = _mono(rendered["sfx_wood_saw"])
    band = static_filter(x, SR, fc=2500.0, q=0.8, kind="bandpass")
    env = static_filter(np.abs(band), SR, fc=400.0, q=0.7, kind="lowpass")
    env = env - static_filter(env, SR, fc=40.0, q=0.7, kind="lowpass")
    spectrum = np.abs(np.fft.rfft(env * np.hanning(len(env))))
    freqs = np.fft.rfftfreq(len(env), 1.0 / SR)
    teeth = spectrum[(freqs > 80) & (freqs < 200)].mean()
    above = spectrum[(freqs > 250) & (freqs < 400)].mean()
    assert 20.0 * np.log10(teeth / above) >= 2.0


def test_la_talla_es_un_golpe_agudo_y_corto(rendered):
    for name in ("sfx_stone_knap_01", "sfx_stone_knap_02"):
        x = _mono(rendered[name])
        assert _band_share(x, 1500, 8000) >= 0.6, name
        peak = int(np.argmax(np.abs(x)))
        assert peak < int(0.02 * SR), name
        env = _envelope_db(x, 0.01)
        # 80 ms despues del golpe ya ha caido al menos 20 dB (el nucleo
        # vitreo tiene tau de 30 ms: ~3 dB cada 10 ms).
        assert env[peak // int(0.01 * SR) + 8] <= env.max() - 20.0, name


def test_atar_son_tirones_separados(rendered):
    # Al menos tres tramos sonoros (dos tirones y el apriete) separados por
    # pausas 30 dB por debajo.
    env = _envelope_db(_mono(rendered["sfx_tie_cord"]), 0.02)
    loud = env > env.max() - 30.0
    bursts = int(np.count_nonzero(loud[1:] & ~loud[:-1]) + loud[0])
    assert bursts >= 3


def test_herramientas_a_nivel_de_los_pasos(rendered):
    # Sonoridad momentanea en la franja de los pasos (-14 a -15 dB) y de los
    # golpes de herramienta: ninguna se dispara ni se pierde.
    for name in ("sfx_wood_saw", "sfx_tie_cord", "sfx_stone_knap_01", "sfx_stone_knap_02"):
        level = k_weighted_momentary_max(_mono(rendered[name]))
        assert -19.0 <= level <= -12.0, f"{name}: {level:.1f}"
