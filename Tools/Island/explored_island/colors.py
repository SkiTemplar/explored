"""Colores del terreno: índice de paleta por punto y color por vértice fundido.

Reglas (de abajo arriba): fondo marino claro u oscuro, arena húmeda en la orilla, arena clara en
la playa con vetas de arena oscura en los valles de las dunas, borde de arena oscura entre playa y
hierba, hierba en tres tonos por zona, roca en tres tonos en las pendientes de más de 35° y en la
huella de las rocas (oscura en el acantilado), tierra en los caminos (con un reborde oscuro), en el
fondo de los surcos de la colina y en manchas de borde definido bajo las palmeras.

La variación de la hierba, la arena y la roca es de alta frecuencia (ruido de 4,5 y 9 m más un
hash por punto): transiciones de 1-2 celdas, no manchas de 20 m. El color por vértice se funde con
un núcleo 3 × 3 (1-4-1), que deja los bordes a una o dos celdas.
"""

from __future__ import annotations

import numpy as np

from explored_island.layout import Layout, rock_influence
from explored_island.noise import gradient_noise, hash01, value_noise
from explored_island.relief import SAND_TOP, ReliefFields

# Índices de Tools/Style/paleta.json (comprobados contra la paleta en los tests).
PALETTE = {
    'arena_clara': 0, 'arena_oscura': 1, 'hierba_clara': 2, 'hierba_oscura': 3, 'roca_media': 7,
    'arena_humeda': 12, 'verde_selva': 13, 'verde_lima': 14, 'roca_clara': 18, 'roca_oscura': 19,
    'hierba_salvia': 35, 'hierba_seca': 36, 'tierra': 37,
}

WET_TOP = 0.35          # m: por debajo, arena húmeda de orilla
UNDERWATER = -0.6       # m: por debajo, fondo marino
SHALLOW_BOTTOM = -2.5   # m: fondo claro hasta aquí, oscuro más abajo
ROCK_SLOPE = 35.0       # grados: pendiente que ya es roca
CLIFF_SLOPE = 50.0      # grados: acantilado (roca oscura)
ROCK_FOOTPRINT = 0.2    # influencia de una roca a partir de la cual se pinta de roca
SAND_EDGE = 0.6         # m sobre SAND_TOP: franja mixta de arena oscura y hierba
DIRT_PATCH = 0.86       # umbral del ruido de 7 m a partir del cual hay mancha de tierra suelta
GULLY_WET = 0.35        # peso del surco a partir del cual la hierba es más verde (escorrentía)
GULLY_DIRT = 0.85       # peso del surco a partir del cual el fondo es tierra
DUNE_TROUGH = -0.1      # m de duna por debajo del cual la arena es oscura
PALM_DIRT = 2.3         # m: radio medio de la mancha de tierra bajo una palmera
PALM_SHADE = 3.6        # m: radio del corro de hierba oscura alrededor
PATH_RIM = 0.9          # m fuera del camino con hierba pisada (oscura)
PATH_WIDEN = 0.25       # m que la tierra del camino invade la hierba (borde ondulado ±0,15 m)
TONE_DARK, TONE_LIGHT = -0.15, 0.15   # umbrales del tono (−1..1) para el tono oscuro y el claro

# Tres tonos (oscuro, medio, claro) por tipo de suelo.
TONES = {
    'hierba': ('hierba_oscura', 'hierba_salvia', 'hierba_clara'),
    'sombra': ('verde_selva', 'hierba_oscura', 'hierba_salvia'),
    'colina': ('hierba_salvia', 'hierba_seca', 'verde_lima'),
    'islote': ('hierba_oscura', 'hierba_salvia', 'roca_clara'),
    'roca': ('roca_oscura', 'roca_media', 'roca_clara'),
}


def _zone_mask(layout: Layout, zone_id: str, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    zone = layout.zone(zone_id)
    return np.hypot(x - zone.center[0], y - zone.center[1]) <= zone.radius


def _islet_mask(layout: Layout, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    main, islet = layout.islands[0], layout.islands[-1]
    return islet.normalized_distance(x, y) < main.normalized_distance(x, y)


def tone(seed: int, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    """Tono −1..1 de alta frecuencia: ruido de 4,5 y 9 m, deriva suave de 22 m y un hash de 0,5 m."""
    jitter = hash01(np.floor(x * 2.0).astype(np.int64) + 100000, np.floor(y * 2.0).astype(np.int64) + 100000,
                    seed, 73) - 0.5
    return (0.65 * gradient_noise(x, y, 3.0, seed + 71) + 0.45 * gradient_noise(x, y, 6.5, seed + 72)
            + 0.2 * gradient_noise(x, y, 22.0, seed + 74) + 0.2 * jitter)


def pick_tone(kind: str, t: np.ndarray, light: float = TONE_LIGHT, dark_below: float = TONE_DARK) -> np.ndarray:
    dark, mid, bright = (PALETTE[n] for n in TONES[kind])
    return np.where(t < dark_below, dark, np.where(t > light, bright, mid))


def palm_dirt(palms: np.ndarray, x: np.ndarray, y: np.ndarray, seed: int) -> tuple[np.ndarray, np.ndarray]:
    """(tierra, corro de sombra) bajo cada palmera: círculo de borde ondulado por el ángulo."""
    dirt = np.zeros(x.shape, dtype=bool)
    shade = np.zeros(x.shape, dtype=bool)
    for k, (px, py) in enumerate(palms):
        dx, dy = x - px, y - py
        d = np.hypot(dx, dy)
        near = d < PALM_SHADE + 1.0
        if not near.any():
            continue
        angle = np.arctan2(dy[near], dx[near])
        phase = 6.283 * hash01(np.array([k]), np.array([k]), seed, 91)[0]
        wobble = 1.0 + 0.18 * np.sin(3.0 * angle + phase) + 0.1 * np.sin(5.0 * angle - 2.0 * phase)
        dirt[near] |= d[near] < PALM_DIRT * wobble
        shade[near] |= d[near] < PALM_SHADE * wobble
    return dirt, shade


def _ground(layout: Layout, x: np.ndarray, y: np.ndarray, t: np.ndarray) -> np.ndarray:
    """Hierba por zona en tres tonos."""
    out = pick_tone('hierba', t)
    shade = _zone_mask(layout, 'palmar', x, y) | _zone_mask(layout, 'bosque_bambu', x, y)
    out = np.where(shade, pick_tone('sombra', t), out)
    hill = _zone_mask(layout, 'colina_faro', x, y) & ~shade
    out = np.where(hill, pick_tone('colina', t, light=0.5, dark_below=-0.3), out)
    return np.where(_islet_mask(layout, x, y), pick_tone('islote', t, light=0.6), out)


def point_colors(layout: Layout, points: np.ndarray, slopes: np.ndarray, fields: ReliefFields,
                 palms: np.ndarray) -> np.ndarray:
    """Índice de paleta en cada punto (centroides o vértices); reglas en el docstring del módulo."""
    x, y, z = points[:, 0], points[:, 1], points[:, 2]
    p = PALETTE
    t = tone(layout.seed, x, y)
    rock = rock_influence(layout, x, y)
    rocky = pick_tone('roca', t)
    dirt, shade = palm_dirt(palms, x, y, layout.seed)
    path_edge = PATH_WIDEN + 0.15 * gradient_noise(x, y, 2.5, layout.seed + 77)

    out = _ground(layout, x, y, t)
    out = np.where(shade & (z >= SAND_TOP), np.where(t > 0.0, p['hierba_oscura'], p['verde_selva']), out)
    out = np.where(dirt & (z >= SAND_TOP - 0.3), p['tierra'], out)
    out = np.where(value_noise(x, y, 7.0, layout.seed + 11) > DIRT_PATCH, p['tierra'], out)
    out = np.where(fields.gully > GULLY_WET, np.where(t > 0.1, p['hierba_salvia'], p['hierba_oscura']), out)
    out = np.where(fields.gully > GULLY_DIRT, p['tierra'], out)
    out = np.where(_zone_mask(layout, 'rocas_pescador', x, y), rocky, out)
    out = np.where((fields.path_dist > path_edge) & (fields.path_dist <= PATH_RIM + path_edge) & (z >= SAND_TOP),
                   p['hierba_oscura'], out)
    edge = (z < SAND_TOP + SAND_EDGE) & (gradient_noise(x, y, 3.0, layout.seed + 23) > 0.0)
    out = np.where(edge, p['arena_oscura'], out)
    sand = np.where(fields.dune < DUNE_TROUGH, p['arena_oscura'], p['arena_clara'])
    sand = np.where(t > 0.75, p['arena_oscura'], sand)
    out = np.where(z < SAND_TOP, sand, out)
    out = np.where((fields.path_dist <= path_edge) & (z >= SAND_TOP - 0.4), p['tierra'], out)
    out = np.where(z < WET_TOP, p['arena_humeda'], out)
    out = np.where(rock > ROCK_FOOTPRINT, rocky, out)
    out = np.where(slopes > ROCK_SLOPE, rocky, out)
    out = np.where(slopes > CLIFF_SLOPE, p['roca_oscura'], out)
    seabed = np.where(z > SHALLOW_BOTTOM, p['arena_clara'], p['arena_oscura'])
    seabed = np.where(rock > ROCK_FOOTPRINT, p['roca_media'], seabed)
    out = np.where(z < UNDERWATER, seabed, out)
    return out.astype(np.int64)


def blend_grid(values: np.ndarray, passes: int = 1) -> np.ndarray:
    """Núcleo 3 × 3 (1-4-1 ⊗ 1-4-1, bordes replicados) sobre (nx, ny, c); copia nueva. El centro pesa
    el 44 %: suaviza el escalón entre tonos sin emborronarlo más allá de una celda."""
    out = values.astype(np.float64, copy=True)
    weights = (1.0, 4.0, 1.0)
    for _ in range(passes):
        pad = np.pad(out, [(1, 1), (1, 1)] + [(0, 0)] * (out.ndim - 2), mode='edge')
        acc = np.zeros_like(out)
        for di, wi in enumerate(weights):
            for dj, wj in enumerate(weights):
                acc += wi * wj * pad[di:di + out.shape[0], dj:dj + out.shape[1]]
        out = acc / 36.0
    return out
