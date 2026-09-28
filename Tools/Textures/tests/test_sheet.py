"""La hoja de contacto tiene que poder montar una tarjeta para cada material (el atlas de
follaje, con BC RGBA y sin ARH, rompía `--sheet`)."""

import numpy as np
import pytest

import gen_textures
from texgen.materials import MATERIALS


@pytest.mark.parametrize("name", list(MATERIALS))
def test_preview_card_for_every_material(generated, name):
    card = gen_textures.preview_card(name, generated[name], 1)
    lit = card["lit"]
    assert lit.ndim == 3 and lit.shape[-1] == 3
    assert np.isfinite(lit).all()
    for thumb in card["thumbs"]:
        assert thumb.shape[-1] == 3


@pytest.mark.parametrize("name", ["Grass", "Dirt"])
def test_stylized_sheet_is_written(generated, tmp_path, name):
    """Una hoja por material estilizado: capa del terreno (teñida por isla) y otra que no."""
    from PIL import Image

    from texgen.stylized_sheet import material_sheet, sheet_path

    spec = MATERIALS[name]
    path = material_sheet(name, generated[name], 1, spec.tile_m, spec.use, sheet_path(tmp_path, name))
    assert path.name == f"texturas-estilizadas-{name}.png"
    with Image.open(path) as img:
        assert img.mode == "RGB" and img.width > 1000 and img.height > 1000


def test_cli_only_stylized_writes_one_sheet_per_material(tmp_path):
    from texgen.stylized import STYLIZED

    gen_textures.main(["--size", "64", "--only-stylized", "--no-legacy", "--no-palette",
                       "--out", str(tmp_path / "out"), "--stylized-sheets", str(tmp_path / "sheets")])
    for name in STYLIZED:
        assert (tmp_path / "out" / f"T_{name}_BC.png").is_file()
        assert (tmp_path / "sheets" / f"texturas-estilizadas-{name}.png").is_file()
    # Solo el juego estilizado: no genera el resto del catálogo.
    assert not (tmp_path / "out" / "T_Rope_BC.png").exists()


def test_cli_rejects_only_with_only_stylized(tmp_path):
    with pytest.raises(SystemExit):
        gen_textures.main(["--size", "64", "--only", "Rope", "--only-stylized", "--out", str(tmp_path)])
