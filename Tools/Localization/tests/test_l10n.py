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
    assert [ns["Namespace"] for ns in doc["Subnamespaces"]] == ["Explored", "ExploredBuilding", "ExploredHarvest", "ExploredUI"]


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


# --------------------------------------------------------------------------- modificadores ICU (biblia 07 §5.3)

from l10n import data as l10n_data  # noqa: E402
from l10n import estilo, glosario, icu  # noqa: E402

PLURAL_ES = "{Count} {Count}|plural(one=pez,other=peces)"
PLURAL_EN = "{Count} {Count}|plural(one=fish,other=fish)"


def test_plural_valido_en_los_dos_idiomas() -> None:
    assert icu.check(PLURAL_ES, "es") == ([], [])
    assert icu.check(PLURAL_EN, "en") == ([], [])
    assert icu.compare(PLURAL_ES, PLURAL_EN) == []
    (mod,) = icu.parse(PLURAL_ES).modifiers
    assert (mod.argument, mod.name, mod.values) == ("Count", "plural", ["one=pez", "other=peces"])


def test_texto_real_del_hud_con_plural(real_cat: Catalogue) -> None:
    wounds = next(e for e in real_cat.entries if e.id == "ExploredUI,CondOpenWounds")
    assert icu.numeric_signature(wounds.es) == icu.numeric_signature(wounds.en) == {("Count", "plural")}


def test_plural_sin_other() -> None:
    errors, _ = icu.check("{N}|plural(one=pez)", "es")
    assert has(errors, "falta la categoría «other»")


def test_plural_sin_one_cae_en_other() -> None:
    errors, _ = icu.check("{N} {N}|plural(other=peces)", "es")
    assert has(errors, "falta one")


def test_categoria_desconocida_y_repetida() -> None:
    errors, _ = icu.check("{N}|plural(uno=pez,other=peces,other=otra)", "es")
    assert has(errors, "«uno» desconocida")
    assert has(errors, "«other» repetida")


def test_categoria_que_el_idioma_no_usa() -> None:
    # En inglés el 0 cae en «other»: «zero=» no se elige nunca.
    _, warnings = icu.check("{N}|plural(zero=no fish,one=fish,other=fish)", "en")
    assert has(warnings, "zero no existe en en")
    # El ordinal inglés necesita one/two/few (1st, 2nd, 3rd).
    errors, _ = icu.check("{N}{N}|ordinal(one=st,other=th)", "en")
    assert has(errors, "falta few, two")


def test_sintaxis_rota() -> None:
    assert has(icu.check("{N}|plural(one=pez,other=peces", "es")[0], "paréntesis sin cerrar")
    assert has(icu.check("{N}|plurals(one=a,other=b)", "es")[0], "modificador desconocido")
    assert has(icu.check("{N}|plural one=a", "es")[0], "sin paréntesis")
    assert has(icu.check('{N}|plural(one="pez,other=peces)', "es")[0], "comillas sin cerrar")
    assert has(icu.check("{N}|plural(pez)", "es")[0], "categoría=texto")
    assert has(icu.check("{N}|", "es")[0], "sin modificador")


def test_modificador_suelto_sin_marcador() -> None:
    errors, _ = icu.check("Tienes |plural(one=pez,other=peces)", "es")
    assert has(errors, "no va pegado a un marcador")


def test_comillas_comas_y_llaves_dentro_de_un_valor() -> None:
    text = '{N}|plural(one="uno, solo",other="{N} (varios)")'
    assert icu.check(text, "es") == ([], [])
    (mod,) = icu.parse(text).modifiers
    assert mod.values == ['one="uno, solo"', 'other="{N} (varios)"']


def test_escape_con_tilde_invertida() -> None:
    # `{ no abre marcador: no hay modificador que validar.
    assert icu.parse("`{N}|plural(roto").modifiers == []
    assert icu.parse("`{N}|plural(roto").errors == ["«|plural(» no va pegado a un marcador «{Arg}»; se vería tal cual en pantalla"]


def test_genero_y_hpp() -> None:
    assert icu.check("{Item}|gender(el,la)", "es") == ([], [])
    assert has(icu.check("{Item}|gender(el)", "es")[0], "dos o tres formas")
    assert has(icu.check("{Item}|gender(one=el,other=la)", "es")[0], "por posición")
    errors, warnings = icu.check("{Item}|hpp(은,는)", "en")
    assert errors == [] and has(warnings, "coreano")


def test_plural_solo_en_un_idioma(src: Sources) -> None:
    src.translations["Explored"]["Verb_PickUp"]["en"] = "Pick up {0}"
    src.translations["Explored"]["Verb_PickUp"]["es"] = "Coger {0}"
    src.loctexts.append(loc("ExploredUI", "Peces", "{Count} {Count}|plural(one=pez,other=peces)"))
    src.translations["ExploredUI"]["Peces"] = {"es": "{Count} {Count}|plural(one=pez,other=peces)", "en": "{Count} fish"}
    assert has(build(src).report.errors, "ExploredUI,Peces", "modificadores de plural distintos")


def test_el_genero_puede_ir_solo_en_espanol() -> None:
    assert icu.compare("{Item}|gender(el,la) {Item}", "the {Item}") == []


def test_plural_roto_en_datos_llega_al_informe(src: Sources) -> None:
    tpl = next(t for t in src.data["templates.json"] if t["id"] == "hacha")
    tpl["nameTemplate"] = "Hacha de {0} y {1}|plural(one=x)"
    tpl["nameTemplateEn"] = "{0} axe with a {1}|plural(one=x) handle"
    assert has(build(src).report.errors, "hacha.nameTemplate", "falta la categoría «other»")


# --------------------------------------------------------------------------- estilo anti-IA (biblia 07 §1.2)


def test_lista_negra_en_los_dos_idiomas() -> None:
    _, warnings = estilo.check("Data.items.items", "x.nameEs", "Un hacha épica", "An epic axe")
    assert has(warnings, "ES «épica»") and has(warnings, "EN «epic»")
    _, warnings = estilo.check("Data.items.items", "x.nameEs", "No es solo un refugio, es tu hogar", "Home")
    assert has(warnings, "no es X, es Y")
    _, warnings = estilo.check("Data.items.items", "x.nameEs", "Diez peces", "Ten fishes")
    assert has(warnings, "invariable")


def test_estilo_no_confunde_palabras_normales() -> None:
    # «Ready to harvest» es una etapa de cultivo, no una pregunta de gancho; «definitivamente» no es «definitivo».
    assert estilo.check("Data.plants.stages", "taro.listo.nameEs", "Listo", "Ready to harvest") == ([], [])
    assert estilo.check("Data.items.items", "x.nameEs", "Se rompe definitivamente", "Breaks for good") == ([], [])


def test_emoji_es_error() -> None:
    errors, _ = estilo.check("Data.items.items", "x.nameEs", "Coco \U0001F965", "Coconut")
    assert has(errors, "emoji")


def test_exclamacion_en_logros() -> None:
    _, warnings = estilo.check("Data.achievements.achievements", "a.descriptionEs", "¡Enciende un fuego!", "Light a fire")
    assert has(warnings, "ES lleva exclamación")
    # En la UI una exclamación puede marcar un peligro real de la ficción.
    assert estilo.check("ExploredUI", "Aviso", "¡Tiburón!", "Shark!") == ([], [])


def test_longitudes_de_logro() -> None:
    _, warnings = estilo.check("Data.achievements.achievements", "a.nameEs", "Uno dos tres cuatro cinco", "One Two.")
    assert has(warnings, "ES con 5 palabras") and has(warnings, "EN con puntuación final")
    long_es = "Cava " + "muy " * 30 + "hondo."
    _, warnings = estilo.check("Data.achievements.achievements", "a.descriptionEs", long_es, "Dig. Then dig again.")
    assert has(warnings, "ES con") and has(warnings, "EN con más de una frase")


def test_logros_reales_cumplen_la_guia(real_cat: Catalogue) -> None:
    achs = [e for e in real_cat.entries if e.namespace == "Data.achievements.achievements"]
    assert len(achs) == 2 * 54
    assert not [w for w in real_cat.report.warnings if "Data.achievements" in w]


def test_excepcion_de_estilo_y_de_longitud(src: Sources) -> None:
    del src.exceptions["estilo"]["ExploredUI,QualityEpic"]
    assert has(build(src).report.warnings, "ExploredUI,QualityEpic", "adjetivo vacío")
    src.exceptions["longitud"]["ExploredUI,NoExiste"] = "sobra"
    assert has(build(src).report.warnings, "ExploredUI,NoExiste", "ya no hace falta")
    src.exceptions["otra"] = {}
    assert has(build(src).report.errors, "sección «otra» desconocida")


# --------------------------------------------------------------------------- glosario (biblia 07 §5.2)


def test_glosario_tiene_las_filas_de_la_biblia(real: Sources) -> None:
    assert real.glossary_errors == []
    assert len(real.glossary) == 69
    assert {t.es for t in real.glossary} >= {"Isla del Humo", "La Meseta", "Galería (minera)", "Dedicatoria"}
    assert all(t.en_required for t in real.glossary if t.es_pattern is not None)


def test_glosario_detecta_traduccion_distinta(real: Sources) -> None:
    assert has(glosario.check(real.glossary, "Tubo de lava de la Isla del Humo", "Lava tube on Ash Island"), "Smoke Island")
    assert has(glosario.check(real.glossary, "Galería inundada", "Flooded gallery"), "tunnel")
    assert glosario.check(real.glossary, "Galería inundada", "Flooded tunnel") == []


def test_glosario_llega_al_informe(src: Sources) -> None:
    next(b for b in src.data["boats.json"]["boats"] if b["id"] == "barco_limon")["nameEn"] = "The Lemon"
    assert has(build(src).report.warnings, "barco_limon", "«Limón»", "glosario")


def test_glosario_mal_formado() -> None:
    text = "| Español | Inglés | Nota | Comprobación |\n|---|---|---|---|\n| A | B | — | `(` → `x` |\n| C | D | — |\n"
    terms, errors = glosario.parse(text)
    assert [t.es for t in terms] == ["A"]
    assert has(errors, "no válida") and has(errors, "3 columnas")


# --------------------------------------------------------------------------- cobertura de Content/Data


def test_todos_los_ficheros_de_datos_se_leen(real: Sources) -> None:
    import pathlib

    names = {p.name for p in (real.repo_root / "Content" / "Data").glob("*.json")}
    assert set(real.data) == names


def test_texto_es_sin_registrar_es_error() -> None:
    texts, errors = l10n_data.extract({"nuevo.json": {"cosas": [{"id": "a", "nameEs": "Cosa", "noteEs": "nota"}]}})
    assert texts == []
    assert has(errors, "nuevo.json", ".cosas[].nameEs «a»", "no está registrado")
    assert not has(errors, "noteEs")


def test_registrados_nuevos_en_el_catalogo(real_cat: Catalogue) -> None:
    ids = {e.id for e in real_cat.entries}
    assert "Data.fish.species,pez_loro.nameEs" in ids
    assert "Data.achievements.achievements,banquete_de_mil_cocos.descriptionEs" in ids
    assert "Data.fases_futuras.trade.wants,medicina.nameEs" in ids
    assert "Data.exploration.landmarks,landing_laguna_amaraje.nameEs" in ids


def test_mismo_nombre_dos_traducciones(src: Sources) -> None:
    next(f for f in src.data["fish.json"]["species"] if f["id"] == "atun")["nameEn"] = "Yellowfin tuna"
    assert has(build(src).report.warnings, "«atún» tiene 2 traducciones distintas")
