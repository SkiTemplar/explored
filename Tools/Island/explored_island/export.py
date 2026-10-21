"""Exporta el terreno de la Isla de Sal a Art/Island/SM_IslaSal_Terrain.fbx.

Reutiliza explored_style.meshkit (MeshBuilder, UV por celda de paleta,
normales planas, un solo material M_Toon y la exportación FBX común) sin
modificarlo. Dos diferencias con un prop: el pivote es el nivel del mar
(no la base de la malla) y el eje Y se invierte al pasar a Blender, porque
el importador FBX de Unreal vuelve a invertirlo (Blender +Y -> Unreal -Y):
así el este del JSON (+Y) es el +Y del mundo de Unreal.

Uso: uv run island-terrain [--json RUTA] [--out RUTA.fbx] [--cell METROS]
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from dataclasses import replace
from pathlib import Path

import bpy

from explored_style import meshkit
from mathutils import Matrix

from explored_island.layout import ISLAND_JSON, REPO_ROOT, Layout, distance_to_paths, load_layout
from explored_island.terrain import TerrainMesh, build_terrain

MESH_NAME = 'SM_IslaSal_Terrain'
DEFAULT_OUT = REPO_ROOT / 'Art' / 'Island' / f'{MESH_NAME}.fbx'


def to_builder(mesh: TerrainMesh) -> meshkit.MeshBuilder:
    """Pasa la malla a coordenadas de Blender (Y invertida, winding invertido)."""
    b = meshkit.MeshBuilder()
    for x, y, z in mesh.verts:
        b.vert((float(x), float(-y), float(z)))
    for (i0, i1, i2), color in zip(mesh.tris, mesh.colors, strict=True):
        b.face((int(i0), int(i2), int(i1)), int(color))
    return b


def build_blender_object(mesh: TerrainMesh) -> bpy.types.Object:
    builder = to_builder(mesh)
    min_z = min(v[2] for v in builder.verts)
    obj = meshkit.build_object(MESH_NAME, builder)
    # build_object deja la base en z = 0; el terreno conserva el nivel del mar en el origen.
    meshkit.mesh_of(obj).transform(Matrix.Translation((0.0, 0.0, min_z)))
    return obj


def path_max_slope(mesh: TerrainMesh, layout: Layout) -> float:
    """Pendiente máxima (grados) de los triángulos sobre los caminos y en tierra firme."""
    on_path = distance_to_paths(layout, mesh.centroids[:, 0], mesh.centroids[:, 1]) <= 0.0
    dry = mesh.centroids[:, 2] > 0.35
    selected = mesh.slopes[on_path & dry]
    return float(selected.max()) if selected.size else 0.0


def stats(mesh: TerrainMesh, layout: Layout) -> dict:
    lo = mesh.verts.min(axis=0)
    hi = mesh.verts.max(axis=0)
    return {
        'mesh': MESH_NAME,
        'triangles': mesh.triangle_count,
        'vertices': int(mesh.verts.shape[0]),
        'bounds_min_m': [round(float(v), 3) for v in lo],
        'bounds_max_m': [round(float(v), 3) for v in hi],
        'path_max_slope_deg': round(path_max_slope(mesh, layout), 2),
        'colors': {str(k): int(v) for k, v in sorted(Counter(mesh.colors.tolist()).items())},
    }


def generate(layout: Layout, out: Path) -> dict:
    """Construye la malla, la exporta a «out» y escribe sus cifras en «out».json."""
    mesh = build_terrain(layout)
    meshkit.reset_scene()
    obj = build_blender_object(mesh)
    out.parent.mkdir(parents=True, exist_ok=True)
    meshkit.export_fbx(obj, str(out))
    info = stats(mesh, layout)
    out.with_suffix('.json').write_text(json.dumps(info, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    return info


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description='Exporta el terreno de la Isla de Sal a FBX.')
    parser.add_argument('--json', type=Path, default=ISLAND_JSON)
    parser.add_argument('--out', type=Path, default=DEFAULT_OUT)
    parser.add_argument('--cell', type=float, default=None, help='tamaño de celda (m); por defecto el del JSON')
    args = parser.parse_args(argv)
    layout = load_layout(args.json)
    if args.cell is not None:
        layout = replace(layout, terrain=replace(layout.terrain, cell=args.cell))
    info = generate(layout, args.out)
    print(f"{args.out}: {info['triangles']} triángulos, pendiente máx. en caminos {info['path_max_slope_deg']}°")
    return 0


if __name__ == '__main__':  # pragma: no cover
    raise SystemExit(main())
