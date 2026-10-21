import copy
import json
import math

import numpy as np
import pytest

from explored_island import layout as lay
from explored_island.layout import ISLAND_JSON, height_at, parse_layout, rock_influence, smoothstep, zone_at

DATA = json.loads(ISLAND_JSON.read_text(encoding='utf-8'))


def h(layout, p):
    return float(height_at(layout, *p))


def test_colina_del_faro_llega_a_unos_35_m(layout):
    assert 33.0 <= h(layout, layout.points['lighthouse']) <= 37.0


def test_inicio_en_la_arena_de_cala_varada(layout):
    start = layout.points['player_start']
    assert 0.3 < h(layout, start) < 1.5
    assert zone_at(layout, *start) == 'cala_varada'


def test_sitio_de_la_balsa_en_la_orilla(layout):
    assert 0.0 < h(layout, layout.points['raft_site']) < 0.6


def test_centros_de_zona_en_tierra(layout):
    for zone in layout.zones:
        assert h(layout, zone.center) > 0.0, zone.id


def test_mar_abierto_lejos_de_las_islas(layout):
    assert h(layout, (0.0, -215.0)) < -3.0
    assert h(layout, (-190.0, -220.0)) < -6.0
    assert zone_at(layout, 0.0, -215.0) is None


def test_islote_del_carguero_al_nordeste(layout):
    islet = layout.island('islote_carguero')
    assert h(layout, islet.center) > 4.0
    assert islet.center[0] > 0 and islet.center[1] > 0


def test_armonicos_deforman_la_costa(layout):
    main = layout.island('isla_sal')
    plain = lay.Island(main.id, main.biome, main.center, main.radius, main.aspect, main.rotation, main.max_height)
    xs = np.array([100.0, -100.0, 0.0, 0.0])
    ys = np.array([0.0, 0.0, 120.0, -120.0])
    assert not np.allclose(main.normalized_distance(xs, ys), plain.normalized_distance(xs, ys))


def test_las_hondonadas_bajan_y_no_pintan_roca(layout):
    hollow = next(r for r in layout.rocks if r.height < 0)
    assert float(rock_influence(layout, *hollow.center)) == 0.0
    assert float(lay.rock_height(hollow, *hollow.center)) == pytest.approx(hollow.height)


def test_rocas_levantan_el_suelo(layout):
    rock = next(r for r in layout.rocks if r.id == 'roca_pescador_grande')
    assert float(rock_influence(layout, *rock.center)) == pytest.approx(1.0)
    assert float(lay.rock_height(rock, rock.center[0] + rock.radius, rock.center[1])) == pytest.approx(0.0)


def test_smoothstep_como_fmath():
    assert float(smoothstep(0.0, 1.0, np.array(-1.0))) == 0.0
    assert float(smoothstep(0.0, 1.0, np.array(2.0))) == 1.0
    assert float(smoothstep(0.0, 1.0, np.array(0.5))) == pytest.approx(0.5)


def test_sigma_de_colina_por_defecto_y_explicita(layout):
    assert layout.zone('colina_faro').sigma == 42.0
    assert layout.zone('cala_varada').sigma == pytest.approx(17.5)
    with pytest.raises(KeyError):
        layout.zone('no_existe')
    with pytest.raises(KeyError):
        layout.island('no_existe')


def test_distancia_a_caminos(layout):
    path = layout.paths[0]
    assert float(lay.distance_to_paths(layout, *path.points[0])) == pytest.approx(-path.width / 2)
    assert float(lay.distance_to_paths(layout, 0.0, -215.0)) > 50.0


@pytest.mark.parametrize('romper', [
    lambda d: d.update(islands=[]),
    lambda d: d['rocks'][0].update(radius=0),
    lambda d: d['terrain'].update(cell=0),
    lambda d: d['terrain'].update(max=[-500, 0]),
    lambda d: d['points'][0].update(position=[1, 2, 3]),
])
def test_rechaza_datos_invalidos(romper):
    data = copy.deepcopy(DATA)
    romper(data)
    with pytest.raises(ValueError):
        parse_layout(data)


def test_campos_opcionales_por_defecto():
    data = copy.deepcopy(DATA)
    data.pop('rocks')
    data.pop('paths')
    for island in data['islands']:
        island.pop('coast', None)
    layout = parse_layout(data)
    assert layout.rocks == () and layout.paths == ()
    assert layout.islands[0].rotation == pytest.approx(math.pi / 2)
