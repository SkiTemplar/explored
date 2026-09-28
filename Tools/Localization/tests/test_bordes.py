"""Casos límite sin tocar el repo: C++ raro, JSON corrupto, exportación determinista y la CLI."""

from __future__ import annotations

import json
import random
from pathlib import Path
from typing import Any

import pytest

from l10n import catalogue, cli, cpp, data, report, unreal
from l10n.catalogue import Catalogue, Entry, Sources, build


def has(msgs: list[str], *needles: str) -> bool:
    return any(all(n in m for n in needles) for m in msgs)


def fuentes(translations: dict[str, Any] | None = None, datos: dict[str, Any] | None = None,
            loctexts: list[cpp.LocText] | None = None) -> Sources:
    return Sources(loctexts or [], [], datos or {}, translations or {}, Path("."))


def loc(ns: str, key: str, text: str, line: int = 1) -> cpp.LocText:
    return cpp.LocText(ns, key, text, "Source/Explored/X.cpp", line)


# --------------------------------------------------------------------------- C++


def test_loctext_con_comillas_y_barras_escapadas() -> None:
    code = r'''#define LOCTEXT_NAMESPACE "Menu"
A = LOCTEXT("Ruta", "Di \"hola\" en C:\\juego\\");
'''
    (t,) = cpp.extract_loctext(code, "X.cpp")
    assert t.source == 'Di "hola" en C:\\juego\\'


def test_macro_multilinea_con_comentarios_entre_argumentos() -> None:
    code = '''
X = NSLOCTEXT(
	"ExploredUI",  // espacio
	"Clave",       /* clave */
	"Texto largo");
'''
    (t,) = cpp.extract_loctext(code, "X.cpp")
    assert (t.namespace, t.key, t.source, t.line) == ("ExploredUI", "Clave", "Texto largo", 2)


def test_literales_concatenados_no_desaparecen_en_silencio() -> None:
    # Antes esta macro no encajaba en la expresión y el texto se perdía del catálogo sin aviso.
    code = 'X = NSLOCTEXT("ExploredUI", "K", "Hola " "mundo");'
    with pytest.raises(ValueError, match=r"X\.cpp:1: no se puede leer"):
        cpp.extract_loctext(code, "X.cpp")


def test_macro_dentro_de_una_cadena_no_cuenta() -> None:
    code = 'Log(TEXT("usa NSLOCTEXT(ns, k, v) o LOCTEXT(k, v)")); X = NSLOCTEXT("A", "K", "v");'
    assert [t.key for t in cpp.extract_loctext(code, "X.cpp")] == ["K"]


def test_doble_barra_dentro_de_cadena_no_es_comentario() -> None:
    code = 'U = TEXT("http://x"); X = NSLOCTEXT("A", "K", "v"); // NSLOCTEXT("A", "Z", "no")'
    assert [t.key for t in cpp.extract_loctext(code, "X.cpp")] == ["K"]


def test_espacio_de_nombres_cambia_con_define_y_undef() -> None:
    code = '''#define LOCTEXT_NAMESPACE "Uno"
A = LOCTEXT("A", "a");
#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "Dos"
B = LOCTEXT("B", "b");
#undef LOCTEXT_NAMESPACE
'''
    assert [(t.namespace, t.key) for t in cpp.extract_loctext(code, "X.cpp")] == [("Uno", "A"), ("Dos", "B")]
    with pytest.raises(ValueError, match=r"X\.cpp:7: LOCTEXT sin LOCTEXT_NAMESPACE"):
        cpp.extract_loctext(code + 'C = LOCTEXT("C", "c");', "X.cpp")


@pytest.mark.parametrize(("raw", "esperado"), [
    (r"\x41\x42", "AB"),  # antes quedaba «\x41\x42» literal y no coincidía con en.json
    (r"Lim\u00f3n", "Limón"),
    (r"\U0001F34B", "\U0001f34b"),
    (r"\uZZZZ", r"\uZZZZ"),  # escape inválido: se deja tal cual en vez de lanzar ValueError
    (r"a\n\t\"\\", 'a\n\t"\\'),
    ("fin\\", "fin\\"),
])
def test_unescape(raw: str, esperado: str) -> None:
    assert cpp.unescape(raw) == esperado


def test_literales_comentados_o_de_registro_no_cuentan() -> None:
    code = '''
void F()
{
	// Label->SetText(FText::FromString(TEXT("Comentado")));
	checkf(false, TEXT("Error interno %s"), *X);
	Title = FText::FromString(TEXT("Mapa"));
}
'''
    assert [(lit.text, lit.category) for lit in cpp.scan_literals(code, "X.cpp")] == [("Mapa", "literal")]


def test_iter_sources_filtra_extension_y_exclusiones(tmp_path: Path) -> None:
    for rel in ("Source/Explored/A.cpp", "Source/Explored/A.txt", "Source/Explored/Tests/T.cpp"):
        (tmp_path / rel).parent.mkdir(parents=True, exist_ok=True)
        (tmp_path / rel).write_text("", encoding="utf-8")
    found = [rel for _, rel in cpp.iter_sources(tmp_path, ["Source/Explored", "NoExiste"], (".cpp",),
                                                 ("Source/Explored/Tests/",))]
    assert found == ["Source/Explored/A.cpp"]


# --------------------------------------------------------------------------- JSON corrupto


def test_repo_real_sin_claves_repetidas() -> None:
    # translations/en.json tenía «ExploredHarvest» dos veces: json se quedaba con la última sin avisar.
    assert Sources.load().problems == []


def test_clave_repetida_en_translations_es_error(tmp_path: Path) -> None:
    path = tmp_path / "en.json"
    path.write_text('{"UI": {"K": {"es": "a", "en": "b"}}, "UI": {"J": {"es": "c", "en": "d"}}}', encoding="utf-8")
    src = Sources.load(tmp_path, path)
    assert has(build(src).report.errors, "translations/en.json", "«UI» repetida")


def test_json_mal_formado_da_error_legible(tmp_path: Path) -> None:
    path = tmp_path / "en.json"
    path.write_text('{"UI": {"K": {"es": "a", "en": "b"},}}', encoding="utf-8")
    with pytest.raises(ValueError, match="translations/en.json: JSON mal formado"):
        Sources.load(tmp_path, path)


def test_raiz_de_translations_no_objeto(tmp_path: Path) -> None:
    path = tmp_path / "en.json"
    path.write_text("[]", encoding="utf-8")
    with pytest.raises(ValueError, match="la raíz debe ser un objeto"):
        Sources.load(tmp_path, path)


@pytest.mark.parametrize(("translations", "needle"), [
    ({"UI": ["K"]}, "UI debe ser un objeto"),
    ({"UI": {"K": "texto"}}, "UI,K debe ser un objeto"),
    ({"UI": {"K": {"es": "a", "en": 5}}}, "UI,K: en debe ser texto"),
    ({"UI": {"K": {"es": "a", "en": "b", "pendiente": True}}}, "UI,K: pendiente debe ser texto"),
])
def test_traducciones_corruptas_son_errores_y_no_trazas(translations: dict[str, Any], needle: str) -> None:
    cat = build(fuentes(translations, loctexts=[loc("UI", "K", "a")]))
    assert has(cat.report.errors, "translations/en.json", needle)
    # La clave del código sigue en el catálogo, sin inglés.
    assert has(cat.report.errors, "UI,K", "sin inglés")


def test_texto_de_datos_que_no_es_cadena() -> None:
    cat = build(fuentes(datos={"items.json": [{"id": "a", "nameEs": "Hacha", "nameEn": 5}]}))
    assert has(cat.report.errors, "Data.items.items,a.nameEs", "«en» debe ser una cadena, no int")


def test_registro_de_datos_que_no_es_objeto() -> None:
    texts, errors = data.extract({"items.json": ["rama"], "plants.json": {"plants": [7, {"id": "p", "stages": ["x"]}]}})
    assert texts == []
    assert has(errors, "items.json", "items #0 no es un objeto")
    assert has(errors, "plants.json", "plants #0 no es un objeto")
    assert has(errors, "plants.json", "stages p.#0 no es un objeto")


def test_listas_paralelas_que_no_son_listas() -> None:
    _, errors = data.extract({"story_es.json": {"petroglyph_themes": ["Sol"], "petroglyph_themes_en": "Sun"}})
    assert has(errors, "petroglyph_themes_en deben ser listas")


def test_lista_inglesa_ausente_deja_textos_sin_ingles() -> None:
    cat = build(fuentes(datos={"story_es.json": {"petroglyph_themes": ["Sol"]}}))
    assert has(cat.report.errors, "Data.story_es.petroglyph_themes,00", "sin inglés")


def test_id_de_datos_repetido() -> None:
    items = [{"id": "a", "nameEs": "Uno", "nameEn": "One"}, {"id": "a", "nameEs": "Dos", "nameEn": "Two"}]
    cat = build(fuentes(datos={"items.json": items}))
    assert has(cat.report.errors, "id repetido Data.items.items,a.nameEs")
    assert [e.en for e in cat.entries] == ["One"]


# --------------------------------------------------------------------------- comprobaciones


def test_texto_vacio_en_el_codigo() -> None:
    cat = build(fuentes({"UI": {"K": {"es": "", "en": "x"}}}, loctexts=[loc("UI", "K", "")]))
    assert has(cat.report.errors, "UI,K sin texto en español")


def test_misma_clave_y_texto_en_dos_sitios_no_es_conflicto() -> None:
    cat = build(fuentes({"UI": {"K": {"es": "Hola", "en": "Hi"}}},
                        loctexts=[loc("UI", "K", "Hola", 1), loc("UI", "K", "Hola", 9)]))
    assert cat.report.errors == []
    (e,) = cat.entries
    assert e.locations == ["Source/Explored/X.cpp:1", "Source/Explored/X.cpp:9"]


def test_espacios_en_los_bordes_distintos() -> None:
    cat = build(fuentes({"UI": {"K": {"es": "Vida: ", "en": "Health:"}}}, loctexts=[loc("UI", "K", "Vida: ")]))
    assert has(cat.report.warnings, "UI,K", "espacios")
    cat = build(fuentes({"UI": {"K": {"es": "Vida: ", "en": "Health: "}}}, loctexts=[loc("UI", "K", "Vida: ")]))
    assert cat.report.warnings == []


def test_pendientes_en_el_catalogo_y_en_el_recuento() -> None:
    cat = build(fuentes({"UI": {"Nueva": {"es": "Nueva", "en": "New", "pendiente": "P-UI9", "nota": "botón"}}}))
    (e,) = cat.entries
    assert (e.origin, e.locations, e.note) == ("pendiente", ["P-UI9"], "botón")
    assert e.to_json()["note"] == "botón"
    assert cat.exported == []
    assert cat.stats()["pendientes"] == 1 and cat.stats()["total"] == 0


# --------------------------------------------------------------------------- Unreal y determinismo


def entrada(ns: str, key: str, es: str | None, en: str | None, line: int = 1) -> Entry:
    return Entry(ns, key, es, en, "cpp", [f"Source/Explored/X.cpp:{line}"])


def test_exportar_sin_texto_fuente_falla_con_la_clave() -> None:
    with pytest.raises(ValueError, match="UI,K: sin texto fuente"):
        unreal.manifest([entrada("UI", "K", None, "x"), entrada("UI", "J", "a", "b")])
    with pytest.raises(ValueError, match="UI,K: sin texto fuente"):
        unreal.archive([entrada("UI", "K", None, "x")], "en")


def test_espacio_de_nombres_vacio_va_a_la_raiz() -> None:
    doc = unreal.archive([entrada("", "K", "Hola", "Hi")], "en")
    assert doc["Subnamespaces"] == []
    assert doc["Children"] == [{"Source": {"Text": "Hola"}, "Translation": {"Text": "Hi"}, "Key": "K"}]


def test_exportacion_no_depende_del_orden_de_entrada() -> None:
    items = [entrada(ns, f"K{i}", f"Texto {i % 3}", f"Text {i % 3}", i + 1) for i, ns in
             enumerate(["UI", "Explored", "UI", "Building", "Explored", "UI"])]
    base = unreal.outputs(items)
    for seed in range(5):
        barajado = items[:]
        random.Random(seed).shuffle(barajado)
        assert unreal.outputs(barajado) == base


def test_catalogo_json_no_depende_del_orden_de_translations() -> None:
    trs = {"UI": {"B": {"es": "b", "en": "B"}, "A": {"es": "a", "en": "A"}},
           "Otro": {"C": {"es": "c", "en": "C", "pendiente": "x"}}}
    lts = [loc("UI", "B", "b", 2), loc("UI", "A", "a", 1)]
    al_reves = {ns: dict(reversed(list(keys.items()))) for ns, keys in reversed(list(trs.items()))}
    a, b = build(fuentes(trs, loctexts=lts)), build(fuentes(al_reves, loctexts=lts))
    assert report.catalogue_json(a) == report.catalogue_json(b)
    assert report.markdown(a) == report.markdown(b)


def test_informe_escapa_barras_y_saltos() -> None:
    lit = cpp.Literal("X.cpp", 3, "a|b\nc", "literal", "Draw(TEXT(\"a|b\"))")
    cat = Catalogue([Entry("UI", "P", "Sí|No", "Yes|No", "pendiente", ["P-1"])], [lit], catalogue.Report())
    md = report.markdown(cat)
    assert "`a\\|b\\nc`" in md
    assert "| `UI,P` | Sí\\|No | Yes\\|No | P-1 |" in md
    assert "### Literal (1)" in md


# --------------------------------------------------------------------------- CLI


CPP_OK = '''#define LOCTEXT_NAMESPACE "ExploredUI"
const FText A = LOCTEXT("Hola", "Hola");
const FText B = NSLOCTEXT("Explored", "Adios", "Adiós");
void F() { Label->SetText(FText::FromString(TEXT("Pequeño texto"))); Size = TEXT("Revisa esto"); }
#undef LOCTEXT_NAMESPACE
'''
EN_OK: dict[str, Any] = {"ExploredUI": {"Hola": {"es": "Hola", "en": "Hello"}},
                         "Explored": {"Adios": {"es": "Adiós", "en": "Bye"}}}


@pytest.fixture
def repo(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """Repo mínimo en tmp_path; la CLI lee y escribe solo ahí."""
    (tmp_path / "Source/Explored").mkdir(parents=True)
    (tmp_path / "Source/Explored/X.cpp").write_text(CPP_OK, encoding="utf-8")
    (tmp_path / "Content/Data").mkdir(parents=True)
    (tmp_path / "Content/Data/items.json").write_text(
        json.dumps([{"id": "limon", "nameEs": "Limón", "nameEn": "Lemon"}]), encoding="utf-8")
    escribir_en(tmp_path, EN_OK)
    monkeypatch.setattr(cli, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(catalogue, "TRANSLATIONS", tmp_path / "en.json")
    return tmp_path


def escribir_en(root: Path, doc: dict[str, Any]) -> None:
    (root / "en.json").write_text(json.dumps(doc, ensure_ascii=False), encoding="utf-8")


def test_cli_check_limpio(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    assert cli.main([]) == 0  # el literal «Pequeño texto» es solo un aviso
    out = capsys.readouterr().out
    assert "ERROR" not in out
    assert "literal sin localizar «Pequeño texto»" in out
    assert "REVISAR" in out and "Revisa esto" in out
    assert "3 textos (2 C++/ini, 1 datos)" in out


def test_cli_strict_falla_con_avisos(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    assert cli.main(["--strict", "--quiet"]) == 1  # el literal sin localizar es un aviso
    code = CPP_OK.replace('Label->SetText(FText::FromString(TEXT("Pequeño texto")));', "")
    (repo / "Source/Explored/X.cpp").write_text(code, encoding="utf-8")
    assert cli.main(["check", "--strict"]) == 0  # «Revisa esto» es para revisar, no un aviso
    escribir_en(repo, {**EN_OK, "Viejo": {"K": {"es": "x", "en": "y"}}})
    assert cli.main(["check"]) == 0
    assert cli.main(["--strict", "--quiet"]) == 1
    assert "0 errores, 1 avisos" in capsys.readouterr().out


def test_cli_error_sale_uno(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    escribir_en(repo, {"ExploredUI": EN_OK["ExploredUI"]})
    assert cli.main(["--quiet"]) == 1
    assert "ERROR  Source/Explored/X.cpp:3: Explored,Adios sin inglés («Adiós»)" in capsys.readouterr().out


def test_cli_json_mal_formado_sale_dos(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    (repo / "en.json").write_text("{", encoding="utf-8")
    assert cli.main(["export", "--check"]) == 2
    assert "ERROR  translations/en.json: JSON mal formado" in capsys.readouterr().out


def test_cli_export_y_check(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    code = CPP_OK.replace('Label->SetText(FText::FromString(TEXT("Pequeño texto")));', "")
    (repo / "Source/Explored/X.cpp").write_text(code, encoding="utf-8")
    assert cli.main(["export", "--check", "--quiet"]) == 1  # aún no hay nada generado
    assert "DESFASADO  Content/Localization/Game/Game.manifest" in capsys.readouterr().out

    assert cli.main(["export", "--quiet"]) == 0
    manifest = json.loads((repo / "Content/Localization/Game/Game.manifest").read_text(encoding="utf-8"))
    assert [ns["Namespace"] for ns in manifest["Subnamespaces"]] == ["Explored", "ExploredUI"]
    en = json.loads((repo / "Content/Localization/Game/en/Game.archive").read_text(encoding="utf-8"))
    assert en["Subnamespaces"][1]["Children"][0]["Translation"] == {"Text": "Hello"}
    assert (repo / report.REPORT_PATH).exists() and (repo / report.CATALOGUE_PATH).exists()
    assert cli.main(["export", "--check", "--quiet"]) == 0

    # Mover el código sin cambiar textos no desfasa; cambiar un texto sí.
    (repo / "Source/Explored/X.cpp").write_text("\n\n" + code, encoding="utf-8")
    assert cli.main(["export", "--check", "--quiet"]) == 0
    escribir_en(repo, {**EN_OK, "ExploredUI": {"Hola": {"es": "Hola", "en": "Hi"}}})
    capsys.readouterr()
    assert cli.main(["export", "--check", "--quiet"]) == 1
    out = capsys.readouterr().out
    assert "DESFASADO  Content/Localization/Game/en/Game.archive" in out
    assert "Game.manifest" not in out


def test_cli_export_utf16_sigue_al_dia(repo: Path) -> None:
    assert cli.main(["export", "--utf16", "--quiet"]) == 0
    raw = (repo / "Content/Localization/Game/es/Game.archive").read_bytes()
    assert raw.startswith(b"\xff\xfe")
    assert json.loads(raw.decode("utf-16"))["FormatVersion"] == 2
    assert cli.stale_files(build(Sources.load(repo)), repo) == []


def test_exportacion_real_es_estable() -> None:
    # Dos construcciones seguidas del repo real producen exactamente los mismos bytes.
    a = cli.generated_files(build(Sources.load()))
    b = cli.generated_files(build(Sources.load()))
    assert a == b
    assert not any(p.startswith("/") or ".." in p for p in a)
