"""
Fixtures compartidas de la batería de Tools/Blender.

Se ejecuta con el módulo `bpy` de PyPI (Blender 5.2 real como módulo de
Python), sin el ejecutable de Blender. Reglas de la batería:

- Nada se escribe en el repo: todas las constantes de rutas de salida
  (EXPORT_DIR, MANIFEST_PATH, DOCS_ART_DIR, OUT_DIR, TEXTURES_DIR,
  CACHE_DIR...) se parchean a directorios temporales, y `_repo_intacto`
  comprueba al final de la sesión que Art/Export y docs/art no han cambiado.
- Estado global aislado: sys.path y sys.argv se restauran al terminar la
  sesión (los scripts insertan sus carpetas en sys.path al importarse, como
  hacen dentro de Blender, y run_props/preview_kit leen sus opciones de
  sys.argv tras «--»).
- Lo caro (generar y exportar un kit entero) se hace UNA vez por sesión y
  lo comparten todos los tests que lo leen.
"""

import importlib
import json
import os
import sys
import time
from pathlib import Path

import bpy  # noqa: F401  (debe importarse antes que bmesh/mathutils)

import pytest

BLENDER_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = BLENDER_DIR.parents[1]
SUBDIRS = ('lib', 'assets', 'animals', 'props')

_SYS_PATH_ORIGINAL = list(sys.path)
_SYS_ARGV_ORIGINAL = list(sys.argv)
for _d in (BLENDER_DIR, *(BLENDER_DIR / s for s in SUBDIRS)):
    if str(_d) not in sys.path:
        sys.path.insert(0, str(_d))
# sin «--»: run_props/preview_kit usan entonces sus opciones por defecto
sys.argv = ['blender']

# Escaneos CC0 que usa rocks_cliffs.py (ver _cliffscan.SOURCE_ASSETS).
ESCANEOS = ('namaqualand_cliff_01', 'namaqualand_cliff_02', 'coastal_cliff_04',
            'namaqualand_boulder_02', 'moon_rock_01')


def importar(nombre):
    """Importa un script del kit por nombre (como hace Blender tras insertar
    sus carpetas en sys.path)."""
    return importlib.import_module(nombre)


def _foto_de(carpeta):
    if not carpeta.exists():
        return {}
    return {str(p): p.stat().st_mtime_ns for p in carpeta.rglob('*') if p.is_file()}


@pytest.fixture(scope='session', autouse=True)
def _repo_intacto():
    """Falla si algún test escribe en las carpetas de salida del repo, y
    restaura sys.path/sys.argv al terminar."""
    vigiladas = [REPO_ROOT / 'Art' / 'Export', REPO_ROOT / 'docs' / 'art']
    antes = [_foto_de(c) for c in vigiladas]
    yield
    despues = [_foto_de(c) for c in vigiladas]
    sys.path[:] = _SYS_PATH_ORIGINAL
    sys.argv[:] = _SYS_ARGV_ORIGINAL
    for carpeta, a, d in zip(vigiladas, antes, despues, strict=True):
        assert a == d, f'los tests han modificado {carpeta}'


@pytest.fixture(scope='session')
def mp_sesion():
    """MonkeyPatch de ámbito de sesión (el de pytest es por función)."""
    with pytest.MonkeyPatch.context() as mp:
        yield mp


# ---------------------------------------------------------------------------
# Texturas y escaneos sintéticos (sustituyen a Art/Export/Textures y a la
# caché de escaneos de Poly Haven, que no existen en CI ni hay red)
# ---------------------------------------------------------------------------
def _png(ruta, w=8, h=8, rgba=(0.3, 0.5, 0.2, 1.0)):
    img = bpy.data.images.new(Path(ruta).stem, w, h, alpha=True)
    img.pixels[:] = list(rgba) * (w * h)
    img.filepath_raw = str(ruta)
    img.file_format = 'PNG'
    img.save()
    bpy.data.images.remove(img)


@pytest.fixture(scope='session', autouse=True)
def texturas_sinteticas(tmp_path_factory, mp_sesion):
    """Texturas PNG mínimas con los nombres que busca common.get_material,
    para cubrir el camino texturizado sin depender de Tools/Textures."""
    carpeta = tmp_path_factory.mktemp('texturas')
    for stem in ('T_FoliageAtlas_BC', 'T_FoliageAtlas_N', 'T_BarkTropical_BC', 'T_BarkTropical_N'):
        _png(carpeta / f'{stem}.png')
    common = importar('common')
    mp_sesion.setattr(common, 'TEXTURES_DIR', str(carpeta))
    return carpeta


def escaneo_sintetico(ruta_gltf, semilla, islas=True, nombre_malla='Scan'):
    """Escribe un glTF que imita un escaneo fotogramétrico: una lámina
    abierta (sin trasera) de ~12 x 9 m con relieve, más un par de islas
    sueltas de «ruido» que keep_largest_island debe descartar."""
    import bmesh
    from mathutils import noise

    common = importar('common')
    common.reset_scene()
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=28, y_segments=22, size=1.0)
    for v in bm.verts:
        x, y = v.co.x, v.co.y
        n = noise.noise((x * 2.5 + semilla, y * 2.5, semilla * 0.37))
        v.co = (x * 6.0, 1.2 * n + 0.8 * x * x, (y + 1.0) * 4.5)
    if islas:
        for k in range(2):
            ret = bmesh.ops.create_icosphere(bm, subdivisions=1, radius=0.2)
            bmesh.ops.translate(bm, verts=ret['verts'], vec=(8.0 + k, 3.0, 1.0))
    me = bpy.data.meshes.new(nombre_malla)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(nombre_malla, me)
    bpy.context.collection.objects.link(obj)
    if nombre_malla != 'Scan':
        # un segundo LOD que import_scan debe descartar al filtrar por nombre
        otro = bpy.data.objects.new('Scan_LOD0', me.copy())
        bpy.context.collection.objects.link(otro)
    os.makedirs(os.path.dirname(ruta_gltf), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(ruta_gltf), export_format='GLTF_SEPARATE',
                              export_materials='NONE')
    common.reset_scene()


@pytest.fixture(scope='session')
def escaneos_cc0(tmp_path_factory, mp_sesion):
    """Caché de escaneos CC0 sintética: el pipeline real de _cliffscan.py
    (voxel remesh, decimate planar, booleano del arco...) corre entero
    sobre ella sin descargar nada."""
    carpeta = tmp_path_factory.mktemp('rocks_cc0')
    for i, asset in enumerate(ESCANEOS):
        nombre = 'Scan_LOD1' if asset == 'moon_rock_01' else 'Scan'
        escaneo_sintetico(carpeta / asset / f'{asset}.gltf', semilla=i * 3.1, nombre_malla=nombre)
    cliffscan = importar('_cliffscan')
    mp_sesion.setattr(cliffscan, 'CACHE_DIR', str(carpeta))
    return carpeta


# ---------------------------------------------------------------------------
# Kits exportados (una vez por sesión)
# ---------------------------------------------------------------------------
def _leer_json(ruta):
    with open(ruta, encoding='utf-8') as f:
        return json.load(f)


def _cronometrar(etiqueta, fn):
    t0 = time.perf_counter()
    fn()
    print(f'\n[tests] {etiqueta}: {time.perf_counter() - t0:.1f} s')


@pytest.fixture(scope='session')
def kit_vegetacion(tmp_path_factory, mp_sesion):
    """Vegetación completa (run_all.main) exportada a un directorio temporal."""
    salida = tmp_path_factory.mktemp('Meshes')
    run_all = importar('run_all')
    mp_sesion.setattr(run_all, 'EXPORT_DIR', str(salida))
    _cronometrar('run_all.main', run_all.main)
    return salida, _leer_json(salida / 'manifest.json')


@pytest.fixture(scope='session')
def kit_props(tmp_path_factory, mp_sesion, escaneos_cc0):
    """Kit de props completo (run_props.main) exportado a un directorio
    temporal, con los escaneos CC0 sintéticos."""
    salida = tmp_path_factory.mktemp('Props')
    run_props = importar('run_props')
    mp_sesion.setattr(run_props, 'EXPORT_DIR', str(salida))
    _cronometrar('run_props.main', run_props.main)
    return salida, _leer_json(salida / 'manifest.json'), _leer_json(salida / 'props.json')


@pytest.fixture(scope='session')
def kit_fauna(tmp_path_factory, mp_sesion):
    """Kit de fauna completo (run_animals.main) exportado a un directorio
    temporal (Fauna/ dentro de un Meshes/ propio, como en el repo)."""
    salida = tmp_path_factory.mktemp('MeshesFauna') / 'Fauna'
    run_animals = importar('run_animals')
    mp_sesion.setattr(run_animals, 'EXPORT_DIR', str(salida))
    _cronometrar('run_animals.main', run_animals.main)
    return salida, _leer_json(salida / 'animals.json')


@pytest.fixture
def validate_en(monkeypatch):
    """Devuelve una función que apunta las constantes de rutas de
    validate.py a los directorios dados (solo durante el test)."""
    validate = importar('validate')

    def apuntar(meshes=None, props=None, fauna=None):
        if meshes is not None:
            monkeypatch.setattr(validate, 'EXPORT_DIR', str(meshes))
            monkeypatch.setattr(validate, 'MANIFEST_PATH', os.path.join(str(meshes), 'manifest.json'))
        if props is not None:
            monkeypatch.setattr(validate, 'EXPORT_DIR_PROPS', str(props))
            monkeypatch.setattr(validate, 'MANIFEST_PATH_PROPS', os.path.join(str(props), 'manifest.json'))
        if fauna is not None:
            monkeypatch.setattr(validate, 'FAUNA_DIR', str(fauna))
            monkeypatch.setattr(validate, 'FAUNA_MANIFEST_PATH', os.path.join(str(fauna), 'animals.json'))
        return validate
    return apuntar


def hash_malla(obj, decimales=4):
    """Huella de las coordenadas de los vértices redondeadas (determinismo)."""
    import hashlib
    h = hashlib.sha256()
    for v in obj.data.vertices:
        h.update(','.join(f'{c:.{decimales}f}' for c in v.co).encode())
        h.update(b';')
    return h.hexdigest()
