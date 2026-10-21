"""Objetos de mano del kit K3 (tope 300 tris cada uno).

Cada `build_*` devuelve un MeshBuilder en metros, con la pieza apoyada en el
plano XY y centrada en el origen (el pivote lo baja `build_object` a la
base). Las piezas que se ven de frente miran a -Y, el frente de la lámina.
"""

from __future__ import annotations

import math

from mathutils import Matrix, Vector

from explored_style import pilots
from explored_style.kits.k3_base import AX_NEG_Y, AX_X, C, ccw, move_verts, polar
from explored_style.meshkit import MeshBuilder, lathe, slab, sweep_path


def build_cuchillo() -> MeshBuilder:
    """El cuchillo del piloto F2 con el bisel en cristal (en papel quemaba)."""
    return pilots.build_knife(bevel=C['cristal'])


# ---------------------------------------------------------------------------
# Cantimplora: cuerpo lenticular de pie con funda verde, aro central de
# hierro, gollete con tapón de latón y dos argollas para la correa (la correa
# suelta se probó y en la lámina se leía como un marco o un asa de bolso).
# ---------------------------------------------------------------------------
def build_cantimplora() -> MeshBuilder:
    b = MeshBuilder()
    r_body = 0.104
    cover, band = C['hierba_oscura'], C['hierro']
    profile = [(0.055, -0.042), (0.088, -0.031), (0.101, -0.012), (r_body, 0.0), (0.101, 0.012),
               (0.088, 0.031), (0.055, 0.042)]
    lathe(b, profile, 12, [cover, cover, band, band, cover, cover], origin=Vector((0.0, 0.0, r_body)),
          axis=AX_NEG_Y, phase=math.pi / 12, cap_bottom=cover, cap_top=cover, tips=(-0.05, 0.05))
    neck = [(0.016, 0.0), (0.016, 0.022), (0.023, 0.022), (0.023, 0.04), (0.018, 0.047)]
    lathe(b, neck, 6, [C['hierro'], C['hierro'], C['laton'], C['laton']],
          origin=Vector((0.0, 0.0, 2 * r_body - 0.012)), cap_bottom=C['hierro'], cap_top=C['laton'])
    lug = [(0.009, -0.0025), (0.015, 0.0), (0.009, 0.0025)]
    for side in (-1.0, 1.0):
        a = math.radians(90.0 - side * 48.0)
        at = Vector((r_body * math.cos(a), 0.0, r_body + r_body * math.sin(a)))
        lathe(b, lug, 5, [C['laton']] * 3, origin=at, axis=AX_NEG_Y, closed=True)
    return b


# ---------------------------------------------------------------------------
# Barómetro de bolsillo: caja de latón de pie, esfera de papel con escala,
# aguja roja, tija y anilla.
# ---------------------------------------------------------------------------
_BARO_R = 0.042
_BARO_SIDES = 12
_BARO_SCALE = 1.2  # de bolsillo, pero legible en la mano y en el suelo


def _baro_dial(b: MeshBuilder, last_ring: list[int], face_h: float) -> None:
    rot = AX_NEG_Y.to_3x3()
    u, v, w = rot @ Vector((1, 0, 0)), rot @ Vector((0, 1, 0)), rot @ Vector((0, 0, 1))
    origin = Vector((0.0, 0.0, _BARO_R))
    phase = math.pi / _BARO_SIDES
    inner = b.ring(origin + w * face_h, u, v, 0.026, _BARO_SIDES, phase)
    colors = []
    for k in range(_BARO_SIDES):
        mid = math.degrees(phase + 2 * math.pi * (k + 0.5) / _BARO_SIDES) % 360
        colors.append(C['tinta'] if k % 2 == 0 and not 220 <= mid <= 320 else C['papel'])
    b.bridge(last_ring, inner, colors)
    b.cap(inner, origin + w * face_h, C['papel'])
    front = origin + w * (face_h + 0.002)
    ang = math.radians(55.0)
    along = Vector((math.cos(ang), 0.0, math.sin(ang)))
    side = Vector((-math.sin(ang), 0.0, math.cos(ang))) * 0.0028
    needle = [front + along * 0.023, front + side, front - along * 0.006, front - side]
    slab(b, ccw(needle, w), w * 0.0016, C['rojo_faro'])


def build_barometro() -> MeshBuilder:
    b = MeshBuilder()
    brass = C['laton']
    profile = [(0.03, 0.0), (0.04, 0.004), (_BARO_R, 0.013), (0.04, 0.021), (0.035, 0.023), (0.034, 0.018)]
    rings = lathe(b, profile, _BARO_SIDES, [brass] * 5, origin=Vector((0.0, 0.0, _BARO_R)), axis=AX_NEG_Y,
                  phase=math.pi / _BARO_SIDES, cap_bottom=brass)
    _baro_dial(b, rings[-1], 0.018)
    mid_y = -0.0115
    top = _BARO_R + _BARO_R * math.cos(math.pi / _BARO_SIDES)
    lathe(b, [(0.0045, 0.0), (0.0045, 0.009)], 6, [brass], origin=Vector((0.0, mid_y, top - 0.004)),
          cap_bottom=brass, cap_top=brass)
    loop = [(0.0075, -0.0022), (0.0115, -0.0022), (0.0115, 0.0022), (0.0075, 0.0022)]
    lathe(b, loop, 8, [brass] * 4, origin=Vector((0.0, mid_y, top + 0.0145)), axis=AX_NEG_Y, closed=True)
    scaled = MeshBuilder()
    scaled.extend(b, Matrix.Scale(_BARO_SCALE, 4))
    return scaled


# ---------------------------------------------------------------------------
# Coco tumbado con fibras en franjas; coco vacío: media cáscara-cuenco con
# borde partido y carne blanca.
# ---------------------------------------------------------------------------
_COCO_SIDES = 10


def _ridges(dark: int, light: int, sides: int = _COCO_SIDES) -> list[int]:
    return [dark if k % 2 else light for k in range(sides)]


def build_coco() -> MeshBuilder:
    b = MeshBuilder()
    profile = [(0.03, -0.1), (0.063, -0.08), (0.082, -0.035), (0.083, 0.015), (0.068, 0.06), (0.036, 0.093)]
    husk = _ridges(C['madera_oscura'], C['madera_media'])
    lathe(b, profile, _COCO_SIDES, [husk] * 5, origin=Vector((0.0, 0.0, 0.083)), axis=AX_X,
          phase=math.pi / _COCO_SIDES, cap_bottom=C['madera_oscura'], cap_top=C['madera_media'],
          tips=(-0.106, 0.114))
    x_axis = Vector((-1.0, 0.0, 0.0))
    for k in range(3):
        a = math.radians(90 + 120 * k)
        eye = Vector((-0.107, 0.016 * math.cos(a), 0.083 + 0.016 * math.sin(a)))
        ring = [eye + Vector((0.0, 0.0065 * math.cos(t), 0.0065 * math.sin(t))) for t in (0.0, 2.1, 4.2)]
        slab(b, ccw(ring, x_axis), x_axis * 0.004, C['tinta'])
    return b


def build_coco_vacio() -> MeshBuilder:
    b = MeshBuilder()
    profile = [(0.03, 0.0), (0.066, 0.018), (0.084, 0.05), (0.085, 0.072), (0.073, 0.075), (0.069, 0.05),
               (0.05, 0.027)]
    husk = _ridges(C['madera_oscura'], C['madera_media'])
    meat = C['papel']
    rings = lathe(b, profile, _COCO_SIDES, [husk, husk, husk, meat, meat, meat], cap_bottom=C['madera_oscura'],
                  cap_top=meat, tips=(0.0, 0.022))
    for ring in (rings[3], rings[4]):
        move_verts(b, ring, lambda k, _co: (0.0, 0.0, 0.008 if k % 2 else -0.003))
    return b


# ---------------------------------------------------------------------------
# Leña: haz de 5 rajas (redondas, medias y cuartos) atado con cuerda de fibra.
# ---------------------------------------------------------------------------
_LOG_R = 0.045


def _section(kind: str) -> tuple[list[tuple[float, float]], list[bool]]:
    """Sección (y, z) en sentido antihorario y si cada cara es corteza."""
    if kind == 'redonda':
        angles = [2 * math.pi * k / 6 + math.pi / 6 for k in range(6)]
        return [(_LOG_R * math.cos(a), _LOG_R * math.sin(a)) for a in angles], [True] * 6
    if kind == 'media':
        angles = [math.pi * k / 3 for k in range(4)]
        return [(_LOG_R * math.cos(a), _LOG_R * math.sin(a)) for a in angles], [True, True, True, False]
    r = _LOG_R * 1.2
    arc = [(r * math.cos(a), r * math.sin(a)) for a in (math.pi * k / 4 for k in range(3))]
    return [(0.0, 0.0), *arc], [False, True, True, False]


def _split_log(b: MeshBuilder, kind: str, length: float, at: tuple[float, float, float], roll: float,
               yaw: float) -> None:
    pts, bark = _section(kind)
    cr, sr = math.cos(roll), math.sin(roll)
    cy, sy = math.cos(yaw), math.sin(yaw)

    def place(x: float, y: float, z: float, scale: float) -> Vector:
        yy, zz = (y * cr - z * sr) * scale, (y * sr + z * cr) * scale
        return Vector((at[0] + x * cy - yy * sy, at[1] + x * sy + yy * cy, at[2] + zz))

    stations = [(-length / 2, 1.0), (0.0, 1.06), (length / 2, 0.97)]
    rings = [[b.vert(place(x, y, z, s)) for y, z in pts] for x, s in stations]
    side = [C['madera_oscura'] if is_bark else C['madera_clara'] for is_bark in bark]
    b.bridge(rings[0], rings[1], side)
    b.bridge(rings[1], rings[2], side)
    for k in range(1, len(pts) - 1):
        b.face((rings[0][0], rings[0][k + 1], rings[0][k]), C['fibra'])
        b.face((rings[2][0], rings[2][k], rings[2][k + 1]), C['fibra'])


def build_lena() -> MeshBuilder:
    b = MeshBuilder()
    r = _LOG_R
    _split_log(b, 'redonda', 0.4, (0.0, -0.085, r), 0.2, 0.04)
    _split_log(b, 'media', 0.38, (0.02, 0.0, 0.004), 0.0, -0.03)
    _split_log(b, 'redonda', 0.42, (-0.015, 0.085, r), 0.5, -0.05)
    _split_log(b, 'cuarto', 0.36, (0.01, -0.045, 0.072), math.radians(45), 0.06)
    _split_log(b, 'media', 0.4, (-0.02, 0.045, 0.072), math.radians(15), -0.02)
    loop = [(-0.137, 0.04), (-0.112, 0.1), (-0.05, 0.13), (0.05, 0.13), (0.112, 0.1), (0.137, 0.04),
            (0.07, 0.004), (-0.07, 0.004)]
    sweep_path(b, [(0.04, y, z + 0.004) for y, z in loop], [0.0065] * len(loop), 4, C['fibra'],
          up=(1.0, 0.0, 0.0), phase=math.pi / 4, closed=True)
    return b


# ---------------------------------------------------------------------------
# Fibra de palma: haz en reloj de arena, atado en la cintura, con puntas
# deshilachadas y hebras sueltas.
# ---------------------------------------------------------------------------
def build_fibra() -> MeshBuilder:
    b = MeshBuilder()
    sides = 8
    strands = _ridges(C['fibra'], C['arena_oscura'], sides)
    axis_z = Vector((0.0, 0.0, 0.042))
    profile = [(0.042, -0.16), (0.033, -0.09), (0.02, -0.02), (0.02, 0.02), (0.033, 0.09), (0.042, 0.16)]
    rings = lathe(b, profile, sides, [strands] * 5, origin=axis_z, axis=AX_X,
                  cap_bottom=C['madera_clara'], cap_top=C['madera_clara'], tips=(-0.14, 0.14))
    for ring, sign in ((rings[0], -1.0), (rings[-1], 1.0)):
        move_verts(b, ring, lambda k, _co, s=sign: (s * (0.022 if k % 2 else -0.004), 0.0, 0.0))
    tie = [(0.019, -0.016), (0.025, -0.016), (0.025, 0.016), (0.019, 0.016)]
    lathe(b, tie, sides, [C['madera_oscura']] * 4, origin=axis_z, axis=AX_X, closed=True)
    loose = (((-0.12, 0.02, 0.06), (-0.2, 0.05, 0.075), (-0.25, 0.09, 0.05)),
             ((-0.13, -0.025, 0.03), (-0.21, -0.06, 0.02), (-0.26, -0.07, 0.006)),
             ((0.12, 0.025, 0.065), (0.19, 0.04, 0.09), (0.24, 0.06, 0.08)),
             ((0.13, -0.02, 0.025), (0.2, -0.055, 0.012), (0.25, -0.05, 0.004)))
    for path in loose:
        sweep_path(b, path, [0.0035, 0.003, 0.002], 3, C['fibra'], cap_start=C['fibra'], cap_end=C['fibra'])
    return b


# ---------------------------------------------------------------------------
# Cuerda: rollo de dos vueltas con trenzado en espiral y un cabo suelto.
# ---------------------------------------------------------------------------
_ROPE_R = 0.016


def build_cuerda() -> MeshBuilder:
    b = MeshBuilder()
    turns, per_turn = 2, 12
    n = turns * per_turn
    pts = []
    for i in range(n + 1):
        f = i / n
        pts.append(polar(0.11 - 0.014 * f, 2 * math.pi * turns * f, _ROPE_R + 1.75 * _ROPE_R * turns * f))
    end = 2 * math.pi * turns
    top_z = pts[-1].z
    pts += [polar(0.135, end + 0.3, top_z * 0.55), polar(0.18, end + 0.55, _ROPE_R),
            polar(0.25, end + 0.72, _ROPE_R)]
    sides = 5
    colors = [[C['madera_clara'] if (k - s) % sides in (0, 1) else C['fibra'] for k in range(sides)]
              for s in range(len(pts) - 1)]
    tip = pts[-1] + (pts[-1] - pts[-2]).normalized() * 0.025
    sweep_path(b, pts, [_ROPE_R] * len(pts), sides, colors, cap_start=C['fibra'], cap_end=C['fibra'],
          tips=(None, tip))
    return b


# ---------------------------------------------------------------------------
# Mecha: tres cabos de fibra retorcidos, atada en un extremo y deshilachada
# en el otro.
# ---------------------------------------------------------------------------
def build_mecha() -> MeshBuilder:
    b = MeshBuilder()
    length, segs, twists, axis_z = 0.3, 12, 3.5, 0.0135
    for k, color in enumerate((C['fibra'], C['madera_clara'], C['fibra'])):
        pts, radii = [], []
        for i in range(segs + 1):
            t = i / segs
            phi = 2 * math.pi * (twists * t + k / 3)
            spread = 0.0065 * (1.0 + 1.6 * max(0.0, t - 0.85) / 0.15)
            center = Vector((-length / 2 + length * t, 0.02 * math.sin(2 * math.pi * t), axis_z))
            pts.append(center + Vector((0.0, spread * math.sin(phi), spread * math.cos(phi))))
            radii.append(0.0062 if t < 0.9 else 0.0045)
        sweep_path(b, pts, radii, 3, color, cap_start=color, cap_end=color)
    tie = C['madera_oscura']
    lathe(b, [(0.016, 0.0), (0.016, 0.02)], 6, [tie], origin=Vector((-length / 2 + 0.005, 0.0, axis_z)),
          axis=AX_X, cap_bottom=tie, cap_top=tie)
    return b


# ---------------------------------------------------------------------------
# Tarro de aceite: frasco de cristal con el aceite dorado hasta la mitad,
# paño de papel por tapa y cordel.
# ---------------------------------------------------------------------------
def build_tarro_aceite() -> MeshBuilder:
    b = MeshBuilder()
    sides = 10
    oil, glass, cloth = C['laton'], C['cristal'], C['papel']
    profile = [(0.046, 0.0), (0.057, 0.007), (0.061, 0.05), (0.06, 0.078), (0.056, 0.105), (0.043, 0.122),
               (0.037, 0.127), (0.047, 0.122), (0.049, 0.142), (0.043, 0.15)]
    rings = lathe(b, profile, sides, [oil, oil, oil, glass, glass, glass, cloth, cloth, cloth],
                  cap_bottom=oil, cap_top=cloth, tips=(0.0, 0.156))
    move_verts(b, rings[7], lambda k, _co: (0.0, 0.0, -0.006 if k % 2 else 0.0))
    cord = [(0.047, 0.129), (0.055, 0.129), (0.055, 0.136), (0.047, 0.136)]
    lathe(b, cord, sides, [C['madera_oscura']] * 4, closed=True)
    return b
