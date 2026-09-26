"""Tests del validador: los datos reales pasan y cada regresión típica se detecta."""

from __future__ import annotations

import pytest

from datacheck import crafting
from datacheck.checks import DataSet, Report, check_building, check_crafting_reachability, run_all


@pytest.fixture(scope="module")
def real() -> DataSet:
    return DataSet.load()


@pytest.fixture(scope="module")
def real_report(real: DataSet) -> Report:
    return run_all(real)


@pytest.fixture
def ds(real: DataSet) -> DataSet:
    return real.copy()


def errors_of(ds: DataSet) -> list[str]:
    return run_all(ds).errors


def any_error(errors: list[str], *needles: str) -> bool:
    return any(all(n in e for n in needles) for e in errors)


def item(ds: DataSet, iid: str) -> dict:
    return next(i for i in ds.items if i["id"] == iid)


def template(ds: DataSet, tid: str) -> dict:
    return next(t for t in ds.templates if t["id"] == tid)


def piece(ds: DataSet, pid: str) -> dict:
    return next(p for p in ds.building["pieces"] if p["id"] == pid)


# --------------------------------------------------------------------------- datos reales


def test_datos_reales_sin_errores(real_report: Report) -> None:
    assert real_report.errors == []


def test_datos_reales_sin_avisos(real_report: Report) -> None:
    assert real_report.warnings == []


def test_toda_plantilla_alcanzable(real: DataSet) -> None:
    reach = crafting.simulate(real.items, real.templates)
    assert {t["id"] for t in real.templates} <= set(reach.reached_templates)


def test_limon_existe_y_cura_escorbuto(real: DataSet) -> None:
    limon = item(real, "limon")
    assert limon["nutritionVitamins"] == 5
    assert "citrico" in limon["tags"]


# --------------------------------------------------------------------------- réplica del C++


def inst(ds: DataSet, iid: str) -> crafting.Instance:
    return crafting.leaf(item(ds, iid))


def apply(ds: DataSet, a: crafting.Instance, b: crafting.Instance, verb: str) -> crafting.Instance | None:
    tpl = crafting.best_template(ds.templates, verb, a, b)
    if tpl is None:
        return None
    return crafting.combine(item(ds, tpl["resultDefinitionId"]), a, b)


def test_hacha_en_dos_pasos_como_craftingspec(real: DataSet) -> None:
    mango = apply(real, inst(real, "palo_recto"), inst(real, "liana"), "Atar")
    assert mango is not None
    hacha = apply(real, mango, inst(real, "lasca_pedernal"), "Atar")
    assert hacha is not None and hacha.definition == "hacha"
    assert hacha.prop("Filo") == 3  # hereda el filo de la lasca


def test_arco_sale_de_vara_y_cuerda(real: DataSet) -> None:
    arco = apply(real, inst(real, "vara_flexible"), inst(real, "cuerda"), "Atar")
    assert arco is not None and arco.definition == "arco"


def test_combinacion_simetrica(real: DataSet) -> None:
    a, b = inst(real, "canto_rodado"), inst(real, "pedernal")
    assert apply(real, a, b, "Golpear").definition == apply(real, b, a, "Golpear").definition


def test_coco_y_huevo_no_se_atan(real: DataSet) -> None:
    assert apply(real, inst(real, "coco_maduro"), inst(real, "huevo"), "Atar") is None


# --------------------------------------------------------------------------- regresiones


def test_detecta_arco_sombreado_por_generico(ds: DataSet) -> None:
    arco = template(ds, "arco")
    ds.templates.remove(arco)
    ds.templates.append(arco)  # detrás de atado_generico: empata y pierde siempre
    r = Report()
    check_crafting_reachability(ds, r)
    assert any_error(r.errors, "«arco»", "inalcanzable")


def test_detecta_resultado_inexistente(ds: DataSet) -> None:
    template(ds, "cuchillo")["resultDefinitionId"] = "cuchillo_de_plata"
    assert any_error(errors_of(ds), "cuchillo_de_plata")


def test_detecta_verbo_inexistente(ds: DataSet) -> None:
    template(ds, "cesta")["verbs"] = ["Coser"]
    assert any_error(errors_of(ds), "Coser")


def test_detecta_propiedad_fuera_de_rango(ds: DataSet) -> None:
    item(ds, "obsidiana")["properties"][0]["value"] = 7
    assert any_error(errors_of(ds), "obsidiana", "fuera de [0, 5]")


def test_detecta_propiedad_desconocida(ds: DataSet) -> None:
    item(ds, "liana")["properties"].append({"name": "Magico", "value": 2})
    assert any_error(errors_of(ds), "Magico")


def test_detecta_id_duplicado(ds: DataSet) -> None:
    ds.items.append(dict(item(ds, "liana")))
    assert any_error(errors_of(ds), "duplicado", "liana")


def test_detecta_ingrediente_de_construccion_inexistente(ds: DataSet) -> None:
    piece(ds, "pared_madera")["cost"].append({"item": "clavo_de_oro", "count": 2})
    assert any_error(errors_of(ds), "clavo_de_oro")


def test_detecta_herramienta_no_fabricable(ds: DataSet) -> None:
    piece(ds, "muro_piedra")["tools"] = ["atado_generico"]  # interno: nunca en bruto ni «herramienta»
    ds.templates[:] = [t for t in ds.templates if t["id"] != "atado_generico"]
    assert any_error(errors_of(ds), "muro_piedra", "atado_generico", "no obtenible")


def test_detecta_ciclo_de_piezas(ds: DataSet) -> None:
    piece(ds, "cimiento_piedra")["requiresPieces"] = ["chimenea"]
    r = Report()
    check_building(ds, r, obtainable=ds.item_ids)
    assert any_error(r.errors, "ciclo")


def test_detecta_tier_que_no_mejora(ds: DataSet) -> None:
    for p in ds.building["pieces"]:
        if p["tier"] == "piedra":
            p["integrity"] = 10
    assert any_error(errors_of(ds), "integridad media", "piedra")


def test_detecta_planta_sin_pieza(ds: DataSet) -> None:
    ds.data["plants.json"]["plants"][0]["requiresPiece"] = "invernadero"
    assert any_error(errors_of(ds), "invernadero")


def test_detecta_limonero_arrancable(ds: DataSet) -> None:
    ds.data["plants.json"]["plants"][0]["neverRemoved"] = False
    assert any_error(errors_of(ds), "neverRemoved")


def test_detecta_etapa_final_con_dias(ds: DataSet) -> None:
    ds.data["plants.json"]["plants"][1]["stages"][-1]["days"] = 3
    assert any_error(errors_of(ds), "days=0")


def test_detecta_malla_inventada(ds: DataSet) -> None:
    piece(ds, "pared_bambu")["mesh"] = "SM_Wall_Bamboo"
    assert any_error(errors_of(ds), "SM_Wall_Bamboo")


def test_detecta_pendiente_obsoleto(ds: DataSet) -> None:
    ds.data["meshes_pendientes.json"]["items"].append("no_existe")
    assert any_error(errors_of(ds), "no_existe", "quítalo")


def test_detecta_pendiente_que_falta(ds: DataSet) -> None:
    ds.data["meshes_pendientes.json"]["items"].remove("limon")
    assert any_error(errors_of(ds), "falta «limon»")


def test_detecta_desfase_con_survival_cpp(ds: DataSet) -> None:
    need = next(n for n in ds.data["survival_needs.json"]["needs"] if n["id"] == "sed")
    need["hoursToEmpty"] = 48
    assert any_error(errors_of(ds), "ThirstHours")


def test_detecta_valor_inicial_distinto_del_cpp(ds: DataSet) -> None:
    need = next(n for n in ds.data["survival_needs.json"]["needs"] if n["id"] == "animo")
    need["initial"] = 99
    assert any_error(errors_of(ds), "animo", "SurvivalModel.h")


def test_detecta_fauna_terrestre(ds: DataSet) -> None:
    item(ds, "grasa")["nameEs"] = "Grasa de jabalí"
    assert any_error(errors_of(ds), "jabalí")


def test_no_confunde_rescatado_con_rescate(ds: DataSet) -> None:
    assert not any_error(errors_of(ds), "«rescate»")


def test_detecta_petroglifos_incompletos(ds: DataSet) -> None:
    ds.data["story_es.json"]["petroglyph_themes"].pop()
    assert any_error(errors_of(ds), "petroglifo")
