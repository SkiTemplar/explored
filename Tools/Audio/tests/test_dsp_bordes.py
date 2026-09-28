"""Casos limite de los bloques de DSP: buffers vacios o muy cortos, cortes en
Nyquist, longitudes que no casan, NaN y clipping. Son funciones puras y
rapidas: no generan ningun sonido del catalogo."""

from __future__ import annotations

import numpy as np
import pytest
from scipy.signal import butter, sosfilt

from explored_audio import envelopes, filters, fm, granular, levels, loop, modal, noise, reverb, stereo
from explored_audio.constants import PEAK_CEILING_LINEAR, SAMPLE_RATE
from explored_audio.music import humanize, sequencer, theory

SR = SAMPLE_RATE


@pytest.fixture
def rng() -> np.random.Generator:
    return np.random.default_rng(1234)


# --- niveles -----------------------------------------------------------------


def test_lufs_de_un_buffer_vacio_es_silencio_y_no_nan():
    # Antes: media de un array vacio -> NaN, que acababa en el manifiesto.
    assert levels.lufs_approx(np.zeros(0)) == -120.0
    assert levels.lufs_approx(np.zeros((2, 0))) == -120.0
    assert levels.k_weighted_momentary_max(np.zeros(0)) == -120.0


def test_lufs_de_silencio_y_de_un_seno_de_referencia():
    assert levels.lufs_approx(np.zeros(SR)) == -120.0
    # Un seno de 1 kHz a 0 dBFS de pico mide unos -3 LUFS (K-weighting ~0 dB a 1 kHz).
    t = np.arange(4 * SR) / SR
    sine = np.sin(2 * np.pi * 1000.0 * t)
    assert levels.lufs_approx(sine) == pytest.approx(-3.0, abs=0.3)
    # Estereo con el mismo seno en los dos canales: misma cifra (promedio por canal).
    assert levels.lufs_approx(np.stack([sine, sine])) == pytest.approx(levels.lufs_approx(sine))


def test_sonoridad_momentanea_ve_el_transitorio_que_la_integrada_diluye():
    x = np.zeros(2 * SR)
    x[SR : SR + SR // 10] = np.sin(2 * np.pi * 1000.0 * np.arange(SR // 10) / SR)
    assert levels.k_weighted_momentary_max(x) > levels.lufs_approx(x) + 10.0
    # Ventana mas larga que la señal: promedia la señal entera.
    corto = x[SR : SR + 100]
    assert levels.k_weighted_momentary_max(corto, window_s=1.0) == pytest.approx(levels.lufs_approx(corto))
    assert levels.k_weighted_momentary_max(np.zeros(SR)) == -120.0


def test_match_lufs_alcanza_el_objetivo_y_acota_la_ganancia():
    t = np.arange(2 * SR) / SR
    sine = 0.1 * np.sin(2 * np.pi * 1000.0 * t)
    assert levels.lufs_approx(levels.match_lufs(sine, -23.0)) == pytest.approx(-23.0, abs=1e-6)
    casi_silencio = 1e-5 * sine
    subido = levels.match_lufs(casi_silencio, -23.0, max_gain_db=24.0)
    assert np.max(np.abs(subido)) == pytest.approx(np.max(np.abs(casi_silencio)) * 10 ** (24 / 20))


def test_limitador_y_clamp_respetan_el_techo_y_no_tocan_lo_tranquilo():
    tranquilo = np.array([0.1, -0.2, 0.3])
    assert levels.enforce_peak_ceiling(tranquilo) is tranquilo
    assert levels.linear_safety_clamp(tranquilo) is tranquilo
    fuerte = np.array([0.1, -2.0, 5.0])
    limitado = levels.enforce_peak_ceiling(fuerte)
    assert np.max(np.abs(limitado)) < PEAK_CEILING_LINEAR
    assert np.all(np.sign(limitado) == np.sign(fuerte))
    escalado = levels.linear_safety_clamp(fuerte)
    assert np.max(np.abs(escalado)) == pytest.approx(PEAK_CEILING_LINEAR)
    np.testing.assert_allclose(escalado / fuerte, escalado[0] / fuerte[0])  # ganancia uniforme
    assert levels.peak(np.zeros(0)) == 0.0


def test_remove_dc_por_canal():
    x = np.stack([np.full(10, 0.5), np.full(10, -0.25)]) + np.linspace(-1, 1, 10)
    np.testing.assert_allclose(levels.remove_dc(x).mean(axis=-1), [0.0, 0.0], atol=1e-15)


# --- reverberacion -------------------------------------------------------------


def test_reverb_de_un_buffer_vacio_o_de_una_muestra():
    # Antes: `np.max` de un array vacio lanzaba ValueError (y con el,
    # `render_song` con 0 tiempos).
    assert reverb.schroeder_reverb(np.zeros(0), SR).shape == (0,)
    np.testing.assert_allclose(reverb.schroeder_reverb(np.ones(1), SR, wet=0.0), [1.0])


def test_reverb_deja_cola_y_conserva_el_pico_seco():
    x = np.zeros(SR)
    x[0] = 1.0
    y = reverb.schroeder_reverb(x, SR, room_size=0.8, wet=0.5)
    assert y.shape == x.shape
    assert np.all(np.isfinite(y))
    cola = y[int(0.05 * SR) : int(0.3 * SR)]
    assert np.sqrt(np.mean(cola**2)) > 1e-4
    assert np.max(np.abs(y)) <= 1.0 + 1e-9
    # Silencio entra, silencio sale (sin dividir por cero al igualar picos).
    np.testing.assert_array_equal(reverb.schroeder_reverb(np.zeros(1000), SR), np.zeros(1000))


# --- estereo -------------------------------------------------------------------


@pytest.mark.parametrize("n", [0, 1, 10, 20])
def test_decorrelar_una_senal_mas_corta_que_el_fir(n, rng):
    # Antes: `np.convolve(mode="same")` devolvia 21 muestras y no casaba con los graves.
    out = stereo.decorrelate(rng.standard_normal(n), rng, SR)
    assert out.shape == (2, n)
    assert np.all(np.isfinite(out))


def test_decorrelar_da_canales_distintos_con_graves_en_fase(rng):
    t = np.arange(2 * SR) / SR
    mono = np.sin(2 * np.pi * 60.0 * t) + 0.3 * rng.standard_normal(len(t))
    left, right = stereo.decorrelate(mono, rng, SR)
    assert not np.allclose(left, right)
    sos = butter(4, 100.0, fs=SR, output="sos")
    low_l, low_r = sosfilt(sos, left)[SR:], sosfilt(sos, right)[SR:]
    assert np.corrcoef(low_l, low_r)[0, 1] > 0.9


def test_panoramizacion_de_potencia_constante():
    x = np.ones(4)
    for pan in (-1.0, -0.3, 0.0, 0.7, 1.0):
        lr = stereo.pan_constant_power(x, pan)
        np.testing.assert_allclose(lr[0] ** 2 + lr[1] ** 2, 1.0)
    np.testing.assert_allclose(stereo.pan_constant_power(x, -1.0)[1], 0.0, atol=1e-15)
    # Fuera de rango se satura en el extremo.
    np.testing.assert_array_equal(stereo.pan_constant_power(x, 5.0), stereo.pan_constant_power(x, 1.0))


def test_mezclar_evento_dentro_fuera_y_antes_del_colchon():
    bed = np.zeros((2, 100))
    evento = np.arange(1.0, 11.0)
    centro = np.cos(np.pi / 4)

    dentro = stereo.mix_event_into_bed(bed, evento, 95, 0.0)
    np.testing.assert_allclose(dentro[0, 95:], evento[:5] * centro)
    assert np.all(bed == 0.0), "no debe modificar el colchon original"

    np.testing.assert_array_equal(stereo.mix_event_into_bed(bed, evento, 200, 0.0), bed)

    # Antes: un `position` negativo indexaba desde el final y fallaba.
    antes = stereo.mix_event_into_bed(bed, evento, -4, 0.0)
    np.testing.assert_allclose(antes[1, :6], evento[4:] * centro)
    assert np.all(antes[:, 6:] == 0.0)
    np.testing.assert_array_equal(stereo.mix_event_into_bed(bed, evento, -50, 0.0), bed)


# --- bucles --------------------------------------------------------------------


def test_bucle_sin_fundido_es_un_recorte():
    x = np.arange(30.0)
    np.testing.assert_array_equal(loop.seamless_loop(x, 10, 0), x[:10])


def test_bucle_empalma_con_la_continuacion_natural():
    x = np.arange(40.0)
    out = loop.seamless_loop(x, 20, 10)
    assert out.shape == (20,)
    # La primera muestra es el "futuro" (x[20]) y el fundido lleva a x[10:] sin salto.
    assert out[0] == pytest.approx(x[20])
    np.testing.assert_array_equal(out[10:], x[10:20])
    estereo = loop.seamless_loop(np.stack([x, -x]), 20, 10)
    np.testing.assert_allclose(estereo[1], -out)


def test_bucle_rechaza_senales_cortas_o_fundidos_mas_largos_que_el_bucle():
    with pytest.raises(ValueError, match="mas corta"):
        loop.seamless_loop(np.ones(25), 20, 10)
    # Antes: error de broadcasting sin explicacion.
    with pytest.raises(ValueError, match="fundido"):
        loop.seamless_loop(np.ones(30), 10, 20)


# --- filtros -------------------------------------------------------------------


@pytest.mark.parametrize("kind", ["lowpass", "highpass", "bandpass"])
@pytest.mark.parametrize("fc", [0.0, 24_000.0, 1e6])
def test_filtro_estable_con_corte_en_nyquist_o_fuera_de_rango(kind, fc, rng):
    y = filters.static_filter(rng.standard_normal(4800), SR, fc, kind=kind)
    assert np.all(np.isfinite(y))
    assert np.max(np.abs(y)) < 50.0


def test_paso_bajo_deja_pasar_continua_y_paso_alto_la_quita():
    dc = np.ones(SR)
    assert filters.static_filter(dc, SR, 1000.0, kind="lowpass")[-1] == pytest.approx(1.0, abs=1e-6)
    assert filters.static_filter(dc, SR, 1000.0, kind="highpass")[-1] == pytest.approx(0.0, abs=1e-6)
    with pytest.raises(ValueError, match="desconocido"):
        filters.static_filter(dc, SR, 1000.0, kind="notch")


def test_filtro_variable_con_corte_constante_equivale_al_estatico(rng):
    x = rng.standard_normal(3000)
    y = filters.time_varying_filter(x, SR, np.full(3000, 800.0), kind="bandpass", block_size=256)
    np.testing.assert_allclose(y, filters.static_filter(x, SR, 800.0, kind="bandpass"), atol=1e-12)
    assert filters.time_varying_filter(np.zeros(0), SR, np.zeros(0)).shape == (0,)


def test_filtro_variable_rechaza_una_pista_de_otra_longitud():
    # Antes: los bloques sin pista daban corte NaN y la salida entera era NaN, sin error.
    with pytest.raises(ValueError, match="cutoff_track"):
        filters.time_varying_filter(np.ones(1000), SR, np.full(500, 1000.0))


# --- ruido, envolventes y sintesis ----------------------------------------------


@pytest.mark.parametrize("gen", [noise.pink_noise, noise.brown_noise])
def test_ruidos_de_color_normalizados_y_deterministas(gen):
    a = gen(SR, np.random.default_rng(7))
    b = gen(SR, np.random.default_rng(7))
    np.testing.assert_array_equal(a, b)
    assert np.std(a) == pytest.approx(1.0)
    assert not np.array_equal(a, gen(SR, np.random.default_rng(8)))


def test_el_ruido_rosa_cae_con_la_frecuencia():
    x = noise.pink_noise(8 * SR, np.random.default_rng(0))
    spec = np.abs(np.fft.rfft(x)) ** 2
    freqs = np.fft.rfftfreq(len(x), 1 / SR)
    graves = spec[(freqs > 100) & (freqs < 200)].mean()
    agudos = spec[(freqs > 6400) & (freqs < 12800)].mean()
    # -3 dB/octava durante 6 octavas: unos 18 dB.
    assert 10 * np.log10(graves / agudos) == pytest.approx(18.0, abs=3.0)


def test_envolvente_ar_con_duraciones_nulas():
    env = envelopes.ar_envelope(SR, 0.0, 0.0)
    assert len(env) == 2  # una muestra de ataque y otra de relajacion como minimo
    env = envelopes.ar_envelope(SR, 0.01, 0.02, hold_s=0.005)
    assert len(env) == int(0.01 * SR) + int(0.005 * SR) + int(0.02 * SR)
    assert env.max() == 1.0 and env[0] == 0.0 and env[-1] == 0.0


def test_decaimiento_exponencial_y_lfo():
    d = envelopes.exp_decay(SR, 0.0, 0.1)
    assert d.shape == (1,) and d[0] == 1.0
    d = envelopes.exp_decay(SR, 1.0, 0.1)
    assert d[int(0.1 * SR)] == pytest.approx(np.exp(-1.0), rel=1e-3)
    assert np.all(np.isfinite(envelopes.exp_decay(SR, 0.01, 0.0)))
    lfo = envelopes.sine_lfo(SR, SR, 2.0)
    assert lfo.max() == pytest.approx(1.0, abs=1e-6) and lfo.min() == pytest.approx(-1.0, abs=1e-6)


def test_paseo_aleatorio_en_rango(rng):
    walk = envelopes.smooth_random_walk(SR, rng, 2.0, SR, low=0.3, high=0.9)
    assert walk.min() >= 0.3 - 1e-12 and walk.max() <= 0.9 + 1e-12
    assert walk.max() - walk.min() > 0.2


def test_fit_length_recorta_rellena_y_admite_vacio():
    x = np.array([1.0, 2.0, 3.0])
    assert envelopes.fit_length(x, 3) is x
    np.testing.assert_array_equal(envelopes.fit_length(x, 2), [1.0, 2.0])
    np.testing.assert_array_equal(envelopes.fit_length(x, 5), [1.0, 2.0, 3.0, 3.0, 3.0])
    np.testing.assert_array_equal(envelopes.fit_length(np.zeros(0), 2), [0.0, 0.0])


def test_granos_con_duracion_cero_o_tasa_nula(rng):
    assert granular.render_noise_grains(0, SR, rng, 50.0, (0.01, 0.02), (500.0, 900.0)).shape == (0,)
    assert granular.place_grains(SR, SR, rng, 0.0) == []
    # Rejilla regular (jitter 0): huecos de 1/rate (±1 muestra por truncar el tiempo acumulado).
    pos = [p for p, _ in granular.place_grains(SR, SR, rng, 10.0, jitter=0.0)]
    assert pos[0] == SR // 10 and len(pos) >= 9
    assert np.all(np.abs(np.diff(pos) - SR // 10) <= 1)
    amps = [a for _, a in granular.place_grains(SR, SR, rng, 200.0)]
    assert min(amps) >= 0.5 and max(amps) <= 1.0


def test_granos_no_se_salen_del_buffer(rng):
    out = granular.render_noise_grains(1000, SR, rng, 2000.0, (0.05, 0.1), (500.0, 900.0))
    assert out.shape == (1000,)
    assert np.any(out != 0.0)


def test_sintesis_modal_rechaza_listas_de_modos_desparejadas():
    # `zip` sin `strict` descartaba en silencio los modos sobrantes.
    with pytest.raises(ValueError):
        modal.modal_hit(SR, 0.1, 200.0, [1.0, 2.7], [0.1], [1.0, 0.5])


def test_sintesis_modal_desafina_solo_con_rng(rng):
    a = modal.modal_hit(SR, 0.05, 300.0, [1.0, 2.7], [0.1, 0.05], [1.0, 0.5])
    b = modal.modal_hit(SR, 0.05, 300.0, [1.0, 2.7], [0.1, 0.05], [1.0, 0.5], detune=0.01)
    np.testing.assert_array_equal(a, b)
    c = modal.modal_hit(SR, 0.05, 300.0, [1.0, 2.7], [0.1, 0.05], [1.0, 0.5], rng=rng, detune=0.01)
    assert not np.array_equal(a, c)
    assert modal.modal_hit(SR, 0.0, 300.0, [1.0], [0.1], [1.0]).shape == (1,)


def test_fm_y_aditiva_en_nyquist_siguen_acotadas():
    n = 4800
    nyq = np.full(n, SR / 2)
    env = np.ones(n)
    y = fm.fm_chirp(SR, nyq, 2.0, env * 3.0, env)
    assert np.all(np.isfinite(y)) and np.max(np.abs(y)) <= 1.0
    add = fm.additive_harmonics(SR, np.full(n, 440.0), 6, 0.6, env)
    assert np.max(np.abs(add)) == pytest.approx(1.0)
    # En Nyquist todos los armonicos caen en ceros de muestreo: sin dividir por un pico nulo.
    assert np.all(np.isfinite(fm.additive_harmonics(SR, nyq, 4, 0.5, env)))


# --- teoria y humanizacion ---------------------------------------------------------


def test_grados_negativos_y_por_encima_de_la_escala_envuelven_octavas():
    root = 220.0
    assert theory.degree_freq(root, "major_pentatonic", 0) == root
    assert theory.degree_freq(root, "major_pentatonic", 5) == pytest.approx(2 * root)
    assert theory.degree_freq(root, "major_pentatonic", -5) == pytest.approx(root / 2)
    assert theory.degree_freq(root, "minor_pentatonic", -1) == pytest.approx(theory.semitone_freq(root, -2))
    assert theory.chord_freqs(root, 0, "maj") == pytest.approx([root, theory.semitone_freq(root, 4), theory.semitone_freq(root, 7)])


def test_humanizacion_acotada(rng):
    for _ in range(200):
        assert humanize.humanize_timing_s(0.0, rng, 0.02) >= 0.0
        assert 0.05 <= humanize.humanize_velocity(1.1, rng, 0.5) <= 1.15
        assert 2 ** (-6 / 1200) <= humanize.micro_detune_ratio(rng) <= 2 ** (6 / 1200)


# --- secuenciador ------------------------------------------------------------------


def test_cancion_de_cero_tiempos_es_silencio_vacio():
    # Antes: la reverberacion final fallaba con el bus vacio.
    assert sequencer.render_song(120.0, SR, 0.0, []).shape == (2, 0)


def test_notas_fuera_del_bus_se_descartan_y_las_de_dentro_suenan(rng):
    track = sequencer.Track(instrument="marimba", humanize_timing_s=0.0)
    track.events.append((0.0, 1.0, 440.0, 0.8, 0.0))
    track.events.append((100.0, 1.0, 440.0, 0.8, 0.0))  # mas alla del final
    audio = sequencer.render_song(120.0, SR, 4.0, [track], seed_rng=rng, reverb_wet=0.0)
    assert audio.shape == (2, 2 * SR)
    assert np.max(np.abs(audio[:, : SR // 2])) > 0.05
    assert np.all(np.isfinite(audio))


def test_frase_de_flauta_muy_corta_o_con_notas_solapadas(rng):
    # Con FluidSynth la frase lleva la cola de la muestra: se comprueba el contrato
    # (inicio en la rejilla, audio mono, finito y no vacío), no la longitud exacta.
    audio, start = sequencer.render_flute_phrase([(2.0, 0.0001, 440.0, 0.5)], 120.0, SR, rng)
    assert start == 1.0 and audio.ndim == 1 and audio.size > 0
    assert np.all(np.isfinite(audio))
    notas = [(0.0, 1.0, 440.0, 0.5), (0.5, 1.0, 660.0, 0.5), (4.0, 1.0, 550.0, 0.5)]
    audio, start = sequencer.render_flute_phrase(notas, 120.0, SR, rng)
    assert start == 0.0 and audio.ndim == 1 and audio.size >= int(2.5 * SR)
    assert np.all(np.isfinite(audio))
