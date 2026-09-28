"""Medidor LUFS y cadena de tratamiento de la musica grabada, con senales
sinteticas cuya respuesta se conoce de antemano (no necesitan red)."""

from __future__ import annotations

import hashlib

import numpy as np
import pytest
import soundfile as sf

from explored_audio.constants import SAMPLE_RATE
from explored_audio.recorded.fetch import ensure_original, export_ogg, sha256_file
from explored_audio.recorded.loudness import integrated_lufs, k_weighting, sample_peak_dbfs
from explored_audio.recorded.process import find_sound_bounds, limit_peaks, process
from explored_audio.recorded.sources import load_sources


def _sine(freq: float, seconds: float, amp: float, fs: int = SAMPLE_RATE) -> np.ndarray:
    t = np.arange(int(seconds * fs)) / fs
    return amp * np.sin(2 * np.pi * freq * t)


def test_filtro_k_a_48k_coincide_con_la_tabla_de_la_norma():
    (b1, a1), (b2, a2) = k_weighting(48_000)
    np.testing.assert_allclose(b1, [1.53512485958697, -2.69169618940638, 1.19839281085285], atol=1e-9)
    np.testing.assert_allclose(a1, [1.0, -1.69065929318241, 0.73248077421585], atol=1e-9)
    np.testing.assert_allclose(b2, [1.0, -2.0, 1.0])
    np.testing.assert_allclose(a2, [1.0, -1.99004745483398, 0.99007225036621], atol=1e-9)


@pytest.mark.parametrize("fs", [44_100, 48_000, 96_000])
def test_seno_de_1khz_a_0_dbfs_en_un_canal_mide_menos_3_lufs(fs):
    # Caso de referencia de BS.1770: 997 Hz a 0 dBFS, un canal -> -3,01 LUFS.
    x = _sine(997.0, 5.0, 1.0, fs)
    assert integrated_lufs(x, fs) == pytest.approx(-3.01, abs=0.05)


def test_estereo_suma_los_dos_canales():
    x = _sine(997.0, 5.0, 1.0)
    assert integrated_lufs(np.stack([x, x], axis=1), SAMPLE_RATE) == pytest.approx(0.0, abs=0.05)


def test_la_puerta_ignora_los_silencios():
    # El mismo tono con 20 s de silencio alrededor debe medir igual: sin
    # puertas saldria unos 6 dB por debajo.
    # Tono largo: los bloques de 400 ms que pisan el borde pesan poco.
    tone = _sine(997.0, 30.0, 0.5)
    pad = np.zeros(20 * SAMPLE_RATE)
    padded = np.concatenate([pad, tone, pad])
    assert integrated_lufs(padded, SAMPLE_RATE) == pytest.approx(integrated_lufs(tone, SAMPLE_RATE), abs=0.1)


def test_silencio_y_senal_corta_dan_menos_infinito():
    assert integrated_lufs(np.zeros(SAMPLE_RATE * 2), SAMPLE_RATE) == float("-inf")
    assert integrated_lufs(_sine(440, 0.1, 0.5), SAMPLE_RATE) == float("-inf")
    assert sample_peak_dbfs(np.zeros(10)) == float("-inf")


def test_lufs_rechaza_nan_y_frecuencia_invalida():
    x = _sine(440, 1.0, 0.5)
    x[100] = np.nan
    with pytest.raises(ValueError):
        integrated_lufs(x, SAMPLE_RATE)
    with pytest.raises(ValueError):
        integrated_lufs(_sine(440, 1.0, 0.5), 0)


def test_limitador_nunca_deja_pasar_un_pico():
    rng = np.random.default_rng(7)
    x = rng.normal(0, 0.05, (SAMPLE_RATE * 3, 2))
    x[1000] = 4.0  # un pico aislado muy por encima
    x[50_000:50_010] = -3.0
    y = limit_peaks(x, SAMPLE_RATE, -1.5)
    assert np.max(np.abs(y)) <= 10 ** (-1.5 / 20) + 1e-12
    # Lejos de los picos no toca nada.
    np.testing.assert_array_equal(y[100_000:110_000], x[100_000:110_000])


def test_limites_del_sonido_con_silencio_alrededor():
    fs = SAMPLE_RATE
    x = np.concatenate([np.zeros(3 * fs), _sine(440, 2.0, 0.3), np.zeros(4 * fs)])
    start, end = find_sound_bounds(np.stack([x, x], axis=1), fs)
    assert 2.7 * fs < start < 3.0 * fs
    assert 5.0 * fs < end < 5.3 * fs
    assert find_sound_bounds(np.zeros((fs, 2)), fs) == (0, 0)


def _piano_like(seconds: float = 20.0, fs: int = 44_100) -> np.ndarray:
    """Notas con ataque seco y caida larga, mucho rango dinamico: se parece
    a un piano en lo que importa al limitador y a la puerta."""
    rng = np.random.default_rng(3)
    n = int(seconds * fs)
    out = np.zeros(n)
    t = np.arange(int(3 * fs)) / fs
    for start in np.arange(0.5, seconds - 3, 0.45):
        f = 220 * 2 ** (rng.integers(0, 24) / 12)
        amp = rng.uniform(0.02, 0.3)
        note = amp * np.exp(-t * 1.5) * np.sin(2 * np.pi * f * t)
        i = int(start * fs)
        out[i : i + len(note)] += note[: n - i]
    return np.stack([out, 0.9 * out], axis=1)


def test_process_llega_al_objetivo_sin_pasar_del_techo():
    x = _piano_like()
    x = np.concatenate([np.zeros((44_100 * 2, 2)), x * 0.2, np.zeros((44_100 * 3, 2))])
    y, report = process(x, 44_100, -16.0, -1.5)
    assert report.lufs_out == pytest.approx(-16.0, abs=0.5)
    assert report.peak_out_dbfs <= -1.5 + 1e-9
    assert report.trimmed_head_s > 1.5 and report.trimmed_tail_s > 2.0
    assert y.shape[1] == 2
    assert y.dtype == np.float64


def test_process_es_determinista():
    x = _piano_like(10.0)
    a, _ = process(x, 44_100, -16.0, -1.5)
    b, _ = process(x.copy(), 44_100, -16.0, -1.5)
    assert hashlib.sha256(a.tobytes()).digest() == hashlib.sha256(b.tobytes()).digest()


def test_process_acepta_mono():
    y, _ = process(_piano_like(8.0)[:, 0], 44_100, -16.0, -1.5)
    assert y.ndim == 2 and y.shape[1] == 2


@pytest.mark.parametrize(
    "bad",
    [np.zeros((0, 2)), np.zeros((44_100 * 3, 2)), np.full((44_100, 2), np.nan)],
    ids=["vacio", "silencio", "nan"],
)
def test_process_rechaza_grabaciones_inservibles(bad):
    with pytest.raises(ValueError):
        process(bad, 44_100, -16.0, -1.5)


def test_ogg_exportado_conserva_nivel_y_no_satura(tmp_path):
    y, _ = process(_piano_like(12.0), 44_100, -16.0, -1.5)
    piece = load_sources().pieces[0]
    out = tmp_path / "x.ogg"
    export_ogg(out, y, piece)
    back, fs = sf.read(out, always_2d=True)
    assert fs == SAMPLE_RATE and back.shape[1] == 2
    assert integrated_lufs(back, fs) == pytest.approx(-16.0, abs=0.6)
    assert np.max(np.abs(back)) < 1.0
    with sf.SoundFile(out) as f:
        assert f.title == piece.title


def test_ogg_exportado_es_reproducible_byte_a_byte(tmp_path):
    y, _ = process(_piano_like(6.0), 44_100, -16.0, -1.5)
    piece = load_sources().pieces[0]
    export_ogg(tmp_path / "a.ogg", y, piece)
    export_ogg(tmp_path / "b.ogg", y, piece)
    assert (tmp_path / "a.ogg").read_bytes() == (tmp_path / "b.ogg").read_bytes()


def test_hash_distinto_borra_el_original_y_falla(tmp_path, monkeypatch):
    piece = load_sources().pieces[0]
    path = tmp_path / "src" / f"{piece.id}{piece.extension}"
    path.parent.mkdir(parents=True)
    path.write_bytes(b"no es el fichero revisado")
    import explored_audio.recorded.fetch as fetch

    calls = []

    def fake_download(url, dest, retries=0):
        calls.append(url)
        dest.write_bytes(b"sigue sin serlo")

    monkeypatch.setattr(fetch, "download", fake_download)
    with pytest.raises(ValueError, match="SHA-256"):
        ensure_original(tmp_path, piece)
    assert calls == [piece.download_url], "debe intentar bajarlo de nuevo una sola vez"
    assert not path.exists(), "un fichero con hash equivocado no puede quedarse en la cache"


def test_sha256_file(tmp_path):
    p = tmp_path / "a.bin"
    p.write_bytes(b"abc")
    assert sha256_file(p) == hashlib.sha256(b"abc").hexdigest()
