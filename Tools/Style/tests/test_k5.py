import json
import shutil

import bpy  # noqa: F401  (debe importarse antes que mathutils)

import pytest

from explored_style import check_style, meshkit, raster
from explored_style.kits import k5, k5_formas, k5_lamina
from explored_style.kits.k5_formas import C
from explored_style.meshkit import mesh_of

NUBES = [s for s in k5.K5 if s.is_cloud]
DISCOS = [s for s in k5.K5 if not s.is_cloud]
ANCHO = {'SM_Nube_A': 20.0, 'SM_Nube_B': 30.0, 'SM_Nube_C': 40.0, 'SM_Nube_D': 50.0, 'SM_Nube_E': 60.0}


def test_catalogo_completo():
    assert [s.name for s in NUBES] == list(ANCHO)
    assert [s.name for s in DISCOS] == ['SM_DiscoSol', 'SM_DiscoLuna']
    assert all(s.asset.endswith('_01') for s in k5.K5)
    assert {s.max_tris for s in NUBES} == {400} and {s.max_tris for s in DISCOS} == {64}
    assert k5.spec_named('SM_DiscoSol').max_tris == 64
    with pytest.raises(KeyError):
        k5.spec_named('SM_Inexistente')


@pytest.mark.parametrize('spec', k5.K5, ids=lambda s: s.name)
def test_malla_cumple_estilo_y_presupuesto(escena_vacia, spec):
    obj = k5.generate(spec)
    report = check_style.check_object(obj, spec.max_tris)
    assert report.ok, report.errors
    assert not report.warnings
    assert 0 < report.triangles <= spec.max_tris
    if spec.is_cloud:
        assert report.triangles == k5_formas.NUBES[spec.name].triangles


@pytest.mark.parametrize('spec', NUBES, ids=lambda s: s.name)
def test_nube_forma_colores_y_pivote(escena_vacia, spec):
    obj = k5.generate(spec)
    dims = obj.dimensions
    assert dims.x == pytest.approx(ANCHO[spec.name], abs=0.01)
    assert 0.3 <= dims.z / dims.x <= 0.6  # cúmulo hinchado, ni torta ni torre
    assert 3 <= meshkit.island_count(obj) == len(k5_formas.NUBES[spec.name].lobes) <= 7
    assert meshkit.flipped_face_count(obj) == 0
    cos = [v.co for v in mesh_of(obj).vertices]
    assert (min(c.x for c in cos) + max(c.x for c in cos)) / 2 == pytest.approx(0.0, abs=0.01)
    assert (min(c.y for c in cos) + max(c.y for c in cos)) / 2 == pytest.approx(0.0, abs=0.01)
    colores = meshkit.face_colors(obj)
    assert set(colores) == {C['papel'], C['acero']}
    caras = list(zip(mesh_of(obj).polygons, colores, strict=True))
    base = [c for p, c in caras if p.center.z < 1e-4]
    assert base and set(base) == {C['acero']}  # base plana y gris
    techo = max(caras, key=lambda pc: pc[0].center.z)
    assert techo[1] == C['papel']
    blancas = sum(p.area for p, c in caras if c == C['papel'])
    assert blancas > 0.5 * sum(p.area for p, _ in caras)


@pytest.mark.parametrize('spec', DISCOS, ids=lambda s: s.name)
def test_disco_plano_con_normal_z(escena_vacia, spec):
    obj = k5.generate(spec)
    assert obj.dimensions.z == pytest.approx(0.0, abs=1e-6)
    assert obj.dimensions.x == pytest.approx(1.0, abs=1e-3) and obj.dimensions.y == pytest.approx(1.0, abs=1e-3)
    assert all(p.normal.z > 0.999 for p in mesh_of(obj).polygons)
    assert len(set(meshkit.face_colors(obj))) == 2


def test_colores_de_sol_y_luna(escena_vacia):
    sol = k5.generate(k5.spec_named('SM_DiscoSol'))
    luna = k5.generate(k5.spec_named('SM_DiscoLuna'))
    assert set(meshkit.face_colors(sol)) == {C['laton'], C['cielo_ocaso']}
    assert set(meshkit.face_colors(luna)) == {C['papel'], C['acero']}
    # la sombra de la luna queda en el lado opuesto a la luz (abajo a la derecha)
    grises = [p.center for p, c in zip(mesh_of(luna).polygons, meshkit.face_colors(luna), strict=True)
              if c == C['acero']]
    assert sum(v.x for v in grises) > 0 and sum(v.y for v in grises) < 0


def test_errores_de_construccion():
    lobulo = k5_formas.Lobe(0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    with pytest.raises(ValueError):
        k5_formas.build_cloud(k5_formas.CloudShape(10.0, (lobulo, lobulo), seed=0))
    with pytest.raises(ValueError):
        k5_formas.build_disc([], 8)


def test_disco_generico_con_colores_por_cara():
    zonas = [(0.2, [C['papel']] * 6), (0.5, C['acero'])]
    b = k5_formas.build_disc(zonas, 6, wobble=[0.1] * 6)
    assert b.triangle_count == 6 + 12
    assert all(abs(v[2]) < 1e-9 for v in b.verts)


def test_generate_all_exporta_fbx_validos(fbx_k5, capsys):
    manifest = json.loads((fbx_k5 / 'manifest.json').read_text(encoding='utf-8'))
    assert manifest['kit'] == 'K5 Cielo' and 'propuesta_color' in manifest['notas']
    assert [m['malla'] for m in manifest['mallas']] == [s.asset for s in k5.K5]
    for spec, entry in zip(k5.K5, manifest['mallas'], strict=True):
        assert entry['tris'] <= entry['tope_tris'] == spec.max_tris
        assert ('lobulos' in entry) == spec.is_cloud
        assert check_style.main([str(fbx_k5 / entry['fbx']), '--budget', spec.budget]) == 0
    assert '0 con fallos' in capsys.readouterr().out


def test_lamina_falla_sin_fbx(tmp_path):
    with pytest.raises(FileNotFoundError):
        k5_lamina.make_sheet(tmp_path / 'k5.png', tmp_path)


def test_degradado_de_cielo_con_colores_de_la_paleta():
    from explored_style.palette import load_palette

    pal = load_palette()
    cielo = k5_lamina.sky_gradient(pal, 4, 11)
    arriba = tuple(round(x * 255) for x in cielo[0, 0])
    abajo = tuple(round(x * 255) for x in cielo[-1, 0])
    assert arriba == pal.by_name('cielo_dia').rgb and abajo == pal.by_name('papel').rgb


def test_main_kit_con_lamina_workbench(tmp_path):
    salida = tmp_path / 'k5.png'
    args = ['--out', str(tmp_path / 'fbx'), '--lamina', str(salida), '--engine', 'BLENDER_WORKBENCH',
            '--percent', '10']
    assert k5.main(args) == 0
    assert len(list((tmp_path / 'fbx').glob('SM_*_01.fbx'))) == len(k5.K5)
    imagen = raster.decode_png(salida.read_bytes())
    assert imagen.shape == (k5_lamina.HEIGHT, k5_lamina.WIDTH, 3)


def test_main_kit_sin_lamina(tmp_path):
    assert k5.main(['--out', str(tmp_path), '--sin-lamina', '--lamina', str(tmp_path / 'no.png')]) == 0
    assert not (tmp_path / 'no.png').exists()


@pytest.fixture(scope='module')
def fbx_k5(tmp_path_factory):
    carpeta = tmp_path_factory.mktemp('k5')
    k5.generate_all(carpeta)
    yield carpeta
    shutil.rmtree(carpeta, ignore_errors=True)
