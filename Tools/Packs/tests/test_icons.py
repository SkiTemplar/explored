"""Tests de icons.py: normalización de siluetas y colores de ExploredUIStyle.h."""

from __future__ import annotations

from PIL import Image

import icons


def _silueta(size: int, box: tuple[int, int, int, int]) -> Image.Image:
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    im.paste((10, 20, 30, 255), box)
    return im


def test_normaliza_a_silueta_blanca_centrada_con_margen() -> None:
    out = icons.normalize(_silueta(100, (5, 40, 45, 60)))
    assert out.size == (icons.SIZE, icons.SIZE)
    x0, y0, x1, y1 = out.getchannel("A").getbbox()
    margin = round(icons.SIZE * icons.MARGIN)
    assert abs(x0 - margin) <= 1 and abs((icons.SIZE - x1) - margin) <= 1
    assert abs((y0 + y1) / 2 - icons.SIZE / 2) <= 1
    assert out.getpixel((icons.SIZE // 2, icons.SIZE // 2)) == (255, 255, 255, 255)


def test_iconos_de_distinto_tamano_salen_iguales() -> None:
    # Misma proporción (4:3) a 40 × 30 y a 160 × 120 px.
    a = icons.normalize(_silueta(64, (10, 10, 50, 40)))
    b = icons.normalize(_silueta(200, (30, 50, 190, 170)))
    # Mismo recuadro salvo el redondeo del remuestreo (1 px).
    ba, bb = a.getchannel("A").getbbox(), b.getchannel("A").getbbox()
    assert all(abs(p - q) <= 1 for p, q in zip(ba, bb)), (ba, bb)


def test_icono_vacio_falla() -> None:
    try:
        icons.normalize(Image.new("RGBA", (8, 8)))
    except ValueError:
        return
    raise AssertionError("un icono sin alfa debe fallar")


def test_colores_del_estilo_de_la_ui() -> None:
    colors = icons.style_colors()
    assert {"ColorAccent", "ColorDisabled", "ColorInkDim", "ColorPaper"} <= colors.keys()
    assert all(0 <= v <= 255 for c in colors.values() for v in c)


def test_tinte_multiplica_el_blanco() -> None:
    icon = icons.normalize(_silueta(32, (0, 0, 32, 32)))
    rgb = icons.tinted(icon, (200, 100, 50, 255), (0, 0, 0))
    assert rgb.getpixel((icons.SIZE // 2, icons.SIZE // 2)) == (200, 100, 50)
