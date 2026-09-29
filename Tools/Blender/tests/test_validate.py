"""validate.py tiene que cazar los fallos de verdad, no solo dar el visto
bueno: se le pasan manifiestos con mallas rotas a propósito."""

import json

import bpy

import bmesh
import pytest
from conftest import importar


def _malla_rota(nombre, material='M_Foo', con_col=False):
    """Lámina abierta (no watertight) con una cara degenerada (área 0) y un
    material fuera del kit."""
    bm = bmesh.new()
    a = bm.verts.new((0.0, 0.0, 0.0))
    b = bm.verts.new((1.0, 0.0, 0.0))
    c = bm.verts.new((1.0, 1.0, 0.0))
    d = bm.verts.new((0.0, 1.0, 0.0))
    bm.faces.new((a, b, c, d))
    # triángulo degenerado: tres vértices alineados
    e = bm.verts.new((2.0, 0.0, 0.0))
    f = bm.verts.new((3.0, 0.0, 0.0))
    g = bm.verts.new((4.0, 0.0, 0.0))
    bm.faces.new((e, f, g))
    me = bpy.data.meshes.new(nombre)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(nombre, me)
    bpy.context.collection.objects.link(obj)
    me.materials.append(bpy.data.materials.new(material))
    if con_col:
        me.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
    return obj


def _exportar(ruta, objetos):
    common = importar('common')
    ruta.parent.mkdir(parents=True, exist_ok=True)
    common.export_fbx(str(ruta), objetos)


def _escribir(ruta, datos):
    ruta.write_text(json.dumps(datos), encoding='utf-8')


def test_vegetacion_rota_se_rechaza_con_todos_los_motivos(tmp_path, validate_en, capsys):
    common = importar('common')
    common.reset_scene()
    _exportar(tmp_path / 'Rock' / 'SM_Rota.fbx', [_malla_rota('Rota')])
    common.reset_scene()
    _exportar(tmp_path / 'Rock' / 'SM_Doble.fbx', [_malla_rota('A', 'M_Rock', True), _malla_rota('B', 'M_Rock', True)])
    presupuesto = {'min': 1000, 'max': 3000, 'in_budget': True}
    _escribir(tmp_path / 'manifest.json', {'meshes': [
        {'name': 'SM_Rota', 'file': 'Rock/SM_Rota.fbx', 'category': 'rock', 'triangle_budget': presupuesto},
        {'name': 'SM_Doble', 'file': 'Rock/SM_Doble.fbx', 'category': 'rock', 'triangle_budget': presupuesto},
        {'name': 'SM_NoEsta', 'file': 'Rock/SM_NoEsta.fbx', 'category': 'rock', 'triangle_budget': presupuesto},
    ]})
    validate = validate_en(meshes=tmp_path)
    with pytest.raises(SystemExit) as exc:
        validate.main()
    assert exc.value.code == 1
    out = capsys.readouterr().out
    linea = next(linea for linea in out.splitlines() if 'FALLO SM_Rota' in linea)
    assert 'fuera de presupuesto' in linea
    assert 'geometría degenerada' in linea
    assert 'fuera de rango plausible' in linea
    assert 'falta el atributo de color' in linea
    assert "['M_Foo']" in linea
    assert 'no es watertight' in linea
    assert 'se esperaba 1 objeto de malla al reimportar, hay 2' in out
    assert 'FALLO SM_NoEsta: fichero FBX no encontrado' in out
    assert '0/3 mallas en verde' in out


def test_sin_manifest_termina_con_error(tmp_path, validate_en, capsys):
    validate = validate_en(meshes=tmp_path, props=tmp_path, fauna=tmp_path)
    for fn in (validate.main, validate.main_props, validate.main_animals):
        with pytest.raises(SystemExit):
            fn()
    assert capsys.readouterr().out.count('ERROR: no existe') == 3


def test_props_rotos_se_rechazan(tmp_path, validate_en, capsys):
    common = importar('common')
    common.reset_scene()
    _exportar(tmp_path / 'Baliza' / 'SM_Rota.fbx', [_malla_rota('Rota')])
    common.reset_scene()
    _exportar(tmp_path / 'Baliza' / 'SM_Doble.fbx', [_malla_rota('A'), _malla_rota('B')])
    presupuesto = {'min': 50, 'max': 100, 'in_budget': True}
    _escribir(tmp_path / 'manifest.json', {'meshes': [
        {'name': 'SM_Rota', 'file': 'Baliza/SM_Rota.fbx', 'group': 'Baliza', 'triangle_budget': presupuesto},
        {'name': 'SM_Doble', 'file': 'Baliza/SM_Doble.fbx', 'group': 'Baliza', 'triangle_budget': presupuesto},
        {'name': 'SM_NoEsta', 'file': 'Baliza/SM_NoEsta.fbx', 'group': 'Baliza', 'triangle_budget': presupuesto},
    ]})
    validate = validate_en(props=tmp_path)
    with pytest.raises(SystemExit):
        validate.main_props()
    out = capsys.readouterr().out
    linea = next(linea for linea in out.splitlines() if 'FALLO SM_Rota' in linea)
    for motivo in ('fuera de presupuesto', 'geometría degenerada', 'fuera de rango plausible',
                   'falta el atributo de color', 'fuera del kit estable de props'):
        assert motivo in linea
    assert 'hay 2' in out and 'fichero FBX no encontrado' in out
    assert 'VALIDACIÓN DE PROPS FALLIDA' in out


def test_fauna_con_rig_incoherente_se_rechaza(tmp_path, validate_en, capsys):
    common = importar('common')
    common.reset_scene()
    _exportar(tmp_path / 'Bicho' / 'SM_Bicho_Body.fbx', [_malla_rota('Body', 'M_Otro')])
    common.reset_scene()
    _exportar(tmp_path / 'Bicho' / 'SM_Bicho_Doble.fbx', [_malla_rota('A'), _malla_rota('B')])
    pieza = {'local_offset_cm': [0.0, 0.0, 0.0], 'role': 'body'}
    _escribir(tmp_path / 'animals.json', {'species': [{
        'species': 'Bicho',
        'triangle_budget': {'min': 300, 'max': 400},
        'dimensions_cm': {'x': 999.0, 'y': 1.0, 'z': 1.0},
        'pieces': [
            {**pieza, 'name': 'Body', 'parent': None, 'file': 'Bicho/SM_Bicho_Body.fbx'},
            {**pieza, 'name': 'Otra', 'parent': None, 'file': 'Bicho/SM_Bicho_Doble.fbx', 'role': 'antena'},
            {**pieza, 'name': 'Huerfana', 'parent': 'NoExiste', 'file': 'Bicho/SM_Bicho_NoEsta.fbx'},
        ],
    }, {
        'species': 'Vacio', 'triangle_budget': {'min': 0, 'max': 0},
        'dimensions_cm': {'x': 0.0, 'y': 0.0, 'z': 0.0}, 'pieces': [],
    }]})
    validate = validate_en(fauna=tmp_path)
    with pytest.raises(SystemExit):
        validate.main_animals()
    out = capsys.readouterr().out
    linea = next(linea for linea in out.splitlines() if 'FALLO Bicho' in linea)
    for motivo in ('2 piezas raíz', "padre inexistente: ['Huerfana']", "roles desconocidos: ['antena']",
                   'caras degeneradas', 'falta el atributo de color', "se esperaba solo {'M_Fauna_Bicho'}",
                   'se esperaba 1 malla, hay 2', 'fichero FBX no encontrado', 'fuera de presupuesto',
                   'dimensión x recalculada'):
        assert motivo in linea, motivo
    # una especie sin piezas: 0 raíces, pero sin reventar por la caja vacía
    assert 'FALLO Vacio: 0 piezas raíz' in out
