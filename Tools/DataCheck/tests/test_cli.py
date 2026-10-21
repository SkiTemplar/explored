"""CLI ``datacheck``: códigos de salida y opciones.

Casi todos usan ``--root`` con un repositorio temporal, así que no simulan el catálogo real.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from datacheck import cli
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
    assert "ERROR  items.json: JSON mal formado" in out


def test_quiet_oculta_las_notas(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]) -> None:
    monkeypatch.setattr(cli, "run_all", lambda ds: Report(info=["nota"]))
    root = _repo(tmp_path)
    cli.main(["--root", str(root)])
    assert "INFO   nota" in capsys.readouterr().out
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


def test_datos_reales_pasan_en_modo_estricto(capsys: pytest.CaptureFixture[str]) -> None:
    assert cli.main(["--strict", "--quiet"]) == 0
    assert "0 errores, 0 avisos" in capsys.readouterr().out
