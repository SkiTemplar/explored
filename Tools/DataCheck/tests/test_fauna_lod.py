"""Fauna salvaje: comportamiento por nivel de detalle, población por isla y guano (GDD v2 §3.7)."""

from __future__ import annotations

import pytest

from datacheck import fauna
from datacheck.checks import PROPERTIES, DataSet, Report


@pytest.fixture(scope="module")
def real() -> DataSet:
    return DataSet.load()


@pytest.fixture
def ds(real: DataSet) -> DataSet:
    return real.copy()


def fauna_errors(ds: DataSet) -> list[str]:
    r = Report()
    fauna.check_fauna(ds, r, PROPERTIES)
    return r.errors


def any_error(errors: list[str], *needles: str) -> bool:
    return any(all(n in e for n in needles) for e in errors)


def animal(ds: DataSet, sid: str) -> dict:
    return next(s for s in ds.data["fauna.json"]["species"] if s["id"] == sid)


def island(ds: DataSet, iid: str) -> dict:
    return next(i for i in ds.data["fauna.json"]["islands"] if i["island"] == iid)


def population(ds: DataSet, iid: str, sid: str) -> dict:
    return next(p for p in island(ds, iid)["population"] if p["species"] == sid)


def test_datos_reales_sin_errores(real: DataSet) -> None:
    assert fauna_errors(real) == []


def test_todas_las_especies_declaran_los_tres_niveles(real: DataSet) -> None:
    for s in real.data["fauna.json"]["species"]:
        assert set(s["lodBehavior"]) == {"Full", "Reduced", "Frozen"}, s["id"]


# --------------------------------------------------------------------------- nivel de detalle


def test_falta_un_nivel(ds: DataSet) -> None:
    del animal(ds, "cerdo_salvaje")["lodBehavior"]["Frozen"]
    assert any_error(fauna_errors(ds), "cerdo_salvaje", "lodBehavior")


def test_accion_que_el_nivel_no_permite(ds: DataSet) -> None:
    animal(ds, "cerdo_salvaje")["lodBehavior"]["Reduced"].append("huida_o_carga")
    assert any_error(fauna_errors(ds), "cerdo_salvaje", "Reduced", "huida_o_carga")


def test_congelado_no_hace_nada(ds: DataSet) -> None:
    animal(ds, "gaviota_posada")["lodBehavior"]["Frozen"] = ["rutina"]
    assert any_error(fauna_errors(ds), "gaviota_posada", "Frozen")


def test_la_rutina_sigue_en_reducido(ds: DataSet) -> None:
    animal(ds, "cabra_salvaje")["lodBehavior"]["Reduced"].remove("rutina")
    assert any_error(fauna_errors(ds), "cabra_salvaje", "rutina")


def test_quien_huye_necesita_huida_en_full(ds: DataSet) -> None:
    animal(ds, "cangrejo_cocotero_salvaje")["lodBehavior"]["Full"].remove("huida_o_carga")
    assert any_error(fauna_errors(ds), "cangrejo_cocotero_salvaje", "huida_o_carga")


def test_quien_camina_navega(ds: DataSet) -> None:
    animal(ds, "cerdo_salvaje")["lodBehavior"]["Reduced"].remove("navegacion_gruesa")
    assert any_error(fauna_errors(ds), "cerdo_salvaje", "camina", "navegacion_gruesa")


def test_quien_vuela_no_usa_la_navegacion_del_suelo(ds: DataSet) -> None:
    animal(ds, "fragata_colonia")["lodBehavior"]["Full"].append("navegacion_fina")
    assert any_error(fauna_errors(ds), "fragata_colonia", "vuela")


def test_locomocion_desconocida(ds: DataSet) -> None:
    animal(ds, "gaviota_posada")["locomotion"] = "nado"
    assert any_error(fauna_errors(ds), "gaviota_posada", "locomotion")


# --------------------------------------------------------------------------- población por isla


def test_isla_sin_bloque_de_poblacion(ds: DataSet) -> None:
    del island(ds, "smoke")["population"]
    assert any_error(fauna_errors(ds), "smoke", "population")


def test_especie_sin_poblacion(ds: DataSet) -> None:
    isl = island(ds, "teeth")
    isl["population"] = [p for p in isl["population"] if p["species"] != "gaviota_posada"]
    assert any_error(fauna_errors(ds), "teeth", "sin población", "gaviota_posada")


def test_poblacion_de_especie_que_no_vive_alli(ds: DataSet) -> None:
    island(ds, "landing")["population"].append(
        {"species": "cerdo_salvaje", "groups": 1, "groupSize": [1, 2], "respawnDays": 8})
    assert any_error(fauna_errors(ds), "landing", "cerdo_salvaje", "no está en species")


def test_tamano_de_grupo_invertido(ds: DataSet) -> None:
    population(ds, "emerald", "cerdo_salvaje")["groupSize"] = [4, 2]
    assert any_error(fauna_errors(ds), "emerald", "groupSize")


def test_reposicion_nula(ds: DataSet) -> None:
    population(ds, "emerald", "cerdo_salvaje")["respawnDays"] = 0
    assert any_error(fauna_errors(ds), "emerald", "respawnDays")


def test_tope_de_replicados_por_isla(ds: DataSet) -> None:
    population(ds, "emerald", "cerdo_salvaje")["groups"] = 10
    assert any_error(fauna_errors(ds), "emerald", "tope", "36")


def test_un_grupo_no_cabe_en_full(ds: DataSet) -> None:
    population(ds, "emerald", "cerdo_salvaje")["groupSize"] = [2, 13]
    assert any_error(fauna_errors(ds), "emerald", "13", "10 Hz")


def test_las_aves_no_cuentan_como_replicadas(real: DataSet) -> None:
    # 2 colonias de hasta 14 fragatas pasarían de 12 si se contaran como actores replicados.
    assert population(real, "teeth", "fragata_colonia")["groupSize"][1] > fauna.REPLICATED_FULL_CAP
    assert fauna_errors(real) == []


def test_tope_de_grupos_de_ambiente(ds: DataSet) -> None:
    population(ds, "teeth", "gaviota_posada")["groups"] = 30
    assert any_error(fauna_errors(ds), "teeth", "grupos de ambiente")


# --------------------------------------------------------------------------- guano de Los Dientes


def test_guano_del_deposito_de_la_colonia(real: DataSet) -> None:
    deposit = animal(real, "fragata_colonia")["deposit"]
    assert deposit["item"] == "guano" and deposit["tool"] == "pala"
    assert "guano" not in animal(real, "fragata_colonia")["lootPendiente"]
    inputs = real.data["plants.json"]["compost"]["inputs"]
    assert {"item": "guano"} in inputs


def test_deposito_con_objeto_inexistente(ds: DataSet) -> None:
    animal(ds, "fragata_colonia")["deposit"]["item"] = "estiercol"
    assert any_error(fauna_errors(ds), "fragata_colonia", "deposit", "estiercol")


def test_deposito_con_herramienta_inexistente(ds: DataSet) -> None:
    animal(ds, "fragata_colonia")["deposit"]["tool"] = "rastrillo"
    assert any_error(fauna_errors(ds), "fragata_colonia", "deposit", "rastrillo")


def test_deposito_sin_recogida_en_el_servidor(ds: DataSet) -> None:
    sp = animal(ds, "fragata_colonia")
    del sp["nest"], sp["groundPickup"]
    sp["red"]["recogidas"] = None
    assert any_error(fauna_errors(ds), "fragata_colonia", "recogidas")


def test_nivel_nulo_no_rompe_el_validador(ds: DataSet) -> None:
    animal(ds, "cerdo_salvaje")["lodBehavior"]["Reduced"] = None
    assert any_error(fauna_errors(ds), "cerdo_salvaje", "Reduced")


def test_entrada_de_poblacion_que_no_es_objeto(ds: DataSet) -> None:
    island(ds, "emerald")["population"].append("cerdo_salvaje")
    assert any_error(fauna_errors(ds), "emerald", "population")


def test_grupos_booleano(ds: DataSet) -> None:
    population(ds, "emerald", "cerdo_salvaje")["groups"] = True
    assert any_error(fauna_errors(ds), "emerald", "groups")
