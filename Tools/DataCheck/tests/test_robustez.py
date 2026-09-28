"""Entradas corruptas: el validador informa del problema en vez de romper con una excepción.

Cada test monta un DataSet mínimo (rápido: no simula la fabricación del catálogo real) con el
campo mal formado que antes tumbaba el proceso con TypeError/ValueError/JSONDecodeError.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from datacheck import checks, cooking, fases, fauna, mining, music, packs
from datacheck.checks import REPO_ROOT, DataSet, Report


def _ds(tmp_path: Path, **files: Any) -> DataSet:
    """DataSet con los ficheros dados (clave con «_» en lugar de «.»: items_json → items.json)."""
    return DataSet({name.replace("_json", ".json"): doc for name, doc in files.items()}, tmp_path)


def _has(errors: list[str], *needles: str) -> bool:
    return any(all(n in e for n in needles) for e in errors)


def _item(iid: str, **extra: Any) -> dict[str, Any]:
    base: dict[str, Any] = {"id": iid, "nameEs": iid, "nameEn": iid, "meshPath": "/Engine/BasicShapes/Cube.Cube",
                            "size": "Mediano", "weightKg": 1, "volumeLiters": 1, "tags": [], "properties": []}
    base.update(extra)
    return base


# --------------------------------------------------------------------------- carga


def test_json_mal_formado_se_informa_sin_excepcion(tmp_path: Path) -> None:
    data = tmp_path / "Content" / "Data"
    data.mkdir(parents=True)
    (data / "items.json").write_text("[{\"id\": \"liana\",", encoding="utf-8")
    (data / "verbs.json").write_text("[]", encoding="utf-8")
    ds = DataSet.load(tmp_path)
    assert "items.json" not in ds.data and ds.data["verbs.json"] == []
    assert "items.json" in ds.load_errors
    r = Report()
    checks.check_files_present(ds, r)
    assert _has(r.errors, "items.json", "JSON mal formado")
    # No se informa además como «Falta»: el fichero existe.
    assert not _has(r.errors, "Falta Content/Data/items.json")
    assert _has(r.errors, "Falta Content/Data/templates.json")


def test_copia_conserva_los_errores_de_carga(tmp_path: Path) -> None:
    ds = DataSet({}, tmp_path, {"items.json": "línea 1"})
    assert ds.copy().load_errors == {"items.json": "línea 1"}


def test_json_raiz_de_tipo_equivocado_no_rompe(tmp_path: Path) -> None:
    # items.json como objeto en vez de lista: no hay objetos, se informa del mínimo.
    ds = _ds(tmp_path, items_json={"liana": {}}, templates_json="no", plants_json=[])
    assert ds.items == [] and ds.templates == [] and ds.plants == []
    r = Report()
    checks.check_items_schema(ds, r)
    assert _has(r.errors, "0 objetos")


def test_item_ids_ignora_ids_ausentes(tmp_path: Path) -> None:
    # Un objeto sin id no puede hacer que una referencia ausente (None) parezca válida.
    ds = _ds(tmp_path, items_json=[{"nameEs": "sin id"}, _item("semilla")],
             plants_json={"plants": [{"id": "taro", "stages": []}]})
    assert ds.item_ids == {"semilla"}
    r = Report()
    checks.check_plants(ds, r, obtainable={"semilla"})
    assert _has(r.errors, "plantedFrom «None» no está en items.json")


# --------------------------------------------------------------------------- objetos y plantillas


def test_peso_no_numerico_es_error_y_no_excepcion(tmp_path: Path) -> None:
    ds = _ds(tmp_path, items_json=[_item("piedra", weightKg="mucho", size="Pequeno")])
    r = Report()
    checks.check_items_schema(ds, r)
    assert _has(r.errors, "piedra", "weightKg='mucho'")


def test_propiedad_mal_formada_es_error(tmp_path: Path) -> None:
    ds = _ds(tmp_path, items_json=[_item("liana", properties=["Ata", {"name": "Largo", "value": "3"}])])
    r = Report()
    checks.check_items_schema(ds, r)
    assert _has(r.errors, "liana", "propiedad mal formada")
    assert _has(r.errors, "liana", "Largo='3'")


def test_objeto_que_no_es_diccionario(tmp_path: Path) -> None:
    ds = _ds(tmp_path, items_json=["liana", _item("palo")])
    r = Report()
    checks.check_items_schema(ds, r)
    assert _has(r.errors, "id inválido")
    assert ds.item_ids == {"palo"}


def test_name_template_no_textual(tmp_path: Path) -> None:
    ds = _ds(tmp_path, items_json=[_item("x")], verbs_json=[{"id": "Atar"}],
             templates_json=[{"id": "t", "resultDefinitionId": "x", "verbs": ["Atar"], "nameTemplate": 3,
                              "slots": [{"role": "A", "requirements": [{"property": "Ata", "min": 1}]}]}])
    r = Report()
    checks.check_templates(ds, r)
    assert _has(r.errors, "«t»", "nameTemplate")


def test_plantilla_con_placeholder_de_mas(tmp_path: Path) -> None:
    ds = _ds(tmp_path, items_json=[_item("x")], verbs_json=[{"id": "Atar"}],
             templates_json=[{"id": "t", "resultDefinitionId": "x", "verbs": ["Atar"], "nameTemplate": "{0} y {1}",
                              "slots": [{"role": "A", "requirements": [{"property": "Ata", "min": 1}]}]}])
    r = Report()
    checks.check_templates(ds, r)
    assert _has(r.errors, "«t»", "{1}", "1 slots")


def test_verbos_duplicados_y_sin_descripcion(tmp_path: Path) -> None:
    ds = _ds(tmp_path, verbs_json=[{"id": "Atar", "nameEs": "Atar"}, {"id": "Atar", "nameEs": "Atar"}])
    r = Report()
    checks.check_verbs(ds, r)
    assert _has(r.errors, "duplicado «Atar»")
    assert _has(r.errors, "«Atar»", "description")
    assert _has(r.errors, "falta el verbo «Golpear»")


# --------------------------------------------------------------------------- huerto, construcción, barcos


def test_dias_de_etapa_no_numericos(tmp_path: Path) -> None:
    plant = {"id": "taro", "plantedFrom": "taro", "requiresPiece": "bancal", "seasons": ["seca"],
             "stages": [{"id": "brote", "days": "tres"}, {"id": "maduro", "days": 0}],
             "harvest": {"item": "taro", "min": 1, "max": 2, "everyDays": 0}}
    ds = _ds(tmp_path, items_json=[_item("taro")], plants_json={"plants": [plant]},
             building_pieces_json={"tiers": [], "pieces": [{"id": "bancal"}]})
    r = Report()
    checks.check_plants(ds, r, obtainable={"taro"})
    assert _has(r.errors, "taro", "days='tres'")


def test_tier_sin_orden(tmp_path: Path) -> None:
    ds = _ds(tmp_path, building_pieces_json={"tiers": [{"id": "palma", "order": 0}, {"id": "bambu"}], "pieces": []})
    r = Report()
    checks.check_building(ds, r, obtainable=set())
    assert _has(r.errors, "órdenes de tier no consecutivos")


def test_barco_sin_orden(tmp_path: Path) -> None:
    boats = {"boats": [{"id": "balsa", "type": "Raft", "order": 0}, {"id": "canoa", "type": "Canoe"}]}
    ds = _ds(tmp_path, boats_json=boats)
    r = Report()
    checks.check_boats(ds, r, obtainable=set())
    assert _has(r.errors, "órdenes de progresión no consecutivos")
    assert _has(r.warnings, "EBoatType")


def test_pendiente_de_malla_sin_id(tmp_path: Path) -> None:
    ds = _ds(tmp_path, items_json=[], meshes_pendientes_json={"items": [{"nota": "sin id"}, "fantasma"]})
    r = Report()
    checks.check_meshes(ds, r)
    assert _has(r.errors, "meshes_pendientes.json/items", "«fantasma»", "quítalo")
    assert _has(r.errors, "meshes_pendientes.json/items", "sin id")


# --------------------------------------------------------------------------- minería


def _material(**extra: Any) -> dict[str, Any]:
    m: dict[str, Any] = {"id": "tierra", "hardness": 1.0, "minToolTier": 1,
                         "hitsPerM3": {"1": 6, "2": 6, "3": 6, "4": 6}}
    m.update(extra)
    return m


def test_mineria_material_sin_cpp(tmp_path: Path) -> None:
    # Con el C++ real presente, un material sin «cpp» mezclaba None y str al ordenar.
    ds = DataSet({"mining.json": {"materials": [_material()]}, "items.json": [], "templates.json": []}, REPO_ROOT)
    r = Report()
    mining.check_mining(ds, r)
    assert _has(r.errors, "no coincide con ETerrainMaterial")


def test_mineria_nivel_no_numerico_en_la_tabla(tmp_path: Path) -> None:
    doc = {"tierBonus": 1.5, "minHitsPerM3": 1.0, "materials": [_material(hitsPerM3={"uno": 6, "2": 6, "3": 6, "4": 6})]}
    r = Report()
    mining.check_mining(_ds(tmp_path, mining_json=doc, items_json=[], templates_json=[]), r)
    assert _has(r.errors, "tierra", "hitsPerM3 debe tener los niveles")
    assert _has(r.warnings, "TerrainEditModel")


def test_mineria_formula_de_golpes() -> None:
    assert mining.design_hits(1.0, 1, 1, 1.5, 1.0) == 6.0
    assert mining.design_hits(3.0, 3, 4, 1.5, 10.0) == 12.0
    # Nunca por debajo del suelo.
    assert mining.design_hits(1.0, 1, 4, 3.0, 5.0) == 5.0


# --------------------------------------------------------------------------- fases futuras y fauna


def test_fases_seccion_con_fase_nula(tmp_path: Path) -> None:
    sec = {"fase": None, "redNotaEs": "x", "pieces": [{"id": "rail", "fase": 2}]}
    doc = {"tramway": sec, "livestock": {"fase": 2, "redNotaEs": "x"}, "defenses": {"fase": 2, "redNotaEs": "x"},
           "trade": {"fase": 2, "redNotaEs": "x", "tiers": [{"id": "a", "min": 0, "max": None, "rate": 1}]}}
    r = Report()
    fases.check_future_phases(_ds(tmp_path, fases_futuras_json=doc, items_json=[]), r, checks.BUILDING_SOCKETS)
    assert _has(r.errors, "tramway", "fase None")
    assert _has(r.errors, "tramo «a»")


def test_fauna_valores_de_tipo_equivocado(tmp_path: Path) -> None:
    sp = {"id": "cerdo", "nameEs": "Cerdo", "nameEn": "Pig", "fase": 1, "cppSpecies": None,
          "cppSpeciesPropuesto": "Pig", "healthPoints": 10, "disposition": "huidizo",
          "perception": {"sightConeDeg": "ancho", "sightM": 1, "hearingM": 1, "smellM": 1},
          "routine": [{"hours": [0, 24]}], "flee": {"healthFraction": "media"},
          "loot": [{"item": "carne", "min": "1", "max": 2}], "red": {"clase": "replicada"}}
    doc = {"lod": {"tiers": []}, "species": [sp], "islands": []}
    r = Report()
    fauna.check_fauna(_ds(tmp_path, fauna_json=doc, items_json=[_item("carne")]), r, checks.PROPERTIES)
    assert _has(r.errors, "cerdo", "sightConeDeg")
    assert _has(r.errors, "cerdo", "healthFraction")
    assert _has(r.errors, "cerdo", "min/max")


# --------------------------------------------------------------------------- packs


def test_packs_ids_no_textuales() -> None:
    errs: list[str] = []
    usable = packs.check_manifest({"packs": [{"id": 42}, {"id": "bueno", "source": {"kind": "ftp"}}]}, errs.append)
    assert usable == set()
    assert _has(errs, "«42»", "id no ASCII")
    assert _has(errs, "«bueno»", "fuente «ftp» desconocida")
    errs.clear()
    packs.check_fauna({"fauna_terrestre.json": {"species": [{"id": None, "wild": "si", "phase": "F9"}]}}, errs.append)
    assert _has(errs, "id no ASCII") and _has(errs, "wild") and _has(errs, "fase «F9»")


def test_packs_catalogo_sin_manifiesto(tmp_path: Path) -> None:
    errs: list[str] = []
    packs.check_catalog(tmp_path, {"packs_catalogo.json": {"entries": []}}, errs.append)
    assert _has(errs, "falta Tools/Packs/packs.json")
    errs.clear()
    packs.check_catalog(tmp_path, {}, errs.append)
    assert errs == []


def test_packs_entrada_con_campos_de_tipo_equivocado(tmp_path: Path) -> None:
    manifest = tmp_path / "Tools" / "Packs" / "packs.json"
    manifest.parent.mkdir(parents=True)
    manifest.write_text(json.dumps({"packs": []}), encoding="utf-8")
    catalog = {"lotes": [{"id": 7, "sheet": None}],
               "entries": [{"gameId": 5, "kind": "item", "mesh": 3, "file": None, "size": {"axis": "x", "m": "1"},
                            "pivot": {"kind": "agarre", "gripFromEndM": 1}, "recolor": {}}]}
    errs: list[str] = []
    packs.check_catalog(tmp_path, {"packs_catalogo.json": catalog}, errs.append)
    assert _has(errs, "lote «7»")
    assert _has(errs, "«5»", "no existe como item")
    assert _has(errs, "malla «3»")
    assert _has(errs, "«file» debe ser una ruta relativa")
    assert _has(errs, "recolor sin reglas")


# --------------------------------------------------------------------------- cocina generada


def test_cocina_generada_se_detecta_y_se_regenera(tmp_path: Path) -> None:
    fuels = {"levels": [{"id": "fogata", "nameEs": "Fogata \"viva\"", "pieceId": "fogata", "maxFuelHours": 2,
                         "burnRate": 1, "heat": 0.5, "emberHours": 1, "rainQuench": 1, "windTolerance": 0.5,
                         "smoke": 0.2}],
             "fuels": [], "ignition": []}
    data: dict[str, Any] = {"fuels.json": fuels}
    errs: list[str] = []
    cooking.check_generated(tmp_path, data, errs.append)
    assert _has(errs, "FireData.inl", "--write-cooking")
    written = cooking.write_generated(tmp_path, data)
    assert written == [cooking.FIRE_INL]
    text = (tmp_path / cooking.FIRE_INL).read_text(encoding="utf-8")
    assert 'TEXT("Fogata \\"viva\\"")' in text and "L.MaxFuelHours = 2.0f;" in text
    errs.clear()
    cooking.check_generated(tmp_path, data, errs.append)
    assert errs == []
    # Determinista: generar dos veces da el mismo texto.
    assert cooking.render_fire_inl(fuels) == cooking.render_fire_inl(json.loads(json.dumps(fuels)))


# --------------------------------------------------------------------------- música


def test_musica_exploracion_sin_variante() -> None:
    # Con las islas del C++ real, una pieza de exploración sin «variant» (None) rompía sorted().
    piece = {"id": "mus_explore_x", "role": "explore", "asset": "/Game/Generated/Audio/Musica/mus_explore_x.mus_explore_x",
             "bpm": 100, "beats_per_bar": 4, "bars": 4, "loop": True, "seconds_per_bar": 2.4, "duration_s": 9.6}
    r = Report()
    music.check_music(DataSet({"music_layers.json": {"version": 1, "pieces": [piece, "suelta"]}}, REPO_ROOT), r)
    assert _has(r.errors, "una vez cada isla", "None")
    assert _has(r.errors, "'suelta'", "no es un objeto")
