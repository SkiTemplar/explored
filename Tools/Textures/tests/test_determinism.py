"""Determinismo por semilla y contrato de nombres/manifiesto."""

import json

import numpy as np
import pytest

from texgen.legacy import LEGACY_KINDS, LEGACY_NAMES, generate_legacy
from texgen.materials import MATERIALS, default_seed, generate


@pytest.mark.parametrize("name", list(MATERIALS))
def test_same_seed_same_maps(generated, name):
    again = generate(name, 256)
    for suffix, arr in generated[name].items():
        np.testing.assert_array_equal(arr, again[suffix])


@pytest.mark.parametrize("name", list(MATERIALS))
def test_other_seed_other_maps(generated, name):
    other = generate(name, 256, seed=default_seed(name) + 1)
    first = next(iter(generated[name]))
    assert np.abs(other[first] - generated[name][first]).mean() > 1e-3


def test_default_seeds_are_stable_and_distinct():
    seeds = {n: default_seed(n) for n in MATERIALS}
    assert len(set(seeds.values())) == len(seeds)
    assert default_seed("SandDry") == 90882  # cambiarla altera la textura publicada


def test_legacy_contract():
    """build_materials.py usa estos nombres: no pueden desaparecer ni cambiar de canal."""
    assert set(LEGACY_NAMES) == set(LEGACY_KINDS)
    images = generate_legacy(64)
    assert set(images) == set(LEGACY_NAMES)
    modes = {k: v.mode for k, v in images.items()}
    assert modes == {"T_TerrainDetail": "RGBA", "T_TerrainNormal": "RGB", "T_LeafNoise": "RGBA",
                     "T_WaterFoam": "L", "T_WaterRipple": "RGB"}
    again = generate_legacy(64)
    for k in images:
        assert images[k].tobytes() == again[k].tobytes()


def test_cli_writes_maps_and_manifest(tmp_path):
    import gen_textures

    gen_textures.main(["--size", "64", "--only", "Rope", "SeaFoam", "--out", str(tmp_path), "--no-legacy",
                       "--sheet", str(tmp_path / "sheet.png")])
    for f in ("T_Rope_BC.png", "T_Rope_N.png", "T_Rope_ARH.png", "T_SeaFoam_M.png", "sheet.png"):
        assert (tmp_path / f).is_file(), f
    manifest = json.loads((tmp_path / "textures.json").read_text(encoding="utf-8"))
    for name in LEGACY_NAMES:
        assert name in manifest
    assert manifest["T_Rope_BC"] == {"kind": "color", "srgb": True}
    assert manifest["T_Rope_N"] == {"kind": "normal", "srgb": False}
    assert manifest["T_Rope_ARH"] == {"kind": "masks", "srgb": False}
    assert manifest["T_SeaFoam_M"] == {"kind": "masks", "srgb": False}
    # El manifiesto lista todos los materiales aunque se generen solo algunos.
    assert "T_SandDry_BC" in manifest
