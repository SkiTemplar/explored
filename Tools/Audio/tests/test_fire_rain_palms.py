"""Hoguera, lluvia en tejado de palma y viento en palmeras: cada uno tiene el
caracter que lo distingue de su pariente mas generico del catalogo."""

from __future__ import annotations

import numpy as np
from scipy.signal import butter, sosfiltfilt

from explored_audio.constants import SAMPLE_RATE


def _mono(audio: np.ndarray) -> np.ndarray:
    return audio if audio.ndim == 1 else audio.mean(axis=0)


def _band_share(audio: np.ndarray, low_hz: float, high_hz: float) -> float:
    spectrum = np.abs(np.fft.rfft(_mono(audio))) ** 2
    freqs = np.fft.rfftfreq(audio.shape[-1], 1.0 / SAMPLE_RATE)
    band = (freqs >= low_hz) & (freqs < high_hz)
    return float(spectrum[band].sum() / spectrum.sum())


def _centroid(audio: np.ndarray) -> float:
    spectrum = np.abs(np.fft.rfft(_mono(audio))) ** 2
    freqs = np.fft.rfftfreq(audio.shape[-1], 1.0 / SAMPLE_RATE)
    return float((freqs * spectrum).sum() / spectrum.sum())


def _envelope_db(audio: np.ndarray, win_s: float) -> np.ndarray:
    x = _mono(audio)
    win = int(win_s * SAMPLE_RATE)
    frames = x[: len(x) // win * win].reshape(-1, win)
    return 20.0 * np.log10(np.sqrt(np.mean(frames**2, axis=1)) + 1e-9)


def test_hoguera_tiene_cuerpo_y_chisporroteo(rendered):
    fire = rendered["sfx_fire_loop"]
    # Cuerpo: el rugido de la llama pesa en graves (la version anterior era
    # casi todo siseo por encima de 6 kHz).
    assert _band_share(fire, 60.0, 500.0) >= 0.35
    assert _band_share(fire, 6000.0, 20000.0) <= 0.25
    # Chisporroteo: transitorios muy por encima del lecho en ventanas de 5 ms.
    env = _envelope_db(fire, 0.005)
    assert np.percentile(env, 99.5) - np.median(env) >= 10.0


def test_hoguera_alterna_rachas_vivas_y_calmas(rendered):
    # La actividad de los racimos sube y baja: en ventanas de 1 s, la
    # densidad de agudos cambia claramente de una a otra.
    fire = rendered["sfx_fire_loop"]
    spectrum_env = []
    win = SAMPLE_RATE
    for start in range(0, len(fire) - win, win):
        spectrum_env.append(_band_share(fire[start : start + win], 2000.0, 12000.0))
    assert max(spectrum_env) >= 2.0 * min(spectrum_env)


def test_tejado_de_palma_suena_mas_sordo_que_las_hojas(rendered):
    thatch = rendered["amb_rain_on_thatch"]
    leaves = rendered["amb_rain_on_leaves"]
    assert _centroid(thatch) < _centroid(leaves) * 0.8
    assert _band_share(thatch, 6000.0, 20000.0) < _band_share(leaves, 6000.0, 20000.0)


def test_tejado_de_palma_tiene_goteos_del_alero(rendered):
    # Los goteos sobresalen del lecho de lluvia en ventanas cortas.
    env = _envelope_db(rendered["amb_rain_on_thatch"], 0.01)
    assert np.percentile(env, 99.5) - np.median(env) >= 6.0


def test_tejado_de_palma_son_golpes_sordos_con_cuerpo(rendered):
    thatch = rendered["amb_rain_on_thatch"]
    # Cada gota es un golpe discreto que sobresale del lecho en ventanas de
    # 5 ms (la version anterior eran granos de ruido: unos 16 dB, pero todo
    # en 0,8-4 kHz y sin cuerpo).
    env = _envelope_db(thatch, 0.005)
    assert np.percentile(env, 99.5) - np.median(env) >= 10.0
    # Los foliolos que ceden y el agua de las canales llevan energia a
    # 150-800 Hz; antes era un 3 %.
    assert _band_share(thatch, 150.0, 800.0) >= 0.3
    # La paja se come el agudo.
    assert _band_share(thatch, 4000.0, 20000.0) <= 0.05


def test_tejado_de_palma_tiene_gotera_en_la_cascara(rendered):
    # La gotera cae siempre en la misma cascara: un tono fijo de 650-900 Hz
    # que destaca sobre la lluvia en el espectro medio de todo el bucle.
    thatch = _mono(rendered["amb_rain_on_thatch"])
    spectrum = np.abs(np.fft.rfft(thatch)) ** 2
    freqs = np.fft.rfftfreq(len(thatch), 1.0 / SAMPLE_RATE)
    # Energia en 5 Hz frente a la de los 100 Hz de alrededor: sin tono la
    # razon ronda 1; la cascara (afinada con un ±1 %) da unas 4.
    bin_hz = freqs[1]
    narrow = np.convolve(spectrum, np.ones(int(5.0 / bin_hz)), mode="same") / int(5.0 / bin_hz)
    wide = np.convolve(spectrum, np.ones(int(100.0 / bin_hz)), mode="same") / int(100.0 / bin_hz)
    band = (freqs >= 650.0) & (freqs < 900.0)
    assert float((narrow[band] / wide[band]).max()) >= 2.0


def test_viento_en_palmeras_suena_a_hojas(rendered):
    palms = rendered["amb_wind_palms"]
    wind = rendered["amb_wind_light"]
    # El aleteo y el tableteo de los foliolos llevan energia a 2-6 kHz que el
    # viento solo apenas tiene.
    assert _band_share(palms, 2000.0, 6000.0) >= 2.0 * _band_share(wind, 2000.0, 6000.0)
    # Y las rachas se oyen: la sonoridad por segundo varia mas que en el viento solo.
    assert np.std(_envelope_db(palms, 1.0)) > np.std(_envelope_db(wind, 1.0))


def _flutter_index(audio: np.ndarray) -> float:
    """Profundidad del aleteo: cuanto late a 4-20 Hz la envolvente de la banda de
    2-6 kHz, relativa a su nivel, en los tramos de racha (el 40 % mas fuerte)."""
    x = audio[0] if audio.ndim == 2 else audio
    band = sosfiltfilt(butter(4, [2000.0, 6000.0], btype="band", fs=SAMPLE_RATE, output="sos"), x)
    hop = SAMPLE_RATE // 200
    env = np.sqrt(np.convolve(band**2, np.ones(hop) / hop, "same"))[::hop]
    fast = sosfiltfilt(butter(2, [4.0, 20.0], btype="band", fs=200, output="sos"), env)
    slow = sosfiltfilt(butter(2, 1.0, btype="low", fs=200, output="sos"), env)
    loud = slow > np.percentile(slow, 60)
    return float(fast[loud].std() / slow[loud].mean())


def test_viento_en_palmeras_aletea(rendered):
    # Los foliolos baten a 5-11 Hz: la banda aguda late con ese ritmo. En el
    # viento solo (y en la version anterior, ruido con una modulacion aleatoria
    # comun) ese indice rondaba 0,07-0,10; con las hojas, mas del doble.
    palms = _flutter_index(rendered["amb_wind_palms"])
    wind = _flutter_index(rendered["amb_wind_light"])
    assert palms >= 0.18
    assert palms >= 2.5 * wind
    # Brillo de papel, no siseo: la mayor parte de lo agudo queda por debajo de 6 kHz.
    share_hi = _band_share(rendered["amb_wind_palms"], 6000.0, 20000.0)
    assert share_hi <= 0.6 * _band_share(rendered["amb_wind_palms"], 2000.0, 6000.0)


def test_lluvia_en_hojas_son_gotas_discretas(rendered):
    leaves = rendered["amb_rain_on_leaves"]
    # Cada gota contra una hoja es un golpe que sobresale del lavado de fondo
    # (la version anterior era ruido rosa filtrado: unos 5 dB de margen).
    env = _envelope_db(leaves, 0.005)
    assert np.percentile(env, 99.5) - np.median(env) >= 10.0
    # El «toc» de la lamina y el chasquido del impacto viven en 0,6-6 kHz.
    assert _band_share(leaves, 600.0, 6000.0) >= 0.6
    assert _band_share(leaves, 20.0, 200.0) <= 0.05


def test_lluvia_en_hojas_tiene_cascadas_con_las_rachas(rendered):
    # Las rachas sacuden el dosel y sueltan goterones: la sonoridad por segundo
    # varia claramente mas que en la lluvia sobre el suelo.
    leaves = rendered["amb_rain_on_leaves"]
    light = rendered["amb_rain_light"]
    assert np.std(_envelope_db(leaves, 1.0)) >= 1.0
    assert np.std(_envelope_db(leaves, 1.0)) > 1.5 * np.std(_envelope_db(light, 1.0))


def test_encender_la_hoguera_sopla_y_luego_prende_con_aleteo(rendered):
    ignite = _mono(rendered["sfx_fire_ignite"])
    # Sin retumbo: la version anterior dejaba un tercio de la energia por
    # debajo de 150 Hz. El cuerpo de la llama va en 150-1500 Hz.
    assert _band_share(ignite, 20.0, 150.0) <= 0.2
    assert _band_share(ignite, 150.0, 1500.0) >= 0.6
    # Soplos primero y la llama despues: el ultimo tercio suena bastante mas
    # fuerte que el primero.
    env = _envelope_db(ignite, 0.05)
    third = len(env) // 3
    assert np.mean(env[-third:]) - np.mean(env[:third]) >= 8.0
    # La llama aletea a 8-14 Hz: pico de la envolvente (sin chasquidos ni
    # tendencia lenta) en esa banda.
    flame = ignite[-int(0.7 * SAMPLE_RATE) :]
    smooth = np.convolve(flame, np.ones(24) / 24, "same")
    frames = np.abs(smooth)[: len(smooth) // 480 * 480].reshape(-1, 480).mean(axis=1)
    frames = frames - np.convolve(frames, np.ones(25) / 25, "same")
    spectrum = np.abs(np.fft.rfft(frames * np.hanning(len(frames))))
    freqs = np.fft.rfftfreq(len(frames), 1.0 / 100.0)
    assert 7.0 <= freqs[1 + int(np.argmax(spectrum[1:]))] <= 15.0
