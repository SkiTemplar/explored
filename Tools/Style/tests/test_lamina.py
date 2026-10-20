import shutil

import bpy

import numpy as np
import pytest

from explored_style import lamina, raster
from explored_style.palette import load_palette


def test_load_rgba_no_convierte_color(tmp_path):
    lienzo = raster.new_canvas(3, 2, (10, 20, 30))
    lienzo = raster.fill_rect(lienzo, 0, 0, 1, 1, (200, 100, 50))
    rgba = lamina.load_rgba(raster.write_png(tmp_path / 'p.png', lienzo))
    assert rgba.shape == (2, 3, 4)
    assert np.allclose(rgba[0, 0, :3] * 255, (200, 100, 50), atol=0.5)  # fila 0 = arriba


def test_over_background_y_fit():
    rgba = np.zeros((2, 2, 4), dtype=np.float32)
    rgba[0, 0] = (1.0, 0.0, 0.0, 1.0)
    out = lamina.over_background(rgba, (0, 0, 255))
    assert out[0, 0].tolist() == [255, 0, 0]
    assert out[1, 1].tolist() == [0, 0, 255]
    assert lamina._fit(out, (4, 4)).shape == (4, 4, 3)
    assert lamina._fit(out, (2, 2)) is out


def test_compose_tamano_y_tira():
    pal = load_palette()
    panel = raster.new_canvas(10, 10, (1, 2, 3))
    canvas = lamina.compose({'general': panel, 'cuchillo': panel, 'barril': panel},
                            {'SM_PalmA': 1, 'SM_Cuchillo': 2, 'SM_BarrilLluvia': 3}, pal)
    assert canvas.shape == (lamina.HEIGHT, lamina.WIDTH, 3)
    assert tuple(canvas[lamina.PANEL_H + 10, 20]) == pal.colors[0].rgb


def test_material_y_enums(escena_vacia):
    img = bpy.data.images.new('hoja', 32, 1)
    mat = lamina.toon_preview_material(img)
    assert mat.node_tree is not None
    tipos = {n.type for n in mat.node_tree.nodes}
    assert {'TEX_IMAGE', 'SHADERTORGB', 'VALTORGB', 'EMISSION'} <= tipos
    with pytest.raises(RuntimeError):
        lamina._pick(bpy.types.Light, 'type', 'LASER')
    with pytest.raises(RuntimeError):
        lamina._set_engine(bpy.context.scene, 'MOTOR_INEXISTENTE')


def test_main_sin_fbx_devuelve_2(tmp_path, capsys):
    assert lamina.main(['--export-dir', str(tmp_path), '--out', str(tmp_path / 'l.png')]) == 2
    assert 'faltan FBX' in capsys.readouterr().out


def test_lamina_workbench_reducida(fbx_pilotos, tmp_path):
    carpeta = tmp_path / 'fbx'
    shutil.copytree(fbx_pilotos, carpeta)
    salida = tmp_path / 'lamina.png'
    args = ['--export-dir', str(carpeta), '--out', str(salida), '--engine', 'BLENDER_WORKBENCH', '--percent', '10']
    assert lamina.main(args) == 0
    imagen = raster.decode_png(salida.read_bytes())
    assert imagen.shape == (lamina.HEIGHT, lamina.WIDTH, 3)
