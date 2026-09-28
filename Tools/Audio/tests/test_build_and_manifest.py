"""El pipeline de build escribe WAV + manifest.json coherente, en una carpeta
temporal (nunca se escribe en Art/Export/Audio desde los tests)."""

from __future__ import annotations

import json
import math

import numpy as np
import pytest
import soundfile as sf

from explored_audio import build
from explored_audio.build import build_all, default_output_root, finalize, render_by_name, render_sound
from explored_audio.catalog import SoundSpec
from explored_audio.constants import PEAK_CEILING_LINEAR, SAMPLE_RATE
from explored_audio.io_utils import to_wav_bytes, write_wav
from explored_audio.manifest import build_entry, write_manifest


def test_build_all_escribe_wav_y_manifest_coherente(tmp_path, monkeypatch, rendered):
    # La sintesis ya la cubre la cache de sesion (y su determinismo,
    # `test_determinism`): aqui se comprueba la exportacion, sin volver a
    # generar los 132 sonidos.
    monkeypatch.setattr(build, "render_sound", lambda spec: rendered[spec.name])
    entries = build_all(output_root=tmp_path, verbose=False)
    assert len(entries) == 132

    manifest_path = tmp_path / "manifest.json"
    assert manifest_path.exists()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert manifest["count"] == 132
    assert manifest["sample_rate"] == SAMPLE_RATE
    assert manifest["bit_depth"] == 16

    by_name = {s["name"]: s for s in manifest["sounds"]}
    assert set(by_name) == {e["name"] for e in entries}
    assert [(s["category"], s["name"]) for s in manifest["sounds"]] == sorted(
        (s["category"], s["name"]) for s in manifest["sounds"]
    )
    assert len(list(tmp_path.rglob("*.wav"))) == 132

    # Comprobacion cruzada con un par de ficheros: el WAV en disco coincide
    # con lo que dice el manifiesto (duracion, canales, ruta).
    for name in ("amb_ocean_calm", "sfx_ui_click", "mus_theme"):
        entry = by_name[name]
        wav_path = tmp_path / entry["file"]
        assert wav_path.exists()
        data, sr = sf.read(str(wav_path))
        assert sr == SAMPLE_RATE
        channels = 1 if data.ndim == 1 else data.shape[1]
        assert channels == entry["channels"]
        duration_s = data.shape[0] / sr
        assert abs(duration_s - entry["duration_s"]) < 0.01
        # Lo que hay en disco es exactamente lo renderizado, cuantizado a 16 bits.
        assert wav_path.read_bytes() == to_wav_bytes(rendered[name])


def test_build_all_informa_del_progreso(tmp_path, monkeypatch, rendered, specs_by_name, capsys):
    specs = [specs_by_name["sfx_ui_click"], specs_by_name["amb_ocean_calm"]]
    monkeypatch.setattr(build, "build_catalog", lambda: specs)
    monkeypatch.setattr(build, "render_sound", lambda spec: rendered[spec.name])
    build_all(output_root=tmp_path, verbose=True)
    out = capsys.readouterr().out
    assert "sfx_ui_click" in out and "mono" in out and "one-shot" in out
    assert "amb_ocean_calm" in out and "estereo" in out and "bucle" in out
    assert "2 sonidos exportados" in out


def test_la_raiz_por_defecto_es_art_export_audio_del_repo():
    root = default_output_root()
    assert root.parts[-3:] == ("Art", "Export", "Audio")
    assert (root.parents[2] / "Tools" / "Audio" / "pyproject.toml").exists()


def test_render_by_name_encuentra_el_sonido_o_falla(rendered):
    np.testing.assert_array_equal(render_by_name("sfx_ui_click"), rendered["sfx_ui_click"])
    with pytest.raises(KeyError, match="no_existe"):
        render_by_name("no_existe")


def test_un_generador_con_nan_no_llega_al_wav():
    # Antes el NaN atravesaba el limitador (`tanh(nan)`) y acababa en el WAV
    # y en el manifiesto (`NaN` no es JSON valido).
    spec = SoundSpec("sfx_roto", "Efectos", False, lambda _n: np.array([0.0, np.nan, 0.1]))
    with pytest.raises(ValueError, match="sfx_roto"):
        render_sound(spec)
    with pytest.raises(ValueError, match="no finitos"):
        finalize(np.array([0.0, np.inf]), "Efectos")


@pytest.mark.parametrize("category", ["Efectos", "Ambiente", "Musica"])
def test_finalize_quita_continua_y_respeta_el_techo(category):
    rng = np.random.default_rng(3)
    audio = 3.0 * rng.standard_normal((2, 4800)) + 0.5  # muy por encima del techo y con continua
    out = finalize(audio, category)
    assert out.shape == audio.shape
    assert np.max(np.abs(out)) <= PEAK_CEILING_LINEAR + 1e-12
    assert np.all(np.abs(out.mean(axis=-1)) < 1e-9)


def test_finalize_no_toca_una_senal_tranquila_de_efectos():
    x = 0.1 * np.sin(2 * np.pi * 440.0 * np.arange(4800) / SAMPLE_RATE)
    np.testing.assert_allclose(finalize(x, "Efectos"), x - x.mean(), atol=1e-15)


def test_entrada_del_manifiesto_redondea_y_apunta_al_fichero():
    entry = build_entry("sfx_x", "Efectos", False, 1, 1.234567, -18.3456)
    assert entry == {
        "name": "sfx_x",
        "category": "Efectos",
        "loop": False,
        "channels": 1,
        "duration_s": 1.2346,
        "lufs_approx": -18.35,
        "file": "Efectos/sfx_x.wav",
    }


def test_write_manifest_crea_carpetas_y_ordena(tmp_path):
    entries = [
        build_entry("b", "Efectos", False, 1, 1.0, -20.0),
        build_entry("z", "Ambiente", True, 2, 30.0, -23.0),
        build_entry("a", "Efectos", False, 1, 0.5, -19.0),
    ]
    out = tmp_path / "anidada" / "manifest.json"
    write_manifest(entries, out)
    payload = json.loads(out.read_text(encoding="utf-8"))
    assert payload["count"] == 3
    assert [s["name"] for s in payload["sounds"]] == ["z", "a", "b"]


def test_write_manifest_rechaza_nan(tmp_path):
    entries = [build_entry("a", "Efectos", False, 1, 0.5, math.nan)]
    with pytest.raises(ValueError):
        write_manifest(entries, tmp_path / "manifest.json")


def test_write_wav_mono_y_estereo_ida_y_vuelta(tmp_path):
    t = np.arange(480) / SAMPLE_RATE
    mono = 0.5 * np.sin(2 * np.pi * 1000.0 * t)
    stereo = np.stack([mono, -mono])
    write_wav(tmp_path / "sub" / "mono.wav", mono)
    write_wav(tmp_path / "estereo.wav", stereo)

    data_m, sr_m = sf.read(str(tmp_path / "sub" / "mono.wav"))
    data_s, _ = sf.read(str(tmp_path / "estereo.wav"))
    info = sf.info(str(tmp_path / "estereo.wav"))
    assert sr_m == SAMPLE_RATE and info.subtype == "PCM_16"
    assert data_m.shape == (480,)
    assert data_s.shape == (480, 2)
    # Error de cuantizacion de 16 bits como mucho.
    np.testing.assert_allclose(data_m, mono, atol=1.0 / 32768)
    np.testing.assert_allclose(data_s[:, 1], -mono, atol=1.0 / 32768)
    assert (tmp_path / "estereo.wav").read_bytes() == to_wav_bytes(stereo)
