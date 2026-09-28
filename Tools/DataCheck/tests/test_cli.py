"""CLI ``datacheck``: códigos de salida, opciones y ficheros que escribe.

Casi todos usan ``--root`` con un repositorio temporal, así que no simulan el catálogo real.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from datacheck import cli, cooking
from datacheck.checks import DATA_FILES, Report


def _repo(tmp_path: Path, **files: object) -> Path:
    data = tmp_path / "Content" / "Data"
    data.mkdir(parents=True)
    for name, doc in files.items():
        (data / name.replace("_json", ".json")).write_text(json.dumps(doc), encoding="utf-8")
    return tmp_path


def test_repo_vacio_falla_con_codigo_1(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    rc = cli.main(["--root", str(_repo(tmp_path))])
    out = capsys.readouterr().out
    assert rc == 1
    assert f"ERROR  Falta Content/Data/{DATA_FILES[0]}" in out
    assert out.rstrip().splitlines()[-1].endswith("notas")


def test_json_mal_formado_no_lanza_excepcion(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    root = _repo(tmp_path)
    (root / "Content" / "Data" / "items.json").write_text("{\"id\": ", encoding="utf-8")
    rc = cli.main(["--root", str(root), "--quiet"])
    out = capsys.readouterr().out
    assert rc == 1
    assert "ERROR  Content/Data/items.json: JSON mal formado" in out


def test_quiet_oculta_las_notas(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    root = _repo(tmp_path)
    cli.main(["--root", str(root)])
    assert "INFO   " in capsys.readouterr().out
    cli.main(["--root", str(root), "--quiet"])
    assert "INFO   " not in capsys.readouterr().out


@pytest.mark.parametrize(("report", "strict", "code"), [
    (Report(), False, 0),
    (Report(), True, 0),
    (Report(warnings=["aviso"]), False, 0),
    (Report(warnings=["aviso"]), True, 1),
    (Report(errors=["error"]), False, 1),
])
def test_codigo_de_salida(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str],
                          report: Report, strict: bool, code: int) -> None:
    monkeypatch.setattr(cli, "run_all", lambda ds: report)
    argv = ["--root", str(_repo(tmp_path))] + (["--strict"] if strict else [])
    assert cli.main(argv) == code
    out = capsys.readouterr().out
    assert f"{len(report.errors)} errores, {len(report.warnings)} avisos" in out
    if report.warnings:
        assert "AVISO  aviso" in out


def test_write_pending_es_determinista(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    items = [
        {"id": "zeta", "meshPath": "/Engine/BasicShapes/Cube.Cube", "tags": []},
        {"id": "alfa", "meshPath": "/Engine/BasicShapes/Sphere.Sphere", "tags": []},
        {"id": "interno", "meshPath": "/Engine/BasicShapes/Cube.Cube", "tags": ["interno"]},
        {"id": "hecho", "meshPath": "/Game/Generated/Meshes/Props/SM_Hecho.SM_Hecho", "tags": []},
    ]
    root = _repo(tmp_path, items_json=items, building_pieces_json={"tiers": [], "pieces": [{"id": "muro", "mesh": None}]})
    path = root / "Content" / "Data" / "meshes_pendientes.json"
    cli.main(["--root", str(root), "--write-pending", "--quiet"])
    first = path.read_bytes()
    cli.main(["--root", str(root), "--write-pending", "--quiet"])
    assert path.read_bytes() == first
    doc = json.loads(first)
    assert doc["items"] == ["alfa", "zeta"]
    assert doc["buildingPieces"] == ["muro"]
    out = capsys.readouterr().out
    assert "Escrito Content/Data/meshes_pendientes.json" in out
    # Recién escrito, no puede faltar ni sobrar nada en la lista.
    assert "meshes_pendientes.json/items" not in out


def test_write_cooking_regenera_los_inl(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    root = _repo(tmp_path, fuels_json={"levels": [], "fuels": [], "ignition": []})
    cli.main(["--root", str(root), "--write-cooking", "--quiet"])
    out = capsys.readouterr().out
    assert f"Escrito {cooking.FIRE_INL.as_posix()}" in out
    assert (root / cooking.FIRE_INL).exists()
    assert "FireData.inl no coincide" not in out


def test_datos_reales_pasan_en_modo_estricto(capsys: pytest.CaptureFixture[str]) -> None:
    assert cli.main(["--strict", "--quiet"]) == 0
    assert "0 errores, 0 avisos" in capsys.readouterr().out
