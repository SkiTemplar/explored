import json
import shutil

import bpy  # noqa: F401  (debe importarse antes que mathutils)

import pytest

from explored_style import check_style, meshkit, raster
from explored_style.kits import k3, k3_lamina
from explored_style.kits.k3_base import C
from explored_style.meshkit import mesh_of

# Lado mayor (mín., máx.) en metros: tamaño real con la exageración de juego
# justa para leerse en la mano y en el suelo.
LADO_MAYOR = {
    'SM_Cuchillo': (0.25, 0.32), 'SM_Cantimplora': (0.18, 0.3), 'SM_Barometro': (0.08, 0.14),
    'SM_Coco': (0.18, 0.26), 'SM_CocoVacio': (0.14, 0.2), 'SM_Lena': (0.35, 0.5), 'SM_Fibra': (0.4, 0.6),
    'SM_Cuerda': (0.25, 0.4), 'SM_CanaBambu': (1.3, 1.6), 'SM_CanaPescar': (1.6, 2.0), 'SM_Tronco': (2.45, 2.6),
    'SM_Remo': (1.3, 1.6), 'SM_Pez': (0.28, 0.38), 'SM_PezAsado': (0.4, 0.55), 'SM_TarroAceite': (0.12, 0.18),
    'SM_Mecha': (0.25, 0.35), 'SM_Lente': (0.45, 0.56), 'SM_FaunaPez': (0.25, 0.35),
    'SM_FaunaGaviota': (1.0, 1.3),
}


def test_catalogo_completo():
    nombres = [s.name for s in k3.K3]
    assert len(nombres) == len(set(nombres)) == 19
    assert set(nombres) == set(LADO_MAYOR)
    assert all(s.asset.endswith('_01') for s in k3.K3)
    grandes = {s.name for s in k3.K3 if s.max_tris == 800}
    assert grandes == {'SM_Tronco', 'SM_Remo', 'SM_Lente'}
    assert k3.spec_named('SM_FaunaPez').max_tris == 200
    assert k3.spec_named('SM_FaunaGaviota').max_tris == 400
    with pytest.raises(KeyError):
        k3.spec_named('SM_Inexistente')


@pytest.mark.parametrize('spec', k3.K3, ids=lambda s: s.name)
def test_malla_cumple_estilo_y_presupuesto(escena_vacia, spec):
    obj = k3.generate(spec)
    report = check_style.check_object(obj, spec.max_tris)
    assert report.ok, report.errors
    assert not report.warnings
    assert report.triangles <= spec.max_tris
    assert meshkit.flipped_face_count(obj) == 0
    lado_min, lado_max = LADO_MAYOR[spec.name]
    dims = obj.dimensions
    assert lado_min <= max(dims.x, dims.y, dims.z) <= lado_max


def test_lente_tiene_anillos_escalonados(escena_vacia):
    obj = k3.generate(k3.spec_named('SM_Lente'))
    colores = set(meshkit.face_colors(obj))
    assert {C['cristal'], C['agua_somera'], C['laton']} <= colores
    assert 0.45 <= obj.dimensions.x <= 0.52 and 0.48 <= obj.dimensions.z <= 0.56


def _extremos(obj: bpy.types.Object, eje: int) -> tuple[float, float]:
    """Altura (extensión en Z) de la décima parte delantera y trasera en «eje»."""
    cos = [v.co for v in mesh_of(obj).vertices]
    lo, hi = min(c[eje] for c in cos), max(c[eje] for c in cos)
    corte = (hi - lo) * 0.1

    def alto(sel: list) -> float:
        return max(c.z for c in sel) - min(c.z for c in sel)

    return alto([c for c in cos if c[eje] >= hi - corte]), alto([c for c in cos if c[eje] <= lo + corte])


def test_fauna_pez_mira_a_mas_x(escena_vacia):
    obj = k3.generate(k3.spec_named('SM_FaunaPez'))
    morro, cola = _extremos(obj, 0)
    assert obj.dimensions.x > 4 * obj.dimensions.y
    assert cola > 2 * morro  # la aleta caudal, alta, queda en -X


def test_gaviota_alas_abiertas_y_pico_en_mas_x(escena_vacia):
    obj = k3.generate(k3.spec_named('SM_FaunaGaviota'))
    assert obj.dimensions.y > 1.7 * obj.dimensions.x
    mesh = mesh_of(obj)
    colores = meshkit.face_colors(obj)
    pico = [p.center.x for p, c in zip(mesh.polygons, colores, strict=True) if c == C['laton']]
    assert min(pico) > 0.1


def test_generate_all_exporta_fbx_validos(fbx_k3, capsys):
    manifest = json.loads((fbx_k3 / 'manifest.json').read_text(encoding='utf-8'))
    assert [m['malla'] for m in manifest['mallas']] == [s.asset for s in k3.K3]
    for spec, entry in zip(k3.K3, manifest['mallas'], strict=True):
        assert entry['tris'] <= entry['tope_tris'] == spec.max_tris
        assert entry['colores'] and len(entry['dimensiones_m']) == 3
        assert check_style.main([str(fbx_k3 / entry['fbx']), '--budget', spec.budget]) == 0
    assert '0 con fallos' in capsys.readouterr().out


def test_lamina_falla_sin_fbx(tmp_path):
    with pytest.raises(FileNotFoundError):
        k3_lamina.make_sheet(tmp_path / 'k3.png', tmp_path)


def test_main_kit_con_lamina_workbench(tmp_path):
    salida = tmp_path / 'k3.png'
    args = ['--out', str(tmp_path / 'fbx'), '--lamina', str(salida), '--engine', 'BLENDER_WORKBENCH',
            '--percent', '10']
    assert k3.main(args) == 0
    assert len(list((tmp_path / 'fbx').glob('SM_*_01.fbx'))) == len(k3.K3)
    imagen = raster.decode_png(salida.read_bytes())
    assert imagen.shape == (k3_lamina.HEIGHT, k3_lamina.WIDTH, 3)


def test_main_kit_sin_lamina(tmp_path):
    assert k3.main(['--out', str(tmp_path), '--sin-lamina', '--lamina', str(tmp_path / 'no.png')]) == 0
    assert not (tmp_path / 'no.png').exists()


@pytest.fixture(scope='module')
def fbx_k3(tmp_path_factory):
    carpeta = tmp_path_factory.mktemp('k3')
    k3.generate_all(carpeta)
    yield carpeta
    shutil.rmtree(carpeta, ignore_errors=True)
