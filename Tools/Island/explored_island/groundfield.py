"""Campo de suelo exacto de la Isla de Sal: `Content/Data/ground_field.bin`.

El juego no puede apoyar nada fino (el césped de 0,3 m) con `FIslandLayout::SampleColumn`: el relieve
analítico difiere hasta ~1,5 m de la malla SM_IslaSal_Terrain desde el relieve de F6 (filtro de caja,
detalle de relief.py, caminos nivelados y ruido de facetas). Este fichero guarda la propia malla
(vértices de la rejilla desplazada y la diagonal de cada celda), así que `FGroundField` (C++) devuelve
la altura del triángulo que se dibuja, sin trazas de física, y además el tipo de suelo de cada vértice
(mar, arena, camino, tierra, roca o hierba) sacado de su color de paleta.

Formato (little-endian):

    char[4] "EXGF", u32 versión (1)
    u32 nx, u32 ny                      vértices por eje (índice de vértice = i · ny + j)
    f32 min_x, f32 min_y, f32 cell      rejilla en metros (+X norte, +Y este)
    u32 probes                          puntos de control
    f32[nx · ny · 3]                    vértices (x, y, z)
    u8[nx · ny]                         tipo de suelo por vértice (SURFACES)
    u8[(nx − 1) · (ny − 1)]             1 si la celda (ci, cj) usa la diagonal v00-v11
    f32[probes · 3]                     (x, y, z esperada): FGroundField los comprueba al cargar

Uso: `uv run island-groundfield [--out RUTA]`. `island-terrain` lo reescribe junto al FBX.
"""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from explored_island.colors import PALETTE, PATH_WIDEN
from explored_island.layout import REPO_ROOT, Layout, distance_to_paths, load_layout
from explored_island.noise import hash01
from explored_island.props import load_props
from explored_island.terrain import NO_PALMS, TerrainMesh, build_terrain

FIELD_BIN = REPO_ROOT / 'Content' / 'Data' / 'ground_field.bin'
MAGIC = b'EXGF'
VERSION = 1
HEADER = struct.Struct('<4sIIIfffI')
PROBES = 256
SEA, SAND, PATH, DIRT, ROCK, GRASS = range(6)
SURFACES = ('mar', 'arena', 'camino', 'tierra', 'roca', 'hierba')
SAND_COLORS = {PALETTE['arena_clara'], PALETTE['arena_oscura'], PALETTE['arena_humeda']}
ROCK_COLORS = {PALETTE['roca_media'], PALETTE['roca_clara'], PALETTE['roca_oscura']}
GRASS_COLORS = {PALETTE[n] for n in ('hierba_clara', 'hierba_oscura', 'verde_selva', 'verde_lima', 'hierba_salvia',
                                     'hierba_seca')}
# La tierra del camino invade la hierba hasta PATH_WIDEN + 0,15 m (borde ondulado de colors.py).
PATH_EDGE = PATH_WIDEN + 0.15


@dataclass(frozen=True)
class GroundField:
    nx: int
    ny: int
    min_xy: tuple[float, float]
    cell: float
    verts: np.ndarray     # (nx · ny, 3) float32
    surface: np.ndarray   # (nx · ny,) uint8
    flip: np.ndarray      # ((nx − 1) · (ny − 1),) uint8
    probes: np.ndarray    # (P, 3) float32


def surface_classes(layout: Layout, mesh: TerrainMesh) -> np.ndarray:
    """Tipo de suelo de cada vértice a partir de su color de paleta (lo que se ve en pantalla)."""
    x, y, z = mesh.verts[:, 0], mesh.verts[:, 1], mesh.verts[:, 2]
    index = mesh.vertex_palette
    out = np.full(index.shape, ROCK, dtype=np.uint8)
    out[np.isin(index, list(GRASS_COLORS))] = GRASS
    out[np.isin(index, list(SAND_COLORS))] = SAND
    dirt = index == PALETTE['tierra']
    out[dirt] = DIRT
    out[dirt & (distance_to_paths(layout, x, y) <= PATH_EDGE)] = PATH
    out[z < 0.0] = SEA
    return out


def cell_flips(layout: Layout, nx: int, ny: int) -> np.ndarray:
    """Diagonal de cada celda: la misma tirada que terrain.grid_triangles."""
    ci, cj = np.meshgrid(np.arange(nx - 1), np.arange(ny - 1), indexing='ij')
    return (hash01(ci.ravel(), cj.ravel(), layout.seed, 4) < 0.5).astype(np.uint8)


def grid_size(layout: Layout) -> tuple[int, int]:
    t = layout.terrain
    nx = int(np.ceil((t.max_xy[0] - t.min_xy[0]) / t.cell)) + 1
    ny = int(np.ceil((t.max_xy[1] - t.min_xy[1]) / t.cell)) + 1
    return nx, ny


def cell_triangles(field: GroundField, ci: int, cj: int) -> tuple[np.ndarray, np.ndarray]:
    """Los dos triángulos (3 índices de vértice cada uno) de la celda (ci, cj)."""
    ny = field.ny
    v00, v10, v11, v01 = ci * ny + cj, (ci + 1) * ny + cj, (ci + 1) * ny + cj + 1, ci * ny + cj + 1
    if field.flip[ci * (ny - 1) + cj]:
        return np.array([v00, v10, v11]), np.array([v00, v11, v01])
    return np.array([v00, v10, v01]), np.array([v10, v11, v01])


def _barycentric(tri: np.ndarray, x: float, y: float) -> np.ndarray | None:
    (x0, y0), (x1, y1), (x2, y2) = tri[:, 0:2].astype(np.float64)
    det = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
    if abs(det) < 1e-12:
        return None
    l0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / det
    l1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / det
    weights = np.array([l0, l1, 1.0 - l0 - l1])
    return weights if weights.min() >= -1e-6 else None


def sample(field: GroundField, x: float, y: float) -> tuple[float, int] | None:
    """(altura, tipo de suelo del vértice más cercano) en (x, y); None fuera de la rejilla. Mismo
    algoritmo que FGroundField::Sample: celdas vecinas de la que contiene el punto, por orden."""
    ci = int(np.floor((x - field.min_xy[0]) / field.cell))
    cj = int(np.floor((y - field.min_xy[1]) / field.cell))
    for di in (0, -1, 1):
        for dj in (0, -1, 1):
            i, j = ci + di, cj + dj
            if not (0 <= i < field.nx - 1 and 0 <= j < field.ny - 1):
                continue
            for tri in cell_triangles(field, i, j):
                weights = _barycentric(field.verts[tri], x, y)
                if weights is not None:
                    z = float(weights @ field.verts[tri, 2].astype(np.float64))
                    return z, int(field.surface[tri[int(np.argmax(weights))]])
    return None


def _probe_points(layout: Layout, rng: np.random.Generator) -> np.ndarray:
    t = layout.terrain
    x = rng.uniform(t.min_xy[0] + t.cell, t.max_xy[0] - t.cell, PROBES)
    y = rng.uniform(t.min_xy[1] + t.cell, t.max_xy[1] - t.cell, PROBES)
    return np.column_stack([x, y])


def build_field(layout: Layout, mesh: TerrainMesh) -> GroundField:
    """Campo de suelo de la malla «mesh» (la que exporta island-terrain) con sus puntos de control."""
    nx, ny = grid_size(layout)
    if mesh.verts.shape[0] != nx * ny:
        raise ValueError(f'la malla tiene {mesh.verts.shape[0]} vértices y la rejilla {nx} × {ny}')
    t = layout.terrain
    field = GroundField(nx, ny, (float(t.min_xy[0]), float(t.min_xy[1])), float(t.cell),
                        mesh.verts.astype(np.float32), surface_classes(layout, mesh), cell_flips(layout, nx, ny),
                        np.zeros((0, 3), dtype=np.float32))
    probes = []
    for px, py in _probe_points(layout, np.random.default_rng(layout.seed + 909)):
        hit = sample(field, float(px), float(py))
        if hit is not None:
            probes.append((px, py, hit[0]))
    return GroundField(nx, ny, field.min_xy, field.cell, field.verts, field.surface, field.flip,
                       np.array(probes, dtype=np.float32))


def to_bytes(field: GroundField) -> bytes:
    head = HEADER.pack(MAGIC, VERSION, field.nx, field.ny, field.min_xy[0], field.min_xy[1], field.cell,
                       field.probes.shape[0])
    parts = [field.verts.astype('<f4'), field.surface.astype(np.uint8), field.flip.astype(np.uint8),
             field.probes.astype('<f4')]
    return head + b''.join(p.tobytes() for p in parts)


def from_bytes(data: bytes) -> GroundField:
    magic, version, nx, ny, min_x, min_y, cell, probes = HEADER.unpack_from(data, 0)
    if magic != MAGIC or version != VERSION:
        raise ValueError(f'cabecera no válida: {magic!r} v{version}')
    offset = HEADER.size
    sizes = (nx * ny * 3 * 4, nx * ny, (nx - 1) * (ny - 1), probes * 3 * 4)
    if len(data) != offset + sum(sizes):
        raise ValueError(f'{len(data)} bytes; se esperaban {offset + sum(sizes)}')
    chunks = []
    for size in sizes:
        chunks.append(data[offset:offset + size])
        offset += size
    return GroundField(nx, ny, (min_x, min_y), cell, np.frombuffer(chunks[0], '<f4').reshape(-1, 3),
                       np.frombuffer(chunks[1], np.uint8), np.frombuffer(chunks[2], np.uint8),
                       np.frombuffer(chunks[3], '<f4').reshape(-1, 3))


def write_field(layout: Layout, mesh: TerrainMesh, out: Path = FIELD_BIN) -> GroundField:
    field = build_field(layout, mesh)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(to_bytes(field))
    return field


def surface_counts(field: GroundField) -> dict[str, int]:
    return {name: int((field.surface == k).sum()) for k, name in enumerate(SURFACES)}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog='island-groundfield', description='Genera ground_field.bin.')
    parser.add_argument('--out', type=Path, default=FIELD_BIN)
    args = parser.parse_args(argv)
    # Las manchas de tierra bajo las palmeras solo cambian el color: misma geometría con o sin ellas,
    # pero el tipo de suelo debe ser el del FBX exportado (export.generate usa las mismas palmeras).
    from explored_island.decor import build_decor, palm_spots

    layout, props = load_layout(), load_props()
    palms = palm_spots(build_decor(layout, props), props) if props else NO_PALMS
    field = write_field(layout, build_terrain(layout, props, palms=palms), args.out)
    print(f'[groundfield] {field.nx} × {field.ny} vértices, {field.probes.shape[0]} puntos de control -> '
          f'{args.out}: {surface_counts(field)}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
