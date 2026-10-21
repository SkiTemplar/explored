import json
import shutil

import bpy  # noqa: F401  (debe importarse antes que mathutils)

import pytest
from mathutils import Vector

from explored_style import check_style, meshkit, raster
from explored_style.check_style import BUDGETS, check_object
from explored_style.kits import k2, k2_carguero, k2_faro, k2_lamina, k2_manos

# (alto mín., alto máx., lado mayor mín., lado mayor máx.) en metros
FORMAS = {
    'SM_FaroTorre': (11.9, 12.1, 5.0, 6.5),
    'SM_FaroLinterna': (4.0, 5.0, 2.8, 3.4),
    'SM_FaroCasa': (5.0, 6.0, 6.0, 7.0),
    'SM_CajaSuministros': (0.55, 0.75, 0.9, 1.2),
    'SM_CargueroCasco': (9.0, 17.0, 39.5, 41.0),
    'SM_CargueroPuente': (7.0, 9.5, 8.0, 10.0),
    'SM_RefugioPalma': (2.2, 2.8, 3.0, 4.2),
    'SM_BarrilLluvia': (0.9, 1.05, 0.8, 1.0),
    'SM_Hoguera': (0.45, 0.7, 1.2, 1.8),
    'SM_Balsa': (0.3, 0.5, 2.9, 3.1),
    'SM_ManoIria': (0.08, 0.25, 0.45, 0.6),
}


@pytest.mark.parametrize('spec', k2.SPECS, ids=lambda s: s.name)
def test_malla_k2_cumple_estilo_y_forma(escena_vacia, spec):
    obj = k2.generate(spec)
    report = check_object(obj, BUDGETS[spec.budget], spec.pivot)
    assert report.ok, report.errors
    assert not report.warnings
    alto_min, alto_max, lado_min, lado_max = FORMAS[spec.name]
    assert alto_min <= obj.dimensions.z <= alto_max
    assert lado_min <= max(obj.dimensions.x, obj.dimensions.y) <= lado_max
    assert meshkit.flipped_face_count(obj) == 0
    assert len(set(meshkit.face_colors(obj))) >= 4  # nada monocromo


def test_kit_completo_y_nombres():
    assert len(k2.SPECS) == 11
    assert all(s.fbx == f'{s.name}_01.fbx' for s in k2.SPECS)
    assert k2.spec_by_name('SM_Balsa').budget == 'estructura'
    with pytest.raises(KeyError):
        k2.spec_by_name('SM_Inexistente')


def test_manos_presupuesto_por_mano_y_pivote_en_el_codo(escena_vacia):
    derecha, izquierda = k2_manos.per_hand_triangles(k2_manos.build_hands())
    assert 0 < derecha <= k2_manos.HAND_BUDGET and 0 < izquierda <= k2_manos.HAND_BUDGET
    obj = k2.generate(k2.spec_by_name('SM_ManoIria'))
    xs = [v.co.x for v in meshkit.mesh_of(obj).vertices]
    assert -check_style.ELBOW_BACK <= min(xs) < 0.0 < max(xs)
    # con la regla general fallaría: la excepción es necesaria y está acotada
    assert 'pivote' in {f.rule for f in check_object(obj, BUDGETS['manos']).errors}
    obj.location = (0.0, 0.0, 0.0)
    lejos = meshkit.build_object('SM_Lejos', k2_manos.build_hands(), base_pivot=True)
    meshkit.mesh_of(lejos).transform(meshkit.matrix_at(-0.5, 0.0, 0.0))
    reglas = {f.rule for f in check_object(lejos, BUDGETS['manos'], 'codo').errors}
    assert reglas == {'pivote'}


def test_faro_ranuras_dentro_de_la_linterna():
    assert set(k2_faro.LANTERN_SLOTS) == {'aro_lente', 'tarro_izq', 'tarro_der', 'portamecha'}
    for x, y, z in k2_faro.LANTERN_SLOTS.values():
        assert (x * x + y * y) ** 0.5 < k2_faro.LANTERN_R and 1.0 <= z <= 2.0
    assert k2_faro.shaft_radius(0.6) == pytest.approx(2.25)
    with pytest.raises(ValueError):
        k2_faro.shaft_radius(20.0)


def test_carguero_puente_sobre_la_cubierta():
    montaje = k2_carguero.bridge_mount()
    x, y, z = montaje['ubicacion_m']
    assert x == pytest.approx(k2_carguero.BRIDGE_AT_X, abs=0.5) and abs(y) < 1.5 and 4.0 < z < 8.0
    assert montaje['rotacion_grados'][0] == k2_carguero.HULL_ROLL_DEG
    with pytest.raises(ValueError):
        k2_carguero._deck_at(30.0)


def test_generate_all_exporta_fbx_validos(fbx_k2, capsys):
    manifest = json.loads((fbx_k2 / 'manifest.json').read_text(encoding='utf-8'))
    assert [m['malla'] for m in manifest['mallas']] == [s.object_name for s in k2.SPECS]
    assert set(manifest['ensamblaje']) == {'SM_FaroLinterna_01', 'SM_CargueroPuente_01'}
    for spec, entry in zip(k2.SPECS, manifest['mallas'], strict=True):
        assert entry['tris'] <= entry['tope_tris'] == BUDGETS[spec.budget]
        assert entry['pivote'] == spec.pivot
        assert check_style.main([str(fbx_k2 / entry['fbx']), '--budget', spec.budget]) == 0
    assert manifest['mallas'][-1]['tris_por_mano']['tope'] == 1500
    assert '0 con fallos' in capsys.readouterr().out


def test_place_tag_no_pisa_rotulos():
    ocupados: list[k2_lamina.Rect] = []
    a = k2_lamina.place_tag('SM_UNO', 100, 100, (400, 300), ocupados)
    b = k2_lamina.place_tag('SM_DOS', 100, 100, (400, 300), ocupados)
    c = k2_lamina.place_tag('SM_TRES', 100, 40, (400, 300), ocupados)
    assert not k2_lamina._overlaps(a, b) and not k2_lamina._overlaps(c, a) and not k2_lamina._overlaps(c, b)
    assert all(0 <= r[0] and r[0] + r[2] <= 400 for r in ocupados)


def test_lamina_k2_workbench_reducida(fbx_k2, tmp_path):
    carpeta = tmp_path / 'fbx'
    shutil.copytree(fbx_k2, carpeta)
    salida = tmp_path / 'k2.png'
    k2_lamina.make_sheet(salida, carpeta, engine='BLENDER_WORKBENCH', percent=8)
    imagen = raster.decode_png(salida.read_bytes())
    assert imagen.shape == (k2_lamina.HEIGHT, k2_lamina.WIDTH, 3)
    assert len(meshkit.mesh_of(bpy.data.objects['SM_FaroTorre_01']).vertices) > 0
    torre = bpy.data.objects['SM_FaroTorre_01']
    assert torre.location == Vector((*k2_lamina.PLACES['SM_FaroTorre'][:2], 0.0))


def test_main_sin_lamina(tmp_path):
    assert k2.main(['--out', str(tmp_path), '--sin-lamina']) == 0
    assert sorted(p.name for p in tmp_path.glob('*.fbx')) == sorted(s.fbx for s in k2.SPECS)


def test_main_con_lamina(tmp_path):
    salida = tmp_path / 'k2.png'
    args = ['--out', str(tmp_path), '--lamina', str(salida), '--engine', 'BLENDER_WORKBENCH', '--percent', '5']
    assert k2.main(args) == 0
    assert salida.is_file()
