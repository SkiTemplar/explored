"""Contenido de texto de H4: los datos reales pasan y cada regresión típica se detecta."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from datacheck import achievements, contenido, estilo
from datacheck.checks import DataSet, Report


@pytest.fixture(scope="module")
def real() -> DataSet:
    return DataSet.load()


@pytest.fixture
def ds(real: DataSet) -> DataSet:
    return real.copy()


def content_errors(ds: DataSet) -> list[str]:
    r = Report()
    contenido.check_content(ds, r)
    return r.errors + r.warnings


def any_error(errors: list[str], *needles: str) -> bool:
    return any(all(n in e for n in needles) for e in errors)


def diary(ds: DataSet, did: str) -> dict:
    return next(d for d in ds.data["halden_diaries.json"]["entries"] if d["id"] == did)


def entry(ds: DataSet, eid: str) -> dict:
    return next(e for e in ds.data["journal_entries.json"]["entries"] if e["id"] == eid)


def clue(ds: DataSet, cid: str) -> dict:
    return next(c for c in ds.data["map_clues.json"]["clues"] if c["id"] == cid)


def site(ds: DataSet, sid: str) -> dict:
    return next(s for s in ds.data["ruins.json"]["sites"] if s["id"] == sid)


def collection(ds: DataSet, cid: str) -> dict:
    return next(c for c in ds.data["museum_collections.json"]["collections"] if c["id"] == cid)


def museum_piece(ds: DataSet, file: str, pid: str) -> dict:
    return next(p for p in ds.data[file]["pieces"] if p["id"] == pid)


# --------------------------------------------------------------------------- datos reales


def test_contenido_real_sin_errores(real: DataSet) -> None:
    assert content_errors(real) == []


def test_lee_los_poi_del_cpp(real: DataSet) -> None:
    by_type, by_content = contenido.poi_placements(real.repo_root)
    assert by_type["SextantCave"] == {"landing"}
    assert by_type["RadioStation"] == {"mangrove"}
    assert contenido.ANY_ISLAND in by_type["Viewpoint"]
    assert by_content["camp_halden_2"] == ("HaldenCamp", "smoke")
    assert "SextantCave" in contenido.poi_types(real.repo_root)
    assert "Count" not in contenido.poi_types(real.repo_root)


def test_islas_del_cpp_coinciden_con_sus_fases(real: DataSet) -> None:
    assert contenido.cpp_islands(real.repo_root) == set(contenido.ISLAND_PHASE)


def test_quince_entradas_y_cinco_cuadernos(real: DataSet) -> None:
    assert len(real.data["journal_entries.json"]["entries"]) >= 15
    assert [d["id"] for d in real.data["halden_diaries.json"]["entries"]] == contenido.HALDEN_IDS


def test_ruinas_asignan_los_cuatro_caminos_de_la_biblia(real: DataSet) -> None:
    targets = {s["id"]: s["starPathTarget"] for s in real.data["ruins.json"]["sites"]}
    assert targets["ruin_landing"] == "emerald"
    assert targets["ruin_compass"] == "teeth"
    assert targets["ruin_whitesands"] == "mesa"
    assert targets["ruin_mesa"] == "hidden"
    assert sum(t is not None for t in targets.values()) == 4


def test_juego_de_anzuelos_pide_los_tres_anzuelos(real: DataSet) -> None:
    ach = next(a for a in real.data["achievements.json"]["achievements"] if a["id"] == "juego_de_anzuelos")
    hooks = {c["contains"] for c in ach["condition"]["all"]}
    artifacts = {a["id"] for a in real.data["artifacts.json"]["artifacts"]}
    assert hooks == {"anzuelo_hueso", "anzuelo_nacar", "anzuelo_ceremonial"} <= artifacts


def test_las_piezas_de_museo_nuevas_existen(real: DataSet) -> None:
    pieces = {p["id"]: p for p in real.building["pieces"]}
    for pid in ("pecera_museo", "bandeja_conchas", "marco_herbario", "atril_cuaderno",
                "vitrina_minerales", "panel_fosiles"):
        assert pieces[pid]["category"] == "museo"


def test_ficheros_nuevos_en_utf8_con_salto_final(real: DataSet) -> None:
    for name in contenido.CONTENT_FILES:
        raw = (real.repo_root / "Content" / "Data" / name).read_bytes()
        assert raw.endswith(b"\n") and not raw.startswith(b"\xef\xbb\xbf"), name
        json.loads(raw.decode("utf-8"))


# --------------------------------------------------------------------------- diarios Halden


def test_cuaderno_en_una_isla_sin_ese_poi(ds: DataSet) -> None:
    diary(ds, "halden_01")["island"] = "landing"
    errors = content_errors(ds)
    assert any_error(errors, "halden_01", "RadioStation", "landing")


def test_cuaderno_con_content_id_inexistente(ds: DataSet) -> None:
    diary(ds, "halden_03")["contentId"] = "camp_halden_9"
    assert any_error(content_errors(ds), "halden_03", "camp_halden_9")


def test_cuaderno_con_content_id_de_otra_isla(ds: DataSet) -> None:
    # camp_halden_2 es el campamento del Humo, no el de Esmeralda.
    diary(ds, "halden_03")["contentId"] = "camp_halden_2"
    errors = content_errors(ds)
    assert any_error(errors, "halden_03", "camp_halden_2", "smoke")
    assert any_error(errors, "camp_halden_2", "repetido")


def test_cuaderno_con_isla_desconocida(ds: DataSet) -> None:
    diary(ds, "halden_02")["island"] = "Smoke"
    assert any_error(content_errors(ds), "halden_02", "isla 'Smoke'")


def test_cuaderno_con_poi_que_no_es_epoitype(ds: DataSet) -> None:
    diary(ds, "halden_02")["poi"] = "Camp"
    assert any_error(content_errors(ds), "halden_02", "no es un EPoiType")


def test_falta_un_cuaderno(ds: DataSet) -> None:
    ds.data["halden_diaries.json"]["entries"].pop()
    errors = content_errors(ds)
    assert any_error(errors, "biblia 04 §7.1")


def test_cuaderno_con_dia_que_no_cuadra(ds: DataSet) -> None:
    diary(ds, "halden_01")["expeditionDay"] = 15
    assert any_error(content_errors(ds), "halden_01", "Día 15.")


def test_cuaderno_sin_ingles(ds: DataSet) -> None:
    diary(ds, "halden_04")["textEn"] = ""
    assert any_error(content_errors(ds), "halden_04", "falta textEn")


# --------------------------------------------------------------------------- diario del jugador


def test_entrada_con_suceso_desconocido(ds: DataSet) -> None:
    entry(ds, "diario_amaraje")["trigger"] = {"event": "plane_crash"}
    errors = content_errors(ds)
    assert any_error(errors, "diario_amaraje", "plane_crash")
    assert any_error(errors, "run_start", "ningún disparador")


def test_entrada_con_estadistica_desconocida(ds: DataSet) -> None:
    entry(ds, "diario_primer_fuego")["trigger"] = {"stat": "fire_lit", "op": ">=", "value": 1}
    assert any_error(content_errors(ds), "diario_primer_fuego", "fire_lit")


def test_entrada_con_id_fuera_del_conjunto(ds: DataSet) -> None:
    entry(ds, "diario_primer_limon")["trigger"] = {"stat": "crops_harvested", "contains": "limon"}
    assert any_error(content_errors(ds), "diario_primer_limon", "limon")


def test_entrada_de_aa_con_suceso_de_f2(ds: DataSet) -> None:
    entry(ds, "diario_primera_empalizada")["phase"] = "AA"
    assert any_error(content_errors(ds), "diario_primera_empalizada", "palisade_built", "F2")


def test_entrada_sin_marcador_de_dia(ds: DataSet) -> None:
    entry(ds, "diario_primer_trazo")["textEn"] = "Day 4. Started the map. One crooked line where the beach ends."
    assert any_error(content_errors(ds), "diario_primer_trazo", "{Day}")


def test_entrada_con_exclamacion(ds: DataSet) -> None:
    entry(ds, "diario_primer_fuego")["textEs"] = "Día {Day}. ¡Por fin fuego!"
    assert any_error(content_errors(ds), "diario_primer_fuego", "exclamación")


def test_entrada_demasiado_larga(ds: DataSet) -> None:
    e = entry(ds, "diario_primera_tormenta")
    e["textEs"] = "Día {Day}. " + "El techo aguantó toda la noche. " * 9
    assert any_error(content_errors(ds), "diario_primera_tormenta", "caracteres")


def test_suceso_sin_usar(ds: DataSet) -> None:
    ds.data["journal_entries.json"]["events"].append(
        {"id": "whale_seen", "phase": "AA", "reportedBy": "x", "descriptionEs": "x"})
    assert any_error(content_errors(ds), "whale_seen", "ningún disparador")


# --------------------------------------------------------------------------- pistas


def test_pista_de_un_tesoro_comun(ds: DataSet) -> None:
    clue(ds, "pista_carta_varillas")["artifact"] = "anzuelo_hueso"
    assert any_error(content_errors(ds), "anzuelo_hueso", "comun")


def test_pista_con_escondite_que_no_cuadra(ds: DataSet) -> None:
    clue(ds, "pista_colgante_carey")["hiddenIn"] = "enterrado"
    assert any_error(content_errors(ds), "colgante_carey", "pecio", "enterrado")


def test_pista_de_una_ruina_de_otra_isla(ds: DataSet) -> None:
    clue(ds, "pista_tapa_estrellas")["source"] = {"kind": "altar", "site": "ruin_landing"}
    assert any_error(content_errors(ds), "pista_tapa_estrellas", "ruin_landing", "landing")


def test_pista_de_un_cuaderno_inexistente(ds: DataSet) -> None:
    clue(ds, "pista_figura_navegante")["source"] = {"kind": "diary", "diary": "halden_09"}
    assert any_error(content_errors(ds), "halden_09")


def test_pista_con_poi_ausente_en_la_isla(ds: DataSet) -> None:
    clue(ds, "pista_anzuelo_ceremonial")["poi"] = "Lighthouse"
    assert any_error(content_errors(ds), "pista_anzuelo_ceremonial", "Lighthouse", "emerald")


def test_dos_pistas_para_el_mismo_tesoro(ds: DataSet) -> None:
    clue(ds, "pista_pectoral_nacar")["artifact"] = "tapa_estrellas"
    assert any_error(content_errors(ds), "tapa_estrellas", "ya tiene pista")


def test_pista_con_coordenadas_en_ingles_mas_largas(ds: DataSet) -> None:
    c = clue(ds, "pista_tapa_estrellas")
    c["clueEn"] = c["clueEn"] + " It is exactly the place where the coast turns east, you can't miss it."
    assert any_error(content_errors(ds), "pista_tapa_estrellas", "1,3×")


# --------------------------------------------------------------------------- ruinas


def test_camino_de_estrellas_a_la_propia_isla(ds: DataSet) -> None:
    site(ds, "ruin_landing")["starPathTarget"] = "landing"
    assert any_error(content_errors(ds), "ruin_landing", "propia isla")


def test_dos_caminos_al_mismo_destino(ds: DataSet) -> None:
    site(ds, "ruin_compass")["starPathTarget"] = "emerald"
    assert any_error(content_errors(ds), "ruin_compass", "emerald", "repetido")


def test_tecnica_global_enseñada_dos_veces(ds: DataSet) -> None:
    s = site(ds, "ruin_landing")
    s["teaches"], s["starPathTarget"] = "swell_reading", None
    errors = content_errors(ds)
    assert any_error(errors, "swell_reading", "una sola ruina")


def test_tecnica_global_con_destino(ds: DataSet) -> None:
    site(ds, "ruin_teeth")["starPathTarget"] = "mesa"
    assert any_error(content_errors(ds), "ruin_teeth", "no debe tener starPathTarget")


def test_nadie_ensena_la_isla_oculta(ds: DataSet) -> None:
    site(ds, "ruin_mesa")["starPathTarget"] = "mangrove"
    assert any_error(content_errors(ds), "isla oculta")


def test_ruina_en_otra_isla(ds: DataSet) -> None:
    site(ds, "ruin_teeth")["island"] = "smoke"
    assert any_error(content_errors(ds), "ruin_teeth", "teeth")


# --------------------------------------------------------------------------- museo


def test_ficha_que_no_empieza_por_el_nombre(ds: DataSet) -> None:
    museum_piece(ds, "shells.json", "casco_real")["nameEs"] = "Casco real"
    assert any_error(content_errors(ds), "casco_real", "Casco real.")


def test_ficha_con_lista_negra(ds: DataSet) -> None:
    p = museum_piece(ds, "minerals.json", "amatista_caverna")
    p["descriptionEs"] = "Amatista de caverna. Un increíble cristal violeta."
    assert any_error(content_errors(ds), "amatista_caverna", "increíble")


def test_ficha_en_ingles_con_lista_negra(ds: DataSet) -> None:
    p = museum_piece(ds, "insects.json", "mariposa_cristal")
    p["descriptionEn"] = "Glasswing butterfly. Its breathtaking wings let the light through."
    assert any_error(content_errors(ds), "mariposa_cristal", "breathtaking")


def test_pieza_con_fase_que_no_cuadra(ds: DataSet) -> None:
    museum_piece(ds, "herbarium.json", "orquidea_acantilado")["phase"] = "AA"
    assert any_error(content_errors(ds), "orquidea_acantilado", "F2")


def test_fosil_de_isla_aa_sigue_en_f2(ds: DataSet) -> None:
    museum_piece(ds, "fossils.json", "huella_ave_ceniza")["phase"] = "AA"
    assert any_error(content_errors(ds), "huella_ave_ceniza", "F2")


def test_pieza_con_objeto_inexistente(ds: DataSet) -> None:
    museum_piece(ds, "minerals.json", "cobre_nativo")["item"] = "cobre"
    assert any_error(content_errors(ds), "cobre_nativo", "cobre")


def test_pieza_con_forma_de_conseguir_de_otra_coleccion(ds: DataSet) -> None:
    museum_piece(ds, "insects.json", "hormiga_cortadora")["obtain"] = "cortar_y_prensar"
    assert any_error(content_errors(ds), "hormiga_cortadora", "cortar_y_prensar")


def test_recuento_de_coleccion_desfasado(ds: DataSet) -> None:
    ds.data["shells.json"]["pieces"].pop()
    assert any_error(content_errors(ds), "conchas", "pieces=8")


def test_recuento_de_tesoros_desfasado(ds: DataSet) -> None:
    next(a for a in ds.data["artifacts.json"]["artifacts"] if a["id"] == "anzuelo_hueso")["rarity"] = "raro"
    assert any_error(content_errors(ds), "tesoros", "pieces=12")


def test_mueble_de_museo_huerfano(ds: DataSet) -> None:
    collection(ds, "conchas")["displays"] = ["vitrina_museo"]
    errors = content_errors(ds)
    assert any_error(errors, "bandeja_conchas", "ninguna colección")


def test_mueble_que_no_es_de_museo(ds: DataSet) -> None:
    collection(ds, "peces")["displays"] = ["mesa_cartografia"]
    assert any_error(content_errors(ds), "mesa_cartografia", "museo")


def test_pez_sin_nombre_ingles(ds: DataSet) -> None:
    del ds.data["fish.json"]["species"][0]["nameEn"]
    assert any_error(content_errors(ds), "pez_loro", "nameEn")


def test_pieza_repetida_entre_colecciones(ds: DataSet) -> None:
    museum_piece(ds, "minerals.json", "caliza_veteada")["id"] = "diente_tiburon_fosil"
    assert any_error(content_errors(ds), "diente_tiburon_fosil", "también")


# --------------------------------------------------------------------------- estilo


@pytest.mark.parametrize("text", [
    "Sumérgete en la selva.", "Un viaje de descubrimiento.", "Esto no es solo un refugio.",
    "En definitiva, una concha.", "¿Alguna vez soñaste con esto?",
])
def test_lista_negra_es(text: str) -> None:
    assert estilo.lint(text, "es", "ficha")


@pytest.mark.parametrize("text", [
    "Dive into the jungle.", "It's not just a shell.", "Unleash the tide.", "Ready to sail?",
    "A testament to patience.",
])
def test_lista_negra_en(text: str) -> None:
    assert estilo.lint(text, "en", "ficha")


@pytest.mark.parametrize("text, lang", [
    ("Cuaderno de la expedición. Se desató la tormenta.", "es"),
    ("The pool was ready to fill before dawn.", "en"),
    ("Perfectamente seco.", "es"),
])
def test_lista_negra_sin_falsos_positivos(text: str, lang: str) -> None:
    assert estilo.lint(text, lang, "ficha") == []


def test_emoji_rayas_y_puntos_suspensivos() -> None:
    assert estilo.lint("Una concha 🐚.", "es", "ficha")
    assert estilo.lint("Una concha — rara — y vieja — mucho.", "es", "ficha")
    assert estilo.lint("Una concha rara…", "es", "ficha")


def test_frases_cuentan_el_marcador_de_dia() -> None:
    assert estilo.sentences("Día {Day}. Primera hoguera. Tuve frío.") == 3


def test_logro_con_exclamacion(ds: DataSet) -> None:
    ach = next(a for a in ds.data["achievements.json"]["achievements"] if a["id"] == "juego_de_anzuelos")
    ach["descriptionEs"] = "¡Reúne los tres anzuelos!"
    r = Report()
    achievements.check_achievements(ds, r)
    assert any_error(r.errors, "juego_de_anzuelos", "exclamación")


def test_artifact_ids_found_admite_solo_ids_de_artifacts(ds: DataSet) -> None:
    ach = next(a for a in ds.data["achievements.json"]["achievements"] if a["id"] == "juego_de_anzuelos")
    ach["condition"]["all"][0]["contains"] = "anzuelo_de_oro"
    r = Report()
    achievements.check_achievements(ds, r)
    assert any_error(r.errors, "anzuelo_de_oro", "artifact_ids_found")


def test_sin_cpp_avisa_y_no_revienta(real: DataSet, tmp_path: Path) -> None:
    ds = DataSet(real.copy().data, tmp_path)
    errors = content_errors(ds)
    assert any_error(errors, "PointsOfInterest")
    assert not any_error(errors, "Traceback")
