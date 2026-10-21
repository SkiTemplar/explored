"""Robustez: JSON mal formado o de tipos equivocados se informa como error, nunca como excepción."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from datacheck import checks, music
from datacheck.checks import REPO_ROOT, DataSet, Report


def _ds(tmp_path: Path, **files: Any) -> DataSet:
    data = tmp_path / "Content" / "Data"
    data.mkdir(parents=True)
    for name, doc in files.items():
        (data / name.replace("_json", ".json")).write_text(json.dumps(doc), encoding="utf-8")
    return DataSet.load(tmp_path)


def _has(errors: list[str], *needles: str) -> bool:
    return any(all(n in e for n in needles) for e in errors)


def test_json_mal_formado_se_informa_sin_excepcion(tmp_path: Path) -> None:
    data = tmp_path / "Content" / "Data"
    data.mkdir(parents=True)
    (data / "items.json").write_text("[{\"id\": \"coco\",", encoding="utf-8")
    (data / "verbs.json").write_text("[]", encoding="utf-8")
    ds = DataSet.load(tmp_path)
    assert "items.json" not in ds.data and ds.data["verbs.json"] == []
    r = Report()
    checks.check_files_present(ds, r)
    assert _has(r.errors, "items.json", "JSON mal formado")
    assert not _has(r.errors, "Falta Content/Data/items.json")
    assert _has(r.errors, "Falta Content/Data/recipes.json")


def test_copia_conserva_los_errores_de_carga(tmp_path: Path) -> None:
    ds = DataSet({}, tmp_path, {"items.json": "línea 1"})
    assert ds.copy().load_errors == {"items.json": "línea 1"}


def test_raices_de_tipo_equivocado_no_rompen(tmp_path: Path) -> None:
    ds = _ds(tmp_path, items_json={"coco": {}}, verbs_json="no", recipes_json=[], fuels_json=[], survival_needs_json=3,
             fish_json=None)
    assert ds.items == [] and ds.verbs == [] and ds.recipes == []
    r = checks.run_all(ds)
    assert _has(r.errors, "items.json: la raíz debe ser una lista")
    assert _has(r.errors, "verbs.json: la raíz debe ser una lista")
    assert _has(r.errors, "recipes.json: falta la lista")


def test_entradas_corruptas_no_rompen(tmp_path: Path) -> None:
    ds = _ds(tmp_path,
             items_json=["suelto", {"id": None}, {"id": "coco", "weightKg": "mucho", "tags": "fruta", "stack": "4"}],
             recipes_json={"recipes": [{"id": "cuerda", "inputs": "fibra", "output": "cuerda", "stationNeeds": [3]}]},
             fuels_json={"campfire": {}, "fuels": ["lena"]},
             survival_needs_json={"needs": [7], "consumables": ["coco"], "rainBarrel": "lleno"},
             fish_json={"dawn": "06-08"})
    r = checks.run_all(ds)
    assert _has(r.errors, "no es un objeto")
    assert _has(r.errors, "«None» no es ASCII")
    assert _has(r.errors, "«coco»", "weightKg")
    assert _has(r.errors, "«coco»", "tags")
    assert _has(r.errors, "«cuerda».inputs", "lista")
    assert _has(r.errors, "«cuerda».stationNeeds", "no es un objeto")
    assert _has(r.errors, "«cuerda»", "output")
    assert _has(r.errors, "fish.json dawn")


def test_musica_exploracion_sin_variante() -> None:
    # Con las islas del C++ real, una pieza de exploración sin «variant» (None) rompía sorted().
    piece = {"id": "mus_explore_x", "role": "explore", "asset": "/Game/Generated/Audio/Musica/mus_explore_x.mus_explore_x",
             "bpm": 100, "beats_per_bar": 4, "bars": 4, "loop": True, "seconds_per_bar": 2.4, "duration_s": 9.6}
    r = Report()
    music.check_music(DataSet({"music_layers.json": {"version": 1, "pieces": [piece, "suelta"]}}, REPO_ROOT), r)
    assert _has(r.errors, "una vez cada isla", "None")
    assert _has(r.errors, "'suelta'", "no es un objeto")
