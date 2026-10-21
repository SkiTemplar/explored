import json

import numpy as np
import pytest

from explored_island import decor
from explored_island.layout import distance_to_paths
from explored_island.props import load_props


@pytest.fixture(scope='module')
def props():
    return load_props()


@pytest.fixture(scope='module')
def generado(layout, props):
    return decor.build_decor(layout, props)


def _xy(generado, mallas=None):
    inst = np.array(generado['instances'])
    if mallas is not None:
        inst = inst[np.isin(inst[:, 0], mallas)]
    return inst


def test_cantidad_y_mallas(generado):
    total = len(generado['instances'])
    assert decor.MIN_INSTANCES <= total <= decor.MAX_INSTANCES
    cuenta = decor.counts(generado)
    assert all(n > 0 for n in cuenta.values()), cuenta
    assert cuenta['SM_GrassClump_01'] > cuenta['SM_Shrub_01'] > cuenta['SM_PalmA_01']
    assert all(m.startswith('/Game/Art/K1_Naturaleza/SM_') for m in generado['meshes'])


def test_no_tapa_caminos_ni_objetos(layout, props, generado):
    inst = _xy(generado)
    vegetacion = inst[inst[:, 0] != decor.CLIFF]
    assert (distance_to_paths(layout, vegetacion[:, 1], vegetacion[:, 2]) > decor.PATH_CLEARANCE).all()
    assert decor._clear_of_props(props, inst[:, 1], inst[:, 2]).all()
    start = layout.points['player_start']
    assert (np.hypot(inst[:, 1] - start[0], inst[:, 2] - start[1]) > decor.START_CLEARANCE).all()


def test_por_zona(layout, generado):
    bambu = layout.zone('bosque_bambu')
    inst = _xy(generado, [decor.BAMBOO])
    assert (np.hypot(inst[:, 1] - bambu.center[0], inst[:, 2] - bambu.center[1]) <= bambu.radius).all()
    palmas = _xy(generado, [decor.PALM_A, decor.PALM_B])
    colina = layout.zone('colina_faro')
    lejos = np.hypot(palmas[:, 1] - colina.center[0], palmas[:, 2] - colina.center[1]) > colina.radius * 0.5
    assert lejos.all()  # la cima del faro queda despejada
    acantilados = _xy(generado, [decor.CLIFF])
    assert len(acantilados) >= 10 and (acantilados[:, 5] < 1.0).all()


def test_apoyado_en_el_suelo(layout, generado):
    from explored_island.terrain import build_terrain, surface_height
    malla = build_terrain(layout)
    for m, x, y, z, _yaw, _s in generado['instances'][::25]:
        suelo = surface_height(malla, layout, x, y)
        hundido = decor.CLIFF_SINK if m == decor.CLIFF else 0.05
        assert z == pytest.approx(suelo - hundido, abs=0.02)


def test_determinista(layout, props, generado):
    assert decor.build_decor(layout, props) == generado


def test_main_escribe_json(tmp_path, generado):
    out = tmp_path / 'decor.json'
    assert decor.main(['--out', str(out)]) == 0
    leido = json.loads(out.read_text(encoding='utf-8'))
    assert leido['instances'] == generado['instances'] and leido['meshes'] == generado['meshes']


def test_main_falla_fuera_de_objetivo(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(decor, 'MAX_INSTANCES', 10)
    assert decor.main(['--out', str(tmp_path / 'x.json')]) == 1
    assert 'ERROR' in capsys.readouterr().out


def test_json_versionado_al_dia(generado):
    versionado = json.loads(decor.DECOR_JSON.read_text(encoding='utf-8'))
    assert versionado['instances'] == generado['instances']
