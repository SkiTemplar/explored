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
