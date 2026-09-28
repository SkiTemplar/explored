"""Hoja de contacto de la paleta (texgen/palette_sheet.py): escena de muestra y montaje."""

import warnings

import numpy as np
import pytest
from PIL import Image

from texgen import palette_sheet
from texgen.palette import ISLANDS, TERRAIN_TARGETS, build_atlas, island_swatches
from texgen.palette_sheet import GROUNDS, box, icosphere, prism, render_scene, scene

ISLAND = {i.key: i for i in ISLANDS}


@pytest.mark.parametrize("island", list(ISLAND))
def test_la_escena_solo_usa_celdas_del_atlas_de_la_isla(island):
    """Una muestra con un id que no existe rompería la hoja con KeyError al rasterizar."""
    ids = {s["id"] for s in island_swatches(ISLAND[island])}
    meshes = scene(island)
    assert {m["sw"] for m in meshes} <= ids
    for m in meshes:
        assert m["f"].min() >= 0 and m["f"].max() < len(m["v"])
        assert m["tv"].shape == (len(m["v"]),)
        assert 0.0 <= m["tv"].min() and m["tv"].max() <= 1.0


def test_suelos_de_cada_isla_son_materiales_de_terreno():
    assert set(GROUNDS) == set(ISLAND)
    for g1, g2 in GROUNDS.values():
        assert g1 in TERRAIN_TARGETS and g2 in TERRAIN_TARGETS


def test_mallas_apoyadas_en_el_suelo():
    """box/prism/icosphere nacen en y = y0 (suelo), así las sombras de contacto casan."""
    assert box((0, 0, 0), (1, 1, 1), "x")["v"][:, 1].min() == pytest.approx(0.0)
    assert prism((0, 0, 0), 0.3, 1.0, 6, "x", y0=0.2)["v"][:, 1].min() == pytest.approx(0.2)
    ico = icosphere((0, 0, 0), 0.5, "x", jitter=0.3, seed=1)
    assert ico["v"][:, 1].min() == pytest.approx(0.0)
    assert len(ico["f"]) == 80  # icosaedro subdividido una vez


@pytest.fixture(scope="module")
def landing():
    isl = ISLAND["Landing"]
    sws = island_swatches(isl)
    return isl, sws, build_atlas(sws)


def _render(landing, ground):
    isl, sws, atlas = landing
    return render_scene(isl.key, atlas, sws, ground, ground, isl.vertex["Sand"], isl.vertex["Grass"], w=160, h=72)


def test_render_escena_pequena(landing):
    sand = np.full((16, 16, 3), 0.8)
    with warnings.catch_warnings():
        # Los píxeles de cielo daban NaN (inf * 0) al calcular el punto de suelo.
        warnings.simplefilter("error", RuntimeWarning)
        img = _render(landing, sand)
    assert img.shape == (72, 160, 3)
    assert np.isfinite(img).all() and img.min() >= 0.0 and img.max() <= 1.0
    sky = img[0].mean(axis=0)
    assert sky[2] > sky[0], "arriba hay cielo (azulado)"
    # El suelo usa la textura de terreno: si cambia, cambian las filas de abajo y no el cielo.
    dark = _render(landing, np.full((16, 16, 3), 0.2))
    np.testing.assert_array_equal(dark[0], img[0])
    assert dark[-1].mean() < img[-1].mean() - 0.05


def test_hoja_de_la_paleta_monta_todas_las_islas(tmp_path, monkeypatch):
    llamadas = []

    def fake_render(island, atlas, sws, g1, g2, t1, t2, w, h):
        llamadas.append((island, g1.shape))
        return np.full((h, w, 3), 0.5)

    monkeypatch.setattr(palette_sheet, "render_scene", fake_render)
    terrain = {m: np.full((32, 32, 3), 0.6) for m in TERRAIN_TARGETS}
    raw = {m: np.full((32, 32, 4), 0.3) for m in TERRAIN_TARGETS}  # RGBA también vale
    path = palette_sheet.contact_sheet(tmp_path / "sub" / "hoja.png", terrain, raw, "título")
    assert [c[0] for c in llamadas] == list(ISLAND)
    img = Image.open(path)
    assert img.mode == "RGB" and img.size[0] == 16 + 110 + 400 + 16 + 1000 + 16
