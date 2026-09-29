"""CLI `explored-audio`: subcomandos, codigos de salida y rutas de salida.
Siempre con `--out` a una carpeta temporal: los tests nunca escriben en el
repo (ni en Art/Export/Audio ni en Content/Data)."""

from __future__ import annotations

import json

import pytest

from explored_audio import build
from explored_audio.cli import main
from explored_audio.music.layers import build_payload, to_json


@pytest.fixture
def catalogo_reducido(monkeypatch, rendered, specs_by_name):
    """Dos sonidos (un efecto mono y un ambiente estereo) desde la cache de
    sesion: la CLI recorre el mismo `build_all`, pero sin sintetizar 132."""
    specs = [specs_by_name["sfx_ui_click"], specs_by_name["amb_ocean_calm"]]
    monkeypatch.setattr(build, "build_catalog", lambda: specs)
    monkeypatch.setattr(build, "render_sound", lambda spec: rendered[spec.name])
    return specs


def test_build_escribe_en_la_ruta_pedida(tmp_path, catalogo_reducido, capsys):
    assert main(["build", "--out", str(tmp_path), "--quiet"]) == 0
    assert capsys.readouterr().out == ""
    manifest = json.loads((tmp_path / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["count"] == 2
    assert (tmp_path / "Efectos" / "sfx_ui_click.wav").exists()
    assert (tmp_path / "Ambiente" / "amb_ocean_calm.wav").exists()


def test_build_sin_quiet_imprime_el_progreso(tmp_path, catalogo_reducido, capsys):
    assert main(["build", "--out", str(tmp_path)]) == 0
    out = capsys.readouterr().out
    assert "sfx_ui_click" in out
    assert "2 sonidos exportados" in out


def test_music_layers_escribe_el_json_en_la_ruta_pedida(tmp_path, capsys):
    out_path = tmp_path / "datos" / "music_layers.json"
    assert main(["music-layers", "--out", str(out_path)]) == 0
    assert capsys.readouterr().out.strip() == str(out_path)
    assert out_path.read_text(encoding="utf-8") == to_json(build_payload())


@pytest.mark.parametrize("argv", [[], ["desconocido"], ["build", "--no-existe"]])
def test_argumentos_invalidos_salen_con_codigo_2(argv, capsys):
    with pytest.raises(SystemExit) as exc:
        main(argv)
    assert exc.value.code == 2
    assert "usage" in capsys.readouterr().err


def test_ayuda_sale_con_codigo_0(capsys):
    with pytest.raises(SystemExit) as exc:
        main(["--help"])
    assert exc.value.code == 0
    out = capsys.readouterr().out
    assert "build" in out and "music-layers" in out
