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


def test_compost_en_el_primer_tier_y_con_restos_de_landing(real: DataSet) -> None:
    # El compost acompaña al primer bancal: pila del tier palma y restos que ya se recogen en Landing.
    c = real.data["plants.json"]["compost"]
    pile = next(p for p in real.building["pieces"] if p["id"] == c["piece"])
    assert pile["tier"] == "palma" and pile["tools"] == []
    assert {"item": "hoja_palma"} in c["inputs"] and {"tag": "comida"} in c["inputs"]


def test_detecta_compost_sin_bloque(ds: DataSet) -> None:
    del ds.data["plants.json"]["compost"]
    assert any_error(errors_of(ds), "falta el bloque «compost»")


def test_detecta_compost_distinto_del_cpp(ds: DataSet) -> None:
    ds.data["plants.json"]["compost"]["growthMultiplier"] = 2.0
    ds.data["plants.json"]["compost"]["durationDays"] = 5
    errors = errors_of(ds)
    assert any_error(errors, "CompostGrowth") and any_error(errors, "CompostDays")


def test_detecta_compost_como_resto_de_si_mismo(ds: DataSet) -> None:
    ds.data["plants.json"]["compost"]["inputs"].append({"item": "compost"})
    assert any_error(errors_of(ds), "su propio resto")


def test_detecta_compost_con_capacidad_no_multiplo(ds: DataSet) -> None:
    ds.data["plants.json"]["compost"]["capacity"] = 10
    assert any_error(errors_of(ds), "múltiplo de inputsPerResult")


def test_detecta_pila_de_compost_tardia(ds: DataSet) -> None:
    pile = next(p for p in ds.building["pieces"] if p["id"] == "pila_compost")
    pile["tier"] = "madera"
    assert any_error(errors_of(ds), "tier posterior al bancal")


def test_detecta_compost_con_etiqueta_inexistente(ds: DataSet) -> None:
    ds.data["plants.json"]["compost"]["inputs"] = [{"tag": "estiercol"}]
    errors = errors_of(ds)
    assert any_error(errors, "estiercol") and any_error(errors, "ningún resto aceptado")


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


def test_detecta_fauna_fuera_del_gdd_v2(ds: DataSet) -> None:
    # El GDD v2 §3.7 recupera cerdo y cabra, pero no reptiles ni roedores.
    item(ds, "grasa")["nameEs"] = "Grasa de iguana"
    assert any_error(errors_of(ds), "iguana")


def test_admite_la_fauna_del_gdd_v2(ds: DataSet) -> None:
    # GDD v2 §3.6-§3.7: cerdo salvaje y cabra vuelven con packs de Quaternius.
    item(ds, "grasa")["nameEs"] = "Grasa de jabalí"
    assert not any_error(errors_of(ds), "jabalí")


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


def test_estratos_de_mineria_del_gdd_v2(real_report: Report) -> None:
    assert not any("GDD v2 §3.4" in e or "mining.json" in e for e in real_report.errors)
    assert not any("pico" in n and "GDD v2 §3.4" in n for n in real_report.info)


def test_detecta_estrato_de_mineria_que_desaparece(ds: DataSet) -> None:
    ds.data["items.json"] = [i for i in ds.items if i["id"] != "hierro_meteorito"]
    assert any_error(errors_of(ds), "hierro de meteorito", "GDD v2")


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


# --------------------------------------------------------------------------- minería (GDD v2 §3.4)

from datacheck import mining


def mining_errors(ds: DataSet) -> list[str]:
    r = Report()
    mining.check_mining(ds, r)
    return r.errors


def stratum(ds: DataSet, sid: str) -> dict:
    return next(s for s in ds.data["mining.json"]["strata"] if s["id"] == sid)


def material(ds: DataSet, mid: str) -> dict:
    return next(m for m in ds.data["mining.json"]["materials"] if m["id"] == mid)


def without_tool(ds: DataSet, tid: str) -> None:
    ds.data["mining.json"]["tools"] = [t for t in ds.data["mining.json"]["tools"] if t["id"] != tid]


def test_mineria_real_sin_errores_y_lee_el_cpp(real: DataSet) -> None:
    assert mining_errors(real) == []
    cpp = mining.cpp_materials(real)
    assert cpp and cpp["Basalto"] == (3.0, 3)
    assert {"landing", "emerald", "smoke", "teeth"} <= mining.cpp_islands(real)


def test_mineria_dureza_distinta_del_cpp(ds: DataSet) -> None:
    m = material(ds, "basalto")
    m["hardness"], m["hitsPerM3"] = 2.5, {"3": 15, "4": 10}
    assert any_error(mining_errors(ds), "basalto", "MaterialInfo")


def test_mineria_golpes_fuera_de_la_formula(ds: DataSet) -> None:
    material(ds, "caliza")["hitsPerM3"]["3"] = 10
    assert any_error(mining_errors(ds), "caliza", "fórmula")


def test_mineria_isla_que_no_existe(ds: DataSet) -> None:
    stratum(ds, "basalto")["occurrences"][0]["island"] = "atlantida"
    assert any_error(mining_errors(ds), "atlantida", "EIslandArchetype")


def test_mineria_falta_estrato_del_gdd(ds: DataSet) -> None:
    ds.data["mining.json"]["strata"] = [s for s in ds.data["mining.json"]["strata"] if s["id"] != "azufre"]
    assert any_error(mining_errors(ds), "azufre", "GDD v2")


def test_mineria_cabeza_de_pico_sin_nivel(ds: DataSet) -> None:
    ds.data["items.json"] = ds.items + [{"id": "granito", "tags": ["piedra"], "properties": [{"name": "Rigido", "value": 4}, {"name": "Punta", "value": 3}]}]
    assert any_error(mining_errors(ds), "granito", "no tiene nivel")


def test_mineria_hacha_delante_roba_el_pico(ds: DataSet) -> None:
    tpl = ds.data["templates.json"]
    pico = template(ds, "pico")
    tpl.remove(pico)
    tpl.append(pico)
    assert any_error(mining_errors(ds), "canto_aguzado", "hacha")


def test_canto_rodado_con_mango_sigue_dando_hacha_de_piedra(real: DataSet) -> None:
    # Biblia 01 (días 2-4) y 02 §1.2: el hacha de piedra existe; el pico exige Punta.
    mango = apply(real, inst(real, "tronco_pequeno"), inst(real, "liana"), "Atar")
    for piedra in ("canto_rodado", "basalto", "piedra_plana"):
        assert apply(real, mango, inst(real, piedra), "Atar").definition == "hacha", piedra


def test_pico_de_piedra_sale_del_canto_aguzado(real: DataSet) -> None:
    punta = apply(real, inst(real, "lasca_pedernal"), inst(real, "canto_rodado"), "Tallar")
    assert punta.definition == "canto_aguzado"
    mango = apply(real, inst(real, "tronco_pequeno"), inst(real, "liana"), "Atar")
    assert apply(real, mango, punta, "Atar").definition == "pico"


def test_cabeza_rescatada_de_chapa_o_hierro_en_el_banco(real: DataSet) -> None:
    # Biblia 02 §2.2: pico rescatado de chapa_fuselaje/hierro_meteorito, tras el banco de chatarra.
    for chatarra in ("chapa_fuselaje", "hierro_meteorito"):
        head = apply(real, inst(real, "canto_rodado"), inst(real, chatarra), "Golpear")
        assert head.definition == "cabeza_pico_rescatada", chatarra
    tubo = apply(real, inst(real, "canto_rodado"), inst(real, "tubo_aluminio"), "Golpear")
    assert tubo is None or tubo.definition != "cabeza_pico_rescatada"
    for tid in ("cabeza_pico_de_chapa", "cabeza_pico_de_hierro"):
        assert template(real, tid)["station"] == "banco_chatarra"


def test_plantilla_con_estacion_que_no_existe(ds: DataSet) -> None:
    template(ds, "cabeza_pico_de_chapa")["station"] = "fragua"
    assert any_error(run_all(ds).errors, "cabeza_pico_de_chapa", "fragua")


def test_mineria_pico_de_obsidiana_solo_en_fase_2(ds: DataSet) -> None:
    obs = stratum(ds, "obsidiana")
    obs["surfaceSource"] = False
    for occ in obs["occurrences"]:
        occ["fase"] = 2
    without_tool(ds, "pico_rescatado")
    assert any_error(mining_errors(ds), "fase 1", "nivel 3")


def test_mineria_ciclo_obsidiana_solo_con_obsidiana(ds: DataSet) -> None:
    stratum(ds, "obsidiana")["surfaceSource"] = False
    without_tool(ds, "pico_rescatado")
    errors = mining_errors(ds)
    assert any_error(errors, "se queda en el nivel 3")
    assert any_error(errors, "obsidiana", "ninguna herramienta")


def test_mineria_afloramiento_de_fase_2_no_cuenta_en_fase_1(ds: DataSet) -> None:
    # Obsidiana suelta solo en islas de fase 2: en fase 1 no puede dar el pico de nivel 4.
    for occ in stratum(ds, "obsidiana")["occurrences"]:
        occ["fase"] = 2
    without_tool(ds, "pico_rescatado")
    assert any_error(mining_errors(ds), "fase 1", "nivel 3")


def test_mineria_nivel_saltado_no_cuenta(ds: DataSet) -> None:
    # Cabeza tallada imposible: el nivel 3 no existe aunque la obsidiana suelta dé el 4.
    template(ds, "cabeza_pico_por_tallado")["slots"][0]["requirements"][0]["min"] = 6
    assert any_error(mining_errors(ds), "se queda en el nivel 2")


def test_mineria_veta_mal_formada(ds: DataSet) -> None:
    stratum(ds, "veta_cobre")["vein"]["veinUnits"] = 0
    assert any_error(mining_errors(ds), "veta_cobre", "veinUnits")


def test_mineria_la_cabeza_tallada_sale_de_lasca_y_basalto(real: DataSet) -> None:
    items = {i["id"]: i for i in real.items}
    best = crafting.best_template(real.templates, "Tallar", crafting.leaf(items["lasca_pedernal"]), crafting.leaf(items["basalto"]))
    assert best and best["resultDefinitionId"] == "basalto_tallado"


# --------------------------------------------------------------------------- fauna salvaje (GDD v2 §3.7)

from datacheck import fauna
from datacheck.checks import PROPERTIES


def fauna_errors(ds: DataSet) -> list[str]:
    r = Report()
    fauna.check_fauna(ds, r, PROPERTIES)
    return r.errors


def animal(ds: DataSet, sid: str) -> dict:
    return next(s for s in ds.data["fauna.json"]["species"] if s["id"] == sid)


def fauna_island(ds: DataSet, iid: str) -> dict:
    return next(i for i in ds.data["fauna.json"]["islands"] if i["island"] == iid)


def test_fauna_real_sin_errores_y_lee_el_cpp(real: DataSet) -> None:
    assert fauna_errors(real) == []
    assert {"Gull", "Frigatebird"} <= fauna.cpp_species(real)
    assert fauna.cpp_lod(real)["FullRadiusCm"] == 4000.0


def test_fauna_lod_distinto_del_cpp(ds: DataSet) -> None:
    ds.data["fauna.json"]["lod"]["reducedRadiusCm"] = 20000.0
    assert any_error(fauna_errors(ds), "reducedRadiusCm", "FFaunaLodSettings")


def test_fauna_especie_cpp_inexistente(ds: DataSet) -> None:
    animal(ds, "gaviota_posada")["cppSpecies"] = "Albatross"
    assert any_error(fauna_errors(ds), "Albatross", "EFaunaSpecies")


def test_fauna_rutina_con_hueco(ds: DataSet) -> None:
    animal(ds, "cerdo_salvaje")["routine"].pop()
    assert any_error(fauna_errors(ds), "cerdo_salvaje", "24 h")


def test_fauna_botin_inexistente(ds: DataSet) -> None:
    animal(ds, "cerdo_salvaje")["loot"].append({"item": "chuleta", "min": 1, "max": 1, "tool": "cuchillo"})
    assert any_error(fauna_errors(ds), "chuleta", "items.json")


def test_fauna_isla_de_fase_1_con_especie_de_fase_2(ds: DataSet) -> None:
    fauna_island(ds, "landing")["species"].append("cabra_salvaje")
    assert any_error(fauna_errors(ds), "landing", "cabra_salvaje", "fase 2")


def test_fauna_falta_isla_del_acceso_anticipado(ds: DataSet) -> None:
    ds.data["fauna.json"]["islands"] = [i for i in ds.data["fauna.json"]["islands"] if i["island"] != "smoke"]
    assert any_error(fauna_errors(ds), "smoke", "acceso anticipado")


def test_fauna_pendiente_de_malla(ds: DataSet) -> None:
    ds.data["meshes_pendientes.json"]["fauna"] = []
    assert any_error(errors_of(ds), "meshes_pendientes.json/fauna", "cerdo_salvaje")


def test_fauna_terrestre_ya_no_es_termino_prohibido(real_report: Report) -> None:
    assert not any("cerdo" in e or "cabra" in e for e in real_report.errors)


# --------------------------------------------------------------------------- borradores de fase 2 y 3

from datacheck import fases
from datacheck.checks import BUILDING_SOCKETS


def fases_errors(ds: DataSet) -> list[str]:
    r = Report()
    fases.check_future_phases(ds, r, BUILDING_SOCKETS)
    return r.errors


def future(ds: DataSet) -> dict:
    return ds.data["fases_futuras.json"]


def test_fases_real_sin_errores(real: DataSet) -> None:
    assert fases_errors(real) == []


def test_fases_dato_de_fase_1_usa_el_borrador(ds: DataSet) -> None:
    piece(ds, "muro_piedra")["cost"].append({"item": "lingote_hierro", "count": 1})
    assert any_error(fases_errors(ds), "building_pieces.json", "lingote_hierro", "fase 2/3")


def test_fases_entrada_sin_fase_de_borrador(ds: DataSet) -> None:
    future(ds)["tramway"]["pieces"][0]["fase"] = 1
    assert any_error(fases_errors(ds), "rail_recto", "fase")


def test_fases_coste_con_objeto_inexistente(ds: DataSet) -> None:
    future(ds)["defenses"]["pieces"][0]["cost"].append({"item": "cemento", "count": 2})
    assert any_error(fases_errors(ds), "cemento", "pendingItems")


def test_fases_pendiente_que_ya_existe(ds: DataSet) -> None:
    future(ds)["pendingItems"].append({"id": "cuerda", "fase": 2})
    assert any_error(fases_errors(ds), "cuerda", "ya existe")


def test_fases_trueque_con_precio(ds: DataSet) -> None:
    future(ds)["trade"]["offers"][0]["precio"] = 10
    assert any_error(fases_errors(ds), "precio", "tienda")


def test_fases_tramos_de_reputacion_con_hueco(ds: DataSet) -> None:
    future(ds)["trade"]["tiers"][2]["min"] = 45
    assert any_error(fases_errors(ds), "neutral", "no continúa")


def test_fases_animal_domestico_sin_origen_salvaje(ds: DataSet) -> None:
    future(ds)["livestock"]["species"][1]["wildSource"] = "jabali_gigante"
    assert any_error(fases_errors(ds), "jabali_gigante", "fauna.json")


# --------------------------------------------------------------------------- packs CC0 (GDD v2 §7.1)

from datacheck import packs as packs_check  # noqa: E402


def _catalog(ds: DataSet) -> dict:
    return ds.data["packs_catalogo.json"]


def test_packs_catalogo_cubre_herramientas_de_las_primeras_horas(real: DataSet) -> None:
    covered = {e["gameId"] for e in _catalog(real)["entries"]}
    assert {"hacha", "cuchillo", "pala", "tronco_pequeno", "cuerda"} <= covered


def test_packs_catalogo_id_inexistente(ds: DataSet) -> None:
    _catalog(ds)["entries"][0]["gameId"] = "hacha_laser"
    assert any_error(errors_of(ds), "hacha_laser", "no existe")


def test_packs_catalogo_duplicado(ds: DataSet) -> None:
    cat = _catalog(ds)
    dup = dict(cat["entries"][0])
    dup["mesh"] = "SM_Pack_Otra"
    cat["entries"].append(dup)
    assert any_error(errors_of(ds), "duplicada")


def test_packs_catalogo_muestra_de_paleta_inexistente(ds: DataSet) -> None:
    _catalog(ds)["entries"][0]["recolor"]["default"] = "madera.fluorescente"
    assert any_error(errors_of(ds), "madera.fluorescente", "paleta")


def test_packs_catalogo_pack_desconocido(ds: DataSet) -> None:
    _catalog(ds)["entries"][0]["pack"] = "pack_de_pago"
    assert any_error(errors_of(ds), "pack_de_pago", "packs.json")


def test_packs_catalogo_agarre_fuera_del_mango(ds: DataSet) -> None:
    e = next(e for e in _catalog(ds)["entries"] if e["pivot"]["kind"] == "agarre")
    e["pivot"]["gripFromEndM"] = e["size"]["m"] * 2
    assert any_error(errors_of(ds), "gripFromEndM")


def test_packs_manifiesto_rechaza_licencia_no_cc0(real: DataSet) -> None:
    manifest = packs_check.load_manifest(real.repo_root)
    manifest["packs"][0]["license"]["spdx"] = "CC-BY-4.0"
    manifest["packs"][1]["sha256"] = ""
    errs: list[str] = []
    usable = packs_check.check_manifest(manifest, errs.append)
    assert any("CC-BY-4.0" in e for e in errs)
    assert any("sha256" in e for e in errs)
    assert manifest["packs"][0]["id"] not in usable and manifest["packs"][1]["id"] not in usable


def test_packs_catalogo_cubre_la_lanza_y_la_caza(real: DataSet) -> None:
    covered = {e["gameId"] for e in _catalog(real)["entries"]}
    pending = {p["gameId"] for p in _catalog(real)["pending"]}
    assert {"lanza", "arco", "flecha"} <= covered
    assert "lanza" not in pending


def test_packs_catalogo_centro_del_agarre_desconocido(ds: DataSet) -> None:
    e = next(e for e in _catalog(ds)["entries"] if e["pivot"]["kind"] == "agarre")
    e["pivot"]["centerAt"] = "punta"
    assert any_error(errors_of(ds), "centerAt")


def test_packs_catalogo_replaces_de_pieza_comprueba_la_malla(ds: DataSet) -> None:
    e = dict(next(e for e in _catalog(ds)["entries"] if e["pivot"]["kind"] == "base"))
    e.update(gameId="muelle", kind="pieza", mesh="SM_Pack_MuellePrueba", replaces="SM_Base_Muelle")
    _catalog(ds)["entries"].append(e)
    assert any_error(errors_of(ds), "muelle", "replaces", "SM_Base_Dock")


def test_packs_catalogo_descarte_con_id_inexistente(ds: DataSet) -> None:
    _catalog(ds)["discarded"][0]["gameId"] = "pared_de_neon"
    assert any_error(errors_of(ds), "pared_de_neon", "no existe")


def test_packs_catalogo_cubre_fases_del_huerto(real: DataSet) -> None:
    entries = {e["gameId"]: e for e in _catalog(real)["entries"]}
    assert {"platanera.hijuelo", "pina.roseta", "limonero.arbol_joven"} <= set(entries)
    assert all(entries[g]["kind"] == "planta" for g in ("platanera.hijuelo", "pina.roseta"))


def test_packs_catalogo_pendiente_de_etapa_inexistente(ds: DataSet) -> None:
    _catalog(ds)["pending"].append({"gameId": "taro.florecido", "reason": "prueba"})
    assert any_error(errors_of(ds), "taro.florecido", "etapa")


def test_packs_catalogo_descarte_de_etapa_valida(ds: DataSet) -> None:
    d = dict(_catalog(ds)["discarded"][0], gameId="batata.enredadera")
    _catalog(ds)["discarded"].append(d)
    assert not any_error(errors_of(ds), "batata.enredadera")


# --------------------------------------------------------------------------- red (biblia 08 §2.7)

def fauna_errors(ds: DataSet) -> list[str]:
    from datacheck import fauna
    from datacheck.checks import PROPERTIES

    r = Report()
    fauna.check_fauna(ds, r, PROPERTIES)
    return r.errors


def species(ds: DataSet, sid: str) -> dict:
    return next(s for s in ds.data["fauna.json"]["species"] if s["id"] == sid)


def test_fauna_real_clasificada_para_red(real: DataSet) -> None:
    assert fauna_errors(real) == []
    assert species(real, "cerdo_salvaje")["red"]["clase"] == "replicada"
    assert species(real, "fragata_colonia")["red"]["tiradaDano"] == "servidor"


def test_fauna_sin_red(ds: DataSet) -> None:
    del species(ds, "gaviota_posada")["red"]
    assert any_error(fauna_errors(ds), "gaviota_posada", "red")


def test_fauna_ataque_tirado_en_cliente(ds: DataSet) -> None:
    species(ds, "fragata_colonia")["red"]["tiradaDano"] = None
    assert any_error(fauna_errors(ds), "fragata_colonia", "servidor")


def test_fauna_cazable_como_ambiente(ds: DataSet) -> None:
    species(ds, "cerdo_salvaje")["red"].update(clase="ambiente", ancla="bandada")
    assert any_error(fauna_errors(ds), "cerdo_salvaje", "replicada")


def test_fauna_nidos_sin_estado_en_servidor(ds: DataSet) -> None:
    species(ds, "gaviota_posada")["red"]["recogidas"] = None
    assert any_error(fauna_errors(ds), "gaviota_posada", "nidos")


def test_fases_futuras_sin_nota_de_red(ds: DataSet) -> None:
    del ds.data["fases_futuras.json"]["trade"]["redNotaEs"]
    assert any_error(run_all(ds).errors, "trade", "redNotaEs")


def _fauna_entry(ds: DataSet) -> dict:
    return next(e for e in _catalog(ds)["entries"] if e["kind"] == "fauna")


def test_packs_catalogo_cubre_el_cerdo_salvaje_con_rig(real: DataSet) -> None:
    e = _fauna_entry(real)
    assert e["gameId"] == "cerdo_salvaje" and e["mesh"].startswith("SK_Pack_")
    assert {"Idle", "Walk", "Run", "Death"} <= set(e["rig"]["animations"])


def test_packs_catalogo_fauna_con_id_inexistente(ds: DataSet) -> None:
    _fauna_entry(ds)["gameId"] = "dragon_de_komodo"
    assert any_error(errors_of(ds), "dragon_de_komodo", "no existe como fauna")


def test_packs_catalogo_fauna_sin_rig(ds: DataSet) -> None:
    del _fauna_entry(ds)["rig"]
    assert any_error(errors_of(ds), "bloque rig")


def test_packs_catalogo_fauna_con_malla_estatica(ds: DataSet) -> None:
    _fauna_entry(ds)["mesh"] = "SM_Pack_Cerdo"
    assert any_error(errors_of(ds), "SK_Pack_")


def test_packs_catalogo_comportamiento_con_clip_inexistente(ds: DataSet) -> None:
    _fauna_entry(ds)["rig"]["behaviors"]["cargar"] = "Charge"
    assert any_error(errors_of(ds), "cargar", "Charge")


def test_packs_catalogo_rig_fuera_de_fauna(ds: DataSet) -> None:
    _catalog(ds)["entries"][0]["rig"] = {"skeleton": "SKEL_Pack_Hacha", "animations": ["Idle"]}
    assert any_error(errors_of(ds), "rig solo va en kind fauna")


def test_fauna_y_fauna_terrestre_nombran_igual(ds: DataSet) -> None:
    sp = next(s for s in ds.data["fauna.json"]["species"] if s["id"] == "cerdo_salvaje")
    sp["nameEn"] = "Wild pig"
    assert any_error(fauna_errors(ds), "cerdo_salvaje", "nameEn", "fauna_terrestre.json")


def test_fauna_terrestre_salvaje_sin_ficha_en_fauna(ds: DataSet) -> None:
    for s in ds.data["fauna.json"]["species"]:
        if s["id"] == "cabra_salvaje":
            s["id"] = "cabra_montes"
    for isl in ds.data["fauna.json"]["islands"]:
        isl["species"] = ["cabra_montes" if x == "cabra_salvaje" else x for x in isl["species"]]
    assert any_error(fauna_errors(ds), "fauna_terrestre.json", "cabra_salvaje", "no está en fauna.json")


def test_fauna_terrestre_con_otra_isla(ds: DataSet) -> None:
    reg = next(s for s in ds.data["fauna_terrestre.json"]["species"] if s["id"] == "cerdo_salvaje")
    reg["islands"] = ["Landing"]
    assert any_error(fauna_errors(ds), "cerdo_salvaje", "vive en")


def test_fauna_terrestre_isla_desconocida(ds: DataSet) -> None:
    ds.data["fauna_terrestre.json"]["species"][0]["islands"] = ["Atlantida"]
    assert any_error(errors_of(ds), "Atlantida", "EIslandArchetype")


def test_fauna_terrestre_id_duplicado(ds: DataSet) -> None:
    sp = ds.data["fauna_terrestre.json"]["species"]
    sp.append(dict(sp[0]))
    assert any_error(errors_of(ds), "fauna_terrestre.json", "duplicado")


def test_fauna_terrestre_de_fase_2_no_rompe_el_aislamiento(real: DataSet) -> None:
    # fauna_terrestre.json (packs) nombra cerdo, cabra y gallina con phase F2: no es fase 1.
    assert not any_error(errors_of(real), "fauna_terrestre.json", "borrador")
    assert not any_error(errors_of(real), "packs_catalogo.json", "borrador")


def test_fauna_terrestre_de_acceso_anticipado_con_id_del_borrador(ds: DataSet) -> None:
    sp = next(s for s in ds.data["fauna_terrestre.json"]["species"] if s["id"] == "gallina")
    sp["phase"] = "AA"
    assert any_error(errors_of(ds), "fauna_terrestre.json", "gallina", "borrador")

