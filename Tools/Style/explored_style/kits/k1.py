"""K1 Naturaleza (GDD v3 §11.1): 8 mallas low poly cartoon generadas por script.

- SM_PalmA_01: palmera alta (parte del piloto F2): raíz con contrafuertes en
  madera media, collar de fibra y hojas en arco que no tapan el tronco.
- SM_PalmB_01: variante baja y muy inclinada, 6 hojas y 2 cocos.
- SM_Bamboo_01: macizo de 7 cañas (5 con hojas en ramilletes, 2 cortadas).
- SM_Shrub_01: arbusto de bolas facetadas, claro arriba y oscuro abajo.
- SM_GrassClump_01: mata de hojas curvas con algunas secas.
- SM_RockBig_01 / SM_RockSmall_01: rocas talladas (esfera geodésica recortada
  por planos), color por orientación de la cara.
- SM_CliffRock_01: pieza de acantilado modular de 8 x 4 x 6 m por estratos con
  remate de hierba; los perfiles de x = -4 y x = +4 coinciden para repetirla.

Todo es determinista (semillas fijas): volver a ejecutar regenera los mismos
FBX. Uso: `uv run style-kit-k1 [--out Art/Export/K1_Naturaleza] [--no-lamina]`.
"""

from __future__ import annotations

import argparse
import json
import math
import random
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

import bpy  # noqa: F401  (debe importarse antes que mathutils)

from mathutils import Matrix, Vector

from explored_style import meshkit
from explored_style.meshkit import MeshBuilder, add_polys, clamp_to_planes, cross, geosphere, lathe
from explored_style.palette import REPO_ROOT, load_palette

EXPORT_DIR = REPO_ROOT / 'Art' / 'Export' / 'K1_Naturaleza'

_PAL = load_palette()
C = {c.name: c.index for c in _PAL.colors}

Z_AXIS = Vector((0.0, 0.0, 1.0))
Y_AXIS = Vector((0.0, 1.0, 0.0))


@dataclass(frozen=True)
class KitSpec:
    name: str
    budget: str
    build: Callable[[], MeshBuilder]


def _frame(n: Vector) -> tuple[Vector, Vector]:
    """(u, v) perpendiculares a «n» con u x v = n (anillos antihorarios)."""
    helper = Vector((1.0, 0.0, 0.0)) if abs(n.x) < 0.9 else Y_AXIS
    u = cross(helper, n).normalized()
    return u, cross(n, u)


# ---------------------------------------------------------------------------
# Palmeras
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Frond:
    azimuth_deg: float
    length: float
    lift: float      # pendiente inicial (sube al salir de la corona)
    droop: float     # caída hacia la punta
    width: float     # semiancho máximo de la hoja
    teeth: int = 5


@dataclass(frozen=True)
class PalmSpec:
    trunk: tuple[tuple[float, float, float], ...]   # Bézier cuadrática
    segments: int
    radius: tuple[float, float]                      # base, copa
    fronds: tuple[Frond, ...]
    coconuts: tuple[float, ...]                      # acimut de cada coco


_SIDES = 8

PALM_A = PalmSpec(
    trunk=((0.0, 0.0, 0.0), (0.7, 0.0, 2.4), (1.25, 0.0, 5.15)), segments=10, radius=(0.25, 0.15),
    fronds=(
        Frond(10.0, 2.85, 0.62, 0.84, 0.43), Frond(62.0, 2.6, 0.7, 0.86, 0.4), Frond(118.0, 2.9, 0.58, 0.8, 0.44),
        Frond(170.0, 2.7, 0.66, 0.9, 0.42), Frond(222.0, 2.75, 0.6, 0.82, 0.42), Frond(275.0, 2.8, 0.64, 0.86, 0.43),
        Frond(325.0, 1.9, 1.35, 0.95, 0.34),
    ),
    coconuts=(200.0, 320.0, 80.0),
)
PALM_B = PalmSpec(
    trunk=((0.0, 0.0, 0.0), (0.4, 0.0, 1.55), (2.25, 0.0, 3.35)), segments=8, radius=(0.23, 0.14),
    fronds=(
        Frond(0.0, 2.4, 0.66, 0.86, 0.38), Frond(64.0, 2.25, 0.72, 0.9, 0.36), Frond(128.0, 2.45, 0.6, 0.82, 0.39),
        Frond(190.0, 2.2, 0.7, 0.92, 0.36), Frond(250.0, 2.4, 0.62, 0.84, 0.38),
        Frond(305.0, 1.7, 1.3, 0.95, 0.31),
    ),
    coconuts=(150.0, 270.0),
)


def _bezier(p: PalmSpec, t: float) -> Vector:
    p0, p1, p2 = (Vector(c) for c in p.trunk)
    return p0 * (1 - t) ** 2 + p1 * (2 * (1 - t) * t) + p2 * (t * t)


def _trunk_frame(p: PalmSpec, t: float) -> tuple[Vector, Vector, Vector]:
    """(u, v, n) del anillo en t: horizontal en la raíz y perpendicular al
    tronco a partir del primer segmento."""
    p0, p1, p2 = (Vector(c) for c in p.trunk)
    tangent = ((p1 - p0) * (2 * (1 - t)) + (p2 - p1) * (2 * t)).normalized()
    n = Z_AXIS.lerp(tangent, min(1.0, t / 0.12)).normalized()
    u = cross(Y_AXIS, n).normalized()
    return u, cross(n, u), n


def _trunk_ring(b: MeshBuilder, p: PalmSpec, t: float, radius: float, star: float = 1.0) -> list[int]:
    """Anillo del tronco; «star» < 1 alterna radios (contrafuertes de la raíz)."""
    u, v, _ = _trunk_frame(p, t)
    c = _bezier(p, t)
    out = []
    for k in range(_SIDES):
        a = math.pi / _SIDES + 2.0 * math.pi * k / _SIDES
        r = radius * (1.0 if k % 2 == 0 else star)
        out.append(b.vert(c + u * (math.cos(a) * r) + v * (math.sin(a) * r)))
    return out


def _palm_trunk(b: MeshBuilder, p: PalmSpec) -> None:
    r_base, r_top = p.radius

    def radius(t: float) -> float:
        return r_base + (r_top - r_base) * t

    wood, ring_c = C['madera_media'], C['madera_oscura']
    root = _trunk_ring(b, p, 0.0, r_base * 1.5, star=0.72)
    b.cap(root, _bezier(p, 0.0), wood, flip=True)
    mid = _trunk_ring(b, p, 0.025, r_base * 1.18, star=0.86)
    b.bridge(root, mid, wood)
    t0, ledge = 0.06, 0.008
    prev = _trunk_ring(b, p, t0, radius(t0))
    b.bridge(mid, prev, wood)
    span = (1.0 - t0) / p.segments
    for k in range(p.segments):
        t_top = t0 + (k + 1) * span - ledge
        cup = _trunk_ring(b, p, t_top, radius(t_top))
        b.bridge(prev, cup, wood)
        t_next = t0 + (k + 1) * span
        nxt = _trunk_ring(b, p, t_next, radius(t_next) * 0.8)
        b.bridge(cup, nxt, ring_c)
        prev = nxt
    u, v, n = _trunk_frame(p, 1.0)
    top = _bezier(p, 1.0)
    collar = b.ring(top + n * 0.1, u, v, r_top * 1.75, _SIDES, phase=math.pi / _SIDES)
    b.bridge(prev, collar, C['fibra'])
    bulb = b.ring(top + n * 0.24, u, v, r_top * 1.15, _SIDES, phase=math.pi / _SIDES)
    b.bridge(collar, bulb, C['hierba_oscura'])
    b.cap(bulb, top + n * 0.38, C['verde_palma'])


def _palm_coconuts(b: MeshBuilder, p: PalmSpec) -> None:
    u, v, n = _trunk_frame(p, 1.0)
    center = _bezier(p, 1.0) - n * 0.02
    r = 0.14
    profile = [(r * 0.62, -r * 0.8), (r, -r * 0.1), (r * 0.7, r * 0.72)]
    for k, ang in enumerate(math.radians(a) for a in p.coconuts):
        radial = u * math.cos(ang) + v * math.sin(ang)
        pos = center + radial * 0.22 - n * (0.08 + 0.04 * k)
        axis = radial.to_track_quat('Z', 'Y').to_matrix().to_4x4()
        lathe(b, profile, 6, [C['madera_oscura']] * 2, origin=pos, axis=axis,
              cap_bottom=C['madera_oscura'], cap_top=C['madera_oscura'])


def frond_stations(f: Frond) -> list[tuple[float, float]]:
    """(s, semiancho) a lo largo del nervio: muesca estrecha y diente ancho
    alternos, con el diente desplazado hacia la punta (foliolos barridos)."""
    def envelope(s: float) -> float:
        return f.width * math.sin(math.pi * min(s, 0.999) ** 0.8) ** 0.7

    out = [(0.0, 0.035)]
    start, end = 0.1, 0.93
    step = (end - start) / f.teeth
    for j in range(f.teeth):
        s_notch = start + j * step
        s_tip = s_notch + step * 0.82
        out.append((s_notch, max(envelope(s_notch) * 0.42, 0.03)))
        out.append((s_tip, envelope(s_tip)))
    return out


def _frond(b: MeshBuilder, root_center: Vector, f: Frond) -> None:
    ang = math.radians(f.azimuth_deg)
    a = Vector((math.cos(ang), math.sin(ang), 0.0))
    side = cross(Z_AXIS, a)
    root = root_center + a * 0.08

    def spine(s: float) -> Vector:
        return root + a * (f.length * s * (1.0 - 0.15 * s)) + Z_AXIS * (f.length * (f.lift * s - f.droop * s * s))

    top, under = C['verde_palma'], C['verde_selva']
    rows = []
    for s, w in frond_stations(f):
        p = spine(s)
        fold = Z_AXIS * (0.36 * w)
        thick = Z_AXIS * (0.045 * (1.0 - s) + 0.012)
        lt, st, rt = p + side * w - fold, p, p - side * w - fold
        rows.append([b.vert(lt), b.vert(st), b.vert(rt),
                     b.vert(lt - thick), b.vert(st - thick * 1.6), b.vert(rt - thick)])
    lt, st, rt, lb, sb, rb = rows[0]
    b.face((lt, lb, sb, st), under)
    b.face((st, sb, rb, rt), under)
    for r0, r1 in zip(rows, rows[1:], strict=False):
        b.face((r0[0], r0[1], r1[1], r1[0]), top)
        b.face((r0[1], r0[2], r1[2], r1[1]), top)
        b.face((r0[3], r1[3], r1[4], r0[4]), under)
        b.face((r0[4], r1[4], r1[5], r0[5]), under)
        b.face((r0[0], r1[0], r1[3], r0[3]), under)
        b.face((r0[2], r0[5], r1[5], r1[2]), under)
    tip = b.vert(spine(1.0))
    lt, st, rt, lb, sb, rb = rows[-1]
    for tri, color in (((lt, st, tip), top), ((st, rt, tip), top), ((lb, tip, sb), under),
                       ((sb, tip, rb), under), ((lt, tip, lb), under), ((rt, rb, tip), under)):
        b.face(tri, color)


def build_palm(p: PalmSpec) -> MeshBuilder:
    b = MeshBuilder()
    _palm_trunk(b, p)
    _palm_coconuts(b, p)
    _, _, n = _trunk_frame(p, 1.0)
    crown = _bezier(p, 1.0) + n * 0.26
    for frond in p.fronds:
        _frond(b, crown, frond)
    return b


# ---------------------------------------------------------------------------
# SM_Bamboo
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Cane:
    base: tuple[float, float]
    height: float
    lean: tuple[float, float]    # desplazamiento de la punta
    radius: float
    internodes: int
    leafy: bool = True


CANES = (
    Cane((0.0, 0.02), 4.5, (0.2, 0.15), 0.085, 6),
    Cane((0.24, 0.14), 3.9, (0.85, 0.45), 0.078, 5),
    Cane((-0.22, 0.12), 4.1, (-0.8, 0.4), 0.078, 5),
    Cane((0.06, -0.24), 3.4, (0.3, -0.85), 0.072, 4),
    Cane((-0.2, -0.17), 3.0, (-0.75, -0.55), 0.068, 4),
    Cane((0.3, -0.1), 1.25, (0.06, -0.04), 0.08, 2, leafy=False),
    Cane((-0.04, 0.33), 0.8, (-0.03, 0.05), 0.075, 1, leafy=False),
)
_CANE_SIDES = 6
_NODE_SWELL = 1.16


def _cane_point(c: Cane, t: float) -> Vector:
    """Curva suave: sube recta y se abre hacia la punta."""
    lean = Vector((c.lean[0], c.lean[1], 0.0))
    return Vector((c.base[0], c.base[1], 0.0)) + lean * (t * t) + Z_AXIS * (c.height * t)


def _cane_axis(c: Cane, t: float) -> Vector:
    lean = Vector((c.lean[0], c.lean[1], 0.0))
    return (lean * (2 * t) + Z_AXIS * c.height).normalized()


def _cane_ring(b: MeshBuilder, c: Cane, t: float, r: float) -> list[int]:
    u, v = _frame(_cane_axis(c, t))
    return b.ring(_cane_point(c, t), u, v, r, _CANE_SIDES)


def _leaf(b: MeshBuilder, base: Vector, azimuth: float, length: float, width: float, droop: float) -> None:
    """Hoja lanceolada cerrada (8 tris): haz claro, envés oscuro."""
    d = Vector((math.cos(azimuth), math.sin(azimuth), -math.tan(droop))).normalized()
    s = cross(Z_AXIS, d).normalized()
    up = cross(d, s)
    h = 0.018
    bv = b.vert(base)
    tv = b.vert(base + d * length - up * (0.18 * length))
    lv = b.vert(base + d * (0.38 * length) + s * width)
    rv = b.vert(base + d * (0.38 * length) - s * width)
    uv = b.vert(base + d * (0.4 * length) + up * h)
    dv = b.vert(base + d * (0.4 * length) - up * h)
    top, under = C['hierba_oscura'], C['verde_selva']
    for tri in ((bv, rv, uv), (rv, tv, uv), (tv, lv, uv), (lv, bv, uv)):
        b.face(tri, top)
    for tri in ((rv, bv, dv), (tv, rv, dv), (lv, tv, dv), (bv, lv, dv)):
        b.face(tri, under)


def _cane(b: MeshBuilder, c: Cane, seed: int) -> None:
    body, node = C['verde_lima'], C['hierba_clara']
    rng = random.Random(seed)
    neck = 0.05 / c.height
    prev = _cane_ring(b, c, 0.0, c.radius * _NODE_SWELL)
    b.cap(prev, _cane_point(c, 0.0), node, flip=True)
    for k in range(c.internodes):
        t0, t1 = k / c.internodes, (k + 1) / c.internodes
        neck_ring = _cane_ring(b, c, t0 + neck, c.radius)
        b.bridge(prev, neck_ring, node)
        taper = 1.0 - 0.25 * t1 if c.leafy else 1.0
        prev = _cane_ring(b, c, t1, c.radius * _NODE_SWELL * taper)
        b.bridge(neck_ring, prev, body)
        if c.leafy and k >= c.internodes - 4:
            ax = _cane_axis(c, t1)
            count = (3, 4, 4, 5)[k - c.internodes + 4]
            phase = rng.uniform(0.0, 2.0 * math.pi)
            for j in range(count):
                az = phase + 2.0 * math.pi * j / count
                out = Vector((math.cos(az), math.sin(az), 0.0))
                base = _cane_point(c, t1) + out * (c.radius * 0.8) - ax * 0.02
                _leaf(b, base, az, rng.uniform(0.62, 0.82), rng.uniform(0.09, 0.115),
                      math.radians(rng.uniform(18.0, 38.0)))
    tip = _cane_point(c, 1.0) + _cane_axis(c, 1.0) * (0.06 if c.leafy else 0.0)
    b.cap(prev, tip, body if c.leafy else C['madera_clara'])


def build_bamboo() -> MeshBuilder:
    b = MeshBuilder()
    for k, cane in enumerate(CANES):
        _cane(b, cane, seed=17 + k)
    return b


# ---------------------------------------------------------------------------
# SM_Shrub y SM_GrassClump
# ---------------------------------------------------------------------------
def _by_up(light: int, mid: int, dark: int, hi: float = 0.55, lo: float = -0.25) -> Callable[[Vector, Vector], int]:
    def color(normal: Vector, _center: Vector) -> int:
        return light if normal.z > hi else dark if normal.z < lo else mid
    return color


def _blob(b: MeshBuilder, center: Vector, radii: tuple[float, float, float], frequency: int, seed: int,
          color_of: Callable[[Vector, Vector], int], floor: float = -0.55,
          cuts: int = 0, turn: float = 0.0, grounded: bool = True,
          depth: tuple[float, float] = (0.74, 0.9)) -> None:
    """Esfera geodésica deformada: jitter radial, «cuts» planos de talla y
    base plana (z local ≥ «floor»). Con «grounded», «center» es el punto de
    apoyo de la base; si no, el centro de la esfera. Las caras conservan el
    sentido."""
    rng = random.Random(seed)
    unit, faces = geosphere(frequency)
    planes = [(Vector((0.0, 0.0, -1.0)), -floor)]
    for _ in range(cuts):
        az, el = rng.uniform(0.0, 2.0 * math.pi), rng.uniform(-0.15, 1.05)
        n = Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
        planes.append((n, rng.uniform(depth[0], depth[1])))
    rot = Matrix.Rotation(turn, 3, 'Z')
    scale = Vector(radii)
    origin = center + Z_AXIS * (-floor * scale.z if grounded else 0.0)
    verts = []
    for p in unit:
        q = clamp_to_planes(p * (1.0 + rng.uniform(-0.09, 0.09)), planes)
        verts.append(origin + rot @ Vector((q.x * scale.x, q.y * scale.y, q.z * scale.z)))
    add_polys(b, verts, faces, color_of)


_SHRUB_BLOBS = (
    ((0.0, 0.0, 0.36), (0.55, 0.5, 0.48)),
    ((0.42, 0.18, 0.27), (0.4, 0.38, 0.36)),
    ((-0.38, 0.22, 0.25), (0.38, 0.36, 0.34)),
    ((0.08, -0.36, 0.22), (0.36, 0.34, 0.3)),
    ((-0.05, 0.12, 0.72), (0.32, 0.3, 0.28)),
)


def build_shrub() -> MeshBuilder:
    b = MeshBuilder()
    color = _by_up(C['hierba_clara'], C['hierba_oscura'], C['verde_selva'], hi=0.5, lo=-0.2)
    for k, (center, radii) in enumerate(_SHRUB_BLOBS):
        _blob(b, Vector(center), radii, 2, seed=31 + k, color_of=color, floor=-0.7, cuts=2, turn=0.9 * k,
              grounded=False)
    return b


GRASS_BLADES = 30
_GOLDEN = math.pi * (3.0 - math.sqrt(5.0))


def _blade(b: MeshBuilder, k: int, rng: random.Random) -> None:
    az = k * _GOLDEN + rng.uniform(-0.2, 0.2)
    radial = math.sqrt((k + 0.5) / GRASS_BLADES)
    out = Vector((math.cos(az), math.sin(az), 0.0))
    side = Vector((-math.sin(az), math.cos(az), 0.0))
    h = rng.uniform(0.42, 0.7) * (1.15 - 0.45 * radial)
    tilt0 = math.radians(8.0 + 32.0 * radial + rng.uniform(-5.0, 5.0))
    tilt1 = tilt0 + math.radians(rng.uniform(18.0, 32.0))
    base = out * (0.16 * radial)
    d0 = out * math.sin(tilt0) + Z_AXIS * math.cos(tilt0)
    d1 = out * math.sin(tilt1) + Z_AXIS * math.cos(tilt1)
    mid = base + d0 * (h * 0.5)
    tip = mid + d1 * (h * 0.5)
    dry = k % 9 == 4
    low, high = (C['arena_oscura'], C['fibra']) if dry else (C['hierba_oscura'], C['hierba_clara'])
    w = rng.uniform(0.032, 0.042)
    rings = []
    for center, axis, width in ((base, d0, w), (mid, (d0 + d1).normalized(), w * 0.75)):
        rings.append(b.ring(center, side, cross(axis, side), width, 3, phase=math.pi / 2, radius_v=0.012))
    b.cap(rings[0], base, low, flip=True)
    b.bridge(rings[0], rings[1], low)
    b.cap(rings[1], tip, high)


def build_grass() -> MeshBuilder:
    b = MeshBuilder()
    rng = random.Random(53)
    for k in range(GRASS_BLADES):
        _blade(b, k, rng)
    return b


# ---------------------------------------------------------------------------
# Rocas
# ---------------------------------------------------------------------------
def _rock_color(normal: Vector, _center: Vector) -> int:
    """Tapa clara, flancos medios y panza oscura (las bandas de M_Toon hacen el resto)."""
    if normal.z > 0.6:
        return C['roca_clara']
    return C['roca_oscura'] if normal.z < -0.3 else C['roca_media']


def build_rock_big() -> MeshBuilder:
    b = MeshBuilder()
    _blob(b, Vector((0.0, 0.0, 0.0)), (1.25, 0.95, 1.2), 4, seed=7, color_of=_rock_color,
          floor=-0.42, cuts=14, turn=0.3, depth=(0.64, 0.82))
    _blob(b, Vector((1.05, -0.45, 0.0)), (0.62, 0.55, 0.7), 3, seed=11, color_of=_rock_color,
          floor=-0.35, cuts=11, turn=1.1, depth=(0.64, 0.82))
    _blob(b, Vector((-0.95, -0.7, 0.0)), (0.28, 0.24, 0.22), 2, seed=13, color_of=_rock_color,
          floor=-0.3, cuts=6, turn=0.4, depth=(0.66, 0.84))
    return b


def build_rock_small() -> MeshBuilder:
    b = MeshBuilder()
    _blob(b, Vector((0.0, 0.0, 0.0)), (0.36, 0.3, 0.32), 3, seed=23, color_of=_rock_color,
          floor=-0.4, cuts=11, turn=0.6, depth=(0.64, 0.82))
    _blob(b, Vector((0.33, -0.2, 0.0)), (0.15, 0.13, 0.13), 2, seed=29, color_of=_rock_color,
          floor=-0.35, cuts=6, turn=1.9, depth=(0.66, 0.84))
    return b


# ---------------------------------------------------------------------------
# SM_CliffRock: estratos facetados, cornisas y remate de hierba
# ---------------------------------------------------------------------------
CLIFF_W, CLIFF_D, CLIFF_H = 8.0, 4.0, 6.0
CLIFF_COLUMNS = 16
ROWS_PER_STRATUM = 5
_CHAMFER = 0.16
_LEDGE_RISE = 0.05


@dataclass(frozen=True)
class Stratum:
    z0: float          # base media (la junta con el estrato de abajo ondula)
    recess: float      # retranqueo medio respecto al frente (+Y = hacia dentro)
    amp: float         # relieve del frente
    phase: float
    lean: float        # cuánto se inclina hacia dentro la pared
    joint_wave: float  # amplitud vertical de la junta inferior
    colors: tuple[int, int, int, int, int]  # chaflán inf., pared baja, pared alta, chaflán sup., cornisa


_DK, _MD, _LT = C['roca_oscura'], C['roca_media'], C['roca_clara']
STRATA = (
    Stratum(0.0, 0.0, 0.34, 0.4, 0.16, 0.0, (_DK, _DK, _DK, _MD, _LT)),
    Stratum(2.0, 0.55, 0.32, 2.1, 0.2, 0.3, (_MD, _MD, _MD, _LT, _LT)),
    Stratum(3.15, 1.0, 0.3, 4.0, 0.16, 0.22, (_MD, _MD, _MD, _LT, _LT)),
    Stratum(4.55, 1.4, 0.26, 5.3, 0.12, 0.2, (_MD, _MD, _LT, _LT, C['verde_selva'])),
    Stratum(5.55, 1.3, 0.1, 1.2, 0.0, 0.05,
            (C['hierba_oscura'], C['hierba_oscura'], C['hierba_oscura'], C['hierba_clara'], C['hierba_clara'])),
)


def _hash01(i: int, s: int) -> float:
    """Pseudoaleatorio determinista en [0, 1) por columna y estrato."""
    v = math.sin(12.9898 * i + 78.233 * s) * 43758.5453
    return v - math.floor(v)


def _phase_u(x: float) -> float:
    return 2.0 * math.pi * (x + CLIFF_W / 2) / CLIFF_W


def _joint_z(s: int, x: float) -> float:
    """Altura de la junta inferior del estrato s (periódica en 8 m)."""
    if s >= len(STRATA):
        return CLIFF_H
    st = STRATA[s]
    u = _phase_u(x)
    return st.z0 + st.joint_wave * (0.65 * math.sin(u + st.phase) + 0.35 * math.sin(3 * u + 2 * st.phase))


def _column_x(s: int) -> list[float]:
    """X de las columnas del estrato: irregulares, con los extremos fijos."""
    step = CLIFF_W / CLIFF_COLUMNS
    xs = [-CLIFF_W / 2 + i * step for i in range(CLIFF_COLUMNS + 1)]
    return [x if i in (0, CLIFF_COLUMNS) else x + 0.3 * step * (_hash01(i, s + 7) - 0.5)
            for i, x in enumerate(xs)]


def _profile(s: int, xs: list[float]) -> list[float]:
    """Y del frente: senos de periodo 8 m (x = ±4 coinciden) más facetas
    pseudoaleatorias por columna que se anulan en los extremos."""
    st = STRATA[s]
    out = []
    for i, x in enumerate(xs):
        u = _phase_u(x)
        wave = 0.6 * math.sin(u + st.phase) + 0.4 * math.sin(3 * u + 2 * st.phase)
        if i in (0, len(xs) - 1):
            facet = 0.0
        else:   # faceta de ±0,6·amp y, a veces, una grieta vertical
            crack = 0.32 if _hash01(i, s + 11) > 0.8 else 0.0
            facet = 1.2 * st.amp * (_hash01(i, s) - 0.5) + crack
        out.append(-CLIFF_D / 2 + 0.12 + st.recess + st.amp * wave + facet)
    return out


def _stratum_rows(s: int, ys: list[float], xs: list[float]) -> list[list[Vector]]:
    st = STRATA[s]
    rise = _LEDGE_RISE if s else 0.0
    rows: list[list[Vector]] = [[], [], [], [], []]
    for i, (x, y) in enumerate(zip(xs, ys, strict=True)):
        z0, z1 = _joint_z(s, x) + rise, _joint_z(s + 1, x)
        c = min(_CHAMFER, (z1 - z0) / 4)
        bulge = 0.06 if i in (0, len(xs) - 1) else 0.14 * _hash01(i, s + 3)
        rows[0].append(Vector((x, y + c, z0)))
        rows[1].append(Vector((x, y, z0 + c)))
        rows[2].append(Vector((x, y + st.lean * 0.5 - bulge, (z0 + z1) / 2)))
        rows[3].append(Vector((x, y + st.lean, z1 - c)))
        rows[4].append(Vector((x, y + st.lean + c, z1)))
    return rows


def cliff_rows() -> list[list[Vector]]:
    """Filas del frente de abajo arriba (5 por estrato: chaflán, pared baja,
    abombado, pared alta, chaflán). Cada estrato queda detrás del anterior
    salvo el remate de hierba, que vuela sobre la roca (cornisa invertida)."""
    rows: list[list[Vector]] = []
    prev_top: list[float] | None = None
    for s in range(len(STRATA)):
        xs = _column_x(s)
        ys = _profile(s, xs)
        if prev_top is not None:
            if s == len(STRATA) - 1:
                ys = [min(y, t - _CHAMFER - 0.18) for y, t in zip(ys, prev_top, strict=True)]
            else:
                ys = [max(y, t - _CHAMFER + 0.14) for y, t in zip(ys, prev_top, strict=True)]
        stratum = _stratum_rows(s, ys, xs)
        rows.extend(stratum)
        prev_top = [p.y for p in stratum[-1]]
    return rows


def _band_colors() -> list[int]:
    out = []
    for st in STRATA:
        out.extend(st.colors)
    return out[:-1]


def build_cliff() -> MeshBuilder:
    b = MeshBuilder()
    rows = cliff_rows()
    ids = [[b.vert(p) for p in row] for row in rows]
    back_y = CLIFF_D / 2
    left = [b.vert((-CLIFF_W / 2, back_y, row[0].z)) for row in rows]
    right = [b.vert((CLIFF_W / 2, back_y, row[-1].z)) for row in rows]
    top = [left[-1], *(b.vert((p.x, back_y, CLIFF_H)) for p in rows[-1][1:-1]), right[-1]]
    bottom = [left[0], *(b.vert((p.x, back_y, 0.0)) for p in rows[0][1:-1]), right[0]]
    n = CLIFF_COLUMNS
    for k, color in enumerate(_band_colors()):
        lo, hi = ids[k], ids[k + 1]
        for i in range(n):   # dos tris por celda: el frente no es plano
            if (i + k) % 2:
                b.face((lo[i], lo[i + 1], hi[i + 1]), color)
                b.face((lo[i], hi[i + 1], hi[i]), color)
            else:
                b.face((lo[i], lo[i + 1], hi[i]), color)
                b.face((lo[i + 1], hi[i + 1], hi[i]), color)
        b.face((lo[0], hi[0], left[k + 1], left[k]), _MD)
        b.face((lo[n], right[k], right[k + 1], hi[n]), _MD)
    for i in range(n):
        b.face((ids[-1][i], ids[-1][i + 1], top[i + 1], top[i]), C['hierba_clara'])
        b.face((ids[0][i], bottom[i], bottom[i + 1], ids[0][i + 1]), _DK)
    ring = [*left, *top[1:], *reversed(right[:-1]), *reversed(bottom[1:-1])]
    b.cap(ring, (0.0, back_y, CLIFF_H / 2), _DK)
    return b


# ---------------------------------------------------------------------------
# Kit
# ---------------------------------------------------------------------------
KIT: tuple[KitSpec, ...] = (
    KitSpec('SM_PalmA_01', 'palmera', lambda: build_palm(PALM_A)),
    KitSpec('SM_PalmB_01', 'palmera', lambda: build_palm(PALM_B)),
    KitSpec('SM_Bamboo_01', 'palmera', build_bamboo),
    KitSpec('SM_Shrub_01', 'vegetacion', build_shrub),
    KitSpec('SM_GrassClump_01', 'vegetacion', build_grass),
    KitSpec('SM_RockBig_01', 'palmera', build_rock_big),
    KitSpec('SM_RockSmall_01', 'vegetacion', build_rock_small),
    KitSpec('SM_CliffRock_01', 'palmera', build_cliff),
)


def generate(spec: KitSpec) -> bpy.types.Object:
    return meshkit.build_object(spec.name, spec.build())


def generate_all(out_dir: Path) -> list[dict]:
    """Genera y exporta el kit; devuelve las entradas del manifiesto."""
    out_dir.mkdir(parents=True, exist_ok=True)
    entries = []
    for spec in KIT:
        meshkit.reset_scene()
        obj = generate(spec)
        path = out_dir / f'{spec.name}.fbx'
        meshkit.export_fbx(obj, str(path))
        entries.append({
            'malla': spec.name, 'fbx': path.name, 'presupuesto': spec.budget,
            'tris': meshkit.triangle_count(obj),
            'dimensiones_m': [round(d, 3) for d in obj.dimensions],
            'colores': sorted({_PAL.colors[i].name for i in meshkit.face_colors(obj)}),
            'origen': 'generado por Tools/Style/explored_style/kits/k1.py (sin fuentes externas)',
            'licencia': 'propia del proyecto Explored',
        })
        print(f'[k1] {spec.name}: {entries[-1]["tris"]} tris -> {path}')
    manifest = {'kit': 'K1 Naturaleza', 'generador': 'uv run style-kit-k1', 'mallas': entries}
    (out_dir / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n',
                                           encoding='utf-8')
    return entries


def main(argv: list[str] | None = None) -> int:
    from explored_style.kits import k1_lamina

    parser = argparse.ArgumentParser(prog='style-kit-k1', description='Genera el kit K1 Naturaleza y su lámina.')
    parser.add_argument('--out', type=Path, default=EXPORT_DIR)
    parser.add_argument('--lamina', type=Path, default=k1_lamina.OUT_PATH)
    parser.add_argument('--no-lamina', action='store_true', help='solo FBX y manifiesto')
    parser.add_argument('--engine', default='BLENDER_EEVEE', help='BLENDER_EEVEE o BLENDER_WORKBENCH')
    parser.add_argument('--percent', type=int, default=100, help='porcentaje de resolución de la lámina')
    args = parser.parse_args(argv)
    generate_all(args.out)
    if not args.no_lamina:
        path = k1_lamina.make_sheet(args.lamina, args.out, engine=args.engine, percent=args.percent)
        print(f'[k1] lámina {path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
