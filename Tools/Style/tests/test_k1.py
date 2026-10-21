import json
import shutil

import bpy  # noqa: F401  (debe importarse antes que mathutils)

import pytest

from explored_style import check_style, meshkit, raster
from explored_style.check_style import BUDGETS
from explored_style.kits import k1, k1_lamina
from explored_style.meshkit import mesh_of

# (alto mín., alto máx., lado mayor mín., lado mayor máx., piezas sueltas)
FORMAS = {
    'SM_PalmA_01': (5.8, 6.8, 4.0, 5.5, 1 + 3 + 7),        # tronco+corona, cocos, hojas
    'SM_PalmB_01': (3.8, 4.8, 4.0, 5.5, 1 + 2 + 6),
    'SM_Bamboo_01': (4.2, 4.9, 2.4, 3.6, 7 + 5 * 16),       # cañas, hojas
    'SM_Shrub_01': (0.8, 1.2, 1.2, 1.8, 5),
    'SM_GrassClump_01': (0.4, 0.8, 0.6, 1.1, k1.GRASS_BLADES),
    'SM_RockBig_01': (1.3, 2.0, 2.2, 3.0, 3),
    'SM_RockSmall_01': (0.3, 0.6, 0.5, 0.9, 2),
    'SM_CliffRock_01': (5.99, 6.01, 7.99, 8.01, 1),
}


@pytest.mark.parametrize('spec', k1.KIT, ids=lambda s: s.name)
def test_malla_cumple_estilo_y_forma(escena_vacia, spec):
    obj = k1.generate(spec)
    report = check_style.check_object(obj, BUDGETS[spec.budget])
    assert report.ok, report.errors
    assert not report.warnings
    alto_min, alto_max, lado_min, lado_max, piezas = FORMAS[spec.name]
    assert alto_min <= obj.dimensions.z <= alto_max
    assert lado_min <= max(obj.dimensions.x, obj.dimensions.y) <= lado_max
    assert meshkit.island_count(obj) == piezas
    assert meshkit.flipped_face_count(obj) == 0


def test_kit_completo_y_nombres():
    nombres = [s.name for s in k1.KIT]
    assert len(nombres) == 8 == len(set(nombres))
    assert all(n.startswith('SM_') and n.endswith('_01') for n in nombres)
    assert set(nombres) == set(k1_lamina.LAYOUT)


def test_acantilado_modular(escena_vacia):
    """Los perfiles de x = -4 y x = +4 coinciden: dos piezas se encadenan sin junta."""
    obj = k1.generate(next(s for s in k1.KIT if s.name == 'SM_CliffRock_01'))
    verts = mesh_of(obj).vertices

    def perfil(x: float) -> list[tuple[float, float]]:
        return sorted((round(v.co.y, 4), round(v.co.z, 4)) for v in verts if abs(v.co.x - x) < 1e-5)

    izquierda, derecha = perfil(-k1.CLIFF_W / 2), perfil(k1.CLIFF_W / 2)
    assert len(izquierda) > 2 * len(k1.STRATA)
    assert izquierda == derecha
    assert 3.8 <= obj.dimensions.y <= 4.4


def test_acantilado_estratos_retranqueados():
    filas = k1.cliff_rows()
    assert len(filas) == k1.ROWS_PER_STRATUM * len(k1.STRATA)
    por_estrato = k1.ROWS_PER_STRATUM
    for s in range(1, len(k1.STRATA)):
        arriba, abajo = filas[s * por_estrato], filas[s * por_estrato - 1]
        delante = [a.y - b.y for a, b in zip(arriba, abajo, strict=True)]
        if s == len(k1.STRATA) - 1:
            assert max(delante) < 0.0   # el remate de hierba vuela
        else:
            assert min(delante) > 0.0   # cada estrato queda detrás del anterior


def test_hojas_de_palmera_en_diente_de_sierra():
    for palm in (k1.PALM_A, k1.PALM_B):
        assert 5 <= len(palm.fronds) <= 7
        for frond in palm.fronds:
            anchos = [w for _, w in k1.frond_stations(frond)]
            assert anchos[1::2] < anchos[2::2]


def test_generate_all_exporta_fbx_validos(fbx_k1, capsys):
    manifest = json.loads((fbx_k1 / 'manifest.json').read_text(encoding='utf-8'))
    assert [m['malla'] for m in manifest['mallas']] == [s.name for s in k1.KIT]
    for spec, entry in zip(k1.KIT, manifest['mallas'], strict=True):
        assert entry['fbx'] == f'{spec.name}.fbx'
        assert entry['tris'] <= BUDGETS[spec.budget]
        assert len(entry['dimensiones_m']) == 3 and entry['colores']
        assert check_style.main([str(fbx_k1 / entry['fbx']), '--budget', spec.budget]) == 0
    assert '0 con fallos' in capsys.readouterr().out


def test_anclas_sin_camara(escena_vacia):
    with pytest.raises(RuntimeError):
        k1_lamina.label_anchors()


def test_lamina_falla_sin_fbx(tmp_path):
    with pytest.raises(FileNotFoundError):
        k1_lamina.make_sheet(tmp_path / 'k1.png', tmp_path)


def test_main_con_lamina_workbench_reducida(fbx_k1, tmp_path):
    carpeta = tmp_path / 'fbx'
    shutil.copytree(fbx_k1, carpeta)
    salida = tmp_path / 'k1.png'
    args = ['--out', str(carpeta), '--lamina', str(salida), '--engine', 'BLENDER_WORKBENCH', '--percent', '10']
    assert k1.main(args) == 0
    imagen = raster.decode_png(salida.read_bytes())
    assert imagen.shape == (k1_lamina.HEIGHT, k1_lamina.WIDTH, 3)
    anclas = k1_lamina.label_anchors()
    assert all(0 <= x <= k1_lamina.WIDTH and 0 <= y <= k1_lamina.HEIGHT for x, y in anclas.values())


def test_main_sin_lamina(tmp_path):
    assert k1.main(['--out', str(tmp_path), '--no-lamina']) == 0
    assert sorted(p.name for p in tmp_path.glob('*.fbx')) == sorted(f'{s.name}.fbx' for s in k1.KIT)
