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
