"""Fixtures de Tools/Island. Ningún test escribe en el repo (salidas en tmp_path)."""

from pathlib import Path

import bpy  # noqa: F401  (debe importarse antes que bmesh/mathutils)

import pytest

from explored_island.layout import REPO_ROOT, Layout, load_layout
from explored_island.terrain import TerrainMesh, build_terrain

VIGILADAS = (REPO_ROOT / 'Art' / 'Island', REPO_ROOT / 'Content' / 'Data' / 'island.json',
             REPO_ROOT / 'Content' / 'Data' / 'island_props.json')


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


@pytest.fixture(scope='session')
def layout() -> Layout:
    return load_layout()


@pytest.fixture(scope='session')
def terreno(layout: Layout) -> TerrainMesh:
    return build_terrain(layout)
