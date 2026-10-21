"""Malla del terreno (solo numpy; Blender entra en export.py).

Rejilla regular de `terrain.cell` metros con los vértices desplazados al azar (determinista) y la
diagonal de cada celda elegida por hash. La altura de cada vértice es el relieve base (la analítica
de layout.height_at filtrada con una caja de 5 × 5 muestras) más el detalle de media escala de
relief.py (ondulaciones, lomas, surcos, rellanos, dunas, berma y caminos hundidos), recortada al
fondo marino somero (`terrain.seabed`); el anillo exterior baja a `terrain.skirt` para que el agua
no deje ver el canto.

Lectura low poly moderada: la normal de cada esquina de triángulo mezcla la normal suave del vértice
con la de la cara (`FACET_MIX`), y el color por vértice (colors.py) tiene variación de alta
frecuencia en tres tonos de la paleta por zona.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from explored_island.colors import PALETTE, ROCK_SLOPE, SAND_TOP, blend_grid, point_colors
from explored_island.layout import Layout, distance_to_paths, height_at, rock_influence
from explored_island.noise import hash01, value_noise
from explored_island.paths import Profile, axis_height, build_profiles
from explored_island.relief import PATH_LEVEL, ReliefFields, detail, guard_spots

__all__ = ['PALETTE', 'ROCK_SLOPE', 'SAND_TOP', 'TerrainMesh', 'build_terrain', 'hash01', 'surface_height',
           'terrain_height', 'value_noise']

JITTER = 0.12           # fracción de celda que se desplaza cada vértice interior
FACET_NOISE = 0.22      # m de ruido vertical en tierra firme (fuera de caminos)
ROCK_NOISE = 0.2        # m de ruido vertical sobre las rocas
SMOOTH_RADIUS = 1.0     # celdas: radio del filtro de caja 5 × 5 del relieve base
FACET_MIX = 0.4         # peso de la normal de cara en la normal de cada esquina (0 = suave, 1 = facetado)
NO_PALMS = np.zeros((0, 2))


@dataclass(frozen=True)
class TerrainMesh:
    verts: np.ndarray      # (N, 3) metros, +X norte, +Y este, +Z arriba
    tris: np.ndarray       # (M, 3) índices; normales hacia +Z con la regla de la mano derecha
    colors: np.ndarray     # (M,) índice de paleta por triángulo
    slopes: np.ndarray     # (M,) pendiente en grados
    centroids: np.ndarray  # (M, 3)
    normals: np.ndarray    # (N, 3) normal suave de cada vértice
    vertex_colors: np.ndarray  # (N, 3) color lineal fundido de cada vértice (de la paleta)
    corner_normals: np.ndarray  # (M, 3, 3) normal de cada esquina: mezcla de suave y de cara
    base: np.ndarray       # (N,) altura base (sin detalle) de cada vértice

    @property
    def triangle_count(self) -> int:
        return int(self.tris.shape[0])


def smoothed_height(layout: Layout, x: np.ndarray, y: np.ndarray, radius: float) -> np.ndarray:
    """Relieve analítico filtrado con una caja de 5 × 5 muestras de ±radius metros.

    Se filtra en el espacio continuo (no sobre la rejilla desplazada) para no meter el error
    del desplazamiento de los vértices en la pendiente."""
    offsets = np.linspace(-radius, radius, 5)
    acc = np.zeros(np.broadcast(x, y).shape, dtype=np.float64)
    for dx in offsets:
        for dy in offsets:
            acc += height_at(layout, x + dx, y + dy)
    return acc / float(len(offsets) ** 2)


def path_profiles(layout: Layout) -> list[Profile]:
    radius = SMOOTH_RADIUS * layout.terrain.cell
    return build_profiles(layout, lambda x, y: smoothed_height(layout, x, y, radius))


def relief_fields(layout: Layout, props: dict | None, x: np.ndarray, y: np.ndarray, base: np.ndarray,
                  profiles: list[Profile]) -> ReliefFields:
    """Detalle de relieve en los puntos; la altura del eje del camino solo se calcula cerca de él."""
    dist = distance_to_paths(layout, x, y)
    axis_base = base.copy()
    near = dist < PATH_LEVEL[1]
    if near.any():
        axis_base[near] = axis_height(profiles, x[near], y[near])
    return detail(layout, x, y, base, dist, guard_spots(layout, props), axis_base)


def terrain_height(layout: Layout, props: dict | None, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    """Altura continua del terreno (base + detalle, sin el ruido de facetas por vértice)."""
    base = smoothed_height(layout, x, y, SMOOTH_RADIUS * layout.terrain.cell)
    return base + relief_fields(layout, props, x, y, base, path_profiles(layout)).dz


def grid_vertices(layout: Layout, props: dict | None,
                  profiles: list[Profile]) -> tuple[np.ndarray, np.ndarray, int, int]:
    """Vértices (N, 3) de la rejilla desplazada, su altura base y sus dimensiones (nx, ny)."""
    t = layout.terrain
    nx = int(np.ceil((t.max_xy[0] - t.min_xy[0]) / t.cell)) + 1
    ny = int(np.ceil((t.max_xy[1] - t.min_xy[1]) / t.cell)) + 1
    ii, jj = np.meshgrid(np.arange(nx), np.arange(ny), indexing='ij')
    ii, jj = ii.ravel(), jj.ravel()
    border = (ii == 0) | (jj == 0) | (ii == nx - 1) | (jj == ny - 1)
    jitter = np.where(border, 0.0, JITTER * t.cell)
    x = t.min_xy[0] + ii * t.cell + (hash01(ii, jj, layout.seed, 1) - 0.5) * 2.0 * jitter
    y = t.min_xy[1] + jj * t.cell + (hash01(ii, jj, layout.seed, 2) - 0.5) * 2.0 * jitter
    base = smoothed_height(layout, x, y, SMOOTH_RADIUS * t.cell)
    fields = relief_fields(layout, props, x, y, base, profiles)
    z = base + fields.dz
    rock = rock_influence(layout, x, y)
    on_path = fields.path_dist <= 1.5
    noise = hash01(ii, jj, layout.seed, 3) - 0.5
    facet = np.where((z > SAND_TOP) & ~on_path & (rock < 0.1), FACET_NOISE, 0.0)
    facet = np.where(rock >= 0.3, ROCK_NOISE, facet)
    z = np.maximum(z + noise * 2.0 * facet, t.seabed)
    z = np.where(border, t.skirt, z)
    return np.column_stack([x, y, z]), base, nx, ny


def grid_triangles(nx: int, ny: int, seed: int) -> np.ndarray:
    """Dos triángulos por celda con la diagonal elegida por hash."""
    ci, cj = np.meshgrid(np.arange(nx - 1), np.arange(ny - 1), indexing='ij')
    ci, cj = ci.ravel(), cj.ravel()
    v00 = ci * ny + cj
    v10 = (ci + 1) * ny + cj
    v11 = (ci + 1) * ny + cj + 1
    v01 = ci * ny + cj + 1
    flip = hash01(ci, cj, seed, 4) < 0.5
    a = np.where(flip, np.column_stack([v00, v10, v11]).T, np.column_stack([v00, v10, v01]).T).T
    b = np.where(flip, np.column_stack([v00, v11, v01]).T, np.column_stack([v10, v11, v01]).T).T
    return np.concatenate([a, b])


def orient_up(verts: np.ndarray, tris: np.ndarray) -> np.ndarray:
    """Invierte los triángulos cuya normal (mano derecha) apunta hacia abajo."""
    p0, p1, p2 = verts[tris[:, 0]], verts[tris[:, 1]], verts[tris[:, 2]]
    nz = (p1[:, 0] - p0[:, 0]) * (p2[:, 1] - p0[:, 1]) - (p1[:, 1] - p0[:, 1]) * (p2[:, 0] - p0[:, 0])
    out = tris.copy()
    down = nz < 0
    out[down] = out[down][:, [0, 2, 1]]
    return out


def face_normals(verts: np.ndarray, tris: np.ndarray) -> np.ndarray:
    p0, p1, p2 = verts[tris[:, 0]], verts[tris[:, 1]], verts[tris[:, 2]]
    n = np.cross(p1 - p0, p2 - p0)
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


def vertex_normals(verts: np.ndarray, tris: np.ndarray) -> np.ndarray:
    """Normal suave por vértice: suma de las normales de sus caras ponderadas por el área."""
    p0, p1, p2 = verts[tris[:, 0]], verts[tris[:, 1]], verts[tris[:, 2]]
    weighted = np.cross(p1 - p0, p2 - p0)
    acc = np.zeros_like(verts)
    for k in range(3):
        np.add.at(acc, tris[:, k], weighted)
    return acc / np.maximum(np.linalg.norm(acc, axis=1, keepdims=True), 1e-12)


def corner_normals(smooth: np.ndarray, faces: np.ndarray, tris: np.ndarray, mix: float) -> np.ndarray:
    """(M, 3, 3) normal de cada esquina: (1 − mix) · suave del vértice + mix · normal de la cara."""
    blended = (1.0 - mix) * smooth[tris] + mix * faces[:, None, :]
    return blended / np.maximum(np.linalg.norm(blended, axis=2, keepdims=True), 1e-12)


def build_terrain(layout: Layout, props: dict | None, linear_palette: np.ndarray | None = None,
                  palms: np.ndarray = NO_PALMS, facet_mix: float = FACET_MIX) -> TerrainMesh:
    """Malla del terreno. «props» (island_props.json) marca las estructuras que conservan la altura
    base; «palms» (K, 2) son las palmeras con mancha de tierra; «linear_palette» (K, 3) da el color
    lineal de cada índice de la paleta (sin él, los colores por vértice quedan en gris)."""
    profiles = path_profiles(layout)
    verts, base, nx, ny = grid_vertices(layout, props, profiles)
    tris = orient_up(verts, grid_triangles(nx, ny, layout.seed))
    faces = face_normals(verts, tris)
    slopes = np.degrees(np.arccos(np.clip(faces[:, 2], -1.0, 1.0)))
    centroids = verts[tris].mean(axis=1)
    cx, cy = centroids[:, 0], centroids[:, 1]
    centroid_base = base[tris].mean(axis=1)
    colors = point_colors(layout, centroids, slopes, relief_fields(layout, props, cx, cy, centroid_base, profiles),
                          palms)
    smooth = vertex_normals(verts, tris)
    vertex_slopes = np.degrees(np.arccos(np.clip(smooth[:, 2], -1.0, 1.0)))
    index = point_colors(layout, verts, vertex_slopes, relief_fields(layout, props, verts[:, 0], verts[:, 1], base, profiles), palms)
    lin = linear_palette if linear_palette is not None else np.full((64, 3), 0.5)
    vcolors = blend_grid(lin[index].reshape(nx, ny, 3)).reshape(-1, 3)
    return TerrainMesh(verts, tris, colors, slopes, centroids, smooth, vcolors,
                       corner_normals(smooth, faces, tris, facet_mix), base)


def surface_height(mesh: TerrainMesh, layout: Layout, x: float, y: float) -> float:
    """Altura de la malla en (x, y): busca el triángulo de la celda que lo contiene."""
    t = layout.terrain
    ci = int((x - t.min_xy[0]) // t.cell)
    cj = int((y - t.min_xy[1]) // t.cell)
    best = None
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            cell = (ci + di) * (int(np.ceil((t.max_xy[1] - t.min_xy[1]) / t.cell))) + cj + dj
            for tri_index in (cell, cell + mesh.triangle_count // 2):
                if not 0 <= tri_index < mesh.triangle_count:
                    continue
                h = _height_in_triangle(mesh.verts[mesh.tris[tri_index]], x, y)
                if h is not None:
                    best = h
    if best is None:
        raise ValueError(f'({x}, {y}) queda fuera del terreno')
    return best


def _height_in_triangle(tri: np.ndarray, x: float, y: float) -> float | None:
    (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = tri
    det = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
    if abs(det) < 1e-12:
        return None
    l0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / det
    l1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / det
    l2 = 1.0 - l0 - l1
    if min(l0, l1, l2) < -1e-9:
        return None
    return float(l0 * z0 + l1 * z1 + l2 * z2)
