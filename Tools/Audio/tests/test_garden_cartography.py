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


def test_regar_son_gotas_con_borboteo_y_no_un_soplido(rendered):
    """El agua son muchas gotas, no ruido filtrado en una banda estrecha:
    espectro plano (sin octava que acapare la energia), envolvente granulada a
    2 ms (impactos discretos) y el grave latiendo al ritmo del borboteo del
    recipiente (4-7 glups por segundo mientras se vierte)."""
    audio = rendered["sfx_garden_water"]
    freqs, psd = signal.welch(audio, SAMPLE_RATE, nperseg=4096)
    band = (freqs > 300.0) & (freqs < 8000.0)
    flatness = np.exp(np.mean(np.log(psd[band] + 1e-20))) / np.mean(psd[band])
    assert flatness >= 0.25, f"planitud {flatness:.2f}"
    octaves = [(250.0, 500.0), (500.0, 1000.0), (1000.0, 2000.0), (2000.0, 4000.0)]
    assert max(_band_share(audio, lo, hi) for lo, hi in octaves) <= 0.45

    win = int(0.002 * SAMPLE_RATE)
    env = np.sqrt(np.convolve(audio ** 2, np.ones(win) / win, mode="same"))[::win]
    mid = env[len(env) // 4 : 3 * len(env) // 4]
    assert mid.std() / mid.mean() >= 0.38

    sos = signal.butter(4, [150.0, 500.0], btype="bandpass", fs=SAMPLE_RATE, output="sos")
    low = signal.sosfiltfilt(sos, audio)
    hop = int(0.01 * SAMPLE_RATE)
    low_env = np.sqrt(np.convolve(low ** 2, np.ones(hop) / hop, mode="same"))[::hop]
    pour = low_env[20 : int(0.75 * len(low_env))]
    mod = np.abs(np.fft.rfft((pour - pour.mean()) * np.hanning(len(pour))))
    mod_freqs = np.fft.rfftfreq(len(pour), 0.01)
    search = (mod_freqs > 2.0) & (mod_freqs < 12.0)
    peak = mod_freqs[search][np.argmax(mod[search])]
    assert 3.5 <= peak <= 7.5, f"el borboteo late a {peak:.1f} Hz"


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


def test_pluma_sigue_el_gesto_de_la_mano(rendered):
    """Adherencia-deslizamiento: la sonoridad sube y baja con la rapidez del
    plumin, que cae en cada cambio de sentido de la letra (la envolvente se
    hunde varias veces dentro de cada trazo, no es un siseo plano), y el
    brillo sube con ella. La energia no se concentra en una sola banda
    estrecha del plumin."""
    frame = int(0.01 * SAMPLE_RATE)
    for i in (1, 2):
        audio = rendered[f"sfx_map_pen_scratch_{i:02d}"]
        frames = audio[: len(audio) // frame * frame].reshape(-1, frame)
        rms = np.sqrt((frames ** 2).mean(axis=1))
        # Profundidad de la modulacion rapida dentro de los trazos: la sonoridad
        # de cada tramo de 10 ms frente a su media movil de 0,21 s, solo donde
        # la pluma esta apoyada (las pausas entre trazos no cuentan).
        smooth = np.convolve(rms, np.ones(21) / 21, mode="same")
        active = smooth > 0.3 * smooth.max()
        depth = float((rms[active] / smooth[active]).std())
        assert depth >= 0.6, f"pluma {i}: sonoridad plana dentro del trazo ({depth:.2f})"

        loud = rms > 0.2 * rms.max()
        f = np.fft.rfftfreq(frame, 1.0 / SAMPLE_RATE)
        power = np.abs(np.fft.rfft(frames[loud] * np.hanning(frame), axis=1)) ** 2
        centroid = (power * f).sum(axis=1) / power.sum(axis=1)
        assert np.corrcoef(rms[loud], centroid)[0, 1] > 0.1

        assert _band_share(audio, 4000.0, 7000.0) <= 0.65
        assert _band_share(audio, 2000.0, 4000.0) >= 0.2
