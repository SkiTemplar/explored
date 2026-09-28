"""Tests de Tools/Localization: el repo real está completo y cada regresión típica se detecta."""

from __future__ import annotations

import json

import pytest

from l10n import cli, cpp, unreal
from l10n.catalogue import DEDICATION, Catalogue, Sources, build


@pytest.fixture(scope="module")
def real() -> Sources:
    return Sources.load()


@pytest.fixture(scope="module")
def real_cat(real: Sources) -> Catalogue:
    return build(real)


@pytest.fixture
def src(real: Sources) -> Sources:
    return real.copy()


def has(msgs: list[str], *needles: str) -> bool:
    return any(all(n in m for n in needles) for m in msgs)


# --------------------------------------------------------------------------- repo real


def test_repo_sin_errores(real_cat: Catalogue) -> None:
    assert real_cat.report.errors == []


def test_todo_texto_tiene_ingles(real_cat: Catalogue) -> None:
    assert real_cat.stats()["sin_en"] == 0
    assert all((e.en or "").strip() for e in real_cat.entries)


def test_catalogo_une_cpp_ini_y_datos(real_cat: Catalogue) -> None:
    origins = {e.origin for e in real_cat.entries}
    assert {"cpp", "ini", "datos"} <= origins
    ids = {e.id for e in real_cat.entries}
    assert "ExploredUI,NewGame" in ids
    assert "Explored,Title" in ids  # ProjectDisplayedTitle de DefaultGame.ini
    assert "Data.items.items,limon.nameEs" in ids
    assert "Data.story_es.petroglyph_themes,29" in ids


def test_dedicatoria_igual_en_los_dos_idiomas(real_cat: Catalogue) -> None:
    ded = [e for e in real_cat.entries if DEDICATION in (e.es or "")]
    assert ded, "la dedicatoria debe estar en el catálogo (GDD §15)"
    assert all(e.en == e.es == DEDICATION for e in ded)


def test_ficheros_generados_al_dia(real_cat: Catalogue) -> None:
    assert cli.stale_files(real_cat) == [], "ejecuta: cd Tools/Localization && uv run l10n export"


def test_manifiesto_real_solo_con_espacios_del_codigo(real_cat: Catalogue) -> None:
    doc = unreal.manifest(real_cat.exported)
    assert [ns["Namespace"] for ns in doc["Subnamespaces"]] == ["Explored", "ExploredBuilding", "ExploredClimb", "ExploredHarvest", "ExploredUI"]


# --------------------------------------------------------------------------- C++


CODE = r'''
#define LOCTEXT_NAMESPACE "Menu"
const FText A = NSLOCTEXT("ExploredUI", "Hola", "Hola, \"mundo\"");
const FText B = LOCTEXT("Salir",
	"Salir del juego");
// const FText C = NSLOCTEXT("ExploredUI", "Comentado", "No cuenta");
/* NSLOCTEXT("ExploredUI", "Bloque", "Tampoco") */
const FString Url = TEXT("http://no-es-comentario");
#undef LOCTEXT_NAMESPACE
'''


def test_extrae_nsloctext_y_loctext() -> None:
    texts = cpp.extract_loctext(CODE, "X.cpp")
    assert [(t.namespace, t.key, t.source, t.line) for t in texts] == [
        ("ExploredUI", "Hola", 'Hola, "mundo"', 3),
        ("Menu", "Salir", "Salir del juego", 4),
    ]


def test_loctext_sin_espacio_de_nombres_falla() -> None:
    with pytest.raises(ValueError, match="LOCTEXT_NAMESPACE"):
        cpp.extract_loctext('FText T = LOCTEXT("K", "Texto");', "X.cpp")


LITERALS = """
void F()
{
	Label->SetText(FText::FromString(TEXT("Hola")));
	UE_LOG(LogTemp, Display, TEXT("[Explored] No es de UI %s"),
		TEXT("tampoco la continuación"));
	Row.Text(FText::FromString(TEXT("60")));
	const TCHAR* Size = TEXT("Pequeño");
	const FName Id = TEXT("IA_Jump");
	DrawText(FText::FromString(TEXT("Oculto")));  // loc: ignorar
	Out = FText::FromString(FString::Printf(TEXT("%d x %d"), W, H));
}
"""


def test_clasifica_literales() -> None:
    found = {(lit.text, lit.category) for lit in cpp.scan_literals(LITERALS, "X.cpp")}
    assert found == {("Hola", "literal"), ("60", "invariante"), ("Pequeño", "revisar"), ("%d x %d", "invariante")}


def test_literales_reales_documentados(real_cat: Catalogue) -> None:
    lits = {(lit.path.rsplit("/", 1)[-1], lit.text) for lit in real_cat.literals if lit.category == "literal"}
    # Si esto cambia, actualiza la lista de docs/tecnico/localizacion.md.
    assert lits <= {("ExploredHUD.cpp", "[E] "), ("SExploredSettingsPanel.cpp", "%d min")}


# --------------------------------------------------------------------------- comprobaciones


def loc(ns: str, key: str, text: str) -> cpp.LocText:
    return cpp.LocText(ns, key, text, "Source/Explored/X.cpp", 1)


def test_falta_ingles_en_cpp(src: Sources) -> None:
    src.loctexts.append(loc("ExploredUI", "Nuevo", "Texto nuevo"))
    assert has(build(src).report.errors, "ExploredUI,Nuevo", "sin inglés")


def test_falta_ingles_en_datos(src: Sources) -> None:
    del src.data["items.json"][0]["nameEn"]
    assert has(build(src).report.errors, "rama_seca", "sin inglés")
    del src.data["verbs.json"][0]["descriptionEn"]
    assert has(build(src).report.errors, "Golpear.description", "sin inglés")


def test_valor_vacio(src: Sources) -> None:
    src.data["plants.json"]["plants"][0]["stages"][0]["nameEn"] = "   "
    assert has(build(src).report.errors, "limonero.esqueje", "sin inglés")


def test_marcadores_distintos(src: Sources) -> None:
    tpl = next(t for t in src.data["templates.json"] if t["id"] == "hacha")
    tpl["nameTemplateEn"] = "{0} axe with a {1} handle"
    assert has(build(src).report.errors, "hacha.nameTemplate", "marcadores distintos")
    src.translations["Explored"]["Verb_PickUp"]["en"] = "Pick up {Item}"
    assert has(build(src).report.errors, "Verb_PickUp", "marcadores")


def test_traduccion_desfasada(src: Sources) -> None:
    src.translations["ExploredUI"]["Back"]["es"] = "Atrás"
    assert has(build(src).report.errors, "ExploredUI,Back", "desfasada")


def test_conflicto_de_clave(src: Sources) -> None:
    src.loctexts.append(loc("ExploredUI", "Back", "Regresar"))
    assert has(build(src).report.errors, "ExploredUI,Back", "conflicto")


def test_dedicatoria_no_se_traduce(src: Sources) -> None:
    src.translations["ExploredUI"]["Dedication"]["en"] = "For Almudena, my Lemon"
    assert has(build(src).report.errors, "dedicatoria")


def test_aviso_de_longitud(src: Sources) -> None:
    src.translations["ExploredUI"]["Apply"]["en"] = "Apply all of these changes"
    assert has(build(src).report.warnings, "ExploredUI,Apply", "comprueba que cabe")


def test_traduccion_huerfana(src: Sources) -> None:
    src.translations["ExploredUI"]["Borrada"] = {"es": "Ya no existe", "en": "Gone"}
    assert has(build(src).report.warnings, "ExploredUI,Borrada", "no aparece en el código")


def test_pendiente_ya_integrada(src: Sources) -> None:
    # La dedicatoria ya está en el código (P-UI2): se simula que en.json aún la marca como pendiente.
    src.translations["ExploredUI"]["Dedication"]["pendiente"] = "SExploredMainMenu y SExploredCredits"
    src.loctexts.append(loc("ExploredUI", "Dedication", DEDICATION))
    cat = build(src)
    assert has(cat.report.warnings, "ExploredUI,Dedication", "quita «pendiente»")
    assert any(e.id == "ExploredUI,Dedication" for e in cat.exported)


def test_listas_paralelas_desparejas(src: Sources) -> None:
    src.data["story_es.json"]["petroglyph_themes_en"].pop()
    assert has(build(src).report.errors, "petroglyph_themes_en", "29")


def test_plantilla_sin_nombre_no_se_exige() -> None:
    from l10n import data

    texts, errors = data.extract({"templates.json": [{"id": "afilar", "nameEs": "Afilar", "nameEn": "Sharpen", "nameTemplate": ""}]})
    assert errors == []
    assert [t.key for t in texts] == ["afilar.nameEs"]


# --------------------------------------------------------------------------- Unreal


def entries(*items: tuple[str, str, str, str]) -> list:
    from l10n.catalogue import Entry

    return [Entry(ns, key, es, en, "cpp", [f"Source/Explored/X.cpp:{i + 1}"]) for i, (ns, key, es, en) in enumerate(items)]


def test_manifiesto_agrupa_claves_con_el_mismo_texto() -> None:
    doc = unreal.manifest(entries(("UI", "Effects", "Efectos", "Effects"), ("UI", "VolEffects", "Efectos", "Effects"),
                                  ("UI", "Back", "Volver", "Back")))
    assert doc["FormatVersion"] == 1 and doc["Namespace"] == "" and doc["Children"] == []
    (ui,) = doc["Subnamespaces"]
    assert ui["Namespace"] == "UI"
    efectos = next(c for c in ui["Children"] if c["Source"]["Text"] == "Efectos")
    assert efectos["Keys"] == [{"Key": "Effects", "Path": "Source/Explored/X.cpp - line 1"},
                               {"Key": "VolEffects", "Path": "Source/Explored/X.cpp - line 2"}]


def test_archivos_por_cultura() -> None:
    es_ = unreal.archive(entries(("UI", "Back", "Volver", "Back")), "es")
    en_ = unreal.archive(entries(("UI", "Back", "Volver", "Back")), "en")
    assert es_["FormatVersion"] == 2
    assert es_["Subnamespaces"][0]["Children"] == [{"Source": {"Text": "Volver"}, "Translation": {"Text": "Volver"}, "Key": "Back"}]
    assert en_["Subnamespaces"][0]["Children"][0]["Translation"] == {"Text": "Back"}


def test_espacio_de_nombres_sin_puntos() -> None:
    with pytest.raises(ValueError, match="puntos"):
        unreal.manifest(entries(("Data.items", "k", "a", "b")))


def test_salidas_y_codificacion() -> None:
    files = unreal.outputs(entries(("UI", "Back", "Volver", "Back")))
    assert sorted(files) == ["Content/Localization/Game/Game.manifest",
                             "Content/Localization/Game/en/Game.archive",
                             "Content/Localization/Game/es/Game.archive"]
    raw = unreal.encode(files["Content/Localization/Game/Game.manifest"], utf16=True)
    assert raw.startswith(b"\xff\xfe")
    assert json.loads(raw.decode("utf-16"))["FormatVersion"] == 1


def test_check_ignora_numeros_de_linea() -> None:
    a = b'"Path": "Source/Explored/X.cpp - line 10"\n| `Source/Explored/X.cpp:10` |'
    b = b'"Path": "Source/Explored/X.cpp - line 12"\n| `Source/Explored/X.cpp:12` |'
    assert cli._normalized(a) == cli._normalized(b)
