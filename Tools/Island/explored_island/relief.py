"""Relieve de detalle a escala media sobre el relieve base (pasada «relieve y textura» de F6).

El relieve base (layout.height_at filtrado por terrain.smoothed_height) da la forma y la
jugabilidad; este módulo le suma, de forma determinista y continua en (x, y):

- ondulaciones y lomas (ruido de gradiente, λ 6-25 m) con amplitud por zona (0,45-1,1 m);
- surcos de escorrentía radiales que bajan de la Colina del Faro;
- rellanos rocosos (escalonado parcial) en manchas de las laderas de más de ~16°;
- dunas suaves alargadas y una berma paralela a la orilla en la playa;
- caminos hundidos `PATH_SINK` m, sin pendiente transversal (se nivelan a la altura de su eje) y
  con el borde marcado (escalón, reborde y talud corto en las laderas).

El detalle se anula por debajo de la arena (la costa no se mueve), alrededor de los puntos de
island.json y de las estructuras de island_props.json (se quedan a la misma altura) y cerca del
eje de los caminos (pendiente transitable).
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from explored_island.layout import Layout, height_at, smoothstep
from explored_island.noise import gradient_noise, hash01
from explored_island.props import STRUCTURE_TYPES

SAND_TOP = 1.5            # m: por debajo, arena de playa (mismo valor que terrain.SAND_TOP)

# Ondulaciones y lomas: amplitud (m) por zona; fuera de las zonas, DEFAULT_AMPLITUDE.
DEFAULT_AMPLITUDE = 0.9
ZONE_AMPLITUDE = {'colina_faro': 1.2, 'bosque_bambu': 1.1, 'palmar': 0.75, 'cala_varada': 0.6,
                  'playa_norte': 0.6, 'rocas_pescador': 0.8, 'isla_carguero': 1.0}
UNDULATION_SCALES = (20.0, 10.0, 5.5)   # m (λ ≈ 6-25 m)
UNDULATION_WEIGHTS = (0.55, 0.3, 0.15)
LOMA_SCALE = 15.0          # m: lomas redondeadas (solo la parte positiva del ruido)

# Surcos de escorrentía desde la cima de la Colina del Faro.
GULLY_ZONE = 'colina_faro'
GULLY_COUNT = 13
GULLY_DEPTH = 0.8          # m en el fondo del surco
GULLY_RADII = (9.0, 16.0, 36.0, 50.0)   # m: aparece entre los dos primeros y se apaga entre los dos últimos

# Rellanos rocosos: escalones de TERRACE_STEP m en manchas de las laderas empinadas.
TERRACE_STEP = 1.3
TERRACE_STRENGTH = 0.85
TERRACE_SLOPE = (14.0, 24.0)   # grados de la pendiente base en los que entra el escalonado

# Playa: dunas alargadas y berma a la altura BERM_AT.
DUNE_HEIGHT = 0.45
DUNE_SCALE = 7.0
BERM_HEIGHT = 0.22
BERM_AT = 0.8
BERM_WIDTH = 0.3

# Caminos: hundidos y con el borde marcado; sin detalle cerca del eje.
PATH_SINK = 0.15
PATH_LIP = 0.06            # m del reborde justo fuera del camino
PATH_CALM = (1.0, 6.0)     # m desde el borde: el detalle pasa de PATH_CALM_FLOOR a completo
PATH_CALM_FLOOR = 0.0
PATH_LEVEL = (1.5, 2.7)    # m desde el borde: nivelado completo hasta el primero, nada desde el segundo
PATH_LEVEL_AMOUNT = 1.0    # fracción de la pendiente transversal que se quita

# Protección de puntos de autor y estructuras: radio interior (sin detalle) y exterior (completo).
POINT_GUARD = (3.0, 9.0)
STRUCTURE_GUARD = (4.5, 11.0)


@dataclass(frozen=True)
class ReliefFields:
    """Componentes del detalle en unos puntos; `dz` es lo que se suma a la altura base."""
    dz: np.ndarray
    gully: np.ndarray      # 0-1: fondo de un surco de escorrentía
    ledge: np.ndarray      # 0-1: peso del escalonado (rellanos rocosos)
    dune: np.ndarray       # m: altura de duna (negativa en los valles entre dunas)
    path_dist: np.ndarray  # m: distancia al borde del camino más cercano (≤ 0 = sobre el camino)


def guard_spots(layout: Layout, props: dict | None) -> np.ndarray:
    """(K, 4) x, y, radio interior y exterior de cada punto que conserva la altura base."""
    spots = [(x, y, *POINT_GUARD) for x, y in layout.points.values()]
    for prop in (props or {}).get('props', []):
        if 'anchor' in prop or prop.get('type') not in STRUCTURE_TYPES:
            continue
        spots.append((float(prop['at'][0]), float(prop['at'][1]), *STRUCTURE_GUARD))
    return np.array(spots, dtype=np.float64).reshape(-1, 4)


def guard_weight(spots: np.ndarray, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    """1 donde se conserva la altura base, 0 donde el detalle es completo."""
    out = np.zeros(np.broadcast(x, y).shape)
    for sx, sy, inner, outer in spots:
        out = np.maximum(out, 1.0 - smoothstep(inner, outer, np.hypot(x - sx, y - sy)))
    return out


def zone_amplitude(layout: Layout, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    amp = np.full(np.broadcast(x, y).shape, DEFAULT_AMPLITUDE)
    for zone_id, value in ZONE_AMPLITUDE.items():
        zone = layout.zone(zone_id)
        w = 1.0 - smoothstep(zone.radius * 0.6, zone.radius * 1.15, np.hypot(x - zone.center[0], y - zone.center[1]))
        amp = amp + w * (value - amp)
    return amp


def undulation(seed: int, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    """Ondulaciones (fBm de 3 octavas giradas) más lomas redondeadas; aprox. −1..1."""
    out = np.zeros(np.broadcast(x, y).shape)
    for k, (scale, weight) in enumerate(zip(UNDULATION_SCALES, UNDULATION_WEIGHTS, strict=True)):
        a = 0.6 * k
        u, v = x * np.cos(a) - y * np.sin(a), x * np.sin(a) + y * np.cos(a)
        out = out + weight * gradient_noise(u, v, scale, seed + 41 + k)
    lomas = np.maximum(gradient_noise(x, y, LOMA_SCALE, seed + 45), 0.0) ** 1.5
    return 2.0 * out + 1.3 * lomas - 0.2


def gullies(layout: Layout, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    """Peso 0-1 del fondo de los surcos radiales de la colina (serpentean con ruido)."""
    zone = layout.zone(GULLY_ZONE)
    dx, dy = x - zone.center[0], y - zone.center[1]
    r = np.hypot(dx, dy)
    theta = np.arctan2(dy, dx) + 0.22 * gradient_noise(x, y, 12.0, layout.seed + 51)
    phase = theta * GULLY_COUNT / (2.0 * np.pi)
    index = np.floor(phase + 0.5).astype(np.int64) % GULLY_COUNT
    strength = 0.55 + 0.45 * hash01(index, index, layout.seed, 52)
    channel = smoothstep(0.5, 1.0, 0.5 + 0.5 * np.cos(2.0 * np.pi * phase))
    r0, r1, r2, r3 = GULLY_RADII
    radial = smoothstep(r0, r1, r) * (1.0 - smoothstep(r2, r3, r))
    return channel * radial * strength


def base_slope(layout: Layout, x: np.ndarray, y: np.ndarray, step: float = 1.5) -> np.ndarray:
    """Pendiente (grados) del relieve analítico."""
    dzdx = (height_at(layout, x + step, y) - height_at(layout, x - step, y)) / (2.0 * step)
    dzdy = (height_at(layout, x, y + step) - height_at(layout, x, y - step)) / (2.0 * step)
    return np.degrees(np.arctan(np.hypot(dzdx, dzdy)))


def terraces(layout: Layout, x: np.ndarray, y: np.ndarray, base: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """(desplazamiento m, peso 0-1) del escalonado: rellano plano en la mitad baja de cada escalón."""
    f = base / TERRACE_STEP
    frac = f - np.floor(f)
    # Media de (smoothstep(0,5, 1, frac) − frac) = −0,25: se suma para no hundir la ladera.
    shift = (smoothstep(0.5, 1.0, frac) - frac + 0.25) * TERRACE_STEP
    patches = smoothstep(-0.15, 0.3, gradient_noise(x, y, 14.0, layout.seed + 61))
    weight = smoothstep(*TERRACE_SLOPE, base_slope(layout, x, y)) * patches
    return TERRACE_STRENGTH * weight * shift, weight


def beach(seed: int, x: np.ndarray, y: np.ndarray, base: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """(desplazamiento m, altura de duna m): dunas alargadas en la arena y berma junto a la orilla."""
    sand = smoothstep(0.45, 1.0, base) * (1.0 - smoothstep(SAND_TOP + 0.2, SAND_TOP + 1.0, base))
    a = np.radians(35.0)
    u, v = x * np.cos(a) - y * np.sin(a), x * np.sin(a) + y * np.cos(a)
    dune = DUNE_HEIGHT * gradient_noise(u * 0.45, v, DUNE_SCALE, seed + 81) * sand
    berm = BERM_HEIGHT * np.exp(-(((base - BERM_AT) / BERM_WIDTH) ** 2)) * smoothstep(0.15, 0.45, base)
    return dune + berm, dune


def path_profile(path_dist: np.ndarray, base: np.ndarray) -> np.ndarray:
    """Hundimiento con borde marcado: −PATH_SINK dentro, escalón en 0,35 m y reborde fuera."""
    sink = -PATH_SINK * (1.0 - smoothstep(-0.05, 0.3, path_dist))
    lip = PATH_LIP * np.exp(-(((path_dist - 0.6) / 0.3) ** 2))
    return (sink + lip) * smoothstep(0.2, 0.6, base)


def path_level(path_dist: np.ndarray, base: np.ndarray, axis_base: np.ndarray) -> np.ndarray:
    """Desplazamiento que lleva el camino hacia la altura de su eje (corte y relleno en las laderas)."""
    weight = PATH_LEVEL_AMOUNT * (1.0 - smoothstep(*PATH_LEVEL, path_dist)) * smoothstep(0.2, 0.6, base)
    return weight * (axis_base - base)


def detail(layout: Layout, x: np.ndarray, y: np.ndarray, base: np.ndarray, path_dist: np.ndarray,
           spots: np.ndarray, axis_base: np.ndarray | None = None) -> ReliefFields:
    """Detalle de media escala en (x, y) sobre la altura base «base»; «axis_base» es la altura base en
    el punto del eje del camino más cercano (sin él, el camino no se nivela)."""
    land = smoothstep(SAND_TOP - 0.1, SAND_TOP + 1.2, base)
    gully = gullies(layout, x, y) * land
    ledge_dz, ledge = terraces(layout, x, y, base)
    beach_dz, dune = beach(layout.seed, x, y, base)
    rolling = zone_amplitude(layout, x, y) * undulation(layout.seed, x, y) * land
    calm = PATH_CALM_FLOOR + (1.0 - PATH_CALM_FLOOR) * smoothstep(*PATH_CALM, path_dist)
    keep = 1.0 - guard_weight(spots, x, y)
    dz = (rolling - GULLY_DEPTH * gully + ledge_dz * land + beach_dz) * calm * keep + path_profile(path_dist, base)
    if axis_base is not None:
        dz = dz + path_level(path_dist, base, axis_base) * keep
    return ReliefFields(dz, gully * calm * keep, ledge * land * calm * keep, dune * calm * keep, path_dist)
