"""Interfaz y colocar pieza.

La interfaz es una familia de laminas tipo kalimba afinadas en re menor
pentatonica, la escala de la musica (antes eran senos sueltos a 1,4-2,3 kHz
sin relacion con ella). Colocar pieza es madera que toca, bascula y asienta
(antes, un modal de 140-200 Hz con un 83 % de la energia bajo 150 Hz)."""

from __future__ import annotations

import numpy as np
from scipy.signal import find_peaks

from explored_audio.build import render_sound
from explored_audio.catalog import SoundSpec
from explored_audio.constants import SAMPLE_RATE
from explored_audio.filters import static_filter
from explored_audio.generators import construction
from explored_audio.levels import k_weighted_momentary_max

SR = SAMPLE_RATE
UI_NAMES = (
    "sfx_ui_click", "sfx_ui_hover", "sfx_ui_open", "sfx_ui_close",
    "sfx_ui_journal_open", "sfx_ui_page_turn", "sfx_ui_discovery_notify",
)
# Re menor pentatonica: re, fa, sol, la, do (semitonos sobre re).
_SCALE_SEMITONES = {0, 3, 5, 7, 10}
_D = 440.0 * 2 ** (-7 / 12)  # re4


def _band_share(x: np.ndarray, low_hz: float, high_hz: float) -> float:
    spectrum = np.abs(np.fft.rfft(x)) ** 2
    freqs = np.fft.rfftfreq(len(x), 1.0 / SR)
    return float(spectrum[(freqs >= low_hz) & (freqs < high_hz)].sum() / spectrum.sum())


def _in_scale(freq: float) -> bool:
    semis = 12 * np.log2(freq / _D)
    nearest = round(semis)
    return abs(semis - nearest) < 0.3 and nearest % 12 in _SCALE_SEMITONES


def _dominant_pitches(x: np.ndarray, count: int = 4) -> list[float]:
    # Solo los picos de al menos un 30 % del maximo: las notas de 50-100 ms
    # tienen lobulos laterales a ~60 Hz que no son notas.
    n_fft = 2**18
    spectrum = np.abs(np.fft.rfft(x, n_fft))
    freqs = np.fft.rfftfreq(n_fft, 1.0 / SR)
    peaks, _ = find_peaks(spectrum, height=0.3 * spectrum.max(), distance=int(60 / (SR / n_fft)))
    peaks = peaks[np.argsort(spectrum[peaks])[::-1][:count]]
    return [float(freqs[p]) for p in peaks]


def test_la_interfaz_suena_en_la_escala_de_la_musica(rendered):
    for name in UI_NAMES:
        # El click cae de afinacion en sus primeros 4 ms a proposito (el
        # contacto): esa caida ensancha el espectro por arriba; basta su pico.
        count = 1 if name == "sfx_ui_click" else 4
        for freq in _dominant_pitches(rendered[name], count):
            assert _in_scale(freq), f"{name}: {freq:.0f} Hz fuera de re menor pentatonica"


def test_la_interfaz_arranca_y_termina_sin_clic(rendered):
    for name in UI_NAMES:
        x = rendered[name]
        assert abs(x[0]) < 2e-3 and abs(x[-1]) < 2e-3, name


def test_hover_es_mas_flojo_que_click(rendered):
    hover = k_weighted_momentary_max(rendered["sfx_ui_hover"], sr=SR)
    click = k_weighted_momentary_max(rendered["sfx_ui_click"], sr=SR)
    assert click - hover >= 2.0


def test_el_descubrimiento_sube_y_se_queda_sonando(rendered):
    x = rendered["sfx_ui_discovery_notify"]
    win = int(0.06 * SR)
    pitches = [_dominant_pitches(x[i * int(0.075 * SR):][:win], 1)[0] for i in range(4)]
    assert all(b > a for a, b in zip(pitches, pitches[1:])), pitches
    tail = x[int(0.6 * SR):int(0.7 * SR)]
    assert 20 * np.log10(np.sqrt(np.mean(tail**2)) / np.max(np.abs(x))) > -40.0


def test_colocar_pieza_suena_a_madera_y_no_a_bombo(rendered):
    x = rendered["sfx_build_place"]
    assert _band_share(x, 150, 1500) >= 0.6
    assert _band_share(x, 0, 150) <= 0.35
    assert abs(x[-1]) < 1e-3


def test_colocar_pieza_toca_y_asienta_en_dos_golpes():
    for name in ("sfx_build_place", "sfx_build_place_b", "sfx_build_place_c"):
        x = render_sound(SoundSpec(name, "Efectos", False, construction.build_place))
        contact = np.abs(static_filter(x, SR, fc=2000.0, q=0.7, kind="highpass"))
        env = static_filter(contact, SR, fc=300.0, q=0.7, kind="lowpass")[: int(0.1 * SR)]
        peaks, _ = find_peaks(env, height=env.max() * 0.2, distance=int(0.015 * SR))
        assert len(peaks) >= 2, name
