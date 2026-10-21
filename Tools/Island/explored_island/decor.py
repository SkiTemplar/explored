"""Vegetación y rocas decorativas de la Isla de Sal (pasada visual de F4).

Genera `Content/Data/island_decor.json`: colocaciones deterministas de mallas de
los kits K1 (hierba, arbustos, rocas pequeñas, bambú, palmeras y acantilados)
que el juego dibuja con instancias (HISM, sin colisión). No tapan caminos ni
objetos de juego: se respeta una distancia a los caminos de `island.json` y a
cada colocación de `island_props.json`.

Reglas por lugar:
- Palmar: palmeras decorativas, arbustos y hierba.
- Bosque de Bambú: matas de bambú, arbustos y hierba.
- Colina del Faro: hierba y rocas pequeñas; sin palmeras (la vista del faro queda libre).
- Resto de la hierba: hierba, arbustos y alguna roca.
- Borde de playa: alguna palmera y rocas pequeñas.
- Acantilados: `SM_CliffRock` cada ~8 m en las pendientes de las Rocas del
  Pescador y en la ladera más empinada de la colina, de frente hacia abajo.

Vegetación media del kit K6 (Césped), con su propia tirada para no mover las palmeras (el terreno
pinta una mancha de tierra bajo cada una): helechos grandes al pie de las palmeras y en el bambú,
hierba alta en los bordes de los caminos y al pie de las rocas, y corros de flores en los claros
(ruido de 18 m) y en la colina. Mismas distancias a caminos y objetos que el resto.

Apoyo: la vegetación se apoya en el punto más bajo de su huella sobre la malla del
terreno (nada flota) y se descarta si la huella tiene más de `BURY_MAX` m de desnivel
(quedaría enterrada por un lado); los acantilados se entierran `CLIFF_SINK` m. El CLI
falla si `grounding_errors` encuentra alguna instancia mal apoyada.

Uso: `uv run island-decor [--out RUTA]`.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from explored_island.layout import REPO_ROOT, Layout, distance_to_paths, height_at, load_layout, rock_influence
from explored_island.props import STRUCTURE_TYPES, load_props
from explored_island.terrain import SAND_TOP, TerrainMesh, build_terrain, surface_height, value_noise

DECOR_JSON = REPO_ROOT / 'Content' / 'Data' / 'island_decor.json'
KIT = '/Game/Art/K1_Naturaleza/'
KIT_K6 = '/Game/Art/K6_Cesped/'
MESHES = ('SM_GrassClump_01', 'SM_Shrub_01', 'SM_RockSmall_01', 'SM_Bamboo_01', 'SM_PalmA_01', 'SM_PalmB_01',
          'SM_CliffRock_01', 'SM_Fern_01', 'SM_GrassTall_01', 'SM_FlowerA_01', 'SM_FlowerB_01')
GRASS, SHRUB, ROCK, BAMBOO, PALM_A, PALM_B, CLIFF, FERN, TALL, FLOWER_A, FLOWER_B = range(len(MESHES))
K6 = (FERN, TALL, FLOWER_A, FLOWER_B)

GRID = 3.2              # m entre candidatos (rejilla con desplazamiento aleatorio)
PATH_CLEARANCE = 2.0    # m libres a cada lado del borde del camino
PROP_CLEARANCE = 3.0    # m alrededor de un objeto de juego pequeño
BIG_CLEARANCE = 8.0     # m alrededor de estructuras (faro, refugios, balsa, carguero...)
BIG_TYPES = STRUCTURE_TYPES
START_CLEARANCE = 6.0   # m alrededor del punto de aparición
MAX_SLOPE = 30.0        # grados: por encima no se pone vegetación
CLIFF_SLOPE = 22.0      # grados: pendiente mínima para un acantilado
CLIFF_SPACING = 8.0     # m entre acantilados (módulo de SM_CliffRock)
CLIFF_SINK = 1.6        # m que se entierra el acantilado
# Fracción de los puntos válidos de cada zona que reciben acantilado (la colina es una loma suave:
# pocas rocas sueltas en su ladera, no un muro).
CLIFF_KEEP = {'rocas_pescador': 1.0, 'colina_faro': 0.15}
MIN_INSTANCES, MAX_INSTANCES = 1500, 2500
# Radio de apoyo (m, a escala 1) de cada malla: la instancia se apoya en el punto más bajo de ese círculo.
FOOTPRINT = {GRASS: 0.3, SHRUB: 0.5, ROCK: 0.35, BAMBOO: 0.35, PALM_A: 0.25, PALM_B: 0.25,
             FERN: 0.3, TALL: 0.2, FLOWER_A: 0.1, FLOWER_B: 0.1}
# Vegetación media K6: rejilla, escalas y probabilidad por sitio.
K6_GRID = 1.6           # m entre candidatos
K6_SCALE = {FERN: (1.3, 1.9), TALL: (1.2, 1.7), FLOWER_A: (1.0, 1.4), FLOWER_B: (1.0, 1.4)}
PALM_RING = (1.3, 4.2)  # m: corona de helechos alrededor de cada palmera
PATH_BAND = 2.2         # m de banda de hierba alta tras PATH_CLEARANCE
CLEARING_SCALE, CLEARING_MIN = 18.0, 0.6   # ruido de los claros con flores
K6_SITES = {'palm': 0.42, 'bamboo': 0.12, 'path': 0.22, 'rock': 0.25, 'clearing': 0.14, 'hill': 0.04,
            'grass': 0.05}
OCCUPIED = 0.9          # m mínimos a otra instancia ya colocada
FOOTPRINT_SAMPLES = 6
SINK = 0.03             # m que se hunde la base bajo el punto más bajo de la huella
BURY_MAX = 0.45         # m de desnivel máximo dentro de la huella
GROUND_TOL = 0.02       # m de tolerancia (redondeo a cm)


def _zone_mask(layout: Layout, zone_id: str, x: np.ndarray, y: np.ndarray, grow: float = 1.0) -> np.ndarray:
    zone = layout.zone(zone_id)
    return np.hypot(x - zone.center[0], y - zone.center[1]) <= zone.radius * grow


def _slope_deg(layout: Layout, x: np.ndarray, y: np.ndarray, step: float = 1.0) -> tuple[np.ndarray, np.ndarray]:
    """Pendiente (grados) y dirección cuesta abajo (yaw en grados, +X = 0) del relieve analítico."""
    dzdx = (height_at(layout, x + step, y) - height_at(layout, x - step, y)) / (2.0 * step)
    dzdy = (height_at(layout, x, y + step) - height_at(layout, x, y - step)) / (2.0 * step)
    slope = np.degrees(np.arctan(np.hypot(dzdx, dzdy)))
    downhill = np.degrees(np.arctan2(-dzdy, -dzdx))
    return slope, downhill


def _clear_of_props(props: dict, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    ok = np.ones(x.shape, dtype=bool)
    for prop in props['props']:
        if 'anchor' in prop:
            continue
        px, py = prop['at']
        radius = BIG_CLEARANCE if prop['type'] in BIG_TYPES else PROP_CLEARANCE
        ok &= np.hypot(x - px, y - py) > radius
    return ok


def candidates(layout: Layout, rng: np.random.Generator) -> tuple[np.ndarray, np.ndarray]:
    t = layout.terrain
    gx = np.arange(t.min_xy[0] + GRID, t.max_xy[0] - GRID, GRID)
    gy = np.arange(t.min_xy[1] + GRID, t.max_xy[1] - GRID, GRID)
    xx, yy = np.meshgrid(gx, gy, indexing='ij')
    x = xx.ravel() + rng.uniform(-0.45, 0.45, xx.size) * GRID
    y = yy.ravel() + rng.uniform(-0.45, 0.45, yy.size) * GRID
    return x, y


def _pick_vegetation(layout: Layout, x: np.ndarray, y: np.ndarray, z: np.ndarray, roll: np.ndarray,
                     kind: np.ndarray) -> np.ndarray:
    """Malla de cada candidato (−1 = vacío) según la zona; «roll» y «kind» son uniformes [0, 1)."""
    out = np.full(x.shape, -1)
    grass_land = z > SAND_TOP + 0.4
    beach = (z > 0.6) & (z <= SAND_TOP + 0.4)
    palmar = _zone_mask(layout, 'palmar', x, y)
    bamboo = _zone_mask(layout, 'bosque_bambu', x, y)
    hill = _zone_mask(layout, 'colina_faro', x, y) & ~palmar & ~bamboo
    clumps = value_noise(x, y, 14.0, layout.seed + 21)   # vegetación en grupos, no uniforme
    general = grass_land & ~palmar & ~bamboo & ~hill

    def put(mask: np.ndarray, density: float, choices: tuple[tuple[int, float], ...]) -> None:
        nonlocal out
        chosen = mask & (roll < density * (0.35 + clumps)) & (out < 0)
        edges = np.cumsum([w for _, w in choices]) / sum(w for _, w in choices)
        mesh = np.array([m for m, _ in choices])[np.searchsorted(edges, kind, side='right').clip(0, len(choices) - 1)]
        out = np.where(chosen, mesh, out)

    put(palmar & grass_land, 0.22, ((GRASS, 5), (SHRUB, 3), (PALM_A, 1), (PALM_B, 1)))
    put(bamboo & grass_land, 0.26, ((BAMBOO, 3), (SHRUB, 3), (GRASS, 4)))
    put(hill & grass_land, 0.15, ((GRASS, 7), (ROCK, 2), (SHRUB, 1)))
    put(general, 0.2, ((GRASS, 6), (SHRUB, 2), (ROCK, 1)))
    put(beach, 0.025, ((ROCK, 3), (PALM_B, 1), (GRASS, 2)))
    return out


def vegetation(layout: Layout, props: dict, rng: np.random.Generator) -> list[list[float]]:
    x, y = candidates(layout, rng)
    roll, kind = rng.random(x.size), rng.random(x.size)
    yaw, scale = rng.uniform(0.0, 360.0, x.size), rng.uniform(0.8, 1.25, x.size)
    z = height_at(layout, x, y)
    slope, _ = _slope_deg(layout, x, y)
    start = layout.points.get('player_start', (1e9, 1e9))
    ok = (slope < MAX_SLOPE) & (rock_influence(layout, x, y) < 0.15)
    ok &= distance_to_paths(layout, x, y) > PATH_CLEARANCE
    ok &= _clear_of_props(props, x, y)
    ok &= np.hypot(x - start[0], y - start[1]) > START_CLEARANCE
    ok &= ~_zone_mask(layout, 'rocas_pescador', x, y, 0.8)
    mesh = np.where(ok, _pick_vegetation(layout, x, y, z, roll, kind), -1)
    out = []
    for i in np.flatnonzero(mesh >= 0):
        m = int(mesh[i])
        s = float(scale[i]) * (1.15 if m in (PALM_A, PALM_B) else 1.0)
        out.append([m, float(x[i]), float(y[i]), float(z[i]) - 0.05, float(yaw[i]), s])
    return out


def cliffs(layout: Layout, props: dict, rng: np.random.Generator) -> list[list[float]]:
    """Acantilados a lo largo de las pendientes de las Rocas del Pescador y la colina."""
    out: list[list[float]] = []
    for zone_id, grow in (('rocas_pescador', 1.2), ('colina_faro', 1.0)):
        zone = layout.zone(zone_id)
        span = zone.radius * grow
        gx = np.arange(zone.center[0] - span, zone.center[0] + span, CLIFF_SPACING)
        gy = np.arange(zone.center[1] - span, zone.center[1] + span, CLIFF_SPACING)
        xx, yy = np.meshgrid(gx, gy, indexing='ij')
        x = xx.ravel() + rng.uniform(-0.3, 0.3, xx.size) * CLIFF_SPACING
        y = yy.ravel() + rng.uniform(-0.3, 0.3, yy.size) * CLIFF_SPACING
        keep = rng.random(x.size) < CLIFF_KEEP[zone_id]
        slope, downhill = _slope_deg(layout, x, y, 2.0)
        z = height_at(layout, x, y)
        ok = keep & (slope > CLIFF_SLOPE) & (z > 1.0) & _zone_mask(layout, zone_id, x, y, grow)
        ok &= distance_to_paths(layout, x, y) > PATH_CLEARANCE + 3.0
        ok &= _clear_of_props(props, x, y)
        for i in np.flatnonzero(ok):
            # El frente de SM_CliffRock mira a −Y en Blender, que el importador FBX convierte en +Y de
            # Unreal: yaw = dirección cuesta abajo − 90° deja la trasera plana contra la ladera.
            out.append([CLIFF, float(x[i]), float(y[i]), float(z[i]) - CLIFF_SINK,
                        float((downhill[i] - 90.0) % 360.0), float(rng.uniform(0.6, 0.85))])
    return out


def _palm_xy(base: list[list[float]], props: dict) -> np.ndarray:
    spots = [(i[1], i[2]) for i in base if i[0] in (PALM_A, PALM_B)]
    spots += [(float(p['at'][0]), float(p['at'][1])) for p in props['props'] if p['type'] == 'palma']
    return np.array(spots, dtype=np.float64).reshape(-1, 2)


def _near_palm(palms: np.ndarray, x: np.ndarray, y: np.ndarray) -> np.ndarray:
    near = np.zeros(x.shape, dtype=bool)
    for px, py in palms:
        d = np.hypot(x - px, y - py)
        near |= (d > PALM_RING[0]) & (d < PALM_RING[1])
    return near


def _free_of(base: list[list[float]], x: np.ndarray, y: np.ndarray) -> np.ndarray:
    """Lejos (OCCUPIED m) de las instancias ya colocadas: cubos de 2 m para no compararlas todas."""
    buckets: dict[tuple[int, int], list[tuple[float, float]]] = {}
    for inst in base:
        buckets.setdefault((int(inst[1] // 2.0), int(inst[2] // 2.0)), []).append((inst[1], inst[2]))
    ok = np.ones(x.shape, dtype=bool)
    for k in range(x.size):
        bx, by = int(x[k] // 2.0), int(y[k] // 2.0)
        near = [p for dx in (-1, 0, 1) for dy in (-1, 0, 1) for p in buckets.get((bx + dx, by + dy), ())]
        ok[k] = all((x[k] - ox) ** 2 + (y[k] - oy) ** 2 >= OCCUPIED * OCCUPIED for ox, oy in near)
    return ok


def k6_sites(layout: Layout, palms: np.ndarray, x: np.ndarray, y: np.ndarray,
             z: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """(probabilidad, malla) de cada candidato según su sitio; manda la primera regla que aplica."""
    grass_land = z > SAND_TOP + 0.4
    path_d = distance_to_paths(layout, x, y)
    rock = rock_influence(layout, x, y)
    clearing = value_noise(x, y, CLEARING_SCALE, layout.seed + 31) > CLEARING_MIN
    flower = np.where(value_noise(x, y, 7.0, layout.seed + 33) > 0.5, FLOWER_A, FLOWER_B)
    rules = (
        (_near_palm(palms, x, y), 'palm', FERN),
        (_zone_mask(layout, 'bosque_bambu', x, y), 'bamboo', FERN),
        ((path_d > PATH_CLEARANCE) & (path_d < PATH_CLEARANCE + PATH_BAND), 'path', TALL),
        ((rock > 0.02) & (rock < 0.15), 'rock', TALL),
        (clearing & ~_zone_mask(layout, 'palmar', x, y), 'clearing', -1),
        (_zone_mask(layout, 'colina_faro', x, y), 'hill', -1),
        (np.ones(x.shape, dtype=bool), 'grass', TALL),
    )
    prob = np.zeros(x.shape)
    mesh = np.full(x.shape, -1)
    for mask, site, m in rules:
        take = mask & grass_land & (prob == 0.0)
        prob = np.where(take, K6_SITES[site], prob)
        mesh = np.where(take, flower if m < 0 else m, mesh)
    return prob, mesh


def k6_vegetation(layout: Layout, props: dict, base: list[list[float]]) -> list[list[float]]:
    """Helechos, hierba alta y flores del kit K6 (tirada propia: no cambia lo ya colocado)."""
    rng = np.random.default_rng(layout.seed + 606)
    t = layout.terrain
    gx = np.arange(t.min_xy[0] + K6_GRID, t.max_xy[0] - K6_GRID, K6_GRID)
    gy = np.arange(t.min_xy[1] + K6_GRID, t.max_xy[1] - K6_GRID, K6_GRID)
    xx, yy = np.meshgrid(gx, gy, indexing='ij')
    x = xx.ravel() + rng.uniform(-0.45, 0.45, xx.size) * K6_GRID
    y = yy.ravel() + rng.uniform(-0.45, 0.45, yy.size) * K6_GRID
    roll, yaw, size = rng.random(x.size), rng.uniform(0.0, 360.0, x.size), rng.random(x.size)
    z = height_at(layout, x, y)
    prob, mesh = k6_sites(layout, _palm_xy(base, props), x, y, z)
    slope, _ = _slope_deg(layout, x, y)
    start = layout.points.get('player_start', (1e9, 1e9))
    ok = (roll < prob) & (slope < MAX_SLOPE) & (rock_influence(layout, x, y) < 0.15)
    ok &= distance_to_paths(layout, x, y) > PATH_CLEARANCE
    ok &= _clear_of_props(props, x, y)
    ok &= np.hypot(x - start[0], y - start[1]) > START_CLEARANCE
    ok &= ~_zone_mask(layout, 'rocas_pescador', x, y, 0.8)
    pick = np.flatnonzero(ok)
    pick = pick[_free_of(base, x[pick], y[pick])]
    out = []
    for i in pick:
        m = int(mesh[i])
        lo, hi = K6_SCALE[m]
        out.append([m, float(x[i]), float(y[i]), float(z[i]), float(yaw[i]), lo + (hi - lo) * float(size[i])])
    return out


def footprint_heights(mesh: TerrainMesh, layout: Layout, x: float, y: float, radius: float) -> np.ndarray:
    """Altura de la malla en el centro y en FOOTPRINT_SAMPLES puntos del círculo de apoyo."""
    angles = np.linspace(0.0, 2.0 * np.pi, FOOTPRINT_SAMPLES, endpoint=False)
    pts = [(x, y)] + [(x + radius * np.cos(a), y + radius * np.sin(a)) for a in angles]
    return np.array([surface_height(mesh, layout, px, py) for px, py in pts])


def ground(mesh: TerrainMesh, layout: Layout, instances: list[list[float]]) -> list[list[float]]:
    """Apoya cada instancia en la malla: la vegetación en el punto más bajo de su huella (nada flota) y
    se descarta si la huella tiene más de BURY_MAX m de desnivel (quedaría enterrada por un lado); los
    acantilados se entierran CLIFF_SINK m bajo el centro."""
    out = []
    for m, x, y, _z, yaw, s in instances:
        if m == CLIFF:
            z = surface_height(mesh, layout, x, y) - CLIFF_SINK
        else:
            heights = footprint_heights(mesh, layout, x, y, FOOTPRINT[int(m)] * s)
            if heights.max() - heights.min() > BURY_MAX:
                continue
            z = float(heights.min()) - SINK
        out.append([int(m), round(x, 2), round(y, 2), round(z, 2), round(yaw, 1), round(s, 2)])
    return out


def grounding_errors(mesh: TerrainMesh, layout: Layout, decor: dict) -> list[str]:
    """Instancias que flotan (base por encima del suelo de su huella) o quedan enterradas."""
    errors = []
    for m, x, y, z, _yaw, s in decor['instances']:
        if m == CLIFF:
            gap = abs(z + CLIFF_SINK - surface_height(mesh, layout, x, y))
            if gap > GROUND_TOL:
                errors.append(f'acantilado en ({x}, {y}) a {gap:.2f} m de su hundimiento')
            continue
        heights = footprint_heights(mesh, layout, x, y, FOOTPRINT[int(m)] * s)
        if z > heights.min() + GROUND_TOL:
            errors.append(f'{MESHES[m]} en ({x}, {y}) flota {z - heights.min():.2f} m')
        if heights.max() - z > BURY_MAX + SINK + GROUND_TOL:
            errors.append(f'{MESHES[m]} en ({x}, {y}) enterrado {heights.max() - z:.2f} m')
    return errors


def build_decor(layout: Layout, props: dict, mesh: TerrainMesh | None = None) -> dict:
    rng = np.random.default_rng(layout.seed + 404)
    instances = vegetation(layout, props, rng) + cliffs(layout, props, rng)
    instances += k6_vegetation(layout, props, instances)
    # La malla añade el relieve de detalle al analítico: se apoya cada instancia en ella.
    grounded = ground(mesh if mesh is not None else build_terrain(layout, props), layout, instances)
    return {
        'version': 1,
        '_comment': 'Vegetación y rocas decorativas (pasada visual de F4). Lo genera Tools/Island (uv run '
                    'island-decor); no se edita a mano. Metros; +X norte, +Y este. «instances»: [malla, x, y, z, '
                    'yaw en grados, escala]; malla = índice de «meshes». Sin colisión: solo decoración. Si una '
                    'malla no está importada, el juego omite sus instancias con un aviso.',
        'meshes': [(KIT_K6 if i in K6 else KIT) + name for i, name in enumerate(MESHES)],
        'instances': grounded,
    }


def palm_spots(decor: dict, props: dict) -> np.ndarray:
    """(K, 2) palmeras decorativas y de juego: el terreno les pinta una mancha de tierra debajo."""
    spots = [(x, y) for m, x, y, *_ in decor['instances'] if m in (PALM_A, PALM_B)]
    spots += [(float(p['at'][0]), float(p['at'][1])) for p in props['props'] if p['type'] == 'palma']
    return np.array(spots, dtype=np.float64).reshape(-1, 2)


def counts(decor: dict) -> dict[str, int]:
    names = [m.rsplit('/', 1)[-1] for m in decor['meshes']]
    out = dict.fromkeys(names, 0)
    for inst in decor['instances']:
        out[names[inst[0]]] += 1
    return out


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog='island-decor', description='Genera island_decor.json.')
    parser.add_argument('--out', type=Path, default=DECOR_JSON)
    args = parser.parse_args(argv)
    layout, props = load_layout(), load_props()
    mesh = build_terrain(layout, props)
    decor = build_decor(layout, props, mesh)
    total = len(decor['instances'])
    if not MIN_INSTANCES <= total <= MAX_INSTANCES:
        print(f'[decor] ERROR: {total} instancias (objetivo {MIN_INSTANCES}-{MAX_INSTANCES})')
        return 1
    errors = grounding_errors(mesh, layout, decor)
    if errors:
        print(f'[decor] ERROR: {len(errors)} instancias mal apoyadas; la primera: {errors[0]}')
        return 1
    lines = ',\n'.join('    ' + json.dumps(i) for i in decor['instances'])
    head = {k: v for k, v in decor.items() if k != 'instances'}
    text = json.dumps(head, ensure_ascii=False, indent=2)[:-2] + ',\n  "instances": [\n' + lines + '\n  ]\n}\n'
    args.out.write_text(text, encoding='utf-8', newline='\n')
    print(f'[decor] {total} instancias -> {args.out}: {counts(decor)}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
