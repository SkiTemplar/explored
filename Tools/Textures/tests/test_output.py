"""Codificación PNG, hoja de contacto y utilidades de salida (texgen/output.py)."""

import numpy as np
import pytest
from PIL import Image, ImageDraw

from texgen import output
from texgen.noise import sample, uv_grid
from texgen.output import contact_sheet, encode, lit_preview, to_u8, write_manifest


def test_to_u8_redondea_y_recorta():
    np.testing.assert_array_equal(to_u8(np.array([-0.5, 0.0, 0.5 / 255, 0.5, 1.0, 2.0])),
                                  [0, 0, 1, 128, 255, 255])


@pytest.mark.parametrize(("suffix", "shape", "mode"), [
    ("BC", (4, 4, 3), "RGB"), ("BC", (4, 4, 4), "RGBA"), ("M", (4, 4, 4), "RGBA"),
    ("ARH", (4, 4, 3), "RGB"), ("H", (4, 4), "L"),
])
def test_encode_elige_el_modo_por_canales(suffix, shape, mode):
    assert encode(suffix, np.full(shape, 0.5)).mode == mode


def test_encode_normal_mapea_menos_uno_uno_a_0_255():
    n = np.zeros((2, 2, 3))
    n[..., 2] = 1.0
    n[0, 0] = (-1.0, 1.0, 0.0)
    px = np.asarray(encode("N", n))
    assert tuple(px[1, 1]) == (128, 128, 255)
    assert tuple(px[0, 0]) == (0, 255, 128)


def test_manifiesto_ordenado_y_con_salto_final(tmp_path):
    path = write_manifest(tmp_path, {"T_B": {"srgb": True, "kind": "color"}, "T_A": {"srgb": False, "kind": "n"}})
    text = path.read_text(encoding="utf-8")
    assert text.endswith("}\n")
    assert text.index('"T_A"') < text.index('"T_B"')
    assert text.index('"kind"') < text.index('"srgb"')


def test_lit_preview_normal_plana_y_rango():
    albedo = np.full((8, 8, 3), 0.5)
    flat = np.zeros((8, 8, 3))
    flat[..., 2] = 1.0
    lit = lit_preview(albedo, flat, None, None)
    assert lit.shape == (8, 8, 3)
    assert np.allclose(lit, lit[0, 0]), "normal plana: iluminación uniforme"
    assert 0.0 <= lit.min() and lit.max() <= 1.0
    # Más oclusión oscurece.
    dark = lit_preview(albedo, flat, np.zeros((8, 8)), None)
    assert (dark < lit).all()


def test_fuente_de_reserva_si_no_hay_ninguna_ttf(monkeypatch):
    real = output.ImageFont.truetype
    tried = []

    def no_font(font, *a, **k):
        if isinstance(font, str):  # rutas del sistema: ninguna existe
            tried.append(font)
            raise OSError("sin fuente")
        return real(font, *a, **k)  # load_default() usa su fuente embebida

    monkeypatch.setattr(output.ImageFont, "truetype", no_font)
    font = output._font(20)
    assert len(tried) == 3
    ImageDraw.Draw(Image.new("RGB", (40, 20))).text((0, 0), "ñ", font=font)


def _cards(n, size=32, seed=0):
    rng = np.random.default_rng(seed)
    return [{"name": f"M{i}", "info": "x", "lit": rng.random((size, size, 3)),
             "thumbs": [rng.random((size, size, 4)), rng.random((size, size)), rng.random((size, size, 3)),
                        rng.random((size, size, 3))]}
            for i in range(n)]


def test_hoja_de_contacto_dimensiones(tmp_path):
    path = contact_sheet(_cards(6), tmp_path / "sub" / "hoja.png", "título", cols=4, big=80)
    img = Image.open(path)
    pad, card_h = 14, 44 + 80 + 20
    assert img.size == (pad + 4 * (80 + pad), 56 + 2 * (card_h + pad) + pad)


def test_hoja_de_contacto_reduce_bits_si_pesa_demasiado(tmp_path, monkeypatch):
    """Por encima del límite se posteriza (6 y luego 5 bits por canal) para poder versionarla."""
    monkeypatch.setattr(output, "SHEET_MAX_BYTES", 1)
    px = np.asarray(Image.open(contact_sheet(_cards(2), tmp_path / "h.png", "t", cols=2, big=40)))
    assert (px & 0b111 == 0).all(), "5 bits por canal"
    monkeypatch.setattr(output, "SHEET_MAX_BYTES", 10**9)
    px = np.asarray(Image.open(contact_sheet(_cards(2), tmp_path / "h.png", "t", cols=2, big=40)))
    assert (px & 0b111 != 0).any(), "bajo el límite no se toca"


def test_sample_multicanal_igual_que_por_canal_y_periodico():
    rng = np.random.default_rng(3)
    field = rng.random((16, 16, 3))
    u, v = uv_grid(16)
    u, v = u * 1.37 + 0.11, v * 0.8 - 0.3
    got = sample(field, u, v)
    for c in range(3):
        np.testing.assert_allclose(got[..., c], sample(field[..., c], u, v), atol=1e-12)
    np.testing.assert_allclose(sample(field, u + 2.0, v - 1.0), got, atol=1e-12)
    # En los centros de píxel reproduce el campo.
    cu, cv = uv_grid(16)
    np.testing.assert_allclose(sample(field, cu, cv), field, atol=1e-12)
