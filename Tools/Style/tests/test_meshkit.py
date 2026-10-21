import pytest
from mathutils import Matrix, Vector

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


@pytest.mark.parametrize('frecuencia', [1, 2, 3])
def test_geosphere_cerrada_y_hacia_fuera(escena_vacia, frecuencia):
    verts, caras = meshkit.geosphere(frecuencia)
    assert len(caras) == 20 * frecuencia ** 2
    assert len(verts) == 10 * frecuencia ** 2 + 2   # Euler: V - E + F = 2
    assert all(abs(v.length - 1.0) < 1e-5 for v in verts)
    b = MeshBuilder()
    meshkit.add_polys(b, verts, caras, lambda n, c: 2 if n.dot(c) > 0 else 9)
    obj = meshkit.build_object('SM_Geo', b)
    assert set(meshkit.face_colors(obj)) == {2}
    assert meshkit.flipped_face_count(obj) == 0
    with pytest.raises(ValueError):
        meshkit.geosphere(0)


def test_face_normal_y_recorte_por_planos():
    tri = [Vector((0, 0, 0)), Vector((1, 0, 0)), Vector((0, 1, 0))]
    assert meshkit.face_normal(tri).to_tuple() == pytest.approx((0.0, 0.0, 1.0))
    assert meshkit.face_normal([Vector((0, 0, 0))] * 3).length == 0.0
    planos = [(Vector((0.0, 0.0, 1.0)), 0.5), (Vector((1.0, 0.0, 0.0)), 0.25)]
    p = Vector((1.0, 0.2, 2.0))
    assert meshkit.clamp_to_planes(p, planos).to_tuple() == pytest.approx((0.25, 0.2, 0.5))
    assert p.to_tuple() == pytest.approx((1.0, 0.2, 2.0))   # no muta la entrada
