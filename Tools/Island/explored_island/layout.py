"""Lectura de Content/Data/island.json y relieve analítico de la Isla de Sal.

`height_at` es una copia exacta de FIslandLayout::SampleColumn (C++): tierras
elípticas con meseta y plataforma, colinas gaussianas de zona y rocas. La
malla del terreno se muestrea con esta misma función, así que el relieve
aproximado que usa el juego (audio, cámaras, fauna) coincide con la malla
salvo por el fondo marino recortado y el ruido de facetas (< 0,3 m).

Coordenadas en metros: +X es el norte y +Y el este (igual que el mundo de
Unreal dividido entre 100).
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[3]
ISLAND_JSON = REPO_ROOT / 'Content' / 'Data' / 'island.json'

# Constantes de FIslandLayout / IslandLayoutDetail (IslandLayout.h/.cpp).
SEA_LEVEL = 0.0
OCEAN_FLOOR = -30.0
SHELF_SLOPE = 0.12
PLATEAU_EDGE = 0.55
# Las colinas de zona se desvanecen hacia la costa entre estas distancias normalizadas.
HILL_FADE_START = 0.3

FloatArray = np.ndarray


@dataclass(frozen=True)
class Island:
    id: str
    biome: str
    center: tuple[float, float]
    radius: float
    aspect: float
    rotation: float  # radianes
    max_height: float
    # Armónicos de la costa: (k, amplitud, fase en radianes); la costa nominal se escala por
    # 1 + Σ a·cos(k·θ + φ), con θ el ángulo en el espacio normalizado de la elipse.
    coast: tuple[tuple[int, float, float], ...] = ()

    @property
    def minor_radius(self) -> float:
        return self.radius * min(max(self.aspect, 1e-4), 1.0)

    def normalized_distance(self, x: FloatArray, y: FloatArray) -> FloatArray:
        dx, dy = x - self.center[0], y - self.center[1]
        c, s = math.cos(self.rotation), math.sin(self.rotation)
        lx, ly = dx * c + dy * s, -dx * s + dy * c
        ux, uy = lx / max(self.radius, 1e-4), ly / self.minor_radius
        nd = np.sqrt(ux ** 2 + uy ** 2)
        if not self.coast:
            return nd
        theta = np.arctan2(uy, ux)
        scale = np.ones_like(nd)
        for k, amplitude, phase in self.coast:
            scale = scale + amplitude * np.cos(k * theta + phase)
        return nd / np.maximum(scale, 0.2)


@dataclass(frozen=True)
class Zone:
    id: str
    name: str
    biome: str
    center: tuple[float, float]
    radius: float
    hill: float
    hill_sigma: float

    @property
    def sigma(self) -> float:
        return self.hill_sigma if self.hill_sigma > 0.0 else max(self.radius * 0.5, 1.0)


@dataclass(frozen=True)
class Rock:
    id: str
    center: tuple[float, float]
    radius: float
    height: float


@dataclass(frozen=True)
class Path_:
    id: str
    width: float
    points: tuple[tuple[float, float], ...]


@dataclass(frozen=True)
class TerrainSettings:
    min_xy: tuple[float, float]
    max_xy: tuple[float, float]
    cell: float
    seabed: float
    skirt: float


@dataclass(frozen=True)
class Layout:
    seed: int
    islands: tuple[Island, ...]
    zones: tuple[Zone, ...]
    rocks: tuple[Rock, ...]
    paths: tuple[Path_, ...]
    points: dict[str, tuple[float, float]]
    terrain: TerrainSettings

    def zone(self, zone_id: str) -> Zone:
        for z in self.zones:
            if z.id == zone_id:
                return z
        raise KeyError(zone_id)

    def island(self, island_id: str) -> Island:
        for i in self.islands:
            if i.id == island_id:
                return i
        raise KeyError(island_id)


def _pair(value: list[float]) -> tuple[float, float]:
    if len(value) != 2:
        raise ValueError(f'se esperaba un par [x, y] y llegó {value!r}')
    return float(value[0]), float(value[1])


def parse_layout(data: dict) -> Layout:
    """Convierte el JSON en un Layout; ValueError si falta algo esencial."""
    islands = tuple(
        Island(i['id'], i['biome'], _pair(i['center']), float(i['radius']), float(i.get('aspect', 1.0)),
               math.radians(float(i.get('rotationDeg', 0.0))), float(i['maxHeight']),
               tuple((int(h[0]), float(h[1]), math.radians(float(h[2]))) for h in i.get('coast', [])))
        for i in data['islands'])
    if not islands:
        raise ValueError('island.json sin tierras')
    zones = tuple(
        Zone(z['id'], z['name'], z['biome'], _pair(z['center']), float(z['radius']), float(z.get('hill', 0.0)),
             float(z.get('hillSigma', 0.0)))
        for z in data['zones'])
    rocks = tuple(Rock(r['id'], _pair(r['center']), float(r['radius']), float(r['height']))
                  for r in data.get('rocks', []))
    for r in rocks:
        if r.radius <= 0.0:
            raise ValueError(f'roca {r.id} con radio no positivo')
    paths = tuple(Path_(p['id'], float(p.get('width', 3.0)), tuple(_pair(q) for q in p['points']))
                  for p in data.get('paths', []))
    t = data['terrain']
    terrain = TerrainSettings(_pair(t['min']), _pair(t['max']), float(t['cell']), float(t['seabed']),
                              float(t['skirt']))
    if terrain.cell <= 0.0 or terrain.max_xy[0] <= terrain.min_xy[0] or terrain.max_xy[1] <= terrain.min_xy[1]:
        raise ValueError('terreno: celda o rectángulo inválidos')
    points = {p['id']: _pair(p['position']) for p in data['points']}
    return Layout(int(data.get('seed', 0)), islands, zones, rocks, paths, points, terrain)


def load_layout(path: Path = ISLAND_JSON) -> Layout:
    return parse_layout(json.loads(path.read_text(encoding='utf-8')))


def smoothstep(a: float, b: float, x: FloatArray) -> FloatArray:
    """FMath::SmoothStep: 0 por debajo de a, 1 desde b, Hermite entre medias."""
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def land_height(island: Island, nd: FloatArray) -> FloatArray:
    beyond = (nd - 1.0) * island.minor_radius
    shelf = np.maximum(OCEAN_FLOOR, -beyond * SHELF_SLOPE)
    rise = 1.0 - smoothstep(PLATEAU_EDGE, 1.0, nd)
    return np.where(nd >= 1.0, shelf, island.max_height * rise)


def rock_height(rock: Rock, x: FloatArray | float, y: FloatArray | float) -> FloatArray:
    d = np.hypot(x - rock.center[0], y - rock.center[1])
    return rock.height * (1.0 - smoothstep(0.0, rock.radius, d))


def rock_influence(layout: Layout, x: FloatArray | float, y: FloatArray | float) -> FloatArray:
    """Peso (0-1) de la roca más cercana: sirve para pintar de roca su huella. Las
    «rocas» de altura negativa son hondonadas (calas) y no cuentan."""
    out = np.zeros_like(np.asarray(x, dtype=float))
    for rock in layout.rocks:
        if rock.height <= 0.0:
            continue
        d = np.hypot(x - rock.center[0], y - rock.center[1])
        out = np.maximum(out, 1.0 - smoothstep(0.0, rock.radius, d))
    return out


def height_at(layout: Layout, x: FloatArray | float, y: FloatArray | float) -> FloatArray:
    """Altura analítica (m) en (x, y); acepta escalares o arrays."""
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    height = np.full(np.broadcast(x, y).shape, OCEAN_FLOOR)
    best_nd = np.full(height.shape, np.inf)
    for island in layout.islands:
        nd = island.normalized_distance(x, y)
        height = np.maximum(height, land_height(island, nd))
        closer = nd < best_nd
        best_nd = np.where(closer, nd, best_nd)
    land = height > SEA_LEVEL
    land_factor = 1.0 - smoothstep(HILL_FADE_START, 1.0, best_nd)
    hills = np.zeros(height.shape)
    for zone in layout.zones:
        if zone.hill <= 0.0:
            continue
        dist_sq = (x - zone.center[0]) ** 2 + (y - zone.center[1]) ** 2
        hills = hills + zone.hill * land_factor * np.exp(-dist_sq / (2.0 * zone.sigma ** 2))
    height = np.where(land, height + hills, height)
    for rock in layout.rocks:
        height = height + rock_height(rock, x, y)
    return height


def zone_at(layout: Layout, x: float, y: float) -> str | None:
    """Zona cuyo círculo contiene el punto (la de centro más cercano), como FindZoneAt."""
    best: tuple[float, str] | None = None
    for zone in layout.zones:
        d = math.hypot(x - zone.center[0], y - zone.center[1])
        if d <= zone.radius and (best is None or d < best[0]):
            best = (d, zone.id)
    return None if best is None else best[1]


def distance_to_paths(layout: Layout, x: FloatArray | float, y: FloatArray | float) -> FloatArray:
    """Distancia (m) al eje del camino más cercano menos su media anchura (≤ 0 = sobre el camino)."""
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    out = np.full(np.broadcast(x, y).shape, np.inf)
    for path in layout.paths:
        for (ax, ay), (bx, by) in zip(path.points[:-1], path.points[1:], strict=True):
            vx, vy = bx - ax, by - ay
            length_sq = max(vx * vx + vy * vy, 1e-9)
            t = np.clip(((x - ax) * vx + (y - ay) * vy) / length_sq, 0.0, 1.0)
            d = np.hypot(x - (ax + t * vx), y - (ay + t * vy)) - path.width * 0.5
            out = np.minimum(out, d)
    return out
