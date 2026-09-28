"""Reglas de logros, fuego y cocina con catálogos sintéticos (sin el catálogo real: rápidos)."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import pytest

from datacheck import achievements, cooking
from datacheck.checks import DataSet, Report


def _has(errors: list[str], *needles: str) -> bool:
    return any(all(n in e for n in needles) for e in errors)


# --------------------------------------------------------------------------- logros: condiciones


STATS: dict[str, dict[str, Any]] = {
    "peces": {"kind": "counter"},
    "fuego": {"kind": "flag"},
    "barcos": {"kind": "set", "values": ["balsa", "limon"]},
    "plantas": {"kind": "set", "valuesFrom": "plants"},
    "abierto": {"kind": "set"},
}


def _ds(tmp_path: Path) -> DataSet:
    data: dict[str, Any] = {
        "items.json": [{"id": "coco"}],
        "plants.json": {"plants": [{"id": "taro"}, {"id": "limonero"}]},
        "building_pieces.json": {"tiers": [], "pieces": [{"id": "fogata"}]},
    }
    return DataSet(data, tmp_path)


def _cond_errors(tmp_path: Path, cond: Any) -> list[str]:
    r = Report()
    achievements.check_condition(_ds(tmp_path), cond, STATS, "logro", r)
    return r.errors


@pytest.mark.parametrize(("cond", "needle"), [
    ("fuego", "debe ser un objeto"),
    ({"stat": "nada", "op": ">=", "value": 1}, "estadística desconocida «nada»"),
    ({"stat": "fuego", "op": ">=", "value": 1}, "es una marca"),
    ({"stat": "peces", "op": "=>", "value": 1}, "operador «=>»"),
    ({"stat": "peces", "op": ">=", "value": float("inf")}, "número finito"),
    ({"stat": "peces", "op": ">=", "value": True}, "número finito"),
    ({"stat": "barcos", "op": ">=", "value": 3}, "solo admite 2 ids"),
    ({"stat": "plantas", "op": ">", "value": 5}, "solo admite 2 ids"),
    ({"stat": "nada", "contains": "x"}, "estadística desconocida"),
    ({"stat": "peces", "contains": "x"}, "no es un conjunto"),
    ({"stat": "barcos", "contains": "canoa"}, "«canoa» no es un id admitido"),
    ({"flag": "nada"}, "estadística desconocida"),
    ({"flag": "peces"}, "no es una marca"),
    ({"all": []}, "«all» necesita una lista no vacía"),
    ({"any": "fuego"}, "«any» necesita una lista no vacía"),
    ({"not": {"flag": "peces"}}, "no es una marca"),
    ({"all": [{"flag": "fuego"}, {"stat": "nada", "op": ">=", "value": 1}]}, "desconocida «nada»"),
    ({"stat": "peces", "value": 1}, "no reconocida"),
])
def test_condicion_invalida(tmp_path: Path, cond: Any, needle: str) -> None:
    assert _has(_cond_errors(tmp_path, cond), needle)


@pytest.mark.parametrize("cond", [
    {"stat": "peces", "op": ">=", "value": 10},
    {"stat": "barcos", "op": ">=", "value": 2},
    {"stat": "abierto", "op": ">=", "value": 99},
    {"stat": "barcos", "contains": "limon"},
    {"stat": "abierto", "contains": "cualquiera"},
    {"any": [{"flag": "fuego"}, {"not": {"flag": "fuego"}}]},
])
def test_condicion_valida(tmp_path: Path, cond: Any) -> None:
    assert _cond_errors(tmp_path, cond) == []


def test_valores_admitidos_por_origen(tmp_path: Path) -> None:
    ds = _ds(tmp_path)
    assert achievements.allowed_values(ds, {"valuesFrom": "items"}) == {"coco"}
    assert achievements.allowed_values(ds, {"valuesFrom": "plants"}) == {"taro", "limonero"}
    assert achievements.allowed_values(ds, {"valuesFrom": "building_pieces"}) == {"fogata"}
    assert achievements.allowed_values(ds, {"kind": "set"}) is None


def test_estadisticas_usadas_recorre_toda_la_condicion() -> None:
    used: set[str] = set()
    achievements._stats_used({"all": [{"stat": "a"}, {"any": [{"flag": "b"}]}, {"not": {"stat": "c"}}],
                              "any": "no es lista"}, used)
    achievements._stats_used("texto", used)
    assert used == {"a", "b", "c"}


def test_catalogo_de_estadisticas_mal_formado(tmp_path: Path) -> None:
    doc = {
        "modes": ["Explorer"],
        "stats": [
            {"id": "Mal-Id"},
            {"id": "peces", "kind": "contador", "scope": "mundo"},
            {"id": "peces", "kind": "counter", "scope": "run", "descriptionEs": "x", "values": ["a"]},
            {"id": "lista", "kind": "set", "scope": "run", "descriptionEs": "x", "values": ["a", "a"],
             "valuesFrom": "rocas"},
        ],
        "achievements": [{"id": "uno", "nameEs": "Almudena", "hidden": "no", "icon": "Icono", "modes": ["Dios"],
                          "condition": {"stat": "peces", "op": ">=", "value": 1}}],
    }
    ds = DataSet({"achievements.json": doc}, tmp_path)
    r = Report()
    achievements.check_achievements(ds, r)
    e = r.errors
    assert _has(e, "Explorer, Survivor y Castaway")
    assert _has(e, "id de estadística inválido «Mal-Id»")
    assert _has(e, "«peces» con kind «contador»") and _has(e, "«peces» con scope «mundo»")
    assert _has(e, "estadística duplicada «peces»")
    assert _has(e, "«peces» tiene values/valuesFrom pero no es un conjunto")
    assert _has(e, "«lista».values debe ser una lista no vacía sin repetidos")
    assert _has(e, "valuesFrom «rocas»")
    assert _has(e, "1 logros; biblia 07 §2 pide entre")
    assert _has(e, "«uno»: falta nameEn") and _has(e, "«uno»: nameEs usa el nombre de la dedicatoria")
    assert _has(e, "hidden debe ser true o false") and _has(e, "icon debe ser")
    assert _has(e, "modes ['Dios']")
    assert _has(e, "faltan los logros del GDD §16")
    assert _has(e, "Falta docs/tecnico/estadisticas.md")
    assert _has(r.warnings, "«lista» no la usa ningún logro")


def test_catalogo_documentado_desfasado(tmp_path: Path) -> None:
    doc_path = tmp_path / achievements.STATS_DOC
    doc_path.parent.mkdir(parents=True)
    doc_path.write_text("| `peces` | counter | profile |\n| `viejo` | flag | run |\n", encoding="utf-8")
    assert achievements.doc_stats(tmp_path) == {"peces": ("counter", "profile"), "viejo": ("flag", "run")}
    doc = {"modes": ["Explorer", "Survivor", "Castaway"],
           "stats": [{"id": "peces", "kind": "counter", "scope": "run", "descriptionEs": "x"},
                     {"id": "nuevo", "kind": "flag", "scope": "run", "descriptionEs": "x"}],
           "achievements": []}
    r = Report()
    achievements.check_achievements(DataSet({"achievements.json": doc}, tmp_path), r)
    assert _has(r.errors, "«peces» documentada como ('counter', 'profile')")
    assert _has(r.errors, "falta la estadística «nuevo»")
    assert _has(r.errors, "«viejo» no está en el catálogo")


# --------------------------------------------------------------------------- fuego


def _level(lid: str, **extra: Any) -> dict[str, Any]:
    lv: dict[str, Any] = {"id": lid, "nameEs": lid, "pieceId": lid, "maxFuelHours": 2, "burnRate": 1, "heat": 0.5,
                          "emberHours": 1, "rainQuench": 1, "windTolerance": 0.5, "smoke": 0.2}
    lv.update(extra)
    return lv


ITEMS: list[dict[str, Any]] = [
    {"id": "yesca", "tags": [], "properties": [{"name": "Inflamable", "value": 3}]},
    {"id": "piedra", "tags": [], "properties": []},
    {"id": "hoja_verde", "tags": [], "properties": []},
    {"id": "tronco", "tags": ["madera"], "properties": []},
    {"id": "setas", "tags": ["comida"], "properties": [{"name": "Toxico", "value": 2}]},
    {"id": "olla", "tags": [], "properties": []},
]


def _fuel_errors(fuels: dict[str, Any]) -> list[str]:
    errs: list[str] = []
    cooking.check_fuels(fuels, ITEMS, {"fogata", "hoguera", "horno_arcilla"}, errs.append)
    return errs


def test_combustibles_validos_sin_errores() -> None:
    fuels = {
        "levels": [_level("fogata"), _level("hoguera", maxFuelHours=4, heat=0.8, emberHours=2),
                   _level("horno_arcilla", maxFuelHours=8, heat=1.2, emberHours=4, enclosed=True)],
        "fuels": [{"item": "yesca", "burnHours": 0.1, "heat": 0.2, "tinder": True},
                  {"item": "tronco", "burnHours": 2, "heat": 0.6},
                  {"item": "hoja_verde", "burnHours": 0.5, "heat": 0.1, "green": True}],
        "ignition": [{"id": i, "toolItem": "piedra", "chance": 0.5, "minutes": 1} for i in cooking.IGNITION],
    }
    assert _fuel_errors(fuels) == []


def test_combustibles_con_todos_los_fallos() -> None:
    fuels = {
        "levels": [_level("hoguera", heat=9, nameEs="", pieceId="x"), _level("fogata")],
        "fuels": [{"item": "yesca", "burnHours": 99, "heat": 0.2, "tinder": True},
                  {"item": "yesca", "burnHours": 1, "heat": 0.2},
                  {"item": "fantasma", "burnHours": 1, "heat": 0.2},
                  {"item": "piedra", "burnHours": 1, "heat": 0.2, "tinder": True}],
        "ignition": [{"id": "cerillas", "toolItem": "mechero", "chance": 0, "minutes": 1}],
    }
    e = _fuel_errors(fuels)
    assert _has(e, "los niveles deben ser")
    assert _has(e, "pieceId «x»") and _has(e, "heat=9") and _has(e, "falta nameEs")
    assert _has(e, "maxFuelHours de «fogata» no supera a «hoguera»")
    assert _has(e, "horno de arcilla debe ser cerrado")
    assert _has(e, "combustible repetido «yesca»") and _has(e, "«fantasma» no está en items.json")
    assert _has(e, "«yesca»: burnHours o heat fuera de rango")
    assert _has(e, "«piedra»: yesca sin Inflamable") and _has(e, "«piedra»: no es madera")
    assert _has(e, "combustible verde")
    assert _has(e, "formas de encender") and _has(e, "toolItem «mechero»") and _has(e, "chance o minutes")


def test_combustibles_sin_yesca() -> None:
    assert _has(_fuel_errors({"levels": [], "fuels": [], "ignition": []}), "al menos una yesca")


# --------------------------------------------------------------------------- recetas


def _recipes(**extra: Any) -> dict[str, Any]:
    doc: dict[str, Any] = {
        "vessels": [{"id": "olla", "nameEs": "Olla", "items": ["olla"], "capacity": 2, "watertight": True},
                    {"id": "brasa", "nameEs": "Brasa", "piece": "fogata", "capacity": 4}],
        "recipes": [{"id": "setas_asadas", "nameEs": "Setas asadas", "technique": "asar", "result": "setas", "minFireLevel": "fogata",
                     "ingredients": [{"item": "setas", "count": 1}], "vessels": ["brasa"], "cookMinutes": 10,
                     "resultCount": 1}],
        "foods": [{"item": "setas", "state": "cocinado", "family": "hongo", "toxicity": 0.5, "food": 5,
                   "water": 0, "warmth": 0, "morale": 0}],
        "improvised": {"technique": "guisar", "result": "setas"},
        "burnt": "setas",
        "preservation": {"families": {"hongo": 1}, "spoiledToxicity": 0.2, "rottenToxicity": 0.5,
                         "stateHours": {"crudo": 1, "cocinado": 2, "ahumado": 3, "salado": 3, "seco": 3}},
    }
    doc.update(extra)
    return doc


def _recipe_errors(doc: dict[str, Any]) -> list[str]:
    errs: list[str] = []
    cooking.check_recipes(doc, ITEMS, {"fogata"}, errs.append)
    return errs


def test_recetas_validas_sin_errores() -> None:
    assert _recipe_errors(_recipes()) == []


def test_recetas_con_fallos() -> None:
    bad = [
        {"id": "r", "technique": "freir", "result": "nada", "minFireLevel": "volcan", "ingredients": [],
         "cookMinutes": 0, "resultCount": 0, "burnAfterMinutes": 9999},
        {"id": "r", "technique": "hervir", "result": "setas", "minFireLevel": "fogata",
         "ingredients": [{"item": "setas", "tag": "comida", "count": 3}, {"tag": "magia", "count": 0}],
         "vessels": ["brasa", "sarten"], "cookMinutes": 5, "resultCount": 1},
        {"id": "h", "technique": "hornear", "result": "setas", "minFireLevel": "fogata",
         "ingredients": [{"item": "fantasma"}], "cookMinutes": 5, "resultCount": 1},
        {"id": "a", "technique": "asar", "result": "setas", "ingredients": [{"item": "setas", "count": 1}],
         "cookMinutes": 5, "resultCount": 1},
        {"id": "s", "technique": "secar", "result": "setas", "minFireLevel": "fogata",
         "ingredients": [{"item": "setas", "count": 1}], "cookMinutes": 5, "resultCount": 1},
    ]
    vessels = [{"id": "olla", "items": ["cazo"], "piece": "horno", "capacity": 9},
               {"id": "olla", "capacity": 2}, {"id": "brasa", "piece": "fogata", "capacity": 2}]
    doc = _recipes(recipes=bad, vessels=vessels, burnt="carbon", improvised={},
                   preservation={"families": {"hongo": 50}, "spoiledToxicity": 0.9, "rottenToxicity": 0.1,
                                 "stateHours": {"crudo": 5, "cocinado": 2, "ahumado": 3, "salado": 3, "seco": 3}})
    doc["foods"].append({"item": "fantasma", "state": "podrido", "family": "roca", "toxicity": 2, "food": 500,
                         "water": 0, "warmth": 0, "morale": 0})
    doc["foods"][0]["toxicity"] = 0
    e = _recipe_errors(doc)
    for needle in ("utensilio repetido «olla»", "objeto «cazo»", "pieza «horno»", "ni objetos ni pieza",
                   "capacity fuera de [1, 8]", "receta repetida «r»", "técnica «freir»", "resultado «nada»",
                   "sin minFireLevel válido", "minFireLevel en una técnica sin fuego",
                   "hornear solo se hace en el horno", "«item» o «tag», no ambos", "ingrediente «fantasma»",
                   "etiqueta «magia»", "count inválido", "sin ingredientes", "utensilio «sarten» no existe",
                   "no caben en «brasa»", "hervir en «brasa», que no es estanco", "cookMinutes fuera",
                   "burnAfterMinutes fuera", "resultCount fuera", "«improvised» necesita",
                   "«burnt» debe ser", "foods: «fantasma» no está", "estado «podrido»", "familia «roca»",
                   "toxicity fuera de [0, 1]", "food fuera de [-50, 100]", "Toxico=2 en items.json pero toxicity 0",
                   "crudo < cocinado", "factor fuera de [0, 20]", "spoiledToxicity ≤ rottenToxicity"):
        assert _has(e, needle), needle


def test_recetas_estados_de_conservacion_incompletos_y_comida_sin_ficha() -> None:
    doc = _recipes(foods=[], burnt=None)
    doc["preservation"]["stateHours"] = {"crudo": 1}
    e = _recipe_errors(doc)
    assert _has(e, "stateHours: estados ['crudo']")
    assert _has(e, "falta la comida «setas»")
    assert _has(e, "el resultado «setas» es comida y no está en foods")


def test_tabla_de_cocina_generada(tmp_path: Path) -> None:
    doc = _recipes()
    doc["preservation"].update(staleFraction=0.5, warmHours=2)
    doc["improvised"].update(cookMinutes=30, efficiency=0.5, warmth=1, morale=0)
    items = [{"id": "setas", "tags": ["comida"], "nutritionEnergy": 1, "nutritionProtein": 2}]
    text = cooking.render_cooking_inl(doc, items)
    assert "R.MinFireLevel = EFireLevel::Fogata;" in text
    assert "R.BurnAfterMinutes = -1.0f;" in text
    assert "F.Effects.Protein = 16.0f;" in text  # 2 × nutrientPointsPerUnit (8)
    assert 'F.Tags.Add(FName(TEXT("comida")));' in text
    assert "D.Preservation.StaleFraction = 0.5f;" in text
    assert "D.Improvised.BurnAfterMinutes = -1.0f;" in text
    assert text == cooking.render_cooking_inl(doc, items)
    files = cooking.generated_files({"recipes.json": doc})
    assert list(files) == [cooking.COOKING_INL]
