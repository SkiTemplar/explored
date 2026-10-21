import copy
import json

import pytest

from explored_style import palette, raster
from explored_style.palette import GDD_COLORS, MAX_COLORS, PALETTE_SIZE


@pytest.fixture(scope='module')
def datos():
    return json.loads(palette.PALETTE_PATH.read_text(encoding='utf-8'))


def test_paleta_real_cumple_el_gdd():
    pal = palette.load_palette()
    assert PALETTE_SIZE <= len(pal) <= MAX_COLORS
    for i, (nombre, hex_value) in enumerate(GDD_COLORS):
        assert (pal.colors[i].name, pal.colors[i].hex) == (nombre, hex_value)
    familias = {c.family for c in pal.colors}
    assert {'arena', 'verde', 'madera', 'roca', 'agua', 'cielo', 'acento', 'ui'} <= familias
    assert pal.index_of('tinta') == 31
    with pytest.raises(KeyError):
        pal.by_name('fucsia')


@pytest.mark.parametrize('rotura', ['sin_lista', 'faltan', 'repetido', 'gdd', 'hex', 'hueco'])
def test_paleta_no_valida_se_rechaza(datos, rotura):
    d = copy.deepcopy(datos)
    if rotura == 'sin_lista':
        del d['colores']
    elif rotura == 'faltan':
        d['colores'] = d['colores'][:PALETTE_SIZE - 1]
    elif rotura == 'repetido':
        d['colores'][20]['hex'] = d['colores'][21]['hex']
    elif rotura == 'gdd':
        d['colores'][0]['hex'] = '#FFFFFF'
    elif rotura == 'hex':
        d['colores'][25]['hex'] = 'azul'
    else:
        d['colores'][31]['indice'] = 70
    with pytest.raises(ValueError):
        palette.parse_palette(d)


def test_celdas_uv():
    assert palette.cell_uv(0) == (0.5 / 32, 0.5)
    assert palette.cell_of_u(palette.cell_uv(17)[0]) == 17
    assert palette.cell_of_u(3 / 32) is None          # en el borde entre celdas
    assert palette.cell_of_u(1.2) is None             # fuera de la hoja
    assert palette.cell_of_u(-0.01) is None
    with pytest.raises(ValueError):
        palette.cell_uv(MAX_COLORS)


def test_celdas_ampliadas_conservan_la_fila_base():
    # Las mallas exportadas con la hoja 32 x 1 tienen v = 0,5: sigue siendo la fila de 0-31.
    assert palette.cell_uv(17) == (17.5 / 32, 0.5)
    u, v = palette.cell_uv(33)
    assert (u, v) == (1.5 / 32, 1.0 - 0.5 / 3)
    assert palette.cell_of_uv(u, v) == 33
    assert palette.cell_of_uv(17.5 / 32, 0.5) == 17
    assert palette.cell_of_uv(17.5 / 32, 1.0 / 6.0) is None   # fila inferior: solo copia
    assert palette.cell_of_uv(17.5 / 32, 2.0 / 3.0) is None   # borde entre filas
    assert palette.cell_of_uv(3 / 32, 0.5) is None


def test_hojas_png(tmp_path):
    pal = palette.load_palette()
    hoja, doc = palette.write_sheets(pal, tmp_path / 'T_Paleta.png', tmp_path / 'doc.png')
    pixeles = raster.decode_png(hoja.read_bytes())
    assert pixeles.shape == (3, 32, 3)
    assert tuple(pixeles[1, 9]) == raster.hex_to_rgb('#C8402F')
    assert tuple(pixeles[2, 9]) == raster.hex_to_rgb('#C8402F')
    assert tuple(pixeles[0, 0]) == pal.by_name('blanco_nube').rgb
    assert raster.decode_png(doc.read_bytes()).shape[1] > 1000
    tira = palette.strip_canvas(pal, 320, 40)
    assert tuple(tira[2, 5]) == pal.colors[0].rgb


def test_main(tmp_path, capsys):
    assert palette.main(['--sheet', str(tmp_path / 's.png'), '--doc', str(tmp_path / 'd.png')]) == 0
    assert (tmp_path / 's.png').is_file()
    malo = tmp_path / 'mala.json'
    malo.write_text('{"colores": []}', encoding='utf-8')
    assert palette.main(['--palette', str(malo), '--sheet', str(tmp_path / 'x.png')]) == 2
    assert 'ERROR' in capsys.readouterr().out
