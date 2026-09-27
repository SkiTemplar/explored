"""Oleaje y compatibilidad mono de los ambientes.

La orilla se compone de olas completas (llegada, rompiente, espuma y resaca)
en vez de un ruido que sube y baja. Y los graves de todos los colchones van en
fase en los dos canales: antes quedaban en contrafase y se anulaban en mono."""

from __future__ import annotations

import numpy as np
from scipy import signal, stats

from explored_audio.constants import SAMPLE_RATE


def _envelope_db(audio: np.ndarray, win_s: float = 0.05) -> np.ndarray:
    mono = audio.mean(axis=0)
    win = int(win_s * SAMPLE_RATE)
    frames = mono[: len(mono) // win * win].reshape(-1, win)
    return 20.0 * np.log10(np.sqrt(np.mean(frames**2, axis=1)) + 1e-9)


def _band_share(audio: np.ndarray, low_hz: float, high_hz: float) -> float:
    spectrum = np.abs(np.fft.rfft(audio.mean(axis=0))) ** 2
    freqs = np.fft.rfftfreq(audio.shape[-1], 1.0 / SAMPLE_RATE)
    band = (freqs >= low_hz) & (freqs < high_hz)
    return float(spectrum[band].sum() / spectrum.sum())


def test_las_olas_se_distinguen_del_lecho(rendered):
    # Cada ola se oye como un suceso: entre la rompiente y el hueco hay
    # bastante mas que el vaiven de 10 dB del colchon anterior.
    for name in ("amb_ocean_calm", "amb_ocean_rough"):
        env = _envelope_db(rendered[name])
        assert np.percentile(env, 95) - np.percentile(env, 5) >= 14.0, name


def test_el_mar_de_fondo_rompe_de_golpe_y_se_retira_despacio(rendered):
    # Subidas bruscas y bajadas lentas: la derivada de la envolvente tiene
    # asimetria positiva (el colchon anterior era simetrico, ~0).
    env = np.convolve(_envelope_db(rendered["amb_ocean_rough"]), np.ones(6) / 6, mode="valid")
    assert stats.skew(np.diff(env)) >= 0.6


def test_la_espuma_se_oye_en_la_orilla_mansa(rendered):
    # Burbujeo de espuma entre 2 y 6 kHz (antes, 4 % de la energia).
    assert _band_share(rendered["amb_ocean_calm"], 2000.0, 6000.0) >= 0.08
    assert _band_share(rendered["amb_ocean_calm"], 6000.0, 24000.0) <= 0.1


def test_el_mar_de_fondo_pesa_mas_en_graves_que_la_orilla_mansa(rendered):
    assert _band_share(rendered["amb_ocean_rough"], 0.0, 150.0) > _band_share(rendered["amb_ocean_calm"], 0.0, 150.0)


def test_los_graves_de_los_ambientes_van_en_fase(catalog, rendered):
    b, a = signal.butter(4, 150.0, fs=SAMPLE_RATE)
    checked = 0
    for spec in catalog:
        if spec.category != "Ambiente":
            continue
        audio = rendered[spec.name]
        mono = audio.mean(axis=0)
        loss_db = 10.0 * np.log10(np.mean(mono**2) / np.mean(audio**2))
        assert loss_db >= -3.5, f"{spec.name}: pierde {loss_db:.1f} dB al sumar a mono"
        # Solo donde los graves pesan: en los demas, lo poco que hay por
        # debajo de 150 Hz es fuga de la banda decorrelada.
        if _band_share(audio, 0.0, 150.0) < 0.05:
            continue
        low = signal.lfilter(b, a, audio, axis=-1)
        corr = float(np.corrcoef(low[0], low[1])[0, 1])
        assert corr >= 0.6, f"{spec.name}: graves L/R {corr:.2f}"
        checked += 1
    # Oleaje, viento y submarino (antes en contrafase: -0.84 a -0.94).
    assert checked >= 5
