import json

import numpy as np
import pytest

from explored_island import decor
from explored_island.layout import distance_to_paths


@pytest.fixture(scope='module')
def generado(layout, props, terreno):
    return decor.build_decor(layout, props, terreno)


def _xy(generado, mallas=None):
    inst = np.array(generado['instances'])
    if mallas is not None:
        inst = inst[np.isin(inst[:, 0], mallas)]
    return inst


def test_cantidad_y_mallas(generado):
    total = len(generado['instances'])
    assert decor.MIN_INSTANCES <= total <= decor.MAX_INSTANCES
    cuenta = decor.counts(generado)
    colocadas = {m: n for m, n in cuenta.items() if m != 'SM_GrassClump_01'}
    assert all(n > 0 for n in colocadas.values()), cuenta
    assert cuenta['SM_GrassClump_01'] == 0, 'las matas K1 ya no se colocan: las sustituye la alfombra K6'
    assert cuenta['SM_Shrub_01'] > cuenta['SM_PalmA_01']
    assert all(m.startswith(('/Game/Art/K1_Naturaleza/SM_', '/Game/Art/K6_Cesped/SM_')) for m in generado['meshes'])
    k6 = [m for m in generado['meshes'] if '/K6_Cesped/' in m]
    assert len(k6) == len(decor.K6) and all(cuenta[m.rsplit('/', 1)[-1]] >= 100 for m in k6), cuenta


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
    lo, hi = decor.CLIFF_SCALE
    assert len(acantilados) >= 5 and ((acantilados[:, 5] >= lo) & (acantilados[:, 5] <= hi)).all()
    # En la loma de la colina el módulo de acantilado se leía como una caja: solo en las Rocas del Pescador.
    pescador = layout.zone('rocas_pescador')
    cerca = np.hypot(acantilados[:, 1] - pescador.center[0], acantilados[:, 2] - pescador.center[1])
    assert (cerca <= pescador.radius * 1.2 + 1e-6).all()


def test_nada_flota_ni_queda_enterrado(layout, terreno, generado):
    assert decor.grounding_errors(terreno, layout, generado) == []
    # La vegetación se apoya en el punto más bajo de su huella, unos centímetros por debajo.
    from explored_island.terrain import surface_height
    for m, x, y, z, _yaw, _s in generado['instances'][::25]:
        suelo = surface_height(terreno, layout, x, y)
        assert z <= suelo - (decor.CLIFF_SINK if m == decor.CLIFF else decor.SINK) + 0.01


def test_comprobacion_de_apoyo_detecta_errores(layout, terreno, generado):
    from explored_island.terrain import surface_height
    x, y = generado['instances'][0][1:3]
    suelo = surface_height(terreno, layout, x, y)
    malo = {'instances': [[decor.GRASS, x, y, suelo + 0.5, 0.0, 1.0], [decor.GRASS, x, y, suelo - 2.0, 0.0, 1.0],
                          [decor.CLIFF, x, y, suelo, 0.0, 0.7]]}
    errores = decor.grounding_errors(terreno, layout, malo)
    assert len(errores) == 3 and 'flota' in errores[0] and 'enterrado' in errores[1] and 'acantilado' in errores[2]


def test_palmeras_con_mancha(props, generado):
    spots = decor.palm_spots(generado, props)
    decorativas = sum(1 for inst in generado['instances'] if inst[0] in (decor.PALM_A, decor.PALM_B))
    de_juego = sum(1 for p in props['props'] if p['type'] == 'palma')
    assert spots.shape == (decorativas + de_juego, 2) and de_juego == 8


def test_determinista(layout, props, terreno, generado):
    assert decor.build_decor(layout, props, terreno) == generado


def test_main_escribe_json(tmp_path, generado):
    out = tmp_path / 'decor.json'
    assert decor.main(['--out', str(out)]) == 0
    leido = json.loads(out.read_text(encoding='utf-8'))
    assert leido['instances'] == generado['instances'] and leido['meshes'] == generado['meshes']


def test_main_falla_fuera_de_objetivo(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(decor, 'MAX_INSTANCES', 10)
    assert decor.main(['--out', str(tmp_path / 'x.json')]) == 1
    assert 'ERROR' in capsys.readouterr().out


def test_main_falla_si_algo_flota(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(decor, 'grounding_errors', lambda *_: ['SM_Shrub_01 en (0, 0) flota 0.30 m'])
    assert decor.main(['--out', str(tmp_path / 'x.json')]) == 1
    assert 'mal apoyadas' in capsys.readouterr().out


def test_json_versionado_al_dia(generado):
    versionado = json.loads(decor.DECOR_JSON.read_text(encoding='utf-8'))
    assert versionado['instances'] == generado['instances']


def test_k6_no_mueve_las_palmeras(layout, props, terreno, generado):
    """La capa K6 tiene su propia tirada: las palmeras (y la tierra que el terreno pinta debajo) siguen igual."""
    rng = np.random.default_rng(layout.seed + 404)
    base = decor.vegetation(layout, props, rng) + decor.cliffs(layout, props, rng)
    sin_k6 = decor.ground(terreno, layout, [i for i in base if i[0] not in decor.DROPPED])
    assert np.array_equal(decor.palm_spots({'instances': sin_k6}, props), decor.palm_spots(generado, props))
    assert generado['instances'][:len(sin_k6)] == sin_k6


def test_k6_por_sitio(layout, props, generado):
    helechos = _xy(generado, [decor.FERN])
    palmas = decor._palm_xy(generado['instances'], props)
    bambu = layout.zone('bosque_bambu')
    cerca = np.zeros(len(helechos), dtype=bool)
    for px, py in palmas:
        cerca |= np.hypot(helechos[:, 1] - px, helechos[:, 2] - py) < decor.PALM_RING[1]
    cerca |= np.hypot(helechos[:, 1] - bambu.center[0], helechos[:, 2] - bambu.center[1]) <= bambu.radius
    assert cerca.all()
    k6 = _xy(generado, list(decor.K6))
    assert ((k6[:, 5] >= 1.0) & (k6[:, 5] <= 1.9)).all()


def test_k6_no_se_amontona(generado):
    inst = _xy(generado)
    k6 = inst[np.isin(inst[:, 0], decor.K6)]
    otras = inst[~np.isin(inst[:, 0], decor.K6)]
    for fila in k6[::7]:
        assert (np.hypot(otras[:, 1] - fila[1], otras[:, 2] - fila[2]) >= decor.OCCUPIED - 0.02).all()
