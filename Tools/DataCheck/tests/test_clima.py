"""storm_curve.json y el espejo del cuerpo (survival_needs.json ↔ IslandBodyModel.h)."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import pytest

from datacheck import clima
from datacheck.checks import DataSet, run_all


@pytest.fixture(scope="module")
def real() -> DataSet:
    return DataSet.load()


@pytest.fixture
def ds(real: DataSet) -> DataSet:
    return real.copy()


def storm(ds: DataSet) -> dict[str, Any]:
    return ds.data["storm_curve.json"]


def window(ds: DataSet, state: str) -> dict[str, Any]:
    return next(w for w in storm(ds)["windows"] if w["state"] == state)


def storm_errors(ds: DataSet) -> list[str]:
    errors: list[str] = []
    clima.check_storm(ds.data, ds.repo_root, errors.append)
    return errors


def test_curva_real_sin_errores(real: DataSet) -> None:
    assert storm_errors(real) == []


def test_detecta_puntos_desordenados_y_fuera_de_rango(ds: DataSet) -> None:
    points = storm(ds)["points"]
    points[1]["hour"] = 6.0
    points[2]["hpa"] = 2000
    errors = storm_errors(ds)
    assert any("orden creciente" in e for e in errors)
    assert any("points[2]" in e for e in errors)


def test_detecta_ventanas_solapadas_y_estado_desconocido(ds: DataSet) -> None:
    window(ds, "SeaClosed")["hours"] = 3.0
    storm(ds)["windows"].append({"state": "Tornado", "day": 3, "hour": 10, "hours": 1})
    errors = storm_errors(ds)
    assert any("se solapa" in e for e in errors)
    assert any("windows[6]" in e for e in errors)


def test_detecta_hitos_del_gdd_movidos(ds: DataSet) -> None:
    window(ds, "Squall")["hour"] = 14.0
    window(ds, "CycloneWall2")["hours"] = 1.0
    errors = storm_errors(ds)
    assert any("«Squall»" in e and "empezar" in e for e in errors)
    assert any("«CycloneWall2»" in e and "terminar" in e for e in errors)


def test_detecta_ojo_que_no_dura_65_s(ds: DataSet) -> None:
    window(ds, "CycloneEye")["hours"] = 1.75
    assert any("CycloneEye" in e and "s reales" in e for e in storm_errors(ds))


def test_detecta_ventana_que_falta(ds: DataSet) -> None:
    storm(ds)["windows"] = [w for w in storm(ds)["windows"] if w["state"] != "CycloneEye"]
    assert any("falta «CycloneEye»" in e for e in storm_errors(ds))


def test_detecta_dia_y_ojo_desfasados_del_cpp(ds: DataSet) -> None:
    storm(ds)["dayLengthMinutes"] = 20
    assert any("dayLengthMinutes=20" in e for e in storm_errors(ds))


def test_detecta_constante_cpp_desaparecida(ds: DataSet, tmp_path: Path) -> None:
    ds.repo_root = tmp_path
    errors = storm_errors(ds)
    assert any("no existe la constante DayLengthMinutes" in e for e in errors)


def test_detecta_lluvia_y_cierre_del_mar_rotos(ds: DataSet) -> None:
    storm(ds)["rain"]["cyclone"] = 1.5
    storm(ds)["forcedReturn"] = {"day": 2, "hour": 17.0}
    errors = storm_errors(ds)
    assert any("storm_curve.json rain" in e for e in errors)
    assert any("forcedReturn" in e for e in errors)


def test_detecta_reapertura_del_mar_antes_del_cierre(ds: DataSet) -> None:
    storm(ds)["raftReopen"] = {"day": 2, "hour": 18.5}
    errors = storm_errors(ds)
    assert any("raftReopen" in e for e in errors)


def test_reapertura_del_mar_valida(ds: DataSet) -> None:
    assert not any("raftReopen" in e for e in storm_errors(ds))


def test_listas_vacias(ds: DataSet) -> None:
    storm(ds)["points"] = []
    storm(ds)["windows"] = "no"
    errors = storm_errors(ds)
    assert any("points: debe ser" in e for e in errors)
    assert any("windows: debe ser" in e for e in errors)


def test_sin_fichero_no_rompe() -> None:
    errors: list[str] = []
    clima.check_storm({}, Path("."), errors.append)
    assert errors == []


def test_cuerpo_desfasado_del_cpp(ds: DataSet) -> None:
    calor = next(n for n in ds.data["survival_needs.json"]["needs"] if n["id"] == "calor")
    calor["fireGainPerMinute"] = 30
    ds.data["survival_needs.json"]["lowThreshold"] = 20
    errors = run_all(ds).errors
    assert any("fireGainPerMinute=30" in e for e in errors)
    assert any("lowThreshold=20" in e for e in errors)
