"""Paleta low poly (texgen/palette.py, paleta.json) y atlas T_Palette_<Isla>: cada color del
JSON está en el atlas, los mips no mezclan celdas, contraste y coherencia con el terreno."""

import itertools
import json
import re
from pathlib import Path

import numpy as np
import pytest

from texgen.materials import generate
from texgen.palette import (
    ACCENT_SWATCHES,
    ALBEDO_MIN,
    ALBEDO_MAX,
    ATLAS,
    CHROMA_CAP,
    ENTORNO_ROW,
    FAMILIES,
    FAUNA_BODY_SWATCHES,
    ISLANDS,
    PACK_ALIASES,
    TERRAIN_ROW,
    TERRAIN_TARGETS,
    build_atlas,
    cell_profile,
    chroma,
    delta_e,
    hex_to_srgb,
    island_swatches,
    srgb_to_linear,
    srgb_to_oklab,
    terrain_target_lab,
)

HERE = Path(__file__).resolve().parents[1]
ROOT = HERE.parents[1]
CELL, M, SIZE = ATLAS["cell_px"], ATLAS["margin_px"], ATLAS["size"]
ISLAND_KEYS = [i.key for i in ISLANDS]


@pytest.fixture(scope="module")
def atlases():
    return {i.key: (island_swatches(i), build_atlas(island_swatches(i))) for i in ISLANDS}


def _u8(a):
    return np.round(a * 255).astype(int)


# --- JSON versionado -----------------------------------------------------------------

def test_committed_json_matches_generator():
    """paleta.json es salida de gen_palette.py: si alguien toca palette.py sin regenerar
    (o edita el JSON a mano), falla."""
    import gen_palette

    committed = (HERE / "paleta.json").read_text(encoding="utf-8")
    assert committed == gen_palette.palette_json_text(), "regenera con gen_palette.py"


def test_json_linear_values_match_srgb():
    data = json.loads((HERE / "paleta.json").read_text(encoding="utf-8"))
    for isl in data["islas"].values():
        for entry in isl["colores"].values():
            for k in ("medio", "arriba", "abajo"):
                lin = srgb_to_linear(hex_to_srgb(entry[k]["srgb"]))
                np.testing.assert_allclose(entry[k]["lineal"], lin, atol=6e-5)


def test_vertex_colors_match_terrain_density():
    """El tinte de terreno de cada isla replica FTerrainDensity::PaletteFor."""
    src = (ROOT / "Source/Explored/WorldGen/TerrainDensity.cpp").read_text(encoding="utf-8")
    for isl in ISLANDS:
        m = re.search(rf"EIslandArchetype::{isl.archetype}: return \{{C\((\d+), (\d+), (\d+)\), "
                      rf"C\((\d+), (\d+), (\d+)\), C\((\d+), (\d+), (\d+)\)\}}", src)
        assert m, f"no encuentro la paleta de {isl.archetype} en TerrainDensity.cpp"
        v = [int(x) for x in m.groups()]
        assert isl.vertex == {"Sand": tuple(v[0:3]), "Grass": tuple(v[3:6]), "Rock": tuple(v[6:9])}


# --- Atlas ---------------------------------------------------------------------------

@pytest.mark.parametrize("island", ISLAND_KEYS)
def test_every_palette_color_exists_in_atlas(atlases, island):
    """Medio en la banda central (y en el centro exacto de la celda), arriba y abajo en los
    extremos de la zona útil, exactos a 8 bits."""
    data = json.loads((HERE / "paleta.json").read_text(encoding="utf-8"))["islas"][island]["colores"]
    _, atlas = atlases[island]
    img = _u8(atlas)
    for sid, e in data.items():
        col, row = e["celda"]
        x = col * CELL + CELL // 2
        mid = _u8(hex_to_srgb(e["medio"]["srgb"]))
        cy = int(e["uv"][1] * SIZE)
        assert (img[cy, x] == mid).all() and (img[cy - 1, x] == mid).all(), sid
        assert (img[int(round(e["v_rango"][0] * SIZE)), x] == _u8(hex_to_srgb(e["arriba"]["srgb"]))).all(), sid
        assert (img[int(round(e["v_rango"][1] * SIZE)) - 1, x] == _u8(hex_to_srgb(e["abajo"]["srgb"]))).all(), sid
        # Todas las columnas de la celda son iguales (nada que sangre en horizontal).
        cell = img[row * CELL:(row + 1) * CELL, col * CELL:(col + 1) * CELL]
        assert (cell == cell[:, :1]).all(), sid


def test_cell_gradient_is_smooth_and_monotonic(atlases):
    """Degradado suave: luminancia monótona de arriba abajo, sin saltos de más de 2/255·3."""
    sws, _ = atlases["Landing"]
    for sw in sws:
        L = srgb_to_oklab(cell_profile(sw))[:, 0]
        assert (np.diff(L) <= 1e-3).all(), sw["id"]
        assert np.abs(np.diff(L)).max() < 0.03, sw["id"]


def test_layout_is_power_of_two_aligned():
    assert SIZE == ATLAS["grid"] * CELL
    assert SIZE & (SIZE - 1) == 0 and CELL & (CELL - 1) == 0
    assert 2 * M + 2 * ATLAS["ramp_px"] + ATLAS["flat_px"] == CELL
    # Zona útil segura para bilineal hasta max_mip: margen >= 2^(max_mip-1) texels de mip 0.
    assert M >= 2 ** (ATLAS["max_mip"] - 1)


def _box_mips(img):
    mips = [img]
    while mips[-1].shape[0] > 1:
        a = mips[-1]
        mips.append((a[0::2, 0::2] + a[1::2, 0::2] + a[0::2, 1::2] + a[1::2, 1::2]) / 4)
    return mips


@pytest.mark.parametrize("island", ISLAND_KEYS)
def test_mips_never_mix_cells(atlases, island):
    """Mips por promedio 2×2 (TMGS_SimpleAverage): hasta celda = 1 texel, cada texel de una
    celda sale solo de esa celda. Se comprueba contra el mip de la celda aislada."""
    sws, atlas = atlases[island]
    mips = _box_mips(atlas)
    for sw in sws:
        col, row = sw["cell"]
        alone = _box_mips(np.repeat(cell_profile(sw)[:, None, :], CELL, axis=1))
        for k in range(int(np.log2(CELL)) + 1):
            s = CELL >> k
            got = mips[k][row * s:(row + 1) * s, col * s:(col + 1) * s]
            np.testing.assert_allclose(got, alone[k], atol=1e-12, err_msg=f"{sw['id']} mip {k}")


def _bilinear(img, x, y):
    h, w = img.shape[:2]
    x0, y0 = int(np.floor(x - 0.5)), int(np.floor(y - 0.5))
    tx, ty = x - 0.5 - x0, y - 0.5 - y0
    p = lambda yy, xx: img[yy % h, xx % w]  # noqa: E731 (wrap como el sampler)
    return ((p(y0, x0) * (1 - tx) + p(y0, x0 + 1) * tx) * (1 - ty)
            + (p(y0 + 1, x0) * (1 - tx) + p(y0 + 1, x0 + 1) * tx) * ty)


@pytest.mark.parametrize("island", ISLAND_KEYS)
def test_bilinear_inside_safe_zone_stays_in_cell_up_to_max_mip(atlases, island):
    """Muestrear con v en la zona útil (bordes incluidos) hasta MaxMip da un color dentro del
    rango arriba..abajo de la propia celda: no entra nada de la vecina."""
    sws, atlas = atlases[island]
    mips = _box_mips(atlas)
    for sw in sws:
        col, row = sw["cell"]
        prof = cell_profile(sw)
        lo, hi = prof.min(axis=0) - 1e-9, prof.max(axis=0) + 1e-9
        for k in range(ATLAS["max_mip"] + 1):
            sc = 2.0 ** k
            for py in (row * CELL + M, row * CELL + CELL / 2, (row + 1) * CELL - M):
                for px in (col * CELL + CELL / 2,):
                    c = _bilinear(mips[k], px / sc, py / sc)
                    assert (c >= lo).all() and (c <= hi).all(), f"{sw['id']} mip {k} v={py}"


def test_atlas_is_deterministic():
    isl = ISLANDS[0]
    assert build_atlas(island_swatches(isl)).tobytes() == build_atlas(island_swatches(isl)).tobytes()


def test_cli_writes_atlases_and_manifest(tmp_path):
    import gen_textures

    gen_textures.main(["--size", "64", "--only", "Rope", "--out", str(tmp_path), "--no-legacy"])
    manifest = json.loads((tmp_path / "textures.json").read_text(encoding="utf-8"))
    from PIL import Image

    for isl in ISLANDS:
        name = f"T_Palette_{isl.key}"
        assert manifest[name] == {"kind": "palette", "srgb": True}
        img = Image.open(tmp_path / f"{name}.png")
        assert img.size == (SIZE, SIZE) and img.mode == "RGB"


# --- Intención de color --------------------------------------------------------------

@pytest.mark.parametrize("island", ISLAND_KEYS)
def test_albedo_and_lightness_ranges(atlases, island):
    for sw in atlases[island][0]:
        if sw["family"] == "entorno":   # referencia de agua/cielo, no es un albedo de prop
            continue
        for k in ("top", "mid", "bottom"):
            assert ALBEDO_MIN - 1e-9 <= sw[k].min() and sw[k].max() <= ALBEDO_MAX + 1e-9, sw["id"]
        dl = srgb_to_oklab(sw["top"])[0] - srgb_to_oklab(sw["bottom"])[0]
        assert 0.08 < dl < 0.2, f"{sw['id']}: degradado {dl:.3f} (plano o sucio)"


@pytest.mark.parametrize("island", ISLAND_KEYS)
def test_swatches_in_a_family_are_distinguishable(atlases, island):
    """Piezas contiguas de un objeto a 1-2 m (mango y hoja de un hacha, aros de un barril)."""
    by_fam = {}
    for sw in atlases[island][0]:
        by_fam.setdefault(sw["family"], []).append(sw)
    for fam in FAMILIES:
        labs = [(s["id"], srgb_to_oklab(s["mid"])) for s in by_fam[fam.key]]
        for (a, la), (b, lb) in itertools.combinations(labs, 2):
            assert delta_e(la, lb) > 0.035, f"{a} ~ {b}: ΔE {delta_e(la, lb):.3f}"


@pytest.mark.parametrize("island", ISLAND_KEYS)
def test_pickups_pop_out_of_every_ground(atlases, island):
    """Lo recogible (comida, recursos, minerales) se lee sobre cualquier suelo de la isla."""
    sws = {s["id"]: srgb_to_oklab(s["mid"]) for s in atlases[island][0]}
    grounds = [v for k, v in sws.items() if k.startswith("terreno.")]
    pickup = {f.key for f in FAMILIES if f.pickup}
    assert {"comida", "recurso", "mineral"} <= pickup
    for k, lab in sws.items():
        if k.split(".")[0] in pickup:
            worst = min(delta_e(lab, g) for g in grounds)
            assert worst > 0.06, f"{k}: ΔE {worst:.3f} contra el suelo"


@pytest.mark.parametrize("island", ISLAND_KEYS)
def test_fauna_bodies_read_on_every_ground(atlases, island):
    """El pelaje de un animal (jabalí en la selva, cabra en la caliza) no se camufla en el suelo."""
    sws = {s["id"]: srgb_to_oklab(s["mid"]) for s in atlases[island][0]}
    grounds = [v for k, v in sws.items() if k.startswith("terreno.")]
    assert FAUNA_BODY_SWATCHES <= set(sws)
    for k in FAUNA_BODY_SWATCHES:
        worst = min(delta_e(sws[k], g) for g in grounds)
        assert worst > 0.06, f"{k}: ΔE {worst:.3f} contra el suelo"


@pytest.mark.parametrize("island", ISLAND_KEYS)
def test_environment_chroma_below_the_sea(atlases, island):
    for sw in atlases[island][0]:
        fam = next((f for f in FAMILIES if f.key == sw["family"]), None)
        if fam is None or fam.accent or sw["id"] in ACCENT_SWATCHES:
            continue
        assert chroma(srgb_to_oklab(sw["mid"])) <= CHROMA_CAP, sw["id"]


def test_islands_differ_but_objects_do_not(atlases):
    """Las familias de entorno cambian por isla; comida, metal, tela y UI son idénticas."""
    base = {s["id"]: s["mid"] for s in atlases["Landing"][0]}
    for isl in ISLAND_KEYS[1:]:
        other = {s["id"]: s["mid"] for s in atlases[isl][0]}
        for fam in FAMILIES:
            same = all(np.array_equal(base[k], other[k]) for k in base if k.startswith(fam.key + "."))
            assert same != fam.graded, f"{fam.key} en {isl}"


@pytest.mark.parametrize("island", ISLAND_KEYS)
def test_stone_and_grass_props_match_their_terrain(atlases, island):
    """Una roca KayKit junto al acantilado escaneado no desentona."""
    sws = {s["id"]: srgb_to_oklab(s["mid"]) for s in atlases[island][0]}
    for prop, ground in (("piedra.basalto", "terreno.basalto"), ("piedra.caliza", "terreno.caliza"),
                         ("vegetacion.hierba", "terreno.hierba")):
        assert delta_e(sws[prop], sws[ground]) < 0.05, f"{prop} vs {ground}"


# --- Terreno armonizado --------------------------------------------------------------

@pytest.mark.parametrize("name", list(TERRAIN_TARGETS))
def test_terrain_mean_tone_matches_palette(generated, name):
    lab = srgb_to_oklab(generated[name]["BC"][..., :3]).reshape(-1, 3).mean(axis=0)
    assert delta_e(lab, terrain_target_lab(name)) < 0.006, f"{name}: {lab}"


def test_harmonized_terrain_is_resolution_independent():
    a = srgb_to_oklab(generate("Grass", 128)["BC"]).reshape(-1, 3).mean(axis=0)
    assert delta_e(a, terrain_target_lab("Grass")) < 0.006


def test_family_rows_are_unique_and_fit_the_grid():
    rows = [f.row for f in FAMILIES] + [TERRAIN_ROW, ENTORNO_ROW]
    assert len(rows) == len(set(rows)), rows
    assert max(rows) < ATLAS["grid"]
    assert all(len(f.swatches) <= ATLAS["grid"] for f in FAMILIES)


def test_pack_aliases_point_to_existing_swatches():
    ids = {s["id"] for s in island_swatches(ISLANDS[0])}
    missing = {k: v for k, v in PACK_ALIASES.items() if v not in ids}
    assert not missing, missing
