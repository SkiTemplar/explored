"""Kit de props (props/run_props.py + props/*.py) exportado entero a tmp,
incluidas las formaciones de acantilado sobre escaneos CC0 sintéticos."""

import json
import os
import shutil

import pytest
from conftest import REPO_ROOT, hash_malla, importar


def test_validate_da_por_bueno_todo_el_kit(kit_props, validate_en, capsys):
    salida, _, _ = kit_props
    validate = validate_en(props=salida)
    validate.main_props()
    out = capsys.readouterr().out
    assert 'FALLO' not in out
    assert 'VALIDACIÓN DE PROPS OK' in out


def test_manifest_coherente_con_lo_exportado(kit_props):
    salida, manifest, _ = kit_props
    run_props = importar('run_props')
    validate = importar('validate')
    esperados = sum(len(importar(m).VARIANTS) for m, _ in run_props.MODULE_GROUPS)
    mallas = manifest['meshes']
    assert manifest['mesh_count'] == len(mallas) == esperados
    assert len({m['name'] for m in mallas}) == len(mallas), 'nombres de prop repetidos'
    for m in mallas:
        assert m['file'] == f"{m['group']}/{m['name']}.fbx"
        assert os.path.isfile(salida / m['file'])
        assert m['group'] in validate.PLAUSIBLE_RANGES_CM_PROPS, m['group']
        p = m['triangle_budget']
        assert p['in_budget'] == (p['min'] <= m['triangles'] <= p['max'])
        assert p['in_budget'], f"{m['name']}: {m['triangles']} tris fuera de [{p['min']}, {p['max']}]"
        assert set(m['material_slots']) <= validate.ALLOWED_MATERIALS_PROPS
        for clave in ('needs_collision', 'collision_complex', 'interactable'):
            assert isinstance(m[clave], bool)


def test_catalogo_props_json_refleja_el_manifest(kit_props):
    _, manifest, catalogo = kit_props
    assert catalogo['prop_count'] == manifest['mesh_count']
    assert [(p['id'], p['file'], p['group'], p['triangles']) for p in catalogo['props']] == \
        [(m['name'], m['file'], m['group'], m['triangles']) for m in manifest['meshes']]


def test_items_coinciden_con_items_json(kit_props):
    """Cada SM_Item_<Id> generado corresponde a un objeto de
    Content/Data/items.json cuyo meshPath apunta justo a esa malla."""
    _, manifest, _ = kit_props
    with open(REPO_ROOT / 'Content' / 'Data' / 'items.json', encoding='utf-8') as f:
        por_id = {i['id']: i for i in json.load(f)}
    run_props = importar('run_props')
    for modulo, grupo in run_props.MODULE_GROUPS:
        for v in importar(modulo).VARIANTS:
            if v.get('group', grupo) != 'Items':
                continue
            item = por_id.get(v['item_id'])
            assert item is not None, f"{v['name']}: item_id {v['item_id']} no está en items.json"
            ruta = f"/Game/Generated/Meshes/Items/SM_{v['name']}.SM_{v['name']}"
            assert item['meshPath'] == ruta
    exportados = {m['name'] for m in manifest['meshes'] if m['group'] == 'Items'}
    assert len(exportados) > 100


@pytest.mark.parametrize(('modulo', 'indice'), [
    ('kit_construccion', 0), ('ruinas_polinesias', 0), ('items_pescados', 0), ('tesoros', 3),
    ('rocks_cliffs', 13),
])
def test_misma_semilla_mismos_vertices(modulo, indice):
    common = importar('common')
    mod = importar(modulo)
    huellas = []
    for _ in range(2):
        common.reset_scene()
        huellas.append(hash_malla(mod.build(mod.VARIANTS[indice])))
    assert huellas[0] == huellas[1]


def test_filtro_de_modulos_conserva_el_resto_del_manifest(kit_props, tmp_path, monkeypatch):
    """`-- --modules=beacon` regenera solo Baliza y conserva las demás
    entradas del manifest anterior (sin duplicar las de Baliza)."""
    salida, manifest, _ = kit_props
    shutil.copy(salida / 'manifest.json', tmp_path / 'manifest.json')
    run_props = importar('run_props')
    monkeypatch.setattr(run_props, 'EXPORT_DIR', str(tmp_path))
    monkeypatch.setattr('sys.argv', ['blender', '--', '--modules=beacon'])
    run_props.main()
    nuevo = json.loads((tmp_path / 'manifest.json').read_text(encoding='utf-8'))
    nombres = [m['name'] for m in nuevo['meshes']]
    assert sorted(nombres) == sorted(m['name'] for m in manifest['meshes'])
    assert nuevo['mesh_count'] == manifest['mesh_count']
    assert os.path.isfile(tmp_path / 'Baliza' / 'SM_Beacon_Frame.fbx')
    assert not os.path.isdir(tmp_path / 'Faro')


def test_filtro_de_modulos_sin_manifest_previo(tmp_path, monkeypatch):
    run_props = importar('run_props')
    monkeypatch.setattr(run_props, 'EXPORT_DIR', str(tmp_path))
    monkeypatch.setattr('sys.argv', ['blender', '--', '--otra', '--modules=star_compass'])
    run_props.main()
    nuevo = json.loads((tmp_path / 'manifest.json').read_text(encoding='utf-8'))
    assert {m['group'] for m in nuevo['meshes']} == {'BrujulaEstelar'}


def test_sin_filtro_tras_doble_guion(monkeypatch):
    run_props = importar('run_props')
    monkeypatch.setattr('sys.argv', ['blender', '--', '--otra=1'])
    assert run_props._parse_module_filter() is None


# ---------------------------------------------------------------------------
# _cliffscan.py (escaneos CC0)
# ---------------------------------------------------------------------------
def test_escaneo_ausente_explica_de_donde_descargarlo(tmp_path, monkeypatch):
    cliffscan = importar('_cliffscan')
    monkeypatch.setattr(cliffscan, 'CACHE_DIR', str(tmp_path))
    with pytest.raises(FileNotFoundError, match='polyhaven.com/a/moon_rock_01'):
        cliffscan.import_scan('moon_rock_01')


def test_import_scan_se_queda_con_la_isla_mayor_y_el_lod_pedido(escaneos_cc0):
    common = importar('common')
    cliffscan = importar('_cliffscan')
    common.reset_scene()
    obj = cliffscan.import_scan('moon_rock_01', lod_substring='LOD1')
    # la lámina de 29 x 23 vértices, sin las dos islas de ruido ni el LOD0
    assert len(obj.data.vertices) == 29 * 23
    assert [o.name for o in common.bpy.data.objects] == [obj.name]
    assert not obj.data.materials


def test_predecimate_solo_si_pasa_del_limite(escaneos_cc0):
    common = importar('common')
    cliffscan = importar('_cliffscan')
    common.reset_scene()
    obj = cliffscan.import_scan('namaqualand_cliff_01')
    caras = len(obj.data.polygons)
    cliffscan.predecimate_if_heavy(obj, max_polys=caras + 1)
    assert len(obj.data.polygons) == caras
    cliffscan.predecimate_if_heavy(obj, max_polys=caras // 4)
    assert len(obj.data.polygons) < caras


def test_crop_to_box_recorta_a_la_caja(escaneos_cc0):
    common = importar('common')
    cliffscan = importar('_cliffscan')
    common.reset_scene()
    obj = cliffscan.import_scan('coastal_cliff_04')
    cliffscan.center_and_ground(obj)
    cliffscan.fuse_and_stylize([obj], voxel_size=0.4, smooth_iterations=0)
    cliffscan.crop_to_box(obj, size=(4.0, 20.0, 20.0))
    xs = [v.co.x for v in obj.data.vertices]
    assert xs and max(xs) <= 2.0 + 1e-3 and min(xs) >= -2.0 - 1e-3


def _ngono_con_tramo_alineado():
    """Un n-gono plano cuyo borde inferior tiene 5 vértices casi alineados
    a 1 mm: el FBX triangulado sacaría triángulos de área ~0."""
    import bpy

    import bmesh
    bm = bmesh.new()
    puntos = [(x * 0.001, 1e-7 * (x % 2), 0.0) for x in range(5)] + [(1.0, 0.5, 0.0), (0.0, 1.0, 0.0)]
    bm.faces.new([bm.verts.new(p) for p in puntos])
    me = bpy.data.meshes.new('Ngono')
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new('Ngono', me)
    bpy.context.collection.objects.link(obj)
    return obj


def _agujas(obj, min_area=1e-7):
    import bmesh
    cliffkit = importar('_cliffkit')
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    n = sum(1 for lt in bm.calc_loop_triangles() if cliffkit._loop_tri_area(lt) < min_area)
    bm.free()
    return n


def test_remove_sliver_triangles_limpia_los_ngonos_alineados():
    """Regresión: SM_CliffWall_Sandstone02 y SM_SeaArch01 exportaban
    triángulos de área ~0 (validate.py: «geometría degenerada»)."""
    common = importar('common')
    cliffkit = importar('_cliffkit')
    common.reset_scene()
    obj = _ngono_con_tramo_alineado()
    assert _agujas(obj) > 0
    cliffkit.remove_sliver_triangles(obj)
    assert _agujas(obj) == 0
    assert len(obj.data.polygons) > 0


def test_remove_sliver_triangles_no_toca_mallas_limpias():
    common = importar('common')
    cliffkit = importar('_cliffkit')
    common.reset_scene()
    common.bpy.ops.mesh.primitive_cube_add()
    cubo = common.bpy.context.object
    antes = [tuple(v.co) for v in cubo.data.vertices]
    cliffkit.remove_sliver_triangles(cubo)
    assert [tuple(v.co) for v in cubo.data.vertices] == antes
    assert len(cubo.data.polygons) == 6
