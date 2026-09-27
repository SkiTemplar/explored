"""Tripas con hambre (`sfx_stomach_growl`), el sonido que `UBodySignalsComponent`
ya cargaba pero que no existia: un tren de pulsos graves por formantes que se
deslizan (el gas empujado por la tripa) y un remate de borboteos."""

from __future__ import annotations

import numpy as np

from explored_audio.constants import SAMPLE_RATE
from explored_audio.generators import body
from explored_audio.levels import k_weighted_momentary_max

SR = SAMPLE_RATE


def _band_share(x: np.ndarray, low_hz: float, high_hz: float) -> float:
    spectrum = np.abs(np.fft.rfft(x)) ** 2
    freqs = np.fft.rfftfreq(len(x), 1.0 / SR)
    return float(spectrum[(freqs >= low_hz) & (freqs < high_hz)].sum() / spectrum.sum())


def test_el_estomago_suena_en_graves_medios(rendered):
    growl = rendered["sfx_stomach_growl"]
    assert _band_share(growl, 0.0, 1000.0) > 0.9
    # Ni bombo ni silbido: la mayor parte entre 100 y 600 Hz.
    assert _band_share(growl, 100.0, 600.0) > 0.5


def test_el_gruñido_domina_sobre_los_borboteos(rendered):
    # Los borboteos van al final: la primera mitad (solo frases de gas) tiene
    # que llevar la mayor parte de la energia, no quedarse de fondo.
    growl = rendered["sfx_stomach_growl"]
    half = len(growl) // 2
    assert np.sum(growl[:half] ** 2) > 0.4 * np.sum(growl**2)


def test_el_estomago_suena_como_el_resto_del_cuerpo(rendered):
    growl = k_weighted_momentary_max(rendered["sfx_stomach_growl"])
    breath = k_weighted_momentary_max(rendered["sfx_breath_tired"])
    assert abs(growl - breath) < 3.0, f"estomago {growl:.1f} frente a respiracion {breath:.1f}"


def test_el_estomago_empieza_y_acaba_en_silencio(rendered):
    growl = rendered["sfx_stomach_growl"]
    edge = int(0.002 * SR)
    peak = np.max(np.abs(growl))
    assert np.max(np.abs(growl[:edge])) < 0.02 * peak
    assert np.max(np.abs(growl[-edge:])) < 0.01 * peak


def test_las_semillas_dan_variantes_distintas():
    a = body.stomach_growl("sfx_stomach_growl")
    b = body.stomach_growl("sfx_stomach_growl_otra")
    assert len(a) != len(b) or not np.allclose(a, b)
