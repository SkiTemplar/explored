"""Master de la musica: sonoridad integrada BS.1770, limitador de pico con
anticipacion y reverb de sala. Primero las piezas de medida por separado
(con señales cuyo resultado se conoce de antemano) y despues las
propiedades del catalogo renderizado: sin silencios, sin clipping, sin
limitar de mas."""

from __future__ import annotations

import numpy as np
import pytest

from explored_audio.build import MUSIC_TARGET_LUFS
from explored_audio.constants import PEAK_CEILING_LINEAR, SAMPLE_RATE
from explored_audio.levels import (
    integrated_lufs,
    lookahead_limiter,
    master_to_lufs,
    true_peak_envelope,
)
from explored_audio.reverb import room_impulse_response, room_reverb

SR = SAMPLE_RATE


def _sine(freq: float, seconds: float, amp: float = 1.0, phase: float = 0.0) -> np.ndarray:
    t = np.arange(int(seconds * SR)) / SR
    return amp * np.sin(2 * np.pi * freq * t + phase)


# --- Sonoridad integrada -----------------------------------------------------


def test_lufs_de_un_seno_de_1khz_a_0_dbfs_mono():
    """Valor de referencia de BS.1770: un seno de 997 Hz a 0 dBFS en un canal
    mide -3,01 LUFS (el filtro K apenas pesa a esa frecuencia)."""
    assert integrated_lufs(_sine(997.0, 5.0), SR) == pytest.approx(-3.01, abs=0.05)


def test_lufs_suma_la_potencia_de_los_dos_canales():
    """Mismo seno en L y R: +3 dB sobre el mono (los canales se suman, no se
    promedian)."""
    mono = _sine(997.0, 5.0, amp=0.5)
    assert integrated_lufs(np.stack([mono, mono]), SR) == pytest.approx(integrated_lufs(mono, SR) + 3.01, abs=0.05)


def test_la_puerta_ignora_el_silencio():
    """Diez segundos de seno y diez de silencio digital miden lo mismo que el
    seno solo: la puerta absoluta descarta los bloques mudos. Los tres
    bloques que cruzan la frontera sonido/silencio si pasan la puerta (es lo
    que dice la norma) y bajan la media una pizca: de ahi la tolerancia."""
    tone = _sine(997.0, 10.0, amp=0.25)
    with_silence = np.concatenate([tone, np.zeros_like(tone)])
    assert integrated_lufs(with_silence, SR) == pytest.approx(integrated_lufs(tone, SR), abs=0.15)


def test_la_puerta_relativa_ignora_el_tramo_mucho_mas_bajo():
    loud = _sine(997.0, 10.0, amp=0.5)
    quiet = _sine(997.0, 10.0, amp=0.5 * 10 ** (-30 / 20))  # 30 dB por debajo
    assert integrated_lufs(np.concatenate([loud, quiet]), SR) == pytest.approx(integrated_lufs(loud, SR), abs=0.15)
    # Sin puerta relativa el tramo bajo arrastraria la media ~3 dB.
    assert integrated_lufs(np.concatenate([loud, quiet]), SR) > integrated_lufs(loud, SR) - 1.0


@pytest.mark.parametrize("audio", [np.zeros(0), np.zeros(SR * 2), np.zeros((2, SR))])
def test_lufs_de_silencio_o_vacio(audio):
    assert integrated_lufs(audio, SR) == -120.0


def test_lufs_de_una_senal_mas_corta_que_un_bloque():
    short = _sine(997.0, 0.2)
    assert integrated_lufs(short, SR) == pytest.approx(-3.01, abs=0.1)


# --- Limitador ---------------------------------------------------------------


def _loud_program(seed: int = 3, seconds: float = 6.0) -> np.ndarray:
    """Ruido con transitorios muy por encima del techo, en estereo."""
    rng = np.random.default_rng(seed)
    n = int(seconds * SR)
    x = 0.3 * rng.standard_normal((2, n))
    for pos in rng.integers(0, n - 200, 30):
        x[:, pos : pos + 200] *= 6.0
    return x


def test_limitador_respeta_el_techo_incluso_entre_muestras():
    y = lookahead_limiter(_loud_program(), PEAK_CEILING_LINEAR, SR)
    assert float(np.max(np.abs(y))) <= PEAK_CEILING_LINEAR
    assert float(true_peak_envelope(y).max()) <= PEAK_CEILING_LINEAR * 1.002


def test_limitador_no_toca_una_senal_bajo_el_techo():
    x = np.stack([_sine(220.0, 2.0, 0.3), _sine(330.0, 2.0, 0.3)])
    assert np.array_equal(lookahead_limiter(x, PEAK_CEILING_LINEAR, SR), x)


def test_limitador_no_distorsiona_la_forma_de_onda():
    """Un seno demasiado alto sale como el mismo seno, mas bajo: la ganancia
    es lenta (sin el recorte ni la saturacion que añaden armonicos)."""
    x = _sine(440.0, 3.0, amp=2.0)
    y = lookahead_limiter(x, PEAK_CEILING_LINEAR, SR)
    mid = slice(SR, 2 * SR)
    ratio = y[mid] / np.where(np.abs(x[mid]) > 1e-3, x[mid], np.nan)
    ratio = ratio[np.isfinite(ratio)]
    assert float(np.std(ratio)) < 1e-3
    assert float(np.max(np.abs(y))) <= PEAK_CEILING_LINEAR


def test_limitador_circular_es_continuo_en_la_union():
    """En un bucle, un pico al final debe bajar tambien el principio: si no,
    la ganancia salta de golpe al volver a empezar."""
    x = _sine(440.0, 2.0, amp=0.5)
    x[-200:] *= 4.0  # pico justo antes de la union
    y = lookahead_limiter(x, PEAK_CEILING_LINEAR, SR, circular=True)
    gain_start = y[:50] / np.where(np.abs(x[:50]) > 1e-3, x[:50], np.nan)
    assert np.nanmax(gain_start) < 0.999


def test_limitador_con_entradas_raras():
    assert lookahead_limiter(np.zeros(0), PEAK_CEILING_LINEAR, SR).size == 0
    silence = np.zeros((2, SR))
    assert np.array_equal(lookahead_limiter(silence, PEAK_CEILING_LINEAR, SR), silence)
    impulse = np.zeros(SR)
    impulse[SR // 2] = 50.0
    y = lookahead_limiter(impulse, PEAK_CEILING_LINEAR, SR)
    assert np.all(np.isfinite(y))
    assert float(np.max(np.abs(y))) <= PEAK_CEILING_LINEAR


def test_master_alcanza_el_objetivo_y_el_techo():
    y = master_to_lufs(_loud_program() * 0.05, MUSIC_TARGET_LUFS, sr=SR)
    assert integrated_lufs(y, SR) == pytest.approx(MUSIC_TARGET_LUFS, abs=0.2)
    assert float(np.max(np.abs(y))) <= PEAK_CEILING_LINEAR


def test_master_de_silencio_no_inventa_senal():
    silence = np.zeros((2, SR))
    assert np.array_equal(master_to_lufs(silence, MUSIC_TARGET_LUFS, sr=SR), silence)


# --- Reverb de sala ----------------------------------------------------------


def test_respuesta_de_sala_determinista_y_decorrelada():
    a = room_impulse_response(SR, 0.6, seed=7)
    b = room_impulse_response(SR, 0.6, seed=7)
    assert np.array_equal(a, b)
    assert np.sum(a[0] ** 2) == pytest.approx(1.0, rel=1e-9)
    corr = float(np.corrcoef(a[0], a[1])[0, 1])
    assert abs(corr) < 0.3, "L y R casi iguales: la sala sonaria en mono"


def test_sala_mas_grande_suena_mas_larga():
    def decay_time(ir: np.ndarray) -> float:
        energy = np.cumsum(ir[0][::-1] ** 2)[::-1]
        below = np.nonzero(energy < energy[0] * 1e-3)[0]  # -30 dB
        return float(below[0]) / SR

    assert decay_time(room_impulse_response(SR, 0.9)) > decay_time(room_impulse_response(SR, 0.2)) * 1.5


def test_la_cola_decae_antes_en_agudos():
    ir = room_impulse_response(SR, 0.7, seed=1)[0]
    spec_early = np.abs(np.fft.rfft(ir[: SR // 5]))
    spec_late = np.abs(np.fft.rfft(ir[SR // 2 : SR // 2 + SR // 5]))
    freqs = np.fft.rfftfreq(SR // 5, 1 / SR)
    lo, hi = freqs < 400, freqs > 4000

    def tilt(spec: np.ndarray) -> float:
        return float(np.sum(spec[hi] ** 2) / np.sum(spec[lo] ** 2))

    assert tilt(spec_late) < tilt(spec_early) * 0.5


def test_reverb_conserva_longitud_y_acepta_mono():
    x = np.zeros(SR)
    x[100] = 1.0
    y = room_reverb(x, SR, 0.5, wet=0.3)
    assert y.shape == (2, SR)
    assert np.all(np.isfinite(y))
    assert float(np.sum(np.abs(y[:, SR // 4 :]))) > 0.0, "sin cola de sala"


# --- Catalogo renderizado ----------------------------------------------------


def _music(catalog):
    return [spec for spec in catalog if spec.category == "Musica"]


def test_pico_verdadero_de_la_musica_bajo_el_techo(catalog, rendered):
    for spec in _music(catalog):
        tp = float(true_peak_envelope(rendered[spec.name]).max())
        assert tp <= PEAK_CEILING_LINEAR * 1.005, f"{spec.name}: pico verdadero {tp:.4f} sobre el techo"


def test_el_limitador_trabaja_poco(catalog, rendered):
    """Un limitador que aplasta la mezcla la deja sin dinamica (y suena a
    "ladrillo"). Se mide el factor de cresta: pico frente a RMS. Una mezcla
    acustica a -16 LUFS debe conservar al menos ~9 dB."""
    for spec in _music(catalog):
        audio = rendered[spec.name]
        crest_db = 20 * np.log10(float(np.max(np.abs(audio))) / float(np.sqrt(np.mean(audio**2))))
        assert crest_db >= 9.0, f"{spec.name}: factor de cresta {crest_db:.1f} dB, demasiado limitado"


def _window_dbfs(audio: np.ndarray, window_s: float, circular: bool) -> np.ndarray:
    mono_sq = np.mean(audio**2, axis=0)
    win = int(window_s * SR)
    if circular:
        mono_sq = np.concatenate([mono_sq, mono_sq[:win]])
    csum = np.concatenate([[0.0], np.cumsum(mono_sq)])
    hop = win // 4
    starts = np.arange(0, len(mono_sq) - win + 1, hop)
    ms = (csum[starts + win] - csum[starts]) / win
    return 10 * np.log10(np.maximum(ms, 1e-20))


def test_musica_sin_huecos_de_silencio(catalog, rendered):
    """Ninguna ventana de 1,5 s del cuerpo de una pieza cae por debajo de
    -50 dBFS RMS: una pista MIDI que no sonase (programa inexistente en el
    soundfont, canal mal asignado) o un recorte que dejase silencio en medio
    del bucle se notaria aqui. En las piezas sin bucle se excluye la cola
    final, que se funde a silencio a proposito."""
    for spec in _music(catalog):
        audio = rendered[spec.name]
        if not spec.is_loop:
            audio = audio[:, : max(audio.shape[-1] - int(3.0 * SR), int(1.5 * SR))]
        levels = _window_dbfs(audio, 1.5, circular=spec.is_loop)
        worst = float(levels.min())
        assert worst > -50.0, f"{spec.name}: hay un tramo de {worst:.1f} dBFS (silencio)"


def test_los_bucles_empiezan_y_acaban_sonando(catalog, rendered):
    """Un bucle no puede arrancar desde silencio: se oiria un hueco en cada
    vuelta. Se compara el primer y el ultimo segundo con el nivel medio."""
    for spec in _music(catalog):
        if not spec.is_loop:
            continue
        audio = rendered[spec.name]
        overall = float(np.sqrt(np.mean(audio**2)))
        for part, label in ((audio[:, :SR], "inicio"), (audio[:, -SR:], "final")):
            level = float(np.sqrt(np.mean(part**2)))
            assert level > overall * 0.1, f"{spec.name}: {label} casi en silencio"
