import math

import pytest
from mathutils import Matrix

from explored_style import meshkit
from explored_style.meshkit import MeshBuilder, lathe, mesh_of


def test_builder_rechaza_ngones_y_colores_fuera_de_rango():
    b = MeshBuilder()
    ids = [b.vert((k, 0, 0)) for k in range(5)]
    with pytest.raises(ValueError):
        b.face(ids, 0)
    with pytest.raises(ValueError):
        b.face(ids[:3], 32)


def test_lathe_valida_colores_y_cierra_con_puntas():
    b = MeshBuilder()
    with pytest.raises(ValueError):
        lathe(b, [(1, 0), (1, 1)], 6, [0, 0])
    rings = lathe(b, [(1, 0), (1, 1)], 6, [3], cap_bottom=3, cap_top=3, tips=(-0.5, 1.5))
    assert len(rings) == 2
    assert b.triangle_count == 24
    assert min(v[2] for v in b.verts) == -0.5


def test_extend_transforma():
    a, b = MeshBuilder(), MeshBuilder()
    b.face([b.vert((0, 0, 0)), b.vert((1, 0, 0)), b.vert((0, 1, 0))], 2)
    a.extend(b, Matrix.Translation((0.0, 0.0, 2.0)))
    a.extend(b)
    assert a.verts[0][2] == 2.0
    assert a.faces[1] == (3, 4, 5)


def test_objeto_cerrado_con_normales_hacia_fuera(escena_vacia):
    b = MeshBuilder()
    lathe(b, [(0.5, 0.2), (0.5, 1.0)], 8, [4], cap_bottom=4, cap_top=4)
    hoop = MeshBuilder()
    lathe(hoop, [(1.0, 0.0), (1.2, 0.0), (1.2, 0.1), (1.0, 0.1)], 8, [5] * 4, closed=True)
    b.extend(hoop)
    obj = meshkit.build_object('SM_Prueba', b)
    assert min(v.co.z for v in mesh_of(obj).vertices) == 0.0
    assert meshkit.flipped_face_count(obj) == 0
    assert meshkit.island_count(obj) == 2
    assert set(meshkit.face_colors(obj)) == {4, 5}
    assert not any(p.use_smooth for p in mesh_of(obj).polygons)


def test_objeto_vacio_falla(escena_vacia):
    with pytest.raises(ValueError):
        meshkit.build_object('SM_Vacio', MeshBuilder())


def test_sweep_valida_argumentos():
    b = MeshBuilder()
    with pytest.raises(ValueError):
        meshkit.sweep(b, [(0, 0, 0)], [0.1], 4, 0)
    with pytest.raises(ValueError):
        meshkit.sweep(b, [(0, 0, 0), (1, 0, 0)], [0.1], 4, 0)
    with pytest.raises(ValueError):
        meshkit.sweep(b, [(0, 0, 0), (1, 0, 0)], [0.1, 0.1], 4, [0, 0])
    with pytest.raises(ValueError):
        meshkit.sweep(b, [(0, 0, 0), (1, 0, 0), (1, 1, 0)], [0.1] * 3, 4, 0, closed=True, cap_start=0)
    with pytest.raises(ValueError):
        meshkit.sweep(b, [(0, 0, 0), (0, 0, 1)], [0.1, 0.1], 4, 0)  # tangente paralela a «up»


def test_sweep_eliptico_y_tramo_vertical(escena_vacia):
    b = MeshBuilder()
    pts = [(0, 0, 0.2), (0.5, 0, 0.2), (0.5, 0, 0.8)]  # el último tramo es paralelo a «up»
    rings = meshkit.sweep(b, pts, [(0.1, 0.05), (0.1, 0.05), 0.05], 6, [3, [4, 5, 4, 5, 4, 5]],
                          cap_start=3, cap_end=4, tips=(None, (0.5, 0.0, 0.9)))
    assert len(rings) == 3
    assert b.triangle_count == 2 * 6 * 2 + 2 * 6
    z = [b.verts[i][2] for i in rings[0]]
    assert max(z) - min(z) == pytest.approx(0.2)  # radio_u sigue a «up»
    obj = meshkit.build_object('SM_Tubo', b)
    assert meshkit.flipped_face_count(obj) == 0
    assert set(meshkit.face_colors(obj)) == {3, 4, 5}


def test_sweep_cerrado_es_un_aro(escena_vacia):
    b = MeshBuilder()
    loop = [(0.0, math.cos(a), 0.5 + math.sin(a)) for a in (2 * math.pi * k / 8 for k in range(8))]
    meshkit.sweep(b, loop, [0.05] * 8, 4, 2, up=(1.0, 0.0, 0.0), closed=True)
    obj = meshkit.build_object('SM_Aro', b)
    assert meshkit.triangle_count(obj) == 8 * 4 * 2
    assert meshkit.flipped_face_count(obj) == 0
    assert meshkit.island_count(obj) == 1


def test_slab_con_y_sin_centro(escena_vacia):
    b = MeshBuilder()
    with pytest.raises(ValueError):
        meshkit.slab(b, [(0, 0, 0), (1, 0, 0)], (0, 0, 0.1), 0)
    meshkit.slab(b, [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)], (0, 0, 0.1), 1, rim=2)
    estrella = [(2.0, 0.0, 0.0), (3.0, 0.4, 0.0), (2.4, 0.5, 0.0), (3.0, 0.9, 0.0), (2.0, 1.0, 0.0)]
    meshkit.slab(b, estrella, (0, 0, 0.1), 3, center=(2.3, 0.5, 0.0))
    assert b.triangle_count == (2 + 2 + 8) + (5 + 5 + 10)
    obj = meshkit.build_object('SM_Placas', b)
    assert meshkit.flipped_face_count(obj) == 0
    assert meshkit.island_count(obj) == 2
    assert set(meshkit.face_colors(obj)) == {1, 2, 3}
