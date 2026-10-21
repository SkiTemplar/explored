"""Malla facetada del terreno (solo numpy; Blender entra en export.py).

Rejilla regular de `terrain.cell` metros con los vértices desplazados al azar
(determinista) y la diagonal de cada celda elegida por hash: triángulos de
2,6-3,7 m sin patrón visible. La altura de cada vértice es la analítica de
layout.height_at, recortada al fondo marino somero (`terrain.seabed`); el
anillo exterior baja a `terrain.skirt` para que el agua no deje ver el canto.

Cada triángulo recibe un índice de la paleta de Tools/Style según su altura,
pendiente y zona (arena clara en la costa, arena húmeda en la orilla, hierba
en el llano, roca en pendientes de más de 35° y en las rocas, tierra en los
caminos). La hierba es salvia desaturada con manchas por zona: más oscura en el
Palmar y el Bosque de Bambú, más clara y seca en la Colina del Faro, manchas
de tierra sueltas y un borde de arena oscura entre la playa y la hierba.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from explored_island.layout import Layout, distance_to_paths, height_at, rock_influence

# Índices de Tools/Style/paleta.json (comprobados contra la paleta en los tests).
PALETTE = {
    'arena_clara': 0, 'arena_oscura': 1, 'hierba_clara': 2, 'hierba_oscura': 3, 'roca_media': 7,
    'arena_humeda': 12, 'verde_selva': 13, 'verde_lima': 14, 'roca_clara': 18, 'roca_oscura': 19,
    'hierba_salvia': 35, 'hierba_seca': 36, 'tierra': 37,
}

JITTER = 0.12           # fracción de celda que se desplaza cada vértice interior
FACET_NOISE = 0.05      # m de ruido vertical en tierra firme (fuera de caminos)
ROCK_NOISE = 0.2        # m de ruido vertical sobre las rocas
SMOOTH_RADIUS = 1.0     # celdas: radio del filtro de caja 5 × 5 del relieve (suaviza crestas y costa)
COLOR_BLUR_PASSES = 1   # pasadas de fundido 3 × 3 de los colores por vértice
WET_TOP = 0.35          # m: por debajo, arena húmeda de orilla
SAND_TOP = 1.5          # m: por debajo, arena clara de playa
UNDERWATER = -0.6       # m: por debajo, fondo marino
SHALLOW_BOTTOM = -2.5   # m: fondo claro hasta aquí, oscuro más abajo
ROCK_SLOPE = 35.0       # grados: pendiente que ya es roca
CLIFF_SLOPE = 50.0      # grados: acantilado (roca oscura)
ROCK_FOOTPRINT = 0.2    # influencia de una roca a partir de la cual se pinta de roca
SAND_EDGE = 0.6         # m sobre SAND_TOP: franja mixta de arena oscura y hierba
DIRT_PATCH = 0.86       # umbral del ruido fino a partir del cual hay mancha de tierra


@dataclass(frozen=True)
class TerrainMesh:
    verts: np.ndarray      # (N, 3) metros, +X norte, +Y este, +Z arriba
    tris: np.ndarray       # (M, 3) índices; normales hacia +Z con la regla de la mano derecha
    colors: np.ndarray     # (M,) índice de paleta por triángulo
    slopes: np.ndarray     # (M,) pendiente en grados
    centroids: np.ndarray  # (M, 3)
    normals: np.ndarray    # (N, 3) normal suave de cada vértice
    vertex_colors: np.ndarray  # (N, 3) color lineal fundido de cada vértice (de la paleta)

    @property
    def triangle_count(self) -> int:
        return int(self.tris.shape[0])


def hash01(a: np.ndarray, b: np.ndarray, seed: int, salt: int) -> np.ndarray:
    """Hash entero determinista de (a, b) a [0, 1)."""
    with np.errstate(over='ignore'):
        h = (a.astype(np.uint64) * np.uint64(0x9E3779B1) ^ b.astype(np.uint64) * np.uint64(0x85EBCA77)
             ^ np.uint64((seed * 0x27D4EB2F + salt * 0x165667B1) & 0xFFFFFFFF))
        h = (h ^ (h >> np.uint64(15))) * np.uint64(0x2C1B3C6D)
        h = (h ^ (h >> np.uint64(12))) * np.uint64(0x297A2D39)
        h = h ^ (h >> np.uint64(15))
    return (h & np.uint64(0xFFFFFF)).astype(np.float64) / float(0x1000000)


def value_noise(x: np.ndarray, y: np.ndarray, scale: float, seed: int) -> np.ndarray:
    """Ruido de valor suave (0-1) con celdas de «scale» metros."""
    gx, gy = x / scale, y / scale
    ix, iy = np.floor(gx), np.floor(gy)
    fx, fy = gx - ix, gy - iy
    sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    ix_i, iy_i = (ix + 100000).astype(np.int64), (iy + 100000).astype(np.int64)
    v00 = hash01(ix_i, iy_i, seed, 7)
    v10 = hash01(ix_i + 1, iy_i, seed, 7)
    v01 = hash01(ix_i, iy_i + 1, seed, 7)
    v11 = hash01(ix_i + 1, iy_i + 1, seed, 7)
    return (v00 * (1 - sx) + v10 * sx) * (1 - sy) + (v01 * (1 - sx) + v11 * sx) * sy


def grid_vertices(layout: Layout) -> tuple[np.ndarray, int, int]:
    """Vértices (N, 3) de la rejilla desplazada y sus dimensiones (nx, ny)."""
    t = layout.terrain
    nx = int(np.ceil((t.max_xy[0] - t.min_xy[0]) / t.cell)) + 1
    ny = int(np.ceil((t.max_xy[1] - t.min_xy[1]) / t.cell)) + 1
    ii, jj = np.meshgrid(np.arange(nx), np.arange(ny), indexing='ij')
    ii, jj = ii.ravel(), jj.ravel()
    border = (ii == 0) | (jj == 0) | (ii == nx - 1) | (jj == ny - 1)
    jitter = np.where(border, 0.0, JITTER * t.cell)
    x = t.min_xy[0] + ii * t.cell + (hash01(ii, jj, layout.seed, 1) - 0.5) * 2.0 * jitter
    y = t.min_xy[1] + jj * t.cell + (hash01(ii, jj, layout.seed, 2) - 0.5) * 2.0 * jitter
    z = smoothed_height(layout, x, y, SMOOTH_RADIUS * t.cell)
    rock = rock_influence(layout, x, y)
    on_path = distance_to_paths(layout, x, y) <= 1.5
    noise = hash01(ii, jj, layout.seed, 3) - 0.5
    facet = np.where((z > SAND_TOP) & ~on_path & (rock < 0.1), FACET_NOISE, 0.0)
    facet = np.where(rock >= 0.3, ROCK_NOISE, facet)
    z = np.maximum(z + noise * 2.0 * facet, t.seabed)
    z = np.where(border, t.skirt, z)
    return np.column_stack([x, y, z]), nx, ny


def smoothed_height(layout: Layout, x: np.ndarray, y: np.ndarray, radius: float) -> np.ndarray:
    """Relieve analítico filtrado con una caja de 5 × 5 muestras de ±radius metros.

    Se filtra en el espacio continuo (no sobre la rejilla desplazada) para no meter el error
    del desplazamiento de los vértices en la pendiente."""
    offsets = np.linspace(-radius, radius, 5)
    acc = np.zeros_like(x, dtype=np.float64)
    for dx in offsets:
        for dy in offsets:
            acc += height_at(layout, x + dx, y + dy)
    return acc / float(len(offsets) ** 2)


def smooth_grid(values: np.ndarray, passes: int) -> np.ndarray:
    """Media 3 × 3 (bordes replicados) sobre una rejilla (nx, ny) o (nx, ny, c); copia nueva."""
    out = values.astype(np.float64, copy=True)
    for _ in range(passes):
        pad = np.pad(out, [(1, 1), (1, 1)] + [(0, 0)] * (out.ndim - 2), mode='edge')
        acc = np.zeros_like(out)
        for di in (0, 1, 2):
            for dj in (0, 1, 2):
                acc += pad[di:di + out.shape[0], dj:dj + out.shape[1]]
        out = acc / 9.0
    return out


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


def _zone_mask(layout: Layout, zone_id: str, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    zone = layout.zone(zone_id)
    return np.hypot(x - zone.center[0], y - zone.center[1]) <= zone.radius


def _islet_mask(layout: Layout, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    main, islet = layout.islands[0], layout.islands[-1]
    return islet.normalized_distance(x, y) < main.normalized_distance(x, y)


def _grass_colors(layout: Layout, x: np.ndarray, y: np.ndarray, pick: np.ndarray) -> np.ndarray:
    """Hierba por zona: salvia con manchas (ruido de 22 m) y manchas de tierra (ruido de 7 m)."""
    p = PALETTE
    patches = value_noise(x, y, 22.0, layout.seed)
    fine = value_noise(x, y, 7.0, layout.seed + 11)
    out = np.where(patches > 0.62, p['hierba_oscura'], p['hierba_salvia'])
    out = np.where(patches < 0.22, p['hierba_clara'], out)
    shade = _zone_mask(layout, 'palmar', x, y) | _zone_mask(layout, 'bosque_bambu', x, y)
    shaded = np.where(patches > 0.5, p['hierba_oscura'], p['hierba_salvia'])
    shaded = np.where(patches > 0.75, p['verde_selva'], shaded)
    out = np.where(shade, shaded, out)
    hill = _zone_mask(layout, 'colina_faro', x, y) & ~shade
    dry = np.where(patches > 0.55, p['hierba_salvia'], p['hierba_seca'])
    dry = np.where((patches < 0.3) & (pick < 0.5), p['verde_lima'], dry)
    out = np.where(hill, dry, out)
    return np.where(fine > DIRT_PATCH, p['tierra'], out)


def face_colors(layout: Layout, centroids: np.ndarray, slopes: np.ndarray) -> np.ndarray:
    """Índice de paleta de cada triángulo (reglas en el docstring del módulo)."""
    x, y, z = centroids[:, 0], centroids[:, 1], centroids[:, 2]
    p = PALETTE
    n = len(z)
    idx = np.arange(n)
    pick = hash01(idx, idx // 7, layout.seed, 5)
    patches = value_noise(x, y, 22.0, layout.seed)
    rock = rock_influence(layout, x, y)
    path = distance_to_paths(layout, x, y) <= 0.0

    islet_top = np.where(patches > 0.75, p['hierba_oscura'], p['hierba_salvia'])
    islet_top = np.where(patches < 0.3, p['roca_clara'], islet_top)
    rocky = np.where(pick < 0.55, p['roca_media'], p['roca_clara'])

    out = _grass_colors(layout, x, y, pick)
    out = np.where(_islet_mask(layout, x, y), islet_top, out)
    out = np.where(_zone_mask(layout, 'rocas_pescador', x, y), rocky, out)
    # Borde de arena oscura en manchas coherentes (ruido de 6 m), no motas sueltas por triángulo.
    edge = (z < SAND_TOP + SAND_EDGE) & (value_noise(x, y, 6.0, layout.seed + 23) > 0.5)
    out = np.where(edge, p['arena_oscura'], out)
    out = np.where(z < SAND_TOP, p['arena_clara'], out)
    out = np.where(path & (z >= SAND_TOP), p['tierra'], out)
    out = np.where(z < WET_TOP, p['arena_humeda'], out)
    out = np.where(rock > ROCK_FOOTPRINT, rocky, out)
    out = np.where(slopes > ROCK_SLOPE, rocky, out)
    out = np.where(slopes > CLIFF_SLOPE, p['roca_oscura'], out)
    seabed = np.where(z > SHALLOW_BOTTOM, p['arena_clara'], p['arena_oscura'])
    seabed = np.where(rock > ROCK_FOOTPRINT, p['roca_media'], seabed)
    out = np.where(z < UNDERWATER, seabed, out)
    return out.astype(np.int64)


def vertex_normals(verts: np.ndarray, tris: np.ndarray) -> np.ndarray:
    """Normal suave por vértice: suma de las normales de sus caras ponderadas por el área."""
    p0, p1, p2 = verts[tris[:, 0]], verts[tris[:, 1]], verts[tris[:, 2]]
    weighted = np.cross(p1 - p0, p2 - p0)
    acc = np.zeros_like(verts)
    for k in range(3):
        np.add.at(acc, tris[:, k], weighted)
    return acc / np.maximum(np.linalg.norm(acc, axis=1, keepdims=True), 1e-12)


def blended_vertex_colors(layout: Layout, verts: np.ndarray, normals: np.ndarray, nx: int, ny: int,
                          linear_palette: np.ndarray) -> np.ndarray:
    """Color lineal por vértice: las mismas reglas que las caras, fundidas con la media 3 × 3."""
    slopes = np.degrees(np.arccos(np.clip(normals[:, 2], -1.0, 1.0)))
    index = face_colors(layout, verts, slopes)
    rgb = linear_palette[index].reshape(nx, ny, 3)
    return smooth_grid(rgb, COLOR_BLUR_PASSES).reshape(-1, 3)


def build_terrain(layout: Layout, linear_palette: np.ndarray | None = None) -> TerrainMesh:
    """Malla del terreno. «linear_palette» (K, 3) da el color lineal de cada índice de la paleta;
    sin él, los colores por vértice quedan en gris (solo para las cifras y los tests de forma)."""
    verts, nx, ny = grid_vertices(layout)
    tris = orient_up(verts, grid_triangles(nx, ny, layout.seed))
    normals = face_normals(verts, tris)
    slopes = np.degrees(np.arccos(np.clip(normals[:, 2], -1.0, 1.0)))
    centroids = verts[tris].mean(axis=1)
    colors = face_colors(layout, centroids, slopes)
    smooth = vertex_normals(verts, tris)
    lin = linear_palette if linear_palette is not None else np.full((64, 3), 0.5)
    vcolors = blended_vertex_colors(layout, verts, smooth, nx, ny, lin)
    return TerrainMesh(verts, tris, colors, slopes, centroids, smooth, vcolors)


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
