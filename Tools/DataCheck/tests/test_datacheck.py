"""Tests del validador v3: los datos reales pasan y cada regresión típica se detecta."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import pytest

from datacheck import music, textos
from datacheck.checks import GDD_ITEMS, GDD_RECIPES, DataSet, Report, run_all


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


def item(ds: DataSet, iid: str) -> dict[str, Any]:
    return next(i for i in ds.items if i["id"] == iid)


def recipe(ds: DataSet, rid: str) -> dict[str, Any]:
    return next(r for r in ds.recipes if r["id"] == rid)


# --------------------------------------------------------------------------- datos reales


def test_datos_reales_sin_errores(real_report: Report) -> None:
    assert real_report.errors == []


def test_datos_reales_sin_avisos(real_report: Report) -> None:
    assert real_report.warnings == []


def test_lista_cerrada_del_gdd(real: DataSet) -> None:
    assert real.item_ids == set(GDD_ITEMS) and len(real.items) == 17
    assert {r["id"] for r in real.recipes} == set(GDD_RECIPES)
    assert [v["id"] for v in real.verbs] == ["encender", "trenzar", "atar", "cocinar", "hervir", "montar"]


# --------------------------------------------------------------------------- objetos


def test_detecta_objeto_fuera_de_la_lista(ds: DataSet) -> None:
    ds.data["items.json"].append(dict(item(ds, "lena"), id="rama_seca"))
    assert any_error(errors_of(ds), "fuera de la lista cerrada", "rama_seca")


def test_detecta_objeto_que_falta(ds: DataSet) -> None:
    ds.data["items.json"] = [i for i in ds.items if i["id"] != "mecha"]
    errs = errors_of(ds)
    assert any_error(errs, "faltan objetos", "mecha")
    assert any_error(errs, "«mecha»", "no está en items.json")


def test_detecta_pila_distinta_del_gdd(ds: DataSet) -> None:
    item(ds, "coco")["stack"] = 5
    assert any_error(errors_of(ds), "«coco»", "stack=5", "dice 4")


def test_detecta_tronco_ligero(ds: DataSet) -> None:
    item(ds, "tronco")["heavy"] = False
    assert any_error(errors_of(ds), "«tronco»", "heavy=False")


def test_detecta_pesado_que_no_va_a_dos_manos(ds: DataSet) -> None:
    item(ds, "lente")["size"] = "Grande"
    assert any_error(errors_of(ds), "«lente»", "DosManos")


def test_detecta_malla_fuera_del_kit(ds: DataSet) -> None:
    item(ds, "remo")["meshPath"] = "/Engine/BasicShapes/Cube.Cube"
    assert any_error(errors_of(ds), "«remo»", "K3_Objetos")


def test_detecta_id_duplicado_y_no_ascii(ds: DataSet) -> None:
    ds.data["items.json"].append(dict(item(ds, "lena")))
    ds.data["items.json"].append(dict(item(ds, "lena"), id="leña"))
    errs = errors_of(ds)
    assert any_error(errs, "duplicado «lena»")
    assert any_error(errs, "«leña» no es ASCII")


def test_detecta_peso_y_tamano_invalidos(ds: DataSet) -> None:
    item(ds, "fibra")["weightKg"] = 0
    item(ds, "fibra")["size"] = "Enorme"
    item(ds, "fibra")["nameEn"] = ""
    errs = errors_of(ds)
    assert any_error(errs, "«fibra»", "weightKg")
    assert any_error(errs, "«fibra»", "size «Enorme»")
    assert any_error(errs, "«fibra»", "falta nameEn")


# --------------------------------------------------------------------------- verbos y recetas


def test_detecta_verbo_de_mas_y_sin_descripcion(ds: DataSet) -> None:
    ds.data["verbs.json"].append(dict(ds.verbs[0], id="golpear"))
    ds.verbs[1]["descriptionEn"] = ""
    errs = errors_of(ds)
    assert any_error(errs, "verbs.json", "lista cerrada")
    assert any_error(errs, "«trenzar»", "descriptionEn")


def test_detecta_receta_con_verbo_inexistente(ds: DataSet) -> None:
    recipe(ds, "cuerda")["verb"] = "golpear"
    errs = errors_of(ds)
    assert any_error(errs, "«cuerda»", "verbo «golpear»")
    assert any_error(errs, "«cuerda»", "GDD §5.6")


def test_detecta_tiempo_distinto_del_gdd(ds: DataSet) -> None:
    recipe(ds, "pez_asado")["seconds"] = 30
    assert any_error(errors_of(ds), "«pez_asado»", "dice ('cocinar', 'hoguera', 20)")


def test_detecta_ingrediente_y_salida_inexistentes(ds: DataSet) -> None:
    recipe(ds, "remo")["inputs"][0]["item"] = "palo_recto"
    recipe(ds, "mecha")["output"] = {"item": "cordel", "count": 0}
    errs = errors_of(ds)
    assert any_error(errs, "«remo».inputs", "«palo_recto»")
    assert any_error(errs, "«mecha»", "salida «cordel»")
    assert any_error(errs, "«mecha»", "count entero positivo")


def test_detecta_salida_doble_o_de_mundo_desconocida(ds: DataSet) -> None:
    recipe(ds, "cuerda")["output"] = {"item": "cuerda", "count": 1, "world": "balsa"}
    recipe(ds, "balsa")["output"] = {"world": "barco"}
    errs = errors_of(ds)
    assert any_error(errs, "«cuerda»", "exactamente")
    assert any_error(errs, "«balsa»", "«barco» desconocida")


def test_detecta_cantidades_lugar_y_pasos_invalidos(ds: DataSet) -> None:
    rec = recipe(ds, "balsa")
    rec["inputs"][0]["count"] = 0
    rec["place"] = "playa"
    rec["steps"] = 0
    rec["tools"] = ["sierra"]
    rec["seconds"] = 0
    rec["nameEs"] = ""
    errs = errors_of(ds)
    assert any_error(errs, "«balsa».inputs", "entero positivo")
    assert any_error(errs, "«balsa»", "lugar «playa»")
    assert any_error(errs, "«balsa»", "steps")
    assert any_error(errs, "«balsa»", "tools")
    assert any_error(errs, "«balsa»", "seconds")
    assert any_error(errs, "«balsa»", "falta nameEs")


def test_detecta_receta_duplicada_y_lista_rota(ds: DataSet) -> None:
    ds.data["recipes.json"]["recipes"].append(dict(recipe(ds, "cuerda")))
    assert any_error(errors_of(ds), "duplicada «cuerda»")
    ds.data["recipes.json"]["recipes"] = "nada"
    assert any_error(errors_of(ds), "falta la lista «recipes»")


def test_detecta_objeto_inalcanzable(ds: DataSet) -> None:
    ds.data["recipes.json"]["recipes"] = [r for r in ds.recipes if r["id"] != "remo"]
    assert any_error(errors_of(ds), "«remo» no sale de ninguna receta")


# --------------------------------------------------------------------------- fuego, cuerpo y pesca


def test_detecta_fuego_desfasado_del_cpp(ds: DataSet) -> None:
    ds.data["fuels.json"]["campfire"]["burnSeconds"] = 240
    ds.data["fuels.json"]["fuels"][0]["secondsPerFuel"] = 60
    errs = errors_of(ds)
    assert any_error(errs, "campfire.burnSeconds=240", "DefaultBurnSeconds=300")
    assert any_error(errs, "«lena».secondsPerFuel=60", "DefaultWoodSeconds=102")


def test_detecta_combustible_fuera_del_gdd(ds: DataSet) -> None:
    ds.data["fuels.json"]["fuels"].append({"item": "fibra", "secondsPerFuel": 10})
    ds.data["fuels.json"]["fuels"].append({"item": "carbon", "secondsPerFuel": 10})
    errs = errors_of(ds)
    assert any_error(errs, "«fibra» no es combustible")
    assert any_error(errs, "«carbon» no está en items.json")
    del ds.data["fuels.json"]["campfire"]
    assert any_error(errors_of(ds), "falta «campfire»")


def test_detecta_consumible_desfasado(ds: DataSet) -> None:
    cons = ds.data["survival_needs.json"]["consumables"]
    coco = next(c for c in cons if c["item"] == "coco")
    coco["thirst"] = 30
    coco["leaves"] = "cascara"
    cons.append({"item": "lena", "thirst": "x"})
    errs = errors_of(ds)
    assert any_error(errs, "«coco».thirst=30", "CoconutThirst=25")
    assert any_error(errs, "deja «cascara»")
    assert any_error(errs, "«lena».thirst debe ser un número")
    assert any_error(errs, "deben ser coco, cantimplora")


def test_detecta_barras_y_kit_rotos(ds: DataSet) -> None:
    doc = ds.data["survival_needs.json"]
    doc["needs"][0]["initial"] = 120
    doc["needs"][1]["lossPerMinute"] = -1
    doc["needs"][2]["nameEn"] = ""
    doc["starterKit"] = doc["starterKit"][:2]
    doc["backpackSlots"] = 10
    errs = errors_of(ds)
    assert any_error(errs, "«sed»", "initial")
    assert any_error(errs, "«hambre»", "lossPerMinute")
    assert any_error(errs, "«energia»", "nameEn")
    assert any_error(errs, "starterKit")
    assert any_error(errs, "backpackSlots=10", "DefaultSlots=8")
    doc["needs"] = doc["needs"][:3]
    assert any_error(errors_of(ds), "las barras deben ser")


def test_detecta_barril_desfasado(ds: DataSet) -> None:
    ds.data["survival_needs.json"]["rainBarrel"]["charges"] = 5
    assert any_error(errors_of(ds), "rainBarrel.charges=5", "MaxCharges=4")


def test_detecta_pesca_desfasada(ds: DataSet) -> None:
    ds.data["fish.json"]["meanBiteSeconds"] = 20
    ds.data["fish.json"]["catch"] = "atun"
    ds.data["fish.json"]["night"] = [20]
    errs = errors_of(ds)
    assert any_error(errs, "meanBiteSeconds=20", "DefaultMeanBiteSeconds=35")
    assert any_error(errs, "catch", "«atun»")
    assert any_error(errs, "night")


def test_constante_cpp_desaparecida(ds: DataSet, tmp_path: Path) -> None:
    src = tmp_path / "Source" / "Explored" / "Cooking"
    src.mkdir(parents=True)
    (src / "CampfireModel.h").write_text("struct FCampfireConfig {};", encoding="utf-8")
    ds.repo_root = tmp_path
    report = run_all(ds)
    assert any_error(report.errors, "no existe la constante DefaultBurnSeconds")
    assert any("no se encuentra" in w for w in report.warnings)


def test_detecta_termino_prohibido(ds: DataSet) -> None:
    item(ds, "coco")["nameEs"] = "Coco de la mascota"
    assert any_error(errors_of(ds), "items.json", "«mascota»")


# --------------------------------------------------------------------------- música (GDD §14.3)

def music_errors(ds: DataSet) -> list[str]:
    r = Report()
    music.check_music(ds, r)
    return r.errors


def music_piece(ds: DataSet, pid: str) -> dict[str, Any]:
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



# --------------------------------------------------------------------------- guía anti-IA (biblia 07 §1)


def _text_errors(ds: DataSet) -> list[str]:
    errors: list[str] = []
    textos.check_player_texts(ds.data, errors.append)
    return errors


def test_textos_reales_limpios(real: DataSet) -> None:
    assert _text_errors(real) == []


@pytest.mark.parametrize("texto, lang, rasgo", [
    ("Una experiencia increíble bajo tierra.", "es", "increíble"),
    ("Sumérgete en la mina.", "es", "sumérgete"),
    ("En definitiva, cava.", "es", "en definitiva"),
    ("Esto no es solo un agujero.", "es", "no es solo"),
    ("Unleash the pick.", "en", "unleash"),
    ("A seamless dig.", "en", "seamless"),
    ("It's not just a hole.", "en", "it's not just"),
])
def test_textos_lista_negra(texto: str, lang: str, rasgo: str) -> None:
    found = textos.problems(texto, lang)
    assert any(rasgo in p.lower() for p in found)


@pytest.mark.parametrize("texto, lang", [
    ("Perfectamente seco.", "es"),  # «perfecto» solo como palabra entera
    ("Elevated walkway", "en"),      # «elevate» no casa dentro de otra palabra
    ("Definitivamente no.", "es"),
])
def test_textos_lista_negra_sin_falsos_positivos(texto: str, lang: str) -> None:
    assert textos.problems(texto, lang) == []


def test_textos_exclamacion_en_un_objeto(ds: DataSet) -> None:
    item(ds, "coco")["nameEs"] = "¡Coco!"
    assert any_error(_text_errors(ds), "coco", "exclamación")


def test_textos_emoji_en_una_receta(ds: DataSet) -> None:
    recipe(ds, "mecha")["nameEn"] = "Wick \U0001F525"
    assert any_error(_text_errors(ds), "mecha", "emoji")


def test_textos_corruptos_no_rompen(ds: DataSet) -> None:
    item(ds, "coco")["nameEs"] = None
    ds.data["recipes.json"]["recipes"].append("no soy un objeto")
    assert _text_errors(ds) == []
