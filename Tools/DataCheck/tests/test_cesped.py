"""grass.json, island_decor.json y ground_field.bin (datacheck.cesped)."""

from __future__ import annotations

import shutil
from pathlib import Path
from typing import Any

import pytest

from datacheck import cesped
from datacheck.checks import DataSet, run_all


@pytest.fixture(scope="module")
def real() -> DataSet:
    return DataSet.load()


@pytest.fixture
def ds(real: DataSet) -> DataSet:
    return real.copy()


def grass(ds: DataSet) -> dict[str, Any]:
    return ds.data["grass.json"]


def errors_of(ds: DataSet) -> list[str]:
    errors: list[str] = []
    cesped.check_vegetation(ds.data, ds.repo_root, errors.append)
    return errors


def has(errors: list[str], *needles: str) -> bool:
    return any(all(n in e for n in needles) for e in errors)


def test_datos_reales_sin_errores(real: DataSet) -> None:
    assert errors_of(real) == []
    assert run_all(real).errors == []


def test_espejo_con_el_cpp(ds: DataSet) -> None:
    grass(ds)["radiusM"] = 80.0
    grass(ds)["density"]["zones"]["palmar"] = 3.0
    errors = errors_of(ds)
    assert has(errors, "radiusM=80", "DefaultRadiusM")
    assert has(errors, "density.zones.palmar=3", "ZonePalmarDensity")


def test_rangos_y_forma(ds: DataSet) -> None:
    g = grass(ds)
    g["cellsPerFrame"] = 0
    g["scale"] = [1.5, 1.0]
    g["version"] = 2
    g["fade"] = {"startM": 60.0, "endM": 50.0}
    g["palms"] = {"trunkM": 4.0, "radiusM": 3.0}
    g["clearings"]["density"] = 2.0
    errors = errors_of(ds)
    for needle in ("cellsPerFrame", "scale", "version", "fade", "palms", "clearings.density"):
        assert has(errors, needle), needle


def test_fundido_mas_alla_del_radio(ds: DataSet) -> None:
    grass(ds)["fade"]["endM"] = 90.0
    assert has(errors_of(ds), "fade.endM")


def test_zonas_y_densidades(ds: DataSet) -> None:
    zones = grass(ds)["density"]["zones"]
    zones["inventada"] = 1.0
    zones["playa_norte"] = -1.0
    del zones["cala_varada"]
    grass(ds)["density"]["default"] = "mucha"
    errors = errors_of(ds)
    assert has(errors, "inventada", "island.json")
    assert has(errors, "playa_norte", "entre 0")
    assert has(errors, "falta cala_varada")
    assert has(errors, "density.default")


def test_mallas_y_materiales(ds: DataSet) -> None:
    g = grass(ds)
    g["meshes"]["fern"]["mesh"] = "/Game/Art/K6_Cesped/SM_Inventada_01"
    g["meshes"]["clover"]["castShadow"] = "no"
    g["meshes"]["tall"] = "x"
    g["material"] = "/Game/Otro/M_X"
    g["fallbackMesh"] = "/Game/Art/K1_Naturaleza/SM_Nada_01"
    errors = errors_of(ds)
    assert has(errors, "SM_Inventada_01", "manifest")
    assert has(errors, "clover.castShadow")
    assert has(errors, "meshes.tall")
    assert has(errors, "grass.json material")
    assert has(errors, "fallbackMesh", "SM_Nada_01")
    del g["meshes"]["tuft_a"]
    g["fallbackMesh"] = "nada"
    errors = errors_of(ds)
    assert has(errors, "meshes: debe tener")
    assert has(errors, "fallbackMesh: debe ser")


def test_mezclas(ds: DataSet) -> None:
    mixes = grass(ds)["mixes"]
    mixes["common"] = {k: 0.0 for k in cesped.KINDS}
    mixes["edge"]["tall"] = -1.0
    mixes["palm"]["cactus"] = 1.0
    errors = errors_of(ds)
    assert has(errors, "mixes.common")
    assert has(errors, "mixes.edge")
    assert has(errors, "mixes.palm")
    del mixes["dirt"]
    assert has(errors_of(ds), "mixes: debe tener")


def test_no_objeto(ds: DataSet) -> None:
    ds.data["grass.json"] = []
    assert has(errors_of(ds), "grass.json: debe ser un objeto")
    ds.data["grass.json"] = {"version": 1}
    errors = errors_of(ds)
    assert has(errors, "density: falta")
    assert has(errors, "clearings: falta")


def test_decoracion(ds: DataSet) -> None:
    decor = ds.data["island_decor.json"]
    decor["meshes"].append("/Game/Art/K6_Cesped/SM_Nada_01")
    decor["meshes"].append("/Engine/Cube")
    decor["instances"][3] = [99, 0, 0, 0, 0, 1]
    errors = errors_of(ds)
    assert has(errors, "SM_Nada_01")
    assert has(errors, "/Engine/Cube")
    assert has(errors, "instancia 3")
    decor["instances"] = decor["instances"] * 2
    assert has(errors_of(ds), "FIslandDecor admite")
    ds.data["island_decor.json"] = {}
    assert has(errors_of(ds), "faltan «meshes»")


def test_campo_de_suelo(tmp_path: Path, real: DataSet) -> None:
    root = tmp_path / "repo"
    (root / "Content" / "Data").mkdir(parents=True)
    errors: list[str] = []
    cesped.check_ground_field(root, errors.append)
    assert has(errors, "Falta Content/Data/ground_field.bin")
    (root / "Content" / "Data" / "ground_field.bin").write_bytes(b"NOPE")
    errors = []
    cesped.check_ground_field(root, errors.append)
    assert has(errors, "EXGF")
    shutil.copy(real.repo_root / "Content" / "Data" / "ground_field.bin", root / "Content" / "Data" / "ground_field.bin")
    errors = []
    cesped.check_ground_field(root, errors.append)
    assert errors == []


def test_sin_cpp_ni_manifests(tmp_path: Path, real: DataSet) -> None:
    """En otro árbol sin Source ni Art: la constante falta y no se comparan manifests."""
    copy = real.copy()
    copy.repo_root = tmp_path
    errors = errors_of(copy)
    assert has(errors, "no existe la constante")
    assert not has(errors, "manifest")
