"""Textos de la historia (historia.py): datos reales limpios y cada regla con su caso negativo."""

from __future__ import annotations

import copy
from typing import Any

import pytest

from datacheck import historia
from datacheck.checks import DataSet, run_all


@pytest.fixture(scope="module")
def real() -> DataSet:
    return DataSet.load()


def story_errors(data: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    historia.check_story(data, errors.append)
    return errors


def has(errors: list[str], *needles: str) -> bool:
    return any(all(n in e for n in needles) for e in errors)


@pytest.fixture
def data(real: DataSet) -> dict[str, Any]:
    return copy.deepcopy({k: real.data[k] for k in ("notebook.json", "objectives.json", "thoughts.json")})


def page(data: dict[str, Any], number: int) -> dict[str, Any]:
    return next(p for p in data["notebook.json"]["pages"] if p["page"] == number)


def test_los_tres_ficheros_se_cargan_y_estan_limpios(real: DataSet) -> None:
    for name in ("notebook.json", "objectives.json", "thoughts.json"):
        assert name in real.data
    assert story_errors(real.data) == []
    assert not [e for e in run_all(real).errors if "json" in e and ("notebook" in e or "objectives" in e or "thoughts" in e)]


def test_ocho_paginas_con_criticas_1_y_8(data: dict[str, Any]) -> None:
    assert sorted(p["page"] for p in data["notebook.json"]["pages"]) == list(range(1, 9))
    page(data, 3)["critical"] = True
    page(data, 8)["critical"] = False
    errs = story_errors(data)
    assert has(errs, "página 3", "críticas") and has(errs, "página 8", "críticas")


def test_detecta_pagina_que_falta_repetida_o_fuera_de_rango(data: dict[str, Any]) -> None:
    pages = data["notebook.json"]["pages"]
    pages[1]["page"] = 1
    pages[2]["page"] = 9
    errs = story_errors(data)
    assert has(errs, "una página por número")
    assert has(errs, "«page» debe ser 1-8")
    assert has(errs, "el id debe ser «cuaderno_p1»")


def test_detecta_textos_largos_y_sin_ingles(data: dict[str, Any]) -> None:
    page(data, 2)["textEs"] = "palabra " * 71
    page(data, 4)["iriaEs"] = "nota " * 21
    del page(data, 5)["iriaEn"]
    errs = story_errors(data)
    assert has(errs, "página 2", "70 palabras")
    assert has(errs, "página 4", "20 palabras")
    assert has(errs, "página 5", "falta «iriaEn»")


def test_detecta_epilogo_incompleto_y_cartel_desconocido(data: dict[str, Any]) -> None:
    doc = data["notebook.json"]
    doc["epilogue"] = [e for e in doc["epilogue"] if e["id"] != "epilogo_parcial"]
    doc["cards"].append({"id": "cartel_extra", "textEs": "Hola", "textEn": "Hi"})
    errs = story_errors(data)
    assert has(errs, "falta «epilogo_parcial»")
    assert has(errs, "«cartel_extra» no lo usa el juego")


def test_objetivos_del_modelo_con_su_limite(data: dict[str, Any]) -> None:
    objectives = data["objectives.json"]["objectives"]
    assert {o["id"] for o in objectives} == historia.OBJECTIVES
    objectives[0]["textEs"] = "x" * 61
    objectives[1]["stage"] = "Zarpar"
    objectives.append(copy.deepcopy(objectives[2]))
    data["objectives.json"]["objectives"] = [o for o in objectives if o["id"] != "obj_enciende_baliza"]
    errs = story_errors(data)
    assert has(errs, "textEs pasa de 60")
    assert has(errs, "etapa «Zarpar» desconocida")
    assert has(errs, "repetido")
    assert has(errs, "falta «obj_enciende_baliza»")


def test_pensamientos_con_su_disparador(data: dict[str, Any]) -> None:
    thoughts = data["thoughts.json"]["thoughts"]
    assert len(thoughts) == 12
    thoughts[0]["trigger"] = "SeeLighthouse"
    thoughts[1]["textEs"] = "uno " * 21
    data["thoughts.json"]["subtitleSeconds"] = 0
    errs = story_errors(data)
    assert has(errs, "no corresponde a este id")
    assert has(errs, "pasa de 20 palabras")
    assert has(errs, "subtitleSeconds")


def test_raices_de_tipo_equivocado_no_rompen() -> None:
    errs = story_errors({"notebook.json": [], "objectives.json": "x", "thoughts.json": None})
    assert has(errs, "una página por número")
    assert has(errs, "falta «obj_bebe_coco»")
    assert has(errs, "subtitleSeconds")
