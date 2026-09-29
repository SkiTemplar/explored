"""Kit de fauna (run_animals.py + animals/*.py).

Desde la 3ª pasada la fauna va SIN esqueleto (ver docstring de
run_animals.py): no hay armadura, ni pesos de vértice, ni acciones; la
«jerarquía de rig» es la lista de piezas con padre y offset local de
animals.json, y la animación sale del canal de color «Anim» en el shader.
Estos tests comprueban justo ese contrato sobre los FBX exportados."""

import math
import os

import bpy

import pytest
from conftest import hash_malla, importar

ARQUETIPOS_RETIRADOS = ['quadrupeds', 'reptiles', 'serpent', 'bat', 'cephalopod', 'arthropods']


def _importar_fbx(ruta):
    antes = set(bpy.data.objects.keys())
    bpy.ops.import_scene.fbx(filepath=str(ruta), colors_type='LINEAR')
    return [bpy.data.objects[n] for n in set(bpy.data.objects.keys()) - antes]


def test_validate_da_por_buena_toda_la_fauna(kit_fauna, validate_en, capsys):
    salida, _ = kit_fauna
    validate = validate_en(fauna=salida)
    validate.main_animals()
    out = capsys.readouterr().out
    assert 'FALLO' not in out
    assert 'VALIDACIÓN DE FAUNA OK' in out


def test_animals_json_coherente_con_lo_exportado(kit_fauna):
    salida, doc = kit_fauna
    run_animals = importar('run_animals')
    validate = importar('validate')
    especies = doc['species']
    esperadas = sum(len(importar(m).VARIANTS) for m in run_animals.MODULE_NAMES)
    assert doc['species_count'] == len(especies) == esperadas
    assert {'R_spine_t', 'G_mask', 'B_side'} <= set(doc['anim_channel'])
    for e in especies:
        assert e['anim'] in {'swim', 'flap', 'pulse', 'scuttle'}
        assert e['material'] == f"M_Fauna_{e['species']}"
        assert e['piece_count'] == len(e['pieces'])
        assert e['triangles_total'] == sum(p['triangles'] for p in e['pieces'])
        assert e['triangle_budget']['in_budget'], e['species']
        nombres = [p['name'] for p in e['pieces']]
        assert len(set(nombres)) == len(nombres)
        for p in e['pieces']:
            assert p['role'] in validate.FAUNA_ALLOWED_ROLES
            assert os.path.isfile(salida / p['file'])
            if 'lod1_file' in p:
                assert os.path.isfile(salida / p['lod1_file'])
                assert p['lod1_triangles'] < p['triangles']


def test_jerarquia_offsets_locales_suman_el_pivote(kit_fauna):
    """pivot_cm (absoluto) = pivote del padre + local_offset_cm, y el grafo
    de padres es un árbol con una única raíz (sin ciclos)."""
    _, doc = kit_fauna
    for e in doc['species']:
        por_nombre = {p['name']: p for p in e['pieces']}
        raices = [p for p in e['pieces'] if p['parent'] is None]
        assert len(raices) == 1, e['species']
        assert raices[0]['local_offset_cm'] == raices[0]['pivot_cm']
        for p in e['pieces']:
            if p['parent'] is None:
                continue
            padre = por_nombre[p['parent']]
            for i in range(3):
                assert math.isclose(padre['pivot_cm'][i] + p['local_offset_cm'][i], p['pivot_cm'][i],
                                    abs_tol=1e-3)
            # sin ciclos: subir por los padres acaba en la raíz
            visto, actual = set(), p
            while actual['parent'] is not None:
                assert actual['name'] not in visto, f"ciclo en {e['species']}"
                visto.add(actual['name'])
                actual = por_nombre[actual['parent']]


def test_fauna_sin_armadura_ni_pesos_ni_acciones(kit_fauna):
    """Contrato de la 3ª pasada: cada FBX es UNA malla rígida, sin huesos,
    sin grupos de vértices (pesos) y sin animación horneada."""
    salida, doc = kit_fauna
    common = importar('common')
    for e in doc['species']:
        common.reset_scene()
        for p in e['pieces']:
            nuevos = _importar_fbx(salida / p['file'])
            assert [o.type for o in nuevos] == ['MESH'], p['file']
            obj = nuevos[0]
            assert not obj.vertex_groups, f"{p['file']} lleva pesos de vértice"
            assert obj.parent is None
            assert obj.animation_data is None or obj.animation_data.action is None
        assert not [o for o in bpy.data.objects if o.type == 'ARMATURE']
        assert len(bpy.data.actions) == 0, f"{e['species']} trae acciones"


def test_canal_anim_sobrevive_al_fbx_con_el_convenio(kit_fauna):
    """Anim.R en [0,1] a lo largo del cuerpo, Anim.G = máscara de apéndice
    por rol y Anim.B = lado según el pivote, leídos del FBX reimportado."""
    salida, doc = kit_fauna
    common = importar('common')
    run_animals = importar('run_animals')
    for e in doc['species']:
        spine = []
        for p in e['pieces']:
            common.reset_scene()
            obj = _importar_fbx(salida / p['file'])[0]
            attrs = obj.data.color_attributes
            assert 'Col' in attrs and 'Anim' in attrs, p['file']
            valores = [tuple(c.color) for c in attrs['Anim'].data]
            mascara = 1.0 if p['role'] in run_animals.ANIM_MASK_ROLES else 0.0
            y = p['pivot_cm'][1]
            lado = 1.0 if y > 0.5 else (0.0 if y < -0.5 else 0.5)
            for r, g, b, a in valores:
                assert -1e-4 <= r <= 1.0 + 1e-4
                assert g == pytest.approx(mascara, abs=1e-3)
                assert b == pytest.approx(lado, abs=1e-3)
                assert a == pytest.approx(1.0, abs=1e-3)
            spine.extend(v[0] for v in valores)
        # el conjunto de la especie cubre de cola (0) a cabeza (1)
        assert min(spine) == pytest.approx(0.0, abs=0.02), e['species']
        assert max(spine) == pytest.approx(1.0, abs=0.02), e['species']


@pytest.mark.parametrize('modulo', ['turtle', 'fish', 'jellyfish', 'birds', *ARQUETIPOS_RETIRADOS])
def test_rig_de_cada_familia_bien_formado(modulo):
    """Una variante por familia de rig (también las retiradas del juego,
    que siguen en el repo): piezas con el origen en su pivote y sin
    rotación, padres existentes, raíz única, material único por especie y
    color de vértice «Col»."""
    common = importar('common')
    validate = importar('validate')
    mod = importar(modulo)
    common.reset_scene()
    res = mod.build(mod.VARIANTS[0])
    especie, piezas = res['species'], res['pieces']
    assert {'type', 'total_length_cm'} <= set(res['locomotion'])
    nombres = {p['name'] for p in piezas}
    assert sum(1 for p in piezas if p['parent'] is None) == 1
    for p in piezas:
        assert p['parent'] is None or p['parent'] in nombres
        assert p['role'] in validate.FAUNA_ALLOWED_ROLES, p['role']
        obj = p['obj']
        assert tuple(obj.rotation_euler) == (0.0, 0.0, 0.0), p['name']
        assert [m.name for m in obj.data.materials] == [f'M_Fauna_{especie}']
        assert 'Col' in obj.data.color_attributes
        assert len(obj.data.polygons) > 0


def test_lod1_aligera_las_piezas_pesadas():
    common = importar('common')
    rig = importar('rig')
    turtle = importar('turtle')
    common.reset_scene()
    piezas = turtle.build(turtle.VARIANTS[0])['pieces']
    pesada = max(piezas, key=lambda p: common.triangle_count(p['obj']))
    lod = rig.make_lod1(pesada, ratio=0.5, min_tris=10)
    assert lod is not None and lod['name'] == pesada['name'] + '_LOD1'
    assert common.triangle_count(lod['obj']) < common.triangle_count(pesada['obj'])
    assert 'Col' in lod['obj'].data.color_attributes
    assert rig.make_lod1(pesada, min_tris=10 ** 9) is None


@pytest.mark.parametrize('modulo', ['fish', 'birds', 'quadrupeds'])
def test_misma_semilla_mismas_piezas(modulo):
    common = importar('common')
    mod = importar(modulo)
    huellas = []
    for _ in range(2):
        common.reset_scene()
        res = mod.build(mod.VARIANTS[-1])
        huellas.append([(p['name'], p['pivot_cm'], hash_malla(p['obj'])) for p in res['pieces']])
    assert huellas[0] == huellas[1]


def test_cm_to_m_escalar_y_tupla():
    rig = importar('rig')
    assert rig.cm_to_m(250.0) == 2.5
    assert rig.cm_to_m((100.0, 50.0)) == (1.0, 0.5)
    assert rig.piece_dict('A', None, [1, 2, 3], 'body', None)['pivot_cm'] == (1, 2, 3)
