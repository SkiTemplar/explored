import copy

import pytest

from explored_island import props as P

DATA = P.load_props()


def test_island_props_cumple_el_gdd(layout):
    assert P.check_props(DATA, layout) == []


def test_main_sin_errores(capsys):
    assert P.main([]) == 0
    assert '0 errores' in capsys.readouterr().out


def _con(romper):
    data = copy.deepcopy(DATA)
    romper(data)
    return data


def _prop(data, pid):
    return next(p for p in data['props'] if p['id'] == pid)


@pytest.mark.parametrize('romper, fragmento', [
    (lambda d: d['props'].pop(0), 'palma'),
    (lambda d: _prop(d, 'lena_cala_1').update(amount=1), 'amount'),
    (lambda d: d['props'].append(dict(d['props'][0])), 'repetido'),
    (lambda d: _prop(d, 'pagina_3').update(at=[60, -90]), 'página 3'),
    (lambda d: _prop(d, 'pagina_8').pop('anchor'), 'anclada'),
    (lambda d: _prop(d, 'pagina_5').update(page=9), 'páginas'),
    (lambda d: _prop(d, 'fibra_palmar_1').update(at=[0, -200]), 'en el agua'),
    (lambda d: _prop(d, 'faro_linterna').update(anchor='nada'), 'ancla'),
    (lambda d: _prop(d, 'tronco_bambu_1').update(type='no_existe'), 'tipo sin definir'),
    (lambda d: _prop(d, 'palma_cala_2').update(mesh='/Game/Otra/SM_X'), 'sustitución'),
    (lambda d: d['types']['palma'].update(mesh='/Game/Art/K1_Naturaleza/SM_Roble_01'), 'GDD §11.1'),
    (lambda d: d['types']['palma'].update(shape='toro'), 'primitiva'),
    (lambda d: d['types']['palma'].update(size=[1, 0, 1]), 'tamaño'),
    (lambda d: d['types']['palma'].update(color=40), 'paleta'),
    (lambda d: d['types']['palma'].update(collision='rara'), 'colisión'),
    (lambda d: _prop(d, 'refugio_cala_volumen').pop('respawn'), 'shelter_volume'),
])
def test_detecta_errores(layout, romper, fragmento):
    errors = P.check_props(_con(romper), layout)
    assert any(fragmento in e for e in errors), errors
