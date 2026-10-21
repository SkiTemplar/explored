"""Fixtures de la batería de Tools/Style.

Se ejecuta con el módulo bpy de PyPI (Blender 5.2 como módulo de Python).
Ningún test escribe en el repo: las salidas van a tmp_path y `_repo_intacto`
lo comprueba al final de la sesión.
"""

from pathlib import Path

import bpy  # noqa: F401  (debe importarse antes que bmesh/mathutils)

import pytest

from explored_style import meshkit, pilots
from explored_style.palette import REPO_ROOT

VIGILADAS = (REPO_ROOT / 'Art' / 'Export' / 'Style', REPO_ROOT / 'Art' / 'Export' / 'K2_Estructuras',
             REPO_ROOT / 'docs' / 'v3' / 'laminas',
             REPO_ROOT / 'Tools' / 'Style' / 'paleta.json')


def _foto(ruta: Path) -> dict[str, int]:
    if ruta.is_file():
        return {str(ruta): ruta.stat().st_mtime_ns}
    if not ruta.exists():
        return {}
    return {str(p): p.stat().st_mtime_ns for p in ruta.rglob('*') if p.is_file()}


@pytest.fixture(scope='session', autouse=True)
def _repo_intacto():
    antes = [_foto(r) for r in VIGILADAS]
    yield
    for ruta, a in zip(VIGILADAS, antes, strict=True):
        assert _foto(ruta) == a, f'los tests han modificado {ruta}'


@pytest.fixture
def escena_vacia():
    meshkit.reset_scene()
    yield
    meshkit.reset_scene()


def cubo(nombre: str = 'SM_Cubo', color: int = 0, lado: float = 1.0) -> bpy.types.Object:
    """Cubo de quads con el pivote en la base que cumple el estilo."""
    b = meshkit.MeshBuilder()
    h = lado / 2
    abajo = [b.vert((x, y, 0.0)) for x, y in ((-h, -h), (h, -h), (h, h), (-h, h))]
    arriba = [b.vert((x, y, lado)) for x, y in ((-h, -h), (h, -h), (h, h), (-h, h))]
    b.bridge(abajo, arriba, color)
    b.face(tuple(reversed(abajo)), color)
    b.face(tuple(arriba), color)
    return meshkit.build_object(nombre, b)


@pytest.fixture(scope='session')
def fbx_pilotos(tmp_path_factory) -> Path:
    """Los tres pilotos exportados una sola vez a un directorio temporal."""
    carpeta = tmp_path_factory.mktemp('pilotos')
    pilots.generate_all(carpeta)
    return carpeta


@pytest.fixture(scope='session')
def fbx_k2(tmp_path_factory) -> Path:
    """El kit K2 exportado una sola vez (FBX + manifest) a un directorio temporal."""
    from explored_style.kits import k2

    carpeta = tmp_path_factory.mktemp('k2')
    k2.generate_all(carpeta)
    return carpeta
