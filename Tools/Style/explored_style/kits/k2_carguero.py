"""K2 · Carguero «Sáhara» varado (presupuesto «carguero», 4.000 tris).

- SM_CargueroCasco: casco de 40 m de eslora (proa hacia +X) cosido por
  cuadernas: obra viva oxidada, obra muerta de hierro con chorreones de óxido,
  regala clara, castillo de proa más alto, dos escotillas, dos palos con
  plumas y una brecha oscura a estribor. Escorado 8° a estribor (-Y) y con la proa
  2° arriba, como varado; la silueta (casco oscuro sobre óxido, palos,
  regala clara) es lo que se lee a 150 m.
- SM_CargueroPuente: superestructura de popa (6 × 6 m) con portillos,
  puente de mando hueco con ventanas (dos con cristal, el resto rotas),
  alerones con barandilla, escalera exterior a estribor, chimenea con banda roja,
  palo con farol, consola y rueda del timón dentro. Se exporta recto; su
  colocación sobre el casco ya escorado está en `bridge_mount()`.
"""

from __future__ import annotations

import math

import bpy  # noqa: F401  (debe importarse antes que mathutils)

from mathutils import Matrix, Vector

from explored_style.meshkit import MeshBuilder, blob, box, chamfer_box, lathe, matrix_at, noise, sweep, tube
from explored_style.pilots import C

HULL_ROLL_DEG = 8.0       # escora: la banda de estribor (-Y, la que mira a la cámara) baja
HULL_PITCH_DEG = -2.0     # rotación sobre Y: negativa levanta la proa (+X)
BRIDGE_AT_X = -14.6       # centro del puente sobre la cubierta de popa
_HALF_BEAM = 4.0
# Cuadernas: (x, manga relativa, cota de quilla, cota de cubierta, altura de la borda)
_STATIONS = (
    (-20.0, 0.82, 2.4, 5.7, 1.0), (-19.2, 0.88, 1.3, 5.7, 1.0), (-17.6, 0.94, 0.5, 5.7, 1.0),
    (-15.0, 0.98, 0.12, 5.7, 1.0), (-11.0, 1.0, 0.0, 5.65, 1.0), (-6.0, 1.0, 0.0, 5.6, 1.0),
    (-1.0, 1.0, 0.0, 5.6, 1.0), (4.0, 1.0, 0.0, 5.65, 1.0), (8.5, 0.99, 0.0, 5.8, 1.0),
    (11.5, 0.96, 0.05, 6.0, 1.6), (14.2, 0.88, 0.15, 6.35, 1.6), (16.5, 0.74, 0.4, 6.7, 1.65),
    (18.2, 0.55, 0.95, 7.0, 1.7), (19.3, 0.33, 1.8, 7.25, 1.75), (20.1, 0.1, 3.0, 7.45, 1.8),
)
# Semiperfil de la cuaderna: (fracción de manga, fracción de puntal) de la quilla a la cubierta.
_SECTION = ((0.0, 0.0), (0.5, 0.0), (0.82, 0.05), (0.96, 0.18), (1.0, 0.38), (1.0, 0.68), (1.004, 0.84),
            (1.008, 0.9), (1.01, 1.0))
_RUST_STREAKS = {2, 5, 6, 9, 12}   # tramos de la obra muerta con chorreón de óxido


def _section_ring(b: MeshBuilder, station: tuple[float, float, float, float, float]) -> list[int]:
    """Anillo cerrado de la cuaderna (antihorario visto desde proa): babor
    (+Y) de la quilla a la regala, cubierta y estribor (-Y) de vuelta."""
    x, beam, keel, deck, bulwark = station
    half = _HALF_BEAM * beam
    depth = deck - keel
    side = [(half * fy, keel + depth * fz) for fy, fz in _SECTION]
    side += [(half * 1.01, deck + bulwark), (half * 1.01 - 0.22, deck + bulwark), (half * 1.01 - 0.22, deck)]
    other = [(-y, z) for y, z in reversed(side[1:])]
    return [b.vert((x, y, z)) for y, z in side + other]


def _span_colors(span: int) -> list[int]:
    """Color de cada cara entre dos cuadernas: 11 filas de babor (fondo,
    pantoque, flotación, costado, cintón claro, borda, regala), la cubierta y
    las 11 de estribor."""
    rust, iron, deck = C['oxido'], C['hierro'], C['roca_media']

    def rows(streak: bool) -> list[int]:
        return [rust, rust, rust, C['rojo_faro'], rust if streak else iron, iron, C['roca_clara'], iron, iron,
                C['roca_clara'], deck]

    return rows((span + 2) in _RUST_STREAKS) + [deck] + list(reversed(rows(span in _RUST_STREAKS)))


def _hull_shell(b: MeshBuilder) -> None:
    rings = [_section_ring(b, s) for s in _STATIONS]
    for k, (r0, r1) in enumerate(zip(rings, rings[1:], strict=False)):
        b.bridge(r0, r1, _span_colors(k))
    b.cap(rings[0], (-20.05, 0.0, 4.2), C['hierro'], flip=True)
    b.cap(rings[-1], (20.15, 0.0, 5.6), C['hierro'])


def _disc(b: MeshBuilder, center: Vector, normal: Vector, radius: float, rim: int, face: int) -> None:
    """Portillo o escobén: aro de «rim» con la cara «face» mirando a «normal»."""
    axis = normal.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    lathe(b, [(radius, 0.0), (radius, 0.06), (radius * 0.72, 0.08)], 8, [rim, rim], origin=center, axis=axis,
          cap_top=face, tips=(0.0, 0.07))


def _hull_fittings(b: MeshBuilder) -> None:
    rust, iron = C['oxido'], C['hierro']
    for x in (-5.5, 3.2):
        chamfer_box(b, (5.2, 5.0, 0.9), 0.1, rust, matrix_at(x, 0.0, 5.95), edge_color=iron)
        for k in range(4):
            box(b, (0.18, 4.8, 0.1), iron, matrix_at(x - 1.8 + 1.2 * k, 0.0, 6.43))
    for x, h, reach in ((-10.0, 9.0, 4.5), (8.0, 9.5, -4.5)):
        tube(b, (x, 0.0, 5.5), (x, 0.0, 5.6 + h), 0.24, 0.16, 8, iron, cap1=iron)
        chamfer_box(b, (0.5, 3.4, 0.3), 0.06, rust, matrix_at(x, 0.0, 5.6 + h * 0.62))
        for side in (-1, 1):
            tube(b, (x, side * 1.6, 5.6 + h * 0.62), (x, side * 1.6, 6.8), 0.06, 0.06, 4, iron)
        tube(b, (x + math.copysign(0.3, reach), 0.0, 6.4), (x + reach, 0.4, 6.3 + h * 0.55), 0.13, 0.09, 6, rust,
             cap1=rust)
    for side in (-1, 1):
        _disc(b, Vector((16.6, side * 3.06, 6.2)), Vector((0.35, side * 1.0, 0.0)).normalized(), 0.42, iron,
              C['tinta'])
    chamfer_box(b, (2.6, 0.24, 1.3), 0.08, C['tinta'], matrix_at(-2.0, -4.04, 3.0), edge_color=C['roca_oscura'])
    chamfer_box(b, (1.0, 0.24, 0.7), 0.06, C['roca_oscura'], matrix_at(-0.3, -4.03, 3.45))
    for side in (-1, 1):
        _anchor(b, side)
    for k in range(6):
        x = 13.5 + 0.9 * k
        box(b, (0.25, 0.25, 0.25 + 0.05 * noise(3, k)), iron, matrix_at(x, 0.0, _deck_at(x) + 0.12))


def _anchor(b: MeshBuilder, side: int) -> None:
    """Ancla de cepo estilizada colgando del escobén, pegada al costado de proa."""
    iron = C['hierro']
    frame = matrix_at(16.95, side * 2.98, 4.6, rz=side * -24.0)
    piece = MeshBuilder()
    chamfer_box(piece, (0.22, 0.16, 1.7), 0.04, iron, matrix_at(0.0, 0.0, 0.55))
    chamfer_box(piece, (1.2, 0.16, 0.2), 0.04, iron, matrix_at(0.0, 0.0, -0.25))
    for sx in (-1, 1):
        chamfer_box(piece, (0.2, 0.16, 0.6), 0.04, iron, matrix_at(sx * 0.58, 0.0, 0.0, ry=sx * -25.0))
    lathe(piece, [(0.12, 1.38), (0.2, 1.38), (0.2, 1.52), (0.12, 1.52)], 6, [iron] * 4, closed=True,
          axis=Matrix.Rotation(math.pi / 2, 4, 'X'))
    b.extend(piece, frame)


def _deck_at(x: float) -> float:
    for s0, s1 in zip(_STATIONS, _STATIONS[1:], strict=False):
        if s0[0] <= x <= s1[0]:
            f = (x - s0[0]) / (s1[0] - s0[0])
            return s0[3] + (s1[3] - s0[3]) * f
    raise ValueError(f'x fuera de la eslora: {x}')


def _hull_rotation() -> Matrix:
    return Matrix.Rotation(math.radians(HULL_PITCH_DEG), 4, 'Y') @ Matrix.Rotation(math.radians(HULL_ROLL_DEG), 4,
                                                                                    'X')


def build_hull() -> MeshBuilder:
    local = MeshBuilder()
    _hull_shell(local)
    _hull_fittings(local)
    b = MeshBuilder()
    b.extend(local, _hull_rotation())
    return b


def bridge_mount() -> dict[str, list[float]]:
    """Ubicación (m, en el espacio del pivote del casco) y rotación (grados)
    con las que SM_CargueroPuente se apoya en la cubierta de popa."""
    hull = build_hull()
    min_z = min(v[2] for v in hull.verts)
    p = _hull_rotation() @ Vector((BRIDGE_AT_X, 0.0, _deck_at(BRIDGE_AT_X)))
    return {'ubicacion_m': [round(p.x, 3), round(p.y, 3), round(p.z - min_z, 3)],
            'rotacion_grados': [HULL_ROLL_DEG, HULL_PITCH_DEG, 0.0]}


# ---------------------------------------------------------------------------
# SM_CargueroPuente
# ---------------------------------------------------------------------------
_PAINT, _RUST, _IRON = C['roca_clara'], C['oxido'], C['hierro']
_L2 = 2.85        # cubierta del puente
_WH = (-1.6, 2.4, -2.6, 2.6, _L2, 5.25)   # x0, x1, y0, y1, z0, z1 de la caseta
_WALL = 0.15


def _porthole(b: MeshBuilder, center: Vector, normal: Vector) -> None:
    _disc(b, center, normal, 0.24, _IRON, C['cristal'])


def _deckhouse(b: MeshBuilder) -> None:
    chamfer_box(b, (6.0, 6.0, _L2 - 0.2), 0.12, _PAINT, matrix_at(0.0, 0.0, (_L2 - 0.2) / 2))
    box(b, (6.04, 6.04, 0.35), _RUST, matrix_at(0.0, 0.0, 0.17))
    for y in (-1.6, 0.0, 1.6):
        _porthole(b, Vector((3.0, y, 1.6)), Vector((1.0, 0.0, 0.0)))
    for x in (-1.5, 1.5):
        _porthole(b, Vector((x, -3.0, 1.6)), Vector((0.0, -1.0, 0.0)))
        _porthole(b, Vector((x, 3.0, 1.6)), Vector((0.0, 1.0, 0.0)))
    box(b, (0.95, 0.1, 1.9), _IRON, matrix_at(0.1, -3.02, 1.0))
    box(b, (0.08, 0.06, 0.08), C['laton'], matrix_at(0.45, -3.08, 1.0))
    chamfer_box(b, (6.8, 8.4, 0.24), 0.06, C['roca_media'], matrix_at(0.05, 0.0, _L2 - 0.12), edge_color=_PAINT)
    for side in (-1, 1):
        for x in (-2.4, 2.4):
            tube(b, (x, side * 3.0, 1.5), (x, side * 3.95, _L2 - 0.2), 0.06, 0.06, 4, _IRON)


def _wall_x(b: MeshBuilder, x: float, y0: float, y1: float, windows: int, glazed: set[int]) -> None:
    """Pared perpendicular a X con ventanas: antepecho, montantes y dintel."""
    z0, z1 = _WH[4], _WH[5]
    sill, head = z0 + 1.05, z1 - 0.4
    width = y1 - y0
    box(b, (_WALL, width, sill - z0), _PAINT, matrix_at(x, (y0 + y1) / 2, (z0 + sill) / 2))
    box(b, (_WALL, width, z1 - head), _PAINT, matrix_at(x, (y0 + y1) / 2, (head + z1) / 2))
    post = 0.18
    pane = (width - post * (windows + 1)) / windows
    for k in range(windows + 1):
        box(b, (_WALL, post, head - sill), _PAINT, matrix_at(x, y0 + post / 2 + k * (pane + post), (sill + head) / 2))
    for k in glazed:
        yc = y0 + post + pane / 2 + k * (pane + post)
        box(b, (0.04, pane, head - sill), C['cristal'], matrix_at(x, yc, (sill + head) / 2))
    box(b, (0.12, width + 0.1, 0.08), _RUST, matrix_at(x + math.copysign(0.1, x), (y0 + y1) / 2, sill))


def _wall_y(b: MeshBuilder, y: float, x0: float, x1: float) -> None:
    """Pared lateral con puerta abierta al alerón (hueco de 0,9 × 2,05 m) y ventana."""
    z0, z1 = _WH[4], _WH[5]
    door0, door1 = x1 - 1.25, x1 - 0.35
    box(b, (door0 - x0, _WALL, z1 - z0), _PAINT, matrix_at((x0 + door0) / 2, y, (z0 + z1) / 2))
    box(b, (x1 - door1, _WALL, z1 - z0), _PAINT, matrix_at((door1 + x1) / 2, y, (z0 + z1) / 2))
    box(b, (door1 - door0, _WALL, z1 - z0 - 2.05), _PAINT, matrix_at((door0 + door1) / 2, y, z1 - (z1 - z0 - 2.05) / 2))
    box(b, (1.0, 0.06, 0.8), C['cielo_noche'], matrix_at(x0 + 1.2, y + math.copysign(0.06, y), z0 + 1.55))
    box(b, (1.15, 0.1, 0.1), _IRON, matrix_at(x0 + 1.2, y + math.copysign(0.08, y), z0 + 1.1))


def _wheelhouse(b: MeshBuilder) -> None:
    x0, x1, y0, y1, z0, z1 = _WH
    _wall_x(b, x1 - _WALL / 2, y0, y1, 5, {0, 3})
    _wall_x(b, x0 + _WALL / 2, y0, y1, 2, set())
    _wall_y(b, y0 + _WALL / 2, x0 + _WALL, x1 - _WALL)
    _wall_y(b, y1 - _WALL / 2, x0 + _WALL, x1 - _WALL)
    chamfer_box(b, (x1 - x0 + 0.7, y1 - y0 + 0.6, 0.22), 0.06, _PAINT, matrix_at((x0 + x1) / 2 + 0.2, 0.0, z1 + 0.11),
                edge_color=_RUST)
    chamfer_box(b, (1.8, 3.2, 1.0), 0.08, C['roca_oscura'], matrix_at(x1 - 0.75, 0.0, z0 + 0.5))
    wheel = Vector((x1 - 1.45, 0.0, z0 + 1.15))
    to_x = Matrix.Rotation(math.pi / 2, 4, 'Y')
    lathe(b, [(0.28, -0.03), (0.34, -0.03), (0.34, 0.03), (0.28, 0.03)], 10, [C['madera_oscura']] * 4,
          origin=wheel, axis=to_x, closed=True)
    for k in range(3):
        a = math.pi * k / 3
        d = Vector((0.0, math.cos(a), math.sin(a))) * 0.4
        tube(b, wheel - d, wheel + d, 0.02, 0.02, 4, C['laton'])
    tube(b, wheel, wheel + Vector((0.45, 0.0, -0.15)), 0.05, 0.05, 6, C['laton'])


def _railing(b: MeshBuilder, path: list[tuple[float, float]], z: float) -> None:
    sweep(b, [(x, y, z + 1.0) for x, y in path], 0.035, 4, _RUST, cap=_RUST)
    sweep(b, [(x, y, z + 0.5) for x, y in path], 0.025, 4, _IRON, cap=_IRON)
    for (xa, ya), (xb, yb) in zip(path, path[1:], strict=False):
        steps = max(1, round(math.hypot(xb - xa, yb - ya) / 1.1))
        for k in range(steps + 1 if (xb, yb) == path[-1] else steps):
            f = k / steps
            box(b, (0.06, 0.06, 1.0), _IRON, matrix_at(xa + (xb - xa) * f, ya + (yb - ya) * f, z + 0.5))


def _ladder(b: MeshBuilder) -> None:
    """Escalera exterior de estribor: del suelo (x = 5,3) al alerón (x = 3,4)."""
    y, foot, top = -3.75, Vector((5.3, 0.0, 0.0)), Vector((3.35, 0.0, _L2))
    for side in (-0.36, 0.36):
        offset = Vector((0.0, y + side, 0.0))
        tube(b, foot + offset, top + offset + Vector((0.0, 0.0, 0.05)), 0.05, 0.05, 4, _RUST, phase=math.pi / 4)
        tube(b, foot + offset + Vector((0.0, 0.0, 1.0)), top + offset + Vector((-0.3, 0.0, 1.0)), 0.03, 0.03, 4,
             _IRON)
        tube(b, foot + offset, foot + offset + Vector((0.0, 0.0, 1.0)), 0.03, 0.03, 4, _IRON)
    rungs = 9
    for k in range(1, rungs + 1):
        p = foot.lerp(top, k / (rungs + 1))
        box(b, (0.22, 0.72, 0.05), _IRON, matrix_at(p.x, y, p.z))


def _funnel_and_mast(b: MeshBuilder) -> None:
    rake = Matrix.Rotation(math.radians(-10.0), 4, 'Y')
    lathe(b, [(0.85, 0.0), (0.8, 1.4), (0.78, 1.5), (0.78, 2.3), (0.76, 2.4), (0.74, 3.3)], 10,
          [_PAINT, C['oxido'], C['rojo_faro'], C['oxido'], _IRON], origin=Vector((-2.7, 0.0, _L2)), axis=rake,
          cap_top=C['tinta'])
    top = Vector((0.2, 0.0, _WH[5] + 0.22))
    tube(b, top, top + Vector((0.0, 0.0, 3.0)), 0.07, 0.05, 6, _IRON, cap1=_IRON)
    tube(b, top + Vector((0.0, -1.0, 2.3)), top + Vector((0.0, 1.0, 2.3)), 0.04, 0.04, 4, _IRON, cap0=_IRON,
         cap1=_IRON)
    blob(b, top + Vector((0.0, 0.0, 3.15)), (0.12, 0.12, 0.14), 6, 2, C['laton'])


def build_bridge() -> MeshBuilder:
    b = MeshBuilder()
    _deckhouse(b)
    _wheelhouse(b)
    _funnel_and_mast(b)
    _ladder(b)
    # barandilla perimetral del alerón; hueco en la esquina de la escalera
    _railing(b, [(2.9, -4.12), (-3.2, -4.12), (-3.2, 4.12), (3.3, 4.12), (3.3, -3.3)], _L2)
    return b
