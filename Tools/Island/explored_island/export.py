"""Exporta el terreno de la Isla de Sal a Art/Island/SM_IslaSal_Terrain.fbx.

Reutiliza explored_style.meshkit (MeshBuilder, UV por celda de paleta, un
material y la exportación FBX común) sin modificarlo. Diferencias con un prop:
el pivote es el nivel del mar (no la base de la malla), el eje Y se invierte al
pasar a Blender, porque el importador FBX de Unreal vuelve a invertirlo
(Blender +Y -> Unreal -Y), y el terreno lleva normales por esquina (mezcla de la
suave del vértice y la de la cara, `terrain.FACET_MIX`) y un atributo de color
por vértice («Col», colores de la paleta fundidos) que lee M_TerrainToon
(Tools/Unreal/materials_f6.py). El alfa de «Col» es el tipo de suelo fundido
igual que el color (surface_alpha): 1 hierba, 0 arena y 0,5 el resto; el
material lo usa para igualar el suelo bajo el césped y para los efectos de la
arena.

Uso: uv run island-terrain [--json RUTA] [--out RUTA.fbx] [--cell METROS]
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from dataclasses import replace
from pathlib import Path

import bpy

import numpy as np
from explored_style import meshkit
from explored_style.palette import load_palette
from mathutils import Matrix

from explored_island.colors import blend_grid
from explored_island.decor import build_decor, palm_spots
from explored_island.groundfield import FIELD_BIN, GRASS_COLORS, SAND_COLORS, grid_size, write_field
from explored_island.layout import ISLAND_JSON, REPO_ROOT, Layout, distance_to_paths, load_layout
from explored_island.props import load_props
from explored_island.terrain import FACET_MIX, TerrainMesh, build_terrain

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


def linear_palette() -> np.ndarray:
    """(K, 3) color lineal de cada índice de Tools/Style/paleta.json."""
    colors = load_palette().colors
    srgb = np.array([[int(c.hex[i:i + 2], 16) / 255.0 for i in (1, 3, 5)] for c in colors])
    return np.where(srgb <= 0.04045, srgb / 12.92, ((srgb + 0.055) / 1.055) ** 2.4)


def surface_alpha(mesh: TerrainMesh, nx: int, ny: int) -> np.ndarray:
    """(N,) alfa del color por vértice: 1 hierba, 0 arena (también la mojada y el fondo) y 0,5 el resto
    (tierra, caminos, roca), fundido con el mismo núcleo 1-4-1 que el color."""
    index = mesh.vertex_palette
    code = np.full(index.shape, 0.5)
    code[np.isin(index, list(GRASS_COLORS))] = 1.0
    code[np.isin(index, list(SAND_COLORS))] = 0.0
    return blend_grid(code.reshape(nx, ny, 1)).reshape(-1)


def build_blender_object(mesh: TerrainMesh, alpha: np.ndarray | None = None) -> bpy.types.Object:
    builder = to_builder(mesh)
    min_z = min(v[2] for v in builder.verts)
    obj = meshkit.build_object(MESH_NAME, builder)
    # build_object deja la base en z = 0; el terreno conserva el nivel del mar en el origen.
    data = meshkit.mesh_of(obj)
    data.transform(Matrix.Translation((0.0, 0.0, min_z)))
    for poly in data.polygons:
        poly.use_smooth = True
    # Normal de cada esquina en el orden de los bucles de to_builder: (i0, i2, i1) y con Y invertida.
    loops = mesh.corner_normals[:, [0, 2, 1], :].reshape(-1, 3) * np.array([1.0, -1.0, 1.0])
    data.normals_split_custom_set([tuple(n) for n in loops.tolist()])
    attr = data.color_attributes.new(name='Col', type='FLOAT_COLOR', domain='POINT')
    a = np.ones(len(mesh.vertex_colors)) if alpha is None else alpha
    rgba = np.concatenate([mesh.vertex_colors, a[:, None]], axis=1)
    attr.data.foreach_set('color', rgba.astype(np.float32).ravel())  # pyright: ignore[reportAttributeAccessIssue]
    data.color_attributes.active_color = attr
    return obj


def export_smooth_fbx(obj: bpy.types.Object, filepath: str) -> str:
    """Como meshkit.export_fbx, pero con normales por vértice y el color por vértice en sRGB."""
    for other in bpy.context.selected_objects or []:
        other.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=filepath, check_existing=False, use_selection=True, global_scale=1.0,
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL', axis_forward='-Z', axis_up='Y',
        object_types={'MESH'}, use_mesh_modifiers=True, mesh_smooth_type='OFF', use_triangles=True,
        use_tspace=False, bake_anim=False, path_mode='STRIP', embed_textures=False, colors_type='SRGB',
    )
    return filepath


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
        'facet_mix': FACET_MIX,
        'colors': {str(k): int(v) for k, v in sorted(Counter(mesh.colors.tolist()).items())},
    }


def generate(layout: Layout, out: Path, props: dict | None = None, field_out: Path | None = None) -> dict:
    """Construye la malla, la exporta a «out» y escribe sus cifras en «out».json. Las palmeras con
    mancha de tierra son las de island_decor (generado aquí mismo, determinista) y las de juego.
    Con «field_out», escribe también el campo de suelo de esa misma malla (groundfield.py)."""
    props = props if props is not None else load_props()
    palms = palm_spots(build_decor(layout, props), props)
    mesh = build_terrain(layout, props, linear_palette(), palms)
    if field_out is not None:
        write_field(layout, mesh, field_out)
    meshkit.reset_scene()
    obj = build_blender_object(mesh, surface_alpha(mesh, *grid_size(layout)))
    out.parent.mkdir(parents=True, exist_ok=True)
    export_smooth_fbx(obj, str(out))
    info = stats(mesh, layout)
    out.with_suffix('.json').write_text(json.dumps(info, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    return info


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description='Exporta el terreno de la Isla de Sal a FBX.')
    parser.add_argument('--json', type=Path, default=ISLAND_JSON)
    parser.add_argument('--out', type=Path, default=DEFAULT_OUT)
    parser.add_argument('--field', type=Path, default=FIELD_BIN, help='campo de suelo (ground_field.bin)')
    parser.add_argument('--cell', type=float, default=None, help='tamaño de celda (m); por defecto el del JSON')
    args = parser.parse_args(argv)
    layout = load_layout(args.json)
    if args.cell is not None:
        layout = replace(layout, terrain=replace(layout.terrain, cell=args.cell))
    info = generate(layout, args.out, field_out=args.field)
    print(f"{args.out}: {info['triangles']} triángulos, pendiente máx. en caminos {info['path_max_slope_deg']}°")
    return 0


if __name__ == '__main__':  # pragma: no cover
    raise SystemExit(main())
