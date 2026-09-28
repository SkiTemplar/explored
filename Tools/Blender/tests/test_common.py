"""Utilidades puras y de escena de lib/common.py, más una comprobación
estática del orden de imports (bpy antes que bmesh/mathutils)."""

import ast
import math

import bpy

import pytest
from conftest import BLENDER_DIR, importar
from mathutils import Matrix, Vector


def test_height_mask_recorta_y_curva():
    common = importar('common')
    assert common.height_mask(5.0, 0.0, 10.0) == 0.5
    assert common.height_mask(-1.0, 0.0, 10.0) == 0.0
    assert common.height_mask(99.0, 0.0, 10.0) == 1.0
    assert common.height_mask(5.0, 0.0, 10.0, curve=2.0) == pytest.approx(0.25)
    assert common.height_mask(5.0, 3.0, 3.0) == 0.0  # rango vacío


def test_atlas_uv_rect_invierte_la_fila():
    common = importar('common')
    for fila, celdas in enumerate(common.ATLAS_LAYOUT):
        for col, celda in enumerate(celdas):
            u0, v0, u1, v1 = common.atlas_uv_rect(celda)
            assert (u0, u1) == (col / common.ATLAS_COLS, (col + 1) / common.ATLAS_COLS)
            # la fila 0 de la imagen es la de arriba: V más alto en Blender
            assert v1 == (common.ATLAS_ROWS - fila) / common.ATLAS_ROWS
            assert v1 - v0 == pytest.approx(1 / common.ATLAS_ROWS)


def test_tintes_con_jitter_cacheado_por_vertice():
    """El jitter se cachea por índice de vértice: el mismo vértice pintado
    desde varias esquinas da el mismo color (sin costuras)."""
    common = importar('common')
    rnd = common.seeded_rng(1)

    class V:
        def __init__(self, i, z):
            self.index = i
            self.co = Vector((0.0, 0.0, z))

    fn = common.tint_along_axis((0.5, 0.5, 0.5), 'z', 0.0, 2.0, jitter=0.1, rnd=rnd)
    a, b = fn(V(3, 1.0)), fn(V(3, 1.0))
    assert a == b and a[3] == 0.5
    fijo = common.constant_tint((2.0, -1.0, 0.5), alpha=0.7, jitter=0.0)
    assert fijo(V(0, 0.0)) == (1.0, 0.0, 0.5, 0.7)


def test_triangle_count_y_dimensiones_cm():
    common = importar('common')
    common.reset_scene()
    caja = common.make_box('Caja', (2.0, 1.0, 0.5))
    assert common.triangle_count(caja) == 12
    assert common.dimensions_cm(caja) == pytest.approx((200.0, 100.0, 50.0))
    vacio = bpy.data.objects.new('Vacio', bpy.data.meshes.new('Vacio'))
    assert common.dimensions_cm(vacio) == (0.0, 0.0, 0.0)


@pytest.mark.parametrize('funcion', ['orient_and_place', 'orient_and_place_zaxis'])
def test_orient_and_place_equivale_a_transform_apply(funcion):
    """Regresión del cambio de rendimiento: hornear la matriz a mano deja
    los mismos vértices que asignar matrix_world + transform_apply."""
    common = importar('common')
    origen, direccion, arriba = (1.0, -2.0, 3.0), (0.3, 0.8, 0.5), (0.0, 0.2, 1.0)
    common.reset_scene()
    hoja = common.make_leaf_blade('Hoja', 1.0, 0.3, 0.05, 0.1, segments=6)
    getattr(common, funcion)(hoja, origen, direccion, arriba)
    rapido = [tuple(v.co) for v in hoja.data.vertices]
    assert hoja.matrix_world == Matrix.Identity(4)
    assert bpy.context.view_layer.objects.active == hoja

    # referencia: la misma matriz aplicada con el operador de Blender
    common.reset_scene()
    ref = common.make_leaf_blade('Hoja', 1.0, 0.3, 0.05, 0.1, segments=6)
    capturada = {}
    original = common._bake_matrix
    try:
        common._bake_matrix = lambda obj, mat: capturada.setdefault('m', mat.copy())
        getattr(common, funcion)(ref, origen, direccion, arriba)
    finally:
        common._bake_matrix = original
    ref.matrix_world = capturada['m']
    common.select_only(ref)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for a, b in zip(rapido, (tuple(v.co) for v in ref.data.vertices), strict=True):
        assert a == pytest.approx(b, abs=1e-5)
    assert capturada['m'].determinant() == pytest.approx(1.0)


def test_orient_and_place_con_vectores_degenerados():
    common = importar('common')
    common.reset_scene()
    hoja = common.make_leaf_blade('Hoja', 1.0, 0.3, 0.05, 0.1, segments=4)
    # forward nulo -> +Y; up paralelo a forward -> se elige otro up
    common.orient_and_place(hoja, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 1.0, 0.0))
    assert all(math.isfinite(c) for v in hoja.data.vertices for c in v.co)
    tronco = common.make_leaf_blade('Tronco', 1.0, 0.3, 0.05, 0.1, segments=4)
    common.orient_and_place_zaxis(tronco, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), up_hint=(0.0, 0.0, 1.0))
    assert all(math.isfinite(c) for v in tronco.data.vertices for c in v.co)


def test_get_material_con_y_sin_texturas(texturas_sinteticas, monkeypatch, tmp_path):
    common = importar('common')
    common.reset_scene()
    hoja = common.get_material('M_Leaf')
    bsdf = hoja.node_tree.nodes['Principled BSDF']
    assert bsdf.inputs['Alpha'].is_linked  # recorte alfa del atlas
    assert not hoja.use_backface_culling
    imagenes = {n.image.name for n in hoja.node_tree.nodes if n.type == 'TEX_IMAGE'}
    assert imagenes == {'T_FoliageAtlas_BC', 'T_FoliageAtlas_N'}
    assert common.get_material('M_Leaf') is hoja  # reutiliza por nombre
    common.reset_scene()
    monkeypatch.setattr(common, 'TEXTURES_DIR', str(tmp_path))  # sin texturas
    roca = common.get_material('M_Rock')
    assert not [n for n in roca.node_tree.nodes if n.type == 'TEX_IMAGE']
    corteza = common.get_material('M_Bark')
    assert not corteza.node_tree.nodes['Principled BSDF'].inputs['Alpha'].is_linked


def test_purge_orphans_borra_lo_huerfano():
    common = importar('common')
    common.reset_scene()
    bpy.data.meshes.new('Huerfana')
    bpy.data.materials.new('Huerfano')
    common.purge_orphans()
    assert 'Huerfana' not in bpy.data.meshes
    assert 'Huerfano' not in bpy.data.materials


def _modulos_del_kit():
    return {p.stem for p in BLENDER_DIR.rglob('*.py')
            if '.venv' not in p.parts and 'tests' not in p.parts}


def test_bpy_se_importa_antes_que_bmesh_y_mathutils():
    """Con el bpy de PyPI, bmesh/mathutils solo se pueden importar después
    de bpy. Cada script debe importar bpy (o un módulo del kit, que ya lo
    importa) antes que ellos a nivel de módulo."""
    propios = _modulos_del_kit()
    for ruta in BLENDER_DIR.rglob('*.py'):
        if '.venv' in ruta.parts or 'tests' in ruta.parts:
            continue
        hay_bpy = False
        for nodo in ast.parse(ruta.read_text(encoding='utf-8')).body:
            if isinstance(nodo, ast.Import):
                nombres = [a.name.split('.')[0] for a in nodo.names]
            elif isinstance(nodo, ast.ImportFrom) and nodo.module:
                nombres = [nodo.module.split('.')[0]]
            else:
                continue
            for n in nombres:
                if n in ('bmesh', 'mathutils', 'bpy_extras'):
                    assert hay_bpy, f'{ruta.relative_to(BLENDER_DIR)}: importa {n} antes que bpy'
                if n == 'bpy' or n in propios:
                    hay_bpy = True
