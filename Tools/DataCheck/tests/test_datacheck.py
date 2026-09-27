"""Tests del validador: los datos reales pasan y cada regresión típica se detecta."""

from __future__ import annotations

import pytest

from datacheck import crafting
from datacheck.checks import (
    DataSet,
    Report,
    check_artifacts,
    check_building,
    check_crafting_reachability,
    check_ruins,
    run_all,
)


from datacheck import cooking, crafting
from datacheck.checks import (
    DataSet, Report, check_building, check_cooking, check_crafting_reachability, check_gdd_food_coverage, run_all,
)


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


def test_angarillas_en_dos_pasos(real: DataSet) -> None:
    asta = apply(real, inst(real, "bambu_grueso"), inst(real, "liana"), "Atar")
    assert asta is not None and asta.definition == "atado_generico"
    angarillas = apply(real, asta, inst(real, "hoja_palma"), "Atar")
    assert angarillas is not None and angarillas.definition == "angarillas"


def test_angarillas_no_sombrea_lanza_ni_hacha(real: DataSet) -> None:
    # Van antes que hacha y lanza en templates.json: las cadenas de CraftingSpec
    # no llevan nada Fibroso >= 2, así que deben seguir dando su herramienta.
    asta = apply(real, inst(real, "bambu_grueso"), inst(real, "liana"), "Atar")
    assert apply(real, asta, inst(real, "hueso_largo"), "Atar").definition == "lanza"
    mango = apply(real, inst(real, "tronco_pequeno"), inst(real, "liana"), "Atar")
    assert apply(real, mango, inst(real, "lasca_pedernal"), "Atar").definition == "hacha"


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


def test_detecta_encaje_desconocido(ds: DataSet) -> None:
    piece(ds, "techo_bambu")["socket"] = "tejado"
    assert any_error(errors_of(ds), "techo_bambu", "socket")


def test_detecta_sin_punto_de_reaparicion(ds: DataSet) -> None:
    piece(ds, "fogata").pop("respawnPoint")
    assert any_error(errors_of(ds), "reaparición")


def test_detecta_tier_que_no_mejora(ds: DataSet) -> None:
    for p in ds.building["pieces"]:
        if p["tier"] == "piedra":
            p["integrity"] = 10
    assert any_error(errors_of(ds), "integridad media", "piedra")


def test_detecta_planta_sin_pieza(ds: DataSet) -> None:
    ds.data["plants.json"]["plants"][0]["requiresPiece"] = "invernadero"
    assert any_error(errors_of(ds), "invernadero")


def test_detecta_aves_sin_espantapajaros(ds: DataSet) -> None:
    ds.building["pieces"] = [p for p in ds.building["pieces"] if p["id"] != "espantapajaros"]
    assert any_error(errors_of(ds), "birdsEat", "espantapajaros")


def test_detecta_birdseat_no_booleano(ds: DataSet) -> None:
    ds.data["plants.json"]["plants"][1]["birdsEat"] = "si"
    assert any_error(errors_of(ds), "birdsEat debe ser")


def test_espantapajaros_en_el_primer_tier(real: DataSet) -> None:
    # Las aves picotean desde el primer bancal: el remedio no puede esperar a otro tier.
    p = next(p for p in real.building["pieces"] if p["id"] == "espantapajaros")
    assert p["tier"] == "palma" and p["category"] == "huerto" and p["tools"] == []


def test_detecta_limonero_arrancable(ds: DataSet) -> None:
    ds.data["plants.json"]["plants"][0]["neverRemoved"] = False
    assert any_error(errors_of(ds), "neverRemoved")


def test_detecta_etapa_final_con_dias(ds: DataSet) -> None:
    ds.data["plants.json"]["plants"][1]["stages"][-1]["days"] = 3
    assert any_error(errors_of(ds), "days=0")


def test_detecta_malla_inventada(ds: DataSet) -> None:
    piece(ds, "pared_bambu")["mesh"] = "SM_Wall_Bamboo"
    assert any_error(errors_of(ds), "SM_Wall_Bamboo")


def test_reconoce_mallas_del_kit_modular(real: DataSet) -> None:
    from datacheck.checks import blender_mesh_names
    names = blender_mesh_names(real.repo_root)
    assert {"SM_Kit_Palm_Wall", "SM_Kit_Stone_Foundation", "SM_Kit_Bamboo_GableShed"} <= names
    assert "SM_Kit_Palm_Nada" not in names


def test_detecta_pendiente_obsoleto(ds: DataSet) -> None:
    ds.data["meshes_pendientes.json"]["items"].append("no_existe")
    assert any_error(errors_of(ds), "no_existe", "quítalo")


def test_detecta_pendiente_que_falta(ds: DataSet) -> None:
    # cualquier entrada que siga pendiente, del grupo que sea (las listas se
    # vacían lote a lote: la de items ya quedó a cero)
    pending = ds.data["meshes_pendientes.json"]
    group = next(g for g, v in pending.items() if isinstance(v, list) and v)
    victim = pending[group][0]
    pending[group].remove(victim)
    assert any_error(errors_of(ds), f"falta «{victim}»")


def test_detecta_desfase_con_survival_cpp(ds: DataSet) -> None:
    need = next(n for n in ds.data["survival_needs.json"]["needs"] if n["id"] == "sed")
    need["hoursToEmpty"] = 48
    assert any_error(errors_of(ds), "ThirstHours")


def test_detecta_valor_inicial_distinto_del_cpp(ds: DataSet) -> None:
    need = next(n for n in ds.data["survival_needs.json"]["needs"] if n["id"] == "animo")
    need["initial"] = 99
    assert any_error(errors_of(ds), "animo", "SurvivalModel.h")


def test_detecta_desfase_con_body_cpp(ds: DataSet) -> None:
    ds.data["survival_needs.json"]["body"]["wounds"]["infectionHours"]["value"] = 99
    assert any_error(errors_of(ds), "WoundInfectionHours")


def test_detecta_evento_de_animo_distinto_del_cpp(ds: DataSet) -> None:
    ds.data["survival_needs.json"]["body"]["moraleEvents"]["StormHit"] = 4
    assert any_error(errors_of(ds), "StormHit")


def test_detecta_fauna_terrestre(ds: DataSet) -> None:
    item(ds, "grasa")["nameEs"] = "Grasa de jabalí"
    assert any_error(errors_of(ds), "jabalí")


def test_no_confunde_rescatado_con_rescate(ds: DataSet) -> None:
    assert not any_error(errors_of(ds), "«rescate»")


def test_detecta_petroglifos_incompletos(ds: DataSet) -> None:
    ds.data["story_es.json"]["petroglyph_themes"].pop()
    assert any_error(errors_of(ds), "petroglifo")


# --------------------------------------------------------------------------- logros (GDD §16)


def achievement(ds: DataSet, aid: str) -> dict:
    return next(a for a in ds.data["achievements.json"]["achievements"] if a["id"] == aid)


def test_logros_reales_son_treinta_con_los_del_gdd(real: DataSet) -> None:
    ids = {a["id"] for a in real.data["achievements.json"]["achievements"]}
    assert len(ids) == 30
    assert {"primer_fuego", "tierra_firme", "sin_mapa", "naufrago_de_verdad", "limon_zarpa"} <= ids


def test_detecta_numero_de_logros(ds: DataSet) -> None:
    ds.data["achievements.json"]["achievements"].pop()
    assert any_error(errors_of(ds), "29 logros")


def test_detecta_logro_duplicado(ds: DataSet) -> None:
    achievement(ds, "wayfinder")["id"] = "cartografo"
    assert any_error(errors_of(ds), "id duplicado", "cartografo")


def test_detecta_id_con_tilde(ds: DataSet) -> None:
    achievement(ds, "cartografo")["id"] = "cartógrafo"
    assert any_error(errors_of(ds), "id inválido")


def test_detecta_falta_de_ingles(ds: DataSet) -> None:
    del achievement(ds, "primer_fuego")["descriptionEn"]
    assert any_error(errors_of(ds), "primer_fuego", "descriptionEn")


def test_detecta_estadistica_desconocida(ds: DataSet) -> None:
    achievement(ds, "primer_fuego")["condition"] = {"stat": "hogueras", "op": ">=", "value": 1}
    assert any_error(errors_of(ds), "estadística desconocida", "hogueras")


def test_detecta_tipo_incompatible(ds: DataSet) -> None:
    achievement(ds, "primer_fuego")["condition"] = {"stat": "fires_lit", "contains": "fogata"}
    assert any_error(errors_of(ds), "no es un conjunto")


def test_detecta_id_no_admitido_en_conjunto(ds: DataSet) -> None:
    achievement(ds, "el_limonero")["condition"] = {"stat": "crops_harvested", "contains": "naranjo"}
    assert any_error(errors_of(ds), "naranjo", "no es un id admitido")


def test_detecta_meta_inalcanzable(ds: DataSet) -> None:
    achievement(ds, "wayfinder")["condition"]["value"] = 6
    assert any_error(errors_of(ds), "wayfinder", "solo admite 5")


def test_detecta_sin_mapa_mal_escrito(ds: DataSet) -> None:
    achievement(ds, "sin_mapa")["condition"] = {"flag": "hidden_island_reached"}
    assert any_error(errors_of(ds), "sin_mapa")


def test_detecta_nombre_de_la_dedicatoria(ds: DataSet) -> None:
    achievement(ds, "el_limonero")["nameEs"] = "Para Almudena"
    assert any_error(errors_of(ds), "dedicatoria")


def test_detecta_estadistica_sin_documentar(ds: DataSet) -> None:
    ds.data["achievements.json"]["stats"].append(
        {"id": "shells_found", "kind": "counter", "scope": "profile", "descriptionEs": "Conchas."})
    assert any_error(errors_of(ds), "estadisticas.md", "shells_found")


def test_detecta_ambito_distinto_del_documentado(ds: DataSet) -> None:
    next(s for s in ds.data["achievements.json"]["stats"] if s["id"] == "fires_lit")["scope"] = "run"
    assert any_error(errors_of(ds), "estadisticas.md", "fires_lit")


# --------------------------------------------------------------------------- ruinas y museo


def ruins_errors(ds: DataSet) -> list[str]:
    r = Report()
    check_ruins(ds, r)
    return r.errors


def artifacts_errors(ds: DataSet) -> list[str]:
    r = Report()
    check_artifacts(ds, r)
    return r.errors


def artifact(ds: DataSet, aid: str) -> dict:
    return next(a for a in ds.data["artifacts.json"]["artifacts"] if a["id"] == aid)


def test_ruinas_y_tesoros_reales_sin_errores(real: DataSet) -> None:
    assert ruins_errors(real) == []
    assert artifacts_errors(real) == []


def test_detecta_tecnica_que_no_esta_en_el_cpp(ds: DataSet) -> None:
    ds.data["ruins.json"]["techniques"][0]["id"] = "star_trail"
    assert any_error(ruins_errors(ds), "RuinsModel.cpp")


def test_detecta_constante_de_caminos_distinta_del_cpp(ds: DataSet) -> None:
    ds.data["ruins.json"]["requiredStarPaths"] = 5
    assert any_error(ruins_errors(ds), "RequiredStarPaths")


def test_detecta_ruina_de_isla_que_falta(ds: DataSet) -> None:
    ds.data["ruins.json"]["sites"].pop(0)
    assert any_error(ruins_errors(ds), "islas del C++")


def test_detecta_tipo_de_tesoro_desconocido(ds: DataSet) -> None:
    artifact(ds, "anzuelo_hueso")["kind"] = "golden_idol"
    assert any_error(artifacts_errors(ds), "golden_idol")


def test_detecta_tesoro_con_malla_inventada(ds: DataSet) -> None:
    artifact(ds, "tapa_pintada")["mesh"] = "SM_Treasure_Crown"
    assert any_error(artifacts_errors(ds), "SM_Treasure_Crown")


def test_detecta_tesoro_que_no_cabe_en_ningun_mueble(ds: DataSet) -> None:
    for d in ds.data["artifacts.json"]["displays"]:
        for slot in d["slots"]:
            slot["maxSize"] = "Mediano"
    assert any_error(artifacts_errors(ds), "remo_ceremonial", "sin ningún hueco")


def test_detecta_tesoro_grande_sin_mueble_construible(ds: DataSet) -> None:
    # regresión: el panel de pared existía en artifacts.json sin pieza en
    # building_pieces.json, así que los tesoros grandes no se podían exponer
    for d in ds.data["artifacts.json"]["displays"]:
        if d["id"] == "panel_museo":
            d["piece"] = None
    assert any_error(artifacts_errors(ds), "remo_ceremonial", "sin ningún hueco construible")


def test_muebles_del_museo_son_piezas_construibles(ds: DataSet) -> None:
    pieces = {p["id"]: p for p in ds.building["pieces"]}
    for d in ds.data["artifacts.json"]["displays"]:
        assert d["piece"] in pieces, d["id"]
        assert pieces[d["piece"]]["category"] == "museo"
        assert pieces[d["piece"]]["mesh"] == d["mesh"], d["id"]


def test_detecta_mueble_con_pieza_inexistente(ds: DataSet) -> None:
    ds.data["artifacts.json"]["displays"][1]["piece"] = "vitrina_de_oro"
    assert any_error(artifacts_errors(ds), "vitrina_de_oro")


def test_detecta_id_de_tesoro_con_tilde(ds: DataSet) -> None:
    artifact(ds, "pectoral_nacar")["id"] = "pectoral_nácar"
    assert any_error(artifacts_errors(ds), "id inválido")


def test_detecta_pocos_tesoros_para_coleccionista(ds: DataSet) -> None:
    ds.data["artifacts.json"]["artifacts"] = ds.data["artifacts.json"]["artifacts"][:9]
    assert any_error(artifacts_errors(ds), "Coleccionista")


# --------------------------------------------------------------------------- fuego y cocina


def cooking_errors(ds: DataSet) -> list[str]:
    r = Report()
    check_cooking(ds, r)
    return r.errors


def recipe(ds: DataSet, rid: str) -> dict:
    return next(r for r in ds.data["recipes.json"]["recipes"] if r["id"] == rid)


def test_tablas_cpp_de_cocina_al_dia(real: DataSet) -> None:
    for rel, text in cooking.generated_files(real.data).items():
        assert (real.repo_root / rel).read_text(encoding="utf-8") == text, rel


def test_detecta_inl_desactualizado(ds: DataSet) -> None:
    ds.data["fuels.json"]["fuels"][0]["burnHours"] = 0.07
    assert any_error(cooking_errors(ds), "FireData.inl", "--write-cooking")


def test_detecta_combustible_que_no_arde(ds: DataSet) -> None:
    ds.data["fuels.json"]["fuels"].append({"item": "canto_rodado", "burnHours": 1, "heat": 0.5, "tinder": False, "green": False})
    assert any_error(cooking_errors(ds), "canto_rodado", "no es madera")


def test_detecta_nivel_de_fuego_que_no_mejora(ds: DataSet) -> None:
    ds.data["fuels.json"]["levels"][1]["heat"] = 0.5
    assert any_error(cooking_errors(ds), "heat", "hoguera")


def test_detecta_hervir_en_recipiente_no_estanco(ds: DataSet) -> None:
    recipe(ds, "agua_hervida")["vessels"].append("espeto")
    assert any_error(cooking_errors(ds), "agua_hervida", "no es estanco")


def test_detecta_tecnica_desconocida(ds: DataSet) -> None:
    recipe(ds, "pescado_asado")["technique"] = "freir"
    assert any_error(cooking_errors(ds), "freir")


def test_detecta_comida_sin_conservacion(ds: DataSet) -> None:
    ds.data["recipes.json"]["foods"] = [f for f in ds.data["recipes.json"]["foods"] if f["item"] != "platano"]
    assert any_error(cooking_errors(ds), "platano")


def test_detecta_yuca_sin_toxicidad(ds: DataSet) -> None:
    next(f for f in ds.data["recipes.json"]["foods"] if f["item"] == "yuca")["toxicity"] = 0
    assert any_error(cooking_errors(ds), "yuca", "Toxico")


def test_detecta_orden_de_conservacion_roto(ds: DataSet) -> None:
    ds.data["recipes.json"]["preservation"]["stateHours"]["ahumado"] = 48
    assert any_error(cooking_errors(ds), "crudo < cocinado")


def test_detecta_hornear_fuera_del_horno(ds: DataSet) -> None:
    recipe(ds, "vasija_barro")["minFireLevel"] = "hoguera"
    assert any_error(cooking_errors(ds), "horno")


# --------------------------------------------------------------------------- embarcaciones


def boat(ds: DataSet, bid: str) -> dict:
    return next(b for b in ds.boats if b["id"] == bid)


def test_limon_exige_piezas_del_albatros(real: DataSet) -> None:
    limon = boat(real, "barco_limon")
    assert set(limon["requiresShipParts"]) == {"Fuselage", "Wing", "Tail", "Engine"}
    assert {"chapa_fuselaje", "tubo_aluminio"} <= {c["item"] for c in limon["cost"]}


def test_detecta_ingrediente_de_barco_inexistente(ds: DataSet) -> None:
    boat(ds, "canoa")["cost"].append({"item": "tronco_de_teca", "count": 1})
    assert any_error(errors_of(ds), "canoa", "tronco_de_teca")


def test_detecta_limon_sin_piezas_del_albatros(ds: DataSet) -> None:
    boat(ds, "barco_limon")["requiresShipParts"] = ["Fuselage"]
    assert any_error(errors_of(ds), "cuatro piezas del Albatros")


def test_detecta_pieza_del_albatros_inventada(ds: DataSet) -> None:
    boat(ds, "barco_limon")["requiresShipParts"].append("Helice")
    assert any_error(errors_of(ds), "Helice")


def test_detecta_progresion_al_reves(ds: DataSet) -> None:
    boat(ds, "canoa")["requiresBoat"] = "canoa_balancin"
    assert any_error(errors_of(ds), "no va antes en la progresión")


def test_detecta_malla_distinta_del_cpp(ds: DataSet) -> None:
    boat(ds, "balsa")["mesh"] = "SM_Canoe"
    assert any_error(errors_of(ds), "balsa", "FBoatDefinition::MeshName")


def test_detecta_tipo_de_barco_desconocido(ds: DataSet) -> None:
    boat(ds, "canoa")["type"] = "Catamaran"
    assert any_error(errors_of(ds), "EBoatType")


def test_detecta_astillero_inexistente(ds: DataSet) -> None:
    boat(ds, "balsa")["station"] = "dique_seco"
    assert any_error(errors_of(ds), "dique_seco")


# --------------------------------------------------------------------------- pesca


def fish(ds: DataSet) -> dict:
    return ds.data["fish.json"]


def test_pesca_captura_sin_objeto(ds: DataSet) -> None:
    ds.items.remove(item(ds, "pargo"))
    assert any_error(errors_of(ds), "fish.json «pargo»", "items.json")


def test_pesca_recompensa_legendaria_inexistente(ds: DataSet) -> None:
    fish(ds)["legendary"][0]["rewards"] = ["trofeo_de_oro"]
    assert any_error(errors_of(ds), "trofeo_de_oro")


def test_pesca_desincronizada_del_cpp(ds: DataSet) -> None:
    next(s for s in fish(ds)["species"] if s["id"] == "atun")["strengthKgf"] = 99.0
    assert any_error(errors_of(ds), "fish.json «atun»", "FishingModel.cpp")


def test_pesca_trampa_desincronizada_del_cpp(ds: DataSet) -> None:
    fish(ds)["traps"]["nasa"]["catches"][0]["perHour"] = 0.5
    assert any_error(errors_of(ds), "trampa «nasa»", "FishingModel.cpp")


def test_pesca_once_peces(ds: DataSet) -> None:
    fish(ds)["species"] = [s for s in fish(ds)["species"] if s["id"] != "dorado"]
    assert any_error(errors_of(ds), "11")


def test_detecta_conjunto_desfasado_con_su_catalogo(ds: DataSet) -> None:
    stats = ds.data["achievements.json"]["stats"]
    boats = next(s for s in stats if s["id"] == "boats_built")
    boats["values"] = ["balsa", "canoa", "canoa_balancin", "limon"]
    assert any_error(errors_of(ds), "boats_built", "boats.json")


def test_detecta_tecnica_con_id_distinto_de_ruins(ds: DataSet) -> None:
    stats = ds.data["achievements.json"]["stats"]
    techniques = next(s for s in stats if s["id"] == "wayfinding_techniques")
    techniques["values"] = ["camino_estrellas", "lectura_oleaje", "aves_atardecer", "nubes_fijas", "color_agua"]
    assert any_error(errors_of(ds), "wayfinding_techniques", "ruins.json")


def _food_notes(ds: DataSet) -> list[str]:
    r = Report()
    check_gdd_food_coverage(ds, r)
    return r.info


def test_cobertura_gdd_comida_es_nota_no_error(real_report: Report) -> None:
    assert not any("GDD §8.8" in e for e in real_report.errors + real_report.warnings)


def test_detecta_comida_del_gdd_que_desaparece(ds: DataSet) -> None:
    ds.data["items.json"] = [i for i in ds.items if i["id"] != "taro"]
    assert any("taro" in n for n in _food_notes(ds))


def test_cuenta_setas_por_tipo(ds: DataSet) -> None:
    notes = _food_notes(ds)
    assert any("comestible 1/2" in n and "toxica 0/2" in n and "alucinogena 0/1" in n for n in notes)
    ds.data["items.json"] = ds.items + [
        {"id": f"seta_{k}{n}", "tags": ["comida", "seta"] + (["alucinogena"] if k == "a" else []),
         "properties": [{"name": "Toxico", "value": 3}] if k == "t" else []}
        for k, n in (("c", 1), ("t", 1), ("t", 2), ("a", 1))
    ]
    assert not any("setas" in n for n in _food_notes(ds))


# --------------------------------------------------------------------------- música (GDD §14.3)

from datacheck import music


def music_errors(ds: DataSet) -> list[str]:
    r = Report()
    music.check_music(ds, r)
    return r.errors


def music_piece(ds: DataSet, pid: str) -> dict:
    return next(p for p in ds.data["music_layers.json"]["pieces"] if p["id"] == pid)


def test_musica_real_sin_errores_y_lee_el_cpp(real: DataSet) -> None:
    r = Report()
    music.check_music(real, r)
    assert r.errors == [] and r.warnings == []
    cpp = (real.repo_root / music.MODEL_CPP).read_text(encoding="utf-8")
    assert "explore" in music._cpp_keys(cpp, "RoleKeys")
    assert len(music._cpp_keys(cpp, "IslandKeys")) == 7
    keys, default = music._cpp_finale_keys(cpp)
    assert default == "voyage" and "voyage" in keys
    assert "Discovery" in music._cpp_required_roles(cpp)


def test_musica_finales_fuera_del_gdd_son_nota(real_report: Report) -> None:
    assert any("finales de música fuera del GDD" in i for i in real_report.info)


def test_musica_papel_desconocido(ds: DataSet) -> None:
    music_piece(ds, "mus_night")["role"] = "noche"
    errs = music_errors(ds)
    assert any_error(errs, "mus_night", "papel «noche» desconocido")
    assert any_error(errs, "falta una pieza con papel «night»")


def test_musica_isla_de_exploracion_mal_escrita(ds: DataSet) -> None:
    music_piece(ds, "mus_explore_mesa")["variant"] = "meseta"
    assert any_error(music_errors(ds), "una vez cada isla")


def test_musica_variacion_diurna_que_no_es_exploracion(ds: DataSet) -> None:
    ds.data["music_layers.json"]["day_variants"]["mesa"].append("mus_night")
    assert any_error(music_errors(ds), "«mesa»", "no es de exploración")


def test_musica_variacion_diurna_sin_su_pieza_primero(ds: DataSet) -> None:
    lst = ds.data["music_layers.json"]["day_variants"]["smoke"]
    lst[0], lst[1] = lst[1], lst[0]
    assert any_error(music_errors(ds), "«smoke»", "propia pieza")


def test_musica_final_que_nunca_sonaria(ds: DataSet) -> None:
    music_piece(ds, "mus_finale_voyage")["variant"] = "zarpar"
    errs = music_errors(ds)
    assert any_error(errs, "final «zarpar» desconocido")
    assert any_error(errs, "final por defecto «voyage»")


def test_musica_bucle_con_compas_a_medias(ds: DataSet) -> None:
    p = music_piece(ds, "mus_sea")
    p["bars"] = 15.5
    p["duration_s"] = p["seconds_per_bar"] * 15.5
    assert any_error(music_errors(ds), "mus_sea", "compases enteros")


def test_musica_tempo_desfasado(ds: DataSet) -> None:
    music_piece(ds, "mus_storm")["bpm"] = 96
    assert any_error(music_errors(ds), "mus_storm", "seconds_per_bar")


def test_musica_descubrimiento_largo(ds: DataSet) -> None:
    p = music_piece(ds, "mus_discovery_01")
    p["bars"] = 4
    p["duration_s"] = p["seconds_per_bar"] * 4
    assert any_error(music_errors(ds), "mus_discovery_01", "motivo corto")


def test_musica_flauta_con_notas_de_mas(ds: DataSet) -> None:
    ds.data["music_layers.json"]["flute"]["semitones"] = [0, 2, 4, 5, 7, 9]
    assert any_error(music_errors(ds), "NumNotes")


# --------------------------------------------------------------------------- mobiliario de base sin pieza

def _idle_base_note(report: Report) -> str:
    return next((n for n in report.info if "mallas de base sin pieza" in n), "")


def test_catre_y_muelle_usan_mallas_existentes(real: DataSet) -> None:
    meshes = {p["id"]: p["mesh"] for p in real.building["pieces"]}
    assert meshes["catre_bambu"] == "SM_Base_Bed"
    assert meshes["muelle"] == "SM_Base_Dock"
    assert meshes["muelle_final"] == "SM_Base_DockEnd"
    assert "muelle" in piece(real, "muelle_final")["requiresPieces"]


def test_malla_de_base_sin_pieza_es_nota_no_error(real_report: Report) -> None:
    note = _idle_base_note(real_report)
    assert "SM_Base_Bed" not in note and "SM_Base_Dock," not in note
    assert not any("mallas de base sin pieza" in e for e in real_report.errors + real_report.warnings)


def test_detecta_malla_de_base_que_queda_sin_pieza(ds: DataSet) -> None:
    ds.data["building_pieces.json"]["pieces"] = [p for p in ds.building["pieces"] if p["id"] != "catre_bambu"]
    assert "SM_Base_Bed" in _idle_base_note(run_all(ds))
