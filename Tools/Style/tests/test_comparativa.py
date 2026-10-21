import numpy as np

from explored_style import comparativa, raster


def _png(path, color, w=40, h=20):
    img = np.zeros((h, w, 3), dtype=np.uint8)
    img[:, :] = color
    raster.write_png(path, img)


def test_mitad_promedia_bloques():
    img = np.zeros((3, 4, 3), dtype=np.uint8)
    img[0, 0] = (200, 200, 200)
    out = comparativa.half(img)
    assert out.shape == (1, 2, 3) and tuple(out[0, 0]) == (50, 50, 50)


def test_lamina_con_y_sin_antes(tmp_path):
    _png(tmp_path / 'f4-cala.png', (255, 0, 0))
    _png(tmp_path / 'f4b-cala.png', (0, 255, 0))
    _png(tmp_path / 'f4b-ciclon.png', (0, 0, 255))
    lamina = comparativa.build('f4', 'f4b', tmp_path, ('cala', 'palmar', 'ciclon'))
    cell_w = 20
    assert lamina.shape[1] == 2 * cell_w + 3 * comparativa.PAD
    fila = comparativa.LABEL_H + comparativa.PAD + comparativa.LABEL_H + 2
    assert tuple(lamina[fila, comparativa.PAD + 2]) == (255, 0, 0)
    assert tuple(lamina[fila, 2 * comparativa.PAD + cell_w + 2]) == (0, 255, 0)


def test_main(tmp_path, capsys):
    _png(tmp_path / 'f4b-cala.png', (0, 255, 0))
    out = tmp_path / 'l.png'
    assert comparativa.main(['--capturas', str(tmp_path), '--out', str(out)]) == 0
    assert out.is_file()
    assert comparativa.main(['--capturas', str(tmp_path / 'nada'), '--out', str(out)]) == 2
    assert 'ERROR' in capsys.readouterr().out
