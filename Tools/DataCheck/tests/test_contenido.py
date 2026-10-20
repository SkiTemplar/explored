"""Contenido de texto de H4: los datos reales pasan y cada regresión típica se detecta."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from datacheck import contenido, estilo
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


def test_sin_cpp_avisa_y_no_revienta(real: DataSet, tmp_path: Path) -> None:
    ds = DataSet(real.copy().data, tmp_path)
    errors = content_errors(ds)
    assert any_error(errors, "PointsOfInterest")
    assert not any_error(errors, "Traceback")
