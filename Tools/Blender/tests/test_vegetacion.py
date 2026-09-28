"""Kit de vegetación y rocas (run_all.py + assets/*.py), validado con las
mismas comprobaciones de validate.py sobre los FBX exportados a tmp."""

import json
import os

import pytest
from conftest import hash_malla, importar

MODULOS = ['palm', 'jungle_tree', 'shrub', 'rock', 'grass', 'debris']


def test_validate_da_por_bueno_todo_el_kit(kit_vegetacion, validate_en, capsys):
    salida, _ = kit_vegetacion
    validate = validate_en(meshes=salida)
    validate.main()  # sys.exit(1) si alguna malla falla
    out = capsys.readouterr().out
    assert 'FALLO' not in out
    assert '[validate] VALIDACIÓN OK' in out


def test_manifest_coherente_con_lo_exportado(kit_vegetacion):
    salida, manifest = kit_vegetacion
    run_all = importar('run_all')
    mallas = manifest['meshes']
    esperadas = sum(len(importar(m).VARIANTS) for m in MODULOS)
    assert manifest['generated_by'] == 'Tools/Blender/run_all.py'
    assert manifest['mesh_count'] == len(mallas) == esperadas
    assert len({m['name'] for m in mallas}) == len(mallas), 'nombres de malla repetidos'
    for m in mallas:
        carpeta = run_all.FAMILY_FOLDERS[m['category']]
        assert m['file'] == f"{carpeta}/{m['name']}.fbx"
        assert os.path.isfile(salida / m['file'])
        lo, hi = run_all.TRIANGLE_BUDGETS[m['category']]
        presupuesto = m['triangle_budget']
        assert (presupuesto['min'], presupuesto['max']) == (lo, hi)
        assert presupuesto['in_budget'] == (lo <= m['triangles'] <= hi)
        assert presupuesto['in_budget'], f"{m['name']}: {m['triangles']} tris fuera de [{lo}, {hi}]"
        assert all(m['dimensions_cm'][k] > 0 for k in 'xyz')
        assert m['material_slots'], f"{m['name']} sin materiales"


def test_manifest_es_json_valido_y_utf8(kit_vegetacion):
    salida, _ = kit_vegetacion
    texto = (salida / 'manifest.json').read_text(encoding='utf-8')
    assert json.loads(texto)['meshes']


def test_semillas_unicas_por_variante():
    for nombre in MODULOS:
        mod = importar(nombre)
        claves = [(v['name'], v['index']) for v in mod.VARIANTS]
        assert len(set(claves)) == len(claves), f'{nombre}: variantes repetidas'
        semillas = [v['seed'] for v in mod.VARIANTS]
        assert len(set(semillas)) == len(semillas), f'{nombre}: semillas repetidas'


@pytest.mark.parametrize(('modulo', 'indice'), [
    ('palm', 0), ('shrub', 5), ('rock', 0), ('grass', 3), ('debris', 0), ('jungle_tree', 3),
])
def test_misma_semilla_mismos_vertices(modulo, indice):
    common = importar('common')
    mod = importar(modulo)
    variante = mod.VARIANTS[indice]
    huellas = []
    for _ in range(2):
        common.reset_scene()
        huellas.append(hash_malla(mod.build(variante)))
    assert huellas[0] == huellas[1]


def test_semillas_distintas_dan_mallas_distintas():
    common = importar('common')
    palm = importar('palm')
    huellas = set()
    for variante in palm.VARIANTS:
        common.reset_scene()
        huellas.add(hash_malla(palm.build(variante)))
    assert len(huellas) == len(palm.VARIANTS)


def test_run_all_encadena_props_y_fauna(monkeypatch):
    """_run_props_kit/_run_animals_kit recargan el módulo y llaman a main()."""
    run_all = importar('run_all')
    llamados = []

    class Falso:
        def __init__(self, nombre):
            self.nombre = nombre

        def main(self):
            llamados.append(self.nombre)

    monkeypatch.setattr(run_all, '_import_or_reload', lambda nombre: Falso(nombre))
    run_all._run_props_kit()
    run_all._run_animals_kit()
    assert llamados == ['run_props', 'run_animals']
