"""K2 · Piezas de supervivencia (presupuesto «estructura», 800 tris).

- SM_CajaSuministros: caja de 0,9 × 0,6 × 0,6 m de tablas alternas con
  esquineros, tapa, dos flejes de hierro, cierre de latón, asas de cuerda y
  placa de papel con franja roja. La caja de mapas del carguero es la misma
  malla con otra instancia de material (GDD §11.1).
- SM_RefugioPalma: cobertizo a un agua de 3,7 × 3 m sobre 4 postes atados;
  techo de paja de palma grueso con flecos de puntas de hoja (dos verdes
  frescas), pared trasera trenzada y medias paredes laterales; abierto por
  delante (-Y), el lado alto.
- SM_BarrilLluvia: el piloto F2 con duelas alternas clara/oscura y un grifo
  grande de latón (caño, llave en T y boquilla) hacia -Y.
- SM_Hoguera: 9 piedras en corro, lecho de ceniza, tipi de 5 leños y 2 leños
  sueltos. El fuego lo pone el motor (sistema de partículas y luz).
- SM_Balsa: 4 troncos de 3 m atados con 2 cuerdas.
"""

from __future__ import annotations

import math
from collections.abc import Sequence

import bpy  # noqa: F401  (debe importarse antes que mathutils)

from mathutils import Matrix, Vector

from explored_style import pilots
from explored_style.meshkit import (
    MeshBuilder,
    blob,
    box,
    chamfer_box,
    cross,
    face_out,
    lathe,
    matrix_at,
    noise,
    sweep,
    tube,
)
from explored_style.pilots import C

Z_AXIS = Vector((0.0, 0.0, 1.0))


# ---------------------------------------------------------------------------
# SM_CajaSuministros
# ---------------------------------------------------------------------------
_CRATE = (0.9, 0.6, 0.56)


def _crate_boards(b: MeshBuilder) -> None:
    sx, sy, sz = _CRATE
    tones = (C['madera_clara'], C['madera_media'])
    bands = ((0.03, 0.19), (0.21, 0.37), (0.39, 0.53))
    for j, (z0, z1) in enumerate(bands):
        zc, h = (z0 + z1) / 2, z1 - z0
        for side in (-1, 1):
            box(b, (sx - 0.12, 0.03, h), tones[(j + (side > 0)) % 2], matrix_at(0.0, side * (sy / 2), zc))
            box(b, (0.03, sy - 0.12, h), tones[(j + 1 + (side > 0)) % 2], matrix_at(side * (sx / 2), 0.0, zc))


def _crate_straps(b: MeshBuilder) -> None:
    sx, sy, sz = _CRATE
    iron = C['hierro']
    for x in (-0.27, 0.27):
        box(b, (0.06, sy + 0.08, 0.02), iron, matrix_at(x, 0.0, sz + 0.07))
        box(b, (0.06, sy + 0.08, 0.02), iron, matrix_at(x, 0.0, 0.01))
        for side in (-1, 1):
            box(b, (0.06, 0.02, sz + 0.06), iron, matrix_at(x, side * (sy / 2 + 0.03), sz / 2 + 0.03))


def build_crate() -> MeshBuilder:
    b = MeshBuilder()
    sx, sy, sz = _CRATE
    box(b, (sx - 0.04, sy - 0.04, sz - 0.02), C['madera_oscura'], matrix_at(0.0, 0.0, sz / 2))
    _crate_boards(b)
    for px in (-1, 1):
        for py in (-1, 1):
            chamfer_box(b, (0.08, 0.08, sz), 0.015, C['madera_oscura'],
                        matrix_at(px * (sx / 2 - 0.02), py * (sy / 2 - 0.02), sz / 2))
    chamfer_box(b, (sx + 0.04, sy + 0.04, 0.07), 0.02, C['madera_clara'], matrix_at(0.0, 0.0, sz + 0.03),
                edge_color=C['madera_media'])
    for y in (-0.16, 0.16):
        box(b, (sx - 0.1, 0.07, 0.03), C['madera_media'], matrix_at(0.0, y, sz + 0.08))
    _crate_straps(b)
    box(b, (0.1, 0.03, 0.14), C['laton'], matrix_at(0.0, -sy / 2 - 0.035, sz - 0.04))
    box(b, (0.05, 0.03, 0.05), C['hierro'], matrix_at(0.0, -sy / 2 - 0.055, sz - 0.07))
    box(b, (0.3, 0.012, 0.16), C['papel'], matrix_at(0.0, -sy / 2 - 0.02, 0.28))
    box(b, (0.3, 0.016, 0.045), C['rojo_faro'], matrix_at(0.0, -sy / 2 - 0.022, 0.28))
    for side in (-1, 1):
        x = side * (sx / 2 + 0.02)
        sweep(b, [(x, -0.13, 0.42), (x + side * 0.05, -0.1, 0.36), (x + side * 0.06, 0.0, 0.34),
                  (x + side * 0.05, 0.1, 0.36), (x, 0.13, 0.42)], 0.018, 5, C['fibra'], cap=C['fibra'])
    return b


# ---------------------------------------------------------------------------
# SM_RefugioPalma
# ---------------------------------------------------------------------------
_SHELTER_X, _SHELTER_Y = 1.55, 1.15
_FRONT_H, _BACK_H = 2.15, 1.25
_ROOF_T = 0.2
_ROOF_X = 1.85
_SLOPE = math.atan2(_FRONT_H - _BACK_H, 2 * _SHELTER_Y)


def _tongue(b: MeshBuilder, root: Vector, along: Vector, across: Vector, length: float, width: float,
            color: int, thick: float = 0.04) -> None:
    """Punta de hoja de palma: prisma triangular de 8 tris (base ancha, punta)."""
    normal = cross(along, across).normalized() * (thick / 2)
    pts = [root - across * (width / 2), root + across * (width / 2), root + along * length]
    top = [b.vert(p + normal) for p in pts]
    bot = [b.vert(p - normal) for p in pts]
    inside = (pts[0] + pts[1] + pts[2]) / 3
    face_out(b, top, color, inside - normal)
    face_out(b, bot, color, inside + normal)
    for k in range(3):
        face_out(b, [top[k], top[(k + 1) % 3], bot[(k + 1) % 3], bot[k]], color, inside)


def _shelter_frame(b: MeshBuilder) -> None:
    post, beam, lash = C['madera_media'], C['madera_oscura'], C['fibra']
    for sx in (-1, 1):
        x = sx * _SHELTER_X
        for y, h in ((-_SHELTER_Y, _FRONT_H), (_SHELTER_Y, _BACK_H)):
            tube(b, (x, y, 0.0), (x * 0.985, y, h + 0.08), 0.07, 0.06, 6, post, cap1=C['madera_clara'])
            tube(b, (x * 0.985, y, h - 0.1), (x * 0.985, y, h + 0.1), 0.085, 0.085, 5, lash)
        tube(b, (x * 0.985, -_SHELTER_Y - 0.2, _FRONT_H + 0.08), (x * 0.985, _SHELTER_Y + 0.15, _BACK_H - 0.0),
             0.05, 0.05, 5, beam)
    for y, z in ((-_SHELTER_Y, _FRONT_H), (_SHELTER_Y, _BACK_H)):
        tube(b, (-_SHELTER_X - 0.25, y, z), (_SHELTER_X + 0.25, y, z), 0.055, 0.055, 5, beam,
             cap0=C['madera_clara'], cap1=C['madera_clara'])


def _roof_frame() -> tuple[Vector, Vector, Vector, Vector, float]:
    """(alero delantero, dirección hacia atrás y abajo, normal, centro, largo) del techo."""
    tan = math.tan(_SLOPE)
    front = Vector((0.0, -_SHELTER_Y - 0.4, _FRONT_H + 0.4 * tan + 0.06))
    back = Vector((0.0, _SHELTER_Y + 0.3, _BACK_H - 0.3 * tan + 0.06))
    down = (back - front).normalized()
    normal = Vector((0.0, math.sin(_SLOPE), math.cos(_SLOPE)))
    return front, down, normal, (front + back) / 2 + normal * (_ROOF_T / 2), (back - front).length


def _shelter_roof(b: MeshBuilder) -> None:
    thatch, dry, green = C['fibra'], C['arena_oscura'], C['verde_palma']
    front, down, normal, center, length = _roof_frame()
    across = Vector((1.0, 0.0, 0.0))
    chamfer_box(b, (2 * _ROOF_X, length, _ROOF_T), 0.06, thatch, matrix_at(*center, rx=-math.degrees(_SLOPE)),
                edge_color=dry)
    hang = Vector((0.0, -0.3, -1.0)).normalized()
    fringe = 11
    for k in range(fringe):
        x = -_ROOF_X + 0.17 + (2 * _ROOF_X - 0.34) * k / (fringe - 1)
        _tongue(b, front + across * x + normal * 0.04, hang, across, 0.42 + 0.08 * noise(11, k), 0.4,
                dry if k % 2 else thatch)
    for side in (-1, 1):
        out = Vector((side * 0.3, 0.0, -1.0)).normalized()
        for k in range(5):
            p = front + down * (0.35 + (length - 0.6) * k / 4) + across * (side * _ROOF_X) + normal * 0.04
            _tongue(b, p, out, down, 0.36 + 0.06 * noise(12, k, side), 0.42, thatch if k % 2 else dry)
    for row, f in enumerate((0.12, 0.45)):
        for k in range(8):
            x = -_ROOF_X + 0.3 + (2 * _ROOF_X - 0.6) * (k + 0.5 * row) / 8
            root = front + down * (length * f) + across * x + normal * (_ROOF_T + 0.025)
            color = green if (row, k) in ((0, 2), (1, 5)) else (dry if (k + row) % 2 else thatch)
            _tongue(b, root, (down + across * 0.15 * noise(13, row, k)).normalized(), across, 0.75, 0.42, color)


def _shelter_walls(b: MeshBuilder) -> None:
    thatch, dry = C['fibra'], C['arena_oscura']
    chamfer_box(b, (2 * _SHELTER_X + 0.1, 0.12, 1.15), 0.04, thatch, matrix_at(0.0, _SHELTER_Y + 0.08, 0.6),
                edge_color=dry)
    for k in range(6):
        x = -_SHELTER_X + 0.3 + (2 * _SHELTER_X - 0.6) * k / 5
        box(b, (0.12, 0.04, 1.1), dry, matrix_at(x, _SHELTER_Y + 0.0, 0.6))
    for sx in (-1, 1):
        chamfer_box(b, (0.1, 1.3, 0.95), 0.03, thatch, matrix_at(sx * (_SHELTER_X + 0.04), _SHELTER_Y - 0.6, 0.5),
                    edge_color=dry)


def build_shelter() -> MeshBuilder:
    b = MeshBuilder()
    _shelter_frame(b)
    _shelter_roof(b)
    _shelter_walls(b)
    box(b, (2.3, 1.5, 0.03), C['fibra'], matrix_at(0.0, 0.1, 0.015))
    return b


# ---------------------------------------------------------------------------
# SM_BarrilLluvia (piloto F2 revisado)
# ---------------------------------------------------------------------------
_STAVE_TONES = (C['madera_clara'], C['madera_oscura'])


def _stave(b: MeshBuilder, k: int) -> None:
    a0 = 2 * math.pi * k / pilots._STAVES + pilots._STAVE_GAP
    a1 = 2 * math.pi * (k + 1) / pilots._STAVES - pilots._STAVE_GAP
    color = _STAVE_TONES[k % 2]
    rows = []
    for z in (*pilots._STAVE_Z, pilots._STAVE_TOP + pilots._STAVE_TOP_JITTER[k]):
        r = pilots._barrel_radius(z)
        rows.append([b.vert(pilots._polar(r, a0, z)), b.vert(pilots._polar(r, a1, z)),
                     b.vert(pilots._polar(r - pilots._STAVE_T, a1, z)), b.vert(pilots._polar(r - pilots._STAVE_T, a0, z))])
    for r0, r1 in zip(rows, rows[1:], strict=False):
        b.face((r0[0], r0[1], r1[1], r1[0]), color)
        b.face((r0[2], r0[3], r1[3], r1[2]), color)
        b.face((r0[3], r0[0], r1[0], r1[3]), color)
        b.face((r0[1], r0[2], r1[2], r1[1]), color)
    o0, o1, i1, i0 = rows[-1]
    b.face((o0, o1, i1, i0), C['madera_clara'])
    o0, o1, i1, i0 = rows[0]
    b.face((o0, i0, i1, o1), color)


def build_barrel() -> MeshBuilder:
    b = MeshBuilder()
    for k in range(pilots._STAVES):
        _stave(b, k)
    pilots._barrel_hoop(b, 0.11, 0.18)
    pilots._barrel_hoop(b, 0.78, 0.85)
    r_water = pilots._barrel_radius(0.84) - pilots._STAVE_T - 0.004
    lathe(b, [(r_water, 0.82), (r_water, 0.845)], pilots._STAVES, [C['agua_media']], phase=math.pi / pilots._STAVES,
          cap_bottom=C['agua_media'], cap_top=C['agua_media'])
    spout_z = 0.3
    r_body = pilots._polyline_radius(spout_z)
    y0 = -(r_body - 0.03)
    brass = C['laton']
    tube(b, (0.0, y0, spout_z), (0.0, y0 - 0.11, spout_z), 0.055, 0.05, 7, C['madera_oscura'],
         cap1=C['madera_oscura'])
    tube(b, (0.0, y0 - 0.1, spout_z), (0.0, y0 - 0.2, spout_z), 0.04, 0.036, 7, brass, cap1=brass)
    tube(b, (0.0, y0 - 0.17, spout_z - 0.02), (0.0, y0 - 0.17, spout_z + 0.12), 0.026, 0.026, 6, brass,
         cap1=brass)
    chamfer_box(b, (0.16, 0.04, 0.035), 0.01, brass, matrix_at(0.0, y0 - 0.17, spout_z + 0.13))
    tube(b, (0.0, y0 - 0.2, spout_z - 0.005), (0.0, y0 - 0.22, spout_z - 0.09), 0.022, 0.018, 6, brass,
         cap1=C['hierro'])
    return b


# ---------------------------------------------------------------------------
# SM_Hoguera
# ---------------------------------------------------------------------------
def _log(b: MeshBuilder, p0: Sequence[float], p1: Sequence[float], radius: float, sides: int, bark: int,
         seed: int) -> None:
    """Leño con corteza y testas claras; perfil algo irregular."""
    a, z = Vector(p0), Vector(p1)
    length = (z - a).length
    axis = (z - a).normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    profile = [(radius * (0.93 + 0.05 * noise(seed, j)), length * f) for j, f in enumerate((0.0, 0.3, 0.7, 1.0))]
    lathe(b, profile, sides, [bark] * 3, origin=a, axis=axis, phase=0.4 * seed,
          cap_bottom=C['madera_clara'], cap_top=C['madera_clara'])


def build_campfire() -> MeshBuilder:
    b = MeshBuilder()
    tones = (C['roca_media'], C['roca_clara'], C['roca_media'], C['roca_oscura'])
    stones = 9
    for k in range(stones):
        a = 2 * math.pi * k / stones + 0.12 * noise(1, k)
        r = 0.56 + 0.04 * noise(2, k)
        size = 0.15 + 0.03 * noise(4, k)
        blob(b, (r * math.cos(a), r * math.sin(a), size * 0.45), (size * 1.25, size, size * 0.85), 6, 3,
             tones[k % 4], jitter=0.14, seed=k + 10, flat_bottom=-0.55)
    lathe(b, [(0.44, 0.0), (0.4, 0.035), (0.18, 0.06)], 10, [C['roca_oscura'], C['tinta']], cap_top=C['tinta'],
          cap_bottom=C['roca_oscura'], tips=(0.0, 0.065))
    for k in range(5):
        a = 2 * math.pi * k / 5 + 0.3
        foot = (0.36 * math.cos(a), 0.36 * math.sin(a), 0.04)
        top = (0.05 * math.cos(a + 0.6), 0.05 * math.sin(a + 0.6), 0.52 + 0.04 * noise(5, k))
        _log(b, foot, top, 0.045, 6, C['madera_oscura'] if k % 2 else C['madera_media'], k)
    _log(b, (-0.6, -0.78, 0.055), (0.15, -0.95, 0.055), 0.055, 6, C['madera_media'], 7)
    _log(b, (-0.45, -0.66, 0.05), (0.25, -0.62, 0.17), 0.05, 6, C['madera_oscura'], 8)
    return b


# ---------------------------------------------------------------------------
# SM_Balsa
# ---------------------------------------------------------------------------
_RAFT_LEN = 3.0
_RAFT_LOGS = (-0.48, -0.16, 0.16, 0.48)
_RAFT_R = 0.165


def _raft_log(b: MeshBuilder, y: float, k: int) -> None:
    sides = 8
    length = _RAFT_LEN - 0.12 * (k % 2) + 0.08 * noise(9, k)
    axis = Matrix.Rotation(math.pi / 2, 4, 'Y')
    origin = Vector((-length / 2, y, _RAFT_R))
    radii = (0.9, 1.0, 0.97, 1.0, 0.88)
    profile = [(_RAFT_R * f * (1.0 + 0.03 * noise(k, j)), length * t)
               for j, (f, t) in enumerate(zip(radii, (0.0, 0.08, 0.5, 0.92, 1.0), strict=True))]
    bark = C['madera_media'] if k % 2 else C['madera_oscura']
    lathe(b, profile, sides, [C['madera_clara'], bark, bark, C['madera_clara']], origin=origin, axis=axis,
          phase=0.3 * k, cap_bottom=C['madera_clara'], cap_top=C['madera_clara'])


def _rope_loop(x: float) -> list[tuple[float, float, float]]:
    """Lazo que abraza los 4 troncos: por encima con ondas y por debajo recto."""
    r, rope = _RAFT_R, 0.03
    top = []
    for k, y in enumerate(_RAFT_LOGS):
        top.append((x, y - 0.09, 2 * r + rope * 0.4 - 0.03))
        top.append((x, y, 2 * r + rope))
        top.append((x, y + 0.09, 2 * r + rope * 0.4 - 0.03))
        if k < len(_RAFT_LOGS) - 1:
            top.append((x, y + 0.16, 1.55 * r))
    side_r = (x, _RAFT_LOGS[-1] + r + rope, r)
    bottom = [(x, y, rope * 0.6) for y in (0.45, 0.15, -0.15, -0.45)]
    side_l = (x, _RAFT_LOGS[0] - r - rope, r)
    return [*top, side_r, *bottom, side_l]


def build_raft() -> MeshBuilder:
    b = MeshBuilder()
    for k, y in enumerate(_RAFT_LOGS):
        _raft_log(b, y, k)
    for x in (-0.95, 0.95):
        path = list(reversed(_rope_loop(x))) if x > 0 else _rope_loop(x)
        sweep(b, path, 0.03, 5, C['fibra'], closed=True)
        sweep(b, [(x + 0.02, _RAFT_LOGS[-1] + _RAFT_R + 0.03, _RAFT_R + 0.02),
                  (x + 0.06, _RAFT_LOGS[-1] + _RAFT_R + 0.1, 0.12), (x + 0.03, _RAFT_LOGS[-1] + _RAFT_R + 0.14, 0.03)],
              0.025, 4, C['fibra'], cap=C['fibra'])
    return b
