import json

import pytest

from explored_style import check_style, meshkit, pilots
from explored_style.check_style import BUDGETS

# (alto mín., alto máx., lado mayor mín., lado mayor máx., piezas sueltas)
FORMAS = {
    'SM_PalmA': (5.0, 6.5, 4.0, 7.0, 1 + 3 + 7),           # tronco+corona, cocos, hojas
    'SM_Cuchillo': (0.02, 0.035, 0.25, 0.32, 3),           # hoja, guarda, mango
    'SM_BarrilLluvia': (0.9, 1.05, 0.65, 0.85, 12 + 2 + 3),  # duelas, aros, agua+grifo+llave
}


@pytest.mark.parametrize('spec', pilots.PILOTS, ids=lambda s: s.name)
def test_piloto_cumple_estilo_y_forma(escena_vacia, spec):
    obj = pilots.generate(spec)
    report = check_style.check_object(obj, BUDGETS[spec.budget])
    assert report.ok, report.errors
    assert not report.warnings
    alto_min, alto_max, lado_min, lado_max, piezas = FORMAS[spec.name]
    assert alto_min <= obj.dimensions.z <= alto_max
    assert lado_min <= max(obj.dimensions.x, obj.dimensions.y) <= lado_max
    assert meshkit.island_count(obj) == piezas
    assert meshkit.flipped_face_count(obj) == 0


def test_palmera_tiene_entre_5_y_7_hojas():
    assert 5 <= len(pilots._FRONDS) <= 7
    for frond in pilots._FRONDS:
        anchos = [w for _, w in pilots._frond_stations(frond)]
        assert anchos[1::2] < anchos[2::2]  # muesca más estrecha que el diente


def test_generate_all_exporta_fbx_validos(fbx_pilotos, capsys):
    manifest = json.loads((fbx_pilotos / 'manifest.json').read_text(encoding='utf-8'))
    assert [m['malla'] for m in manifest['mallas']] == [s.name for s in pilots.PILOTS]
    for spec, entry in zip(pilots.PILOTS, manifest['mallas'], strict=True):
        assert entry['tris'] <= BUDGETS[spec.budget]
        ruta = fbx_pilotos / entry['fbx']
        assert check_style.main([str(ruta), '--budget', spec.budget]) == 0
    assert '0 con fallos' in capsys.readouterr().out


def test_main_pilotos(tmp_path):
    assert pilots.main(['--out', str(tmp_path)]) == 0
    assert sorted(p.name for p in tmp_path.glob('*.fbx')) == sorted(f'{s.name}.fbx' for s in pilots.PILOTS)
