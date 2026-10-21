import numpy as np
import pytest

from explored_island import groundfield as gf
from explored_island.layout import distance_to_paths
from explored_island.terrain import surface_height


@pytest.fixture(scope='module')
def campo(layout, terreno):
    return gf.build_field(layout, terreno)


def test_ida_y_vuelta_binaria(campo):
    leido = gf.from_bytes(gf.to_bytes(campo))
    assert (leido.nx, leido.ny, leido.cell) == (campo.nx, campo.ny, pytest.approx(campo.cell))
    assert np.array_equal(leido.verts, campo.verts)
    assert np.array_equal(leido.surface, campo.surface)
    assert np.array_equal(leido.flip, campo.flip)
    assert np.array_equal(leido.probes, campo.probes)


def test_rechaza_cabecera_o_tamano(campo):
    datos = gf.to_bytes(campo)
    with pytest.raises(ValueError):
        gf.from_bytes(b'XXXX' + datos[4:])
    with pytest.raises(ValueError):
        gf.from_bytes(datos[:-1])


def test_triangulos_iguales_a_la_malla(campo, terreno):
    """La diagonal guardada reproduce los triángulos del FBX (orient_up solo cambia el sentido)."""
    malla = {tuple(sorted(t)) for t in terreno.tris.tolist()}
    rng = np.random.default_rng(3)
    for ci, cj in zip(rng.integers(0, campo.nx - 1, 400), rng.integers(0, campo.ny - 1, 400), strict=True):
        for tri in gf.cell_triangles(campo, int(ci), int(cj)):
            assert tuple(sorted(tri.tolist())) in malla


def test_altura_exacta_de_la_malla(layout, campo, terreno):
    """Mismo triángulo que surface_height: nada de lo apoyado con el campo flota ni se entierra."""
    rng = np.random.default_rng(11)
    t = layout.terrain
    peor = 0.0
    for x, y in zip(rng.uniform(t.min_xy[0] + 2, t.max_xy[0] - 2, 600), rng.uniform(t.min_xy[1] + 2, t.max_xy[1] - 2, 600),
                    strict=True):
        hit = gf.sample(campo, float(x), float(y))
        assert hit is not None
        peor = max(peor, abs(hit[0] - surface_height(terreno, layout, float(x), float(y))))
    assert peor < 2e-3


def test_puntos_de_control(campo):
    assert campo.probes.shape[0] > 200
    for x, y, z in campo.probes[::16]:
        hit = gf.sample(campo, float(x), float(y))
        assert hit is not None and hit[0] == pytest.approx(float(z), abs=1e-3)


def test_fuera_de_la_rejilla(campo):
    assert gf.sample(campo, campo.min_xy[0] - 50.0, 0.0) is None


def test_tipos_de_suelo(layout, campo):
    cuenta = gf.surface_counts(campo)
    assert all(cuenta[n] > 0 for n in gf.SURFACES), cuenta
    x, y, z = campo.verts[:, 0], campo.verts[:, 1], campo.verts[:, 2]
    assert (z[campo.surface == gf.SEA] < 0.0).all()
    assert (distance_to_paths(layout, x, y)[campo.surface == gf.PATH] <= gf.PATH_EDGE + 1e-6).all()
    palmar = layout.zone('palmar')
    dentro = (np.hypot(x - palmar.center[0], y - palmar.center[1]) < palmar.radius * 0.6) & (z > 1.0)
    assert (campo.surface[dentro] == gf.GRASS).mean() > 0.7


def test_rechaza_malla_de_otra_rejilla(layout, terreno):
    from dataclasses import replace

    with pytest.raises(ValueError):
        gf.build_field(replace(layout, terrain=replace(layout.terrain, cell=3.0)), terreno)


def test_fichero_del_repo_al_dia(layout, campo):
    """Content/Data/ground_field.bin es el de la malla actual (si falla: uv run island-groundfield)."""
    en_disco = gf.from_bytes(gf.FIELD_BIN.read_bytes())
    assert np.array_equal(en_disco.verts, campo.verts)
    assert np.array_equal(en_disco.flip, campo.flip)
    # El tipo de suelo del fichero incluye la tierra bajo las palmeras (como el FBX): solo puede
    # diferir del de la malla sin palmeras en esas manchas.
    distinto = en_disco.surface != campo.surface
    assert distinto.mean() < 0.01


def test_cli_escribe_el_fichero(tmp_path):
    out = tmp_path / 'campo.bin'
    assert gf.main(['--out', str(out)]) == 0
    assert gf.from_bytes(out.read_bytes()).probes.shape[0] > 200
