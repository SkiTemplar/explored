import numpy as np
import pytest

from explored_style import raster


def test_hex_to_rgb_y_errores():
    assert raster.hex_to_rgb('#E8D8A8') == (232, 216, 168)
    for malo in ('E8D8A8', '#E8D8A', '#GGGGGG'):
        with pytest.raises(ValueError):
            raster.hex_to_rgb(malo)


def test_luminancia_ordena_claro_y_oscuro():
    assert raster.luminance((255, 255, 255)) == pytest.approx(1.0)
    assert raster.luminance((0, 0, 0)) == 0.0


def test_lienzo_y_rectangulos_no_mutan_el_original():
    base = raster.new_canvas(4, 3, (1, 2, 3))
    con_rect = raster.fill_rect(base, 1, 1, 10, 10, (9, 9, 9))
    assert base[1, 1].tolist() == [1, 2, 3]
    assert con_rect[2, 3].tolist() == [9, 9, 9]
    assert con_rect[0, 0].tolist() == [1, 2, 3]
    with pytest.raises(ValueError):
        raster.new_canvas(0, 3, (0, 0, 0))


def test_paste_recorta_en_los_bordes():
    base = raster.new_canvas(4, 4, (0, 0, 0))
    parche = raster.new_canvas(3, 3, (5, 5, 5))
    out = raster.paste(base, parche, 2, 2)
    assert out[3, 3].tolist() == [5, 5, 5]
    assert out[1, 1].tolist() == [0, 0, 0]
    assert raster.paste(base, parche, 9, 9).tolist() == base.tolist()


def test_texto_pinta_glifos_y_mide_ancho():
    base = raster.new_canvas(40, 10, (0, 0, 0))
    out = raster.draw_text(base, 'a1?', 1, 1, 1, (255, 255, 255))
    assert out.sum() > 0
    assert raster.text_width('ab', 2) == 14
    assert raster.text_width('', 2) == 0


def test_png_ida_y_vuelta():
    lienzo = np.arange(5 * 7 * 3, dtype=np.uint8).reshape(5, 7, 3)
    assert np.array_equal(raster.decode_png(raster.encode_png(lienzo)), lienzo)


def test_png_rechaza_entradas_no_validas():
    with pytest.raises(ValueError):
        raster.encode_png(np.zeros((2, 2, 4), dtype=np.uint8))
    with pytest.raises(ValueError):
        raster.decode_png(b'no es png')


def test_write_png_crea_carpetas(tmp_path):
    ruta = raster.write_png(tmp_path / 'a' / 'b.png', raster.new_canvas(2, 2, (1, 1, 1)))
    assert ruta.read_bytes().startswith(b'\x89PNG')
