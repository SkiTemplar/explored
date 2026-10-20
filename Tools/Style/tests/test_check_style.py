from pathlib import Path
from typing import Any, cast

import bpy

import pytest
from conftest import cubo

from explored_style import check_style, meshkit
from explored_style.check_style import BUDGETS, check_object
from explored_style.meshkit import mesh_of


def _reglas(report) -> set[str]:
    return {f.rule for f in report.errors}


def _guardar_blend(ruta: Path) -> Path:
    bpy.ops.wm.save_as_mainfile(filepath=str(ruta), copy=True)
    return ruta


def _con_textura(obj: bpy.types.Object) -> None:
    mat = mesh_of(obj).materials[0]
    assert mat is not None and mat.node_tree is not None
    tree = mat.node_tree
    tex = cast(bpy.types.ShaderNodeTexImage, tree.nodes.new('ShaderNodeTexImage'))
    tex.image = bpy.data.images.new('T_Detalle', 4, 4)
    bsdf = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')
    tree.links.new(cast(Any, tex).outputs['Color'], cast(Any, bsdf).inputs['Base Color'])


def test_malla_correcta_pasa(escena_vacia):
    report = check_object(cubo(), BUDGETS['objeto_mano'])
    assert report.ok, report.errors
    assert report.triangles == 12
    assert report.cells == (0,)
    assert not report.warnings


def test_textura_falla(escena_vacia):
    obj = cubo()
    _con_textura(obj)
    assert 'texturas' in _reglas(check_object(obj, 300))


def test_dos_materiales_falla(escena_vacia):
    obj = cubo()
    mesh_of(obj).materials.append(bpy.data.materials.new('M_Otro'))
    mesh_of(obj).polygons[0].material_index = 1
    assert 'materiales' in _reglas(check_object(obj, 300))


def test_sin_material_falla(escena_vacia):
    obj = cubo()
    mesh_of(obj).materials.clear()
    assert 'materiales' in _reglas(check_object(obj, 300))


def test_uv_fuera_de_celda_falla(escena_vacia):
    obj = cubo()
    uv = mesh_of(obj).uv_layers[0].data
    poly = mesh_of(obj).polygons[0]
    for k, li in enumerate(poly.loop_indices):
        uv[li].uv = (0.05 * k, 0.5)  # la cara cruza varias celdas
    assert 'uv' in _reglas(check_object(obj, 300))


def test_sin_uv_falla(escena_vacia):
    obj = cubo()
    mesh_of(obj).uv_layers.remove(mesh_of(obj).uv_layers[0])
    assert 'uv' in _reglas(check_object(obj, 300))


def test_ngon_falla(escena_vacia):
    b = meshkit.MeshBuilder()
    for k in range(5):
        b.vert((k * 0.1, (k % 2) * 0.1, 0.0))
    mesh = bpy.data.meshes.new('ngon')
    mesh.from_pydata(b.verts, [], [(0, 1, 2, 3, 4)])
    obj = bpy.data.objects.new('SM_Ngon', mesh)
    bpy.context.scene.collection.objects.link(obj)
    assert 'ngones' in _reglas(check_object(obj, 300))


def test_escala_y_rotacion_fallan(escena_vacia):
    obj = cubo()
    obj.scale = (2.0, 2.0, 2.0)
    obj.rotation_euler = (0.0, 0.0, 0.5)
    report = check_object(obj, 300)
    assert 'escala' in _reglas(report)
    assert len([f for f in report.errors if f.rule == 'escala']) == 2


def test_pivote_fuera_de_la_base_falla(escena_vacia):
    obj = cubo()
    for v in mesh_of(obj).vertices:
        v.co.z += 0.5
    assert 'pivote' in _reglas(check_object(obj, 300))
    obj2 = cubo('SM_Lejos')
    for v in mesh_of(obj2).vertices:
        v.co.x += 5.0
    assert [f.message for f in check_object(obj2, 300).errors if f.rule == 'pivote']


def test_presupuesto_de_tris_falla(escena_vacia):
    assert 'tris' in _reglas(check_object(cubo(), 10))


def test_avisos_no_hacen_fallar(escena_vacia):
    obj = cubo('Cubo')
    for p in mesh_of(obj).polygons:
        p.use_smooth = True
    report = check_object(obj, 300)
    assert report.ok
    assert {f.rule for f in report.warnings} == {'normales', 'nombre'}


def test_parse_budget():
    assert check_style.parse_budget('palmera') == 1500
    assert check_style.parse_budget('120') == 120
    for malo in ('arbol', '0', '-3'):
        with pytest.raises(ValueError):
            check_style.parse_budget(malo)


def test_cli_blend_ok_y_falla(escena_vacia, tmp_path, capsys):
    cubo()
    bueno = _guardar_blend(tmp_path / 'bueno.blend')
    meshkit.reset_scene()
    obj = cubo('SM_Texturado')
    _con_textura(obj)
    mesh_of(obj).materials.append(bpy.data.materials.new('M_Segundo'))
    malo = _guardar_blend(tmp_path / 'malo.blend')
    assert check_style.main([str(bueno), '--budget', 'objeto_mano']) == 0
    assert check_style.main([str(bueno), str(malo), '--budget', 'objeto_mano']) == 1
    salida = capsys.readouterr().out
    assert '[FALLA] malo.blend' in salida
    assert 'x texturas' in salida and 'x materiales' in salida
    assert '1 con fallos' in salida


def test_cli_fbx_con_textura_falla(escena_vacia, tmp_path):
    obj = cubo('SM_TexFbx')
    _con_textura(obj)
    ruta = meshkit.export_fbx(obj, str(tmp_path / 'tex.fbx'))
    report = check_style.check_file(Path(ruta), 300)
    assert not report.ok
    assert 'texturas' in {f.rule for f in report.errors} | {f.rule for m in report.meshes for f in m.errors}


def test_cli_errores_de_uso(tmp_path, capsys):
    otro = tmp_path / 'malla.obj'
    otro.write_text('o x\n', encoding='utf-8')
    assert check_style.main([str(tmp_path / 'no.fbx'), '--budget', 'palmera']) == 2
    assert check_style.main([str(otro), '--budget', 'palmera']) == 2
    assert check_style.main([str(otro), '--budget', 'gigante']) == 2
    err = capsys.readouterr().err
    assert 'no existe' in err and 'formato no soportado' in err and 'presupuesto no válido' in err


def test_fichero_sin_mallas_falla(escena_vacia, tmp_path):
    vacio = _guardar_blend(tmp_path / 'vacio.blend')
    report = check_style.check_file(vacio, 300)
    assert not report.ok
    assert 'el fichero no contiene mallas' in check_style.format_report(report)
