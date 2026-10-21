"""Formas del kit K6 Césped: matas, hierba alta, flores, helecho y tréboles.

Todas las piezas son tarjetas de una cara (hojas planas sin grosor): el
material de Unreal tiene que ser de dos caras (ver NOTAS en k6_cesped.py).
Cada vértice lleva su peso de viento (0 en la base, 1 en la punta) en
`WindBuilder.weights`; k6_cesped lo escribe en el alfa del color por vértice.

Todo es determinista: las variaciones salen de semillas fijas.
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass, field

import bpy  # noqa: F401  (debe importarse antes que mathutils)

from explored_style.meshkit import Coord, MeshBuilder, cross
from explored_style.palette import load_palette
from mathutils import Vector

_PAL = load_palette()
C = {c.name: c.index for c in _PAL.colors}

Z_AXIS = Vector((0.0, 0.0, 1.0))
GOLDEN = math.pi * (3.0 - math.sqrt(5.0))
# Peso máximo de los tréboles: van pegados al suelo y un balanceo completo los
# haría «resbalar» sobre el terreno (desviación documentada del 1 en la punta).
CLOVER_TIP_WEIGHT = 0.35


@dataclass
class WindBuilder(MeshBuilder):
    """MeshBuilder con un peso de viento por vértice (0-1)."""

    weights: list[float] = field(default_factory=list)

    def vert(self, co: Coord, weight: float = 0.0) -> int:
        index = super().vert(co)
        self.weights.append(min(max(float(weight), 0.0), 1.0))
        return index


def _horizontal(azimuth: float) -> tuple[Vector, Vector]:
    """(hacia fuera, de lado) en el plano XY para un acimut en radianes."""
    out = Vector((math.cos(azimuth), math.sin(azimuth), 0.0))
    return out, Vector((-math.sin(azimuth), math.cos(azimuth), 0.0))


# ---------------------------------------------------------------------------
# Hojas de hierba
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Blade:
    azimuth: float               # radianes: hacia dónde se inclina
    length: float                # m a lo largo del nervio
    tilt: float                  # grados desde la vertical en la base
    bend: float                  # grados que se dobla más hasta la punta
    width: float                 # semiancho en la base (m)
    colors: tuple[int, ...]      # uno por tramo, de la base a la punta
    offset: float = 0.0          # m del centro de la mata al pie de la hoja
    twist: float = 0.0           # grados que gira el ancho hasta la punta

    @property
    def segments(self) -> int:
        return len(self.colors)

    @property
    def triangles(self) -> int:
        return 2 * (self.segments - 1) + 1


def blade_spine(blade: Blade) -> list[Vector]:
    """Nervio curvado: «segments» tramos iguales con el ángulo creciendo
    de «tilt» a «tilt + bend» (curva más marcada hacia la punta)."""
    out, _side = _horizontal(blade.azimuth)
    step = blade.length / blade.segments
    points = [out * blade.offset]
    for k in range(blade.segments):
        t = (k + 0.5) / blade.segments
        angle = math.radians(blade.tilt + blade.bend * t ** 1.4)
        points.append(points[-1] + (out * math.sin(angle) + Z_AXIS * math.cos(angle)) * step)
    return points


def add_blade(b: WindBuilder, blade: Blade, origin: Vector | None = None, tip_weight: float = 1.0) -> None:
    """Hoja afilada: quads por tramo y un triángulo en la punta."""
    base = Vector((0.0, 0.0, 0.0)) if origin is None else origin
    spine = blade_spine(blade)
    n = blade.segments
    rows: list[tuple[int, int]] = []
    for k in range(n):
        t = k / n
        _out, side = _horizontal(blade.azimuth + math.radians(blade.twist) * t)
        half = blade.width * (1.0 - 0.4 * t)
        p = base + spine[k]
        w = tip_weight * t
        rows.append((b.vert(p - side * half, w), b.vert(p + side * half, w)))
    for k in range(n - 1):
        (l0, r0), (l1, r1) = rows[k], rows[k + 1]
        b.face((l0, r0, r1, l1), blade.colors[k])
    tip = b.vert(base + spine[-1], tip_weight)
    b.face((rows[-1][0], rows[-1][1], tip), blade.colors[-1])


@dataclass(frozen=True)
class TuftSpec:
    seed: int
    blades: int
    length: tuple[float, float]
    tilt: tuple[float, float]       # grados, de la hoja central a la exterior
    bend: tuple[float, float]
    width: tuple[float, float]
    lower: tuple[int, ...]          # color del tramo bajo (se alterna)
    middle: int
    tip: tuple[int, ...]            # color de la punta (se alterna)
    long_segments: int = 2          # hojas largas con un tramo más
    spread: float = 0.035


def tuft_blades(spec: TuftSpec) -> list[Blade]:
    """Hojas de una mata en espiral áurea: las de fuera, más cortas y abiertas."""
    rng = random.Random(spec.seed)
    blades = []
    for k in range(spec.blades):
        radial = math.sqrt((k + 0.5) / spec.blades)
        length = rng.uniform(*spec.length) * (1.12 - 0.3 * radial)
        long_blade = k < spec.long_segments
        lower = spec.lower[k % len(spec.lower)]
        tip = spec.tip[k % len(spec.tip)]
        colors = (lower, spec.middle, tip) if long_blade else (lower, tip)
        blades.append(Blade(
            azimuth=k * GOLDEN + rng.uniform(-0.25, 0.25),
            length=length,
            tilt=spec.tilt[0] + (spec.tilt[1] - spec.tilt[0]) * radial + rng.uniform(-4.0, 4.0),
            bend=rng.uniform(*spec.bend),
            width=rng.uniform(*spec.width),
            colors=colors,
            offset=spec.spread * radial,
            twist=rng.uniform(-35.0, 35.0),
        ))
    return blades


def build_tuft(spec: TuftSpec) -> WindBuilder:
    b = WindBuilder()
    for blade in tuft_blades(spec):
        add_blade(b, blade)
    return b


TUFTS: dict[str, TuftSpec] = {
    # mata media, tres verdes: oscuro abajo, salvia en medio, claro en la punta
    'SM_GrassTuft_A': TuftSpec(seed=61, blades=7, length=(0.3, 0.4), tilt=(6.0, 34.0), bend=(18.0, 34.0),
                               width=(0.026, 0.033), lower=(C['hierba_oscura'], C['hierba_salvia']),
                               middle=C['hierba_salvia'], tip=(C['hierba_clara'],), long_segments=2),
    # mata baja y abierta de 9 hojas, al sol: salvia y claro, alguna punta lima
    'SM_GrassTuft_B': TuftSpec(seed=67, blades=9, length=(0.27, 0.34), tilt=(14.0, 46.0), bend=(20.0, 38.0),
                               width=(0.023, 0.029), lower=(C['hierba_salvia'],),
                               middle=C['hierba_salvia'], tip=(C['hierba_clara'], C['hierba_clara'], C['verde_lima']),
                               long_segments=0, spread=0.045),
    # mata alta y recta de 5 hojas con la punta doblada
    'SM_GrassTuft_C': TuftSpec(seed=71, blades=5, length=(0.42, 0.48), tilt=(4.0, 22.0), bend=(30.0, 48.0),
                               width=(0.028, 0.035), lower=(C['hierba_oscura'],),
                               middle=C['hierba_salvia'], tip=(C['hierba_clara'],), long_segments=5),
}


# ---------------------------------------------------------------------------
# Hierba alta con espigas
# ---------------------------------------------------------------------------
TALL_TUFT = TuftSpec(seed=73, blades=7, length=(0.56, 0.7), tilt=(3.0, 26.0), bend=(26.0, 44.0),
                     width=(0.02, 0.026), lower=(C['hierba_oscura'], C['hierba_salvia']),
                     middle=C['hierba_salvia'], tip=(C['hierba_clara'],), long_segments=7, spread=0.04)
# (acimut en grados, altura de la espiga, inclinación en grados)
TALL_STALKS = ((40.0, 0.7, 9.0), (170.0, 0.76, 6.0), (290.0, 0.64, 13.0))


def add_card(b: WindBuilder, p0: Vector, p1: Vector, half: float, color: int,
             w0: float, w1: float, side: Vector) -> None:
    """Tallo plano de p0 a p1 (2 tris), con el ancho en «side»."""
    ids = (b.vert(p0 - side * half, w0), b.vert(p0 + side * half, w0),
           b.vert(p1 + side * half * 0.7, w1), b.vert(p1 - side * half * 0.7, w1))
    b.face(ids, color)


def add_seed_head(b: WindBuilder, bottom: Vector, axis: Vector, side: Vector, length: float, half: float,
                  color: int) -> None:
    """Espiga en rombo (2 tris) a lo largo de «axis»."""
    mid = bottom + axis * (length * 0.45)
    ids = (b.vert(bottom, 0.92), b.vert(mid + side * half, 0.96), b.vert(bottom + axis * length, 1.0),
           b.vert(mid - side * half, 0.96))
    b.face((ids[0], ids[1], ids[2]), color)
    b.face((ids[0], ids[2], ids[3]), color)


def build_tall() -> WindBuilder:
    b = WindBuilder()
    for blade in tuft_blades(TALL_TUFT):
        add_blade(b, blade)
    head = 0.09
    for az_deg, height, lean in TALL_STALKS:
        out, side = _horizontal(math.radians(az_deg))
        lean_r = math.radians(lean)
        axis = out * math.sin(lean_r) + Z_AXIS * math.cos(lean_r)
        stem_top = axis * ((height - head) / math.cos(lean_r))
        stem_weight = 0.92
        add_card(b, Vector((0.0, 0.0, 0.0)), stem_top, 0.005, C['hierba_salvia'], 0.0, stem_weight, side)
        add_seed_head(b, stem_top, axis, side, head / math.cos(lean_r), 0.014, C['hierba_seca'])
    return b


# ---------------------------------------------------------------------------
# Flores
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class FlowerHead:
    azimuth: float      # grados: hacia dónde se inclina el tallo
    height: float       # m del centro de la flor
    lean: float         # grados del tallo desde la vertical
    radius: float       # m de la punta de los pétalos


@dataclass(frozen=True)
class FlowerSpec:
    heads: tuple[FlowerHead, ...]
    petals: int
    petal: int
    center: int
    center_sides: int     # 4 (2 tris) o 5 (3 tris)
    stem_sides: int       # 2 = tallo plano (2 tris), 3 = tubo triangular (6 tris)
    leaves: tuple[Blade, ...]


def _head_frame(normal: Vector) -> tuple[Vector, Vector]:
    helper = Vector((1.0, 0.0, 0.0)) if abs(normal.x) < 0.9 else Vector((0.0, 1.0, 0.0))
    u = cross(helper, normal).normalized()
    return u, cross(normal, u)


def add_flower_head(b: WindBuilder, center: Vector, normal: Vector, radius: float, spec: FlowerSpec,
                    phase: float) -> None:
    """Estrella de pétalos en copa (2 tris por pétalo) y botón central
    elevado (polígono convexo de «center_sides» lados)."""
    u, v = _head_frame(normal)
    hub = b.vert(center, 1.0)
    ring = []
    for k in range(2 * spec.petals):
        a = phase + math.pi * k / spec.petals
        tip = k % 2 == 0
        r = radius if tip else radius * 0.42
        lift = radius * (0.22 if tip else 0.06)
        ring.append(b.vert(center + (u * math.cos(a) + v * math.sin(a)) * r + normal * lift, 1.0))
    for k in range(spec.petals):
        notch, tip, nxt = ring[(2 * k - 1) % len(ring)], ring[2 * k], ring[2 * k + 1]
        b.face((hub, notch, tip), spec.petal)
        b.face((hub, tip, nxt), spec.petal)
    disk_c = center + normal * (radius * 0.12)
    disk = [b.vert(disk_c + (u * math.cos(a) + v * math.sin(a)) * (radius * 0.3), 1.0)
            for a in (phase + 2.0 * math.pi * k / spec.center_sides for k in range(spec.center_sides))]
    for k in range(1, spec.center_sides - 1):
        b.face((disk[0], disk[k], disk[k + 1]), spec.center)


def add_stem(b: WindBuilder, top: Vector, sides: int, color: int, azimuth: float) -> None:
    base = Vector((0.0, 0.0, 0.0))
    _out, side = _horizontal(azimuth)
    if sides == 2:
        add_card(b, base, top, 0.006, color, 0.0, 0.95, side)
        return
    axis = (top - base).normalized()
    u, v = _head_frame(axis)
    radius = 0.005
    lower = [b.vert(base + (u * math.cos(a) + v * math.sin(a)) * radius, 0.0)
             for a in (2.0 * math.pi * k / sides for k in range(sides))]
    upper = [b.vert(top + (u * math.cos(a) + v * math.sin(a)) * (radius * 0.7), 0.95)
             for a in (2.0 * math.pi * k / sides for k in range(sides))]
    b.bridge(lower, upper, color)


def build_flower(spec: FlowerSpec) -> WindBuilder:
    b = WindBuilder()
    for leaf in spec.leaves:
        add_blade(b, leaf, tip_weight=0.6)
    for k, head in enumerate(spec.heads):
        az = math.radians(head.azimuth)
        out, _side = _horizontal(az)
        lean = math.radians(head.lean)
        top = (out * math.sin(lean) + Z_AXIS * math.cos(lean)) * (head.height / math.cos(lean))
        add_stem(b, top, spec.stem_sides, C['hierba_oscura'], az + math.pi / 2)
        # la flor mira hacia arriba y un poco hacia fuera
        normal = (Z_AXIS + out * math.sin(lean) * 1.6).normalized()
        add_flower_head(b, top, normal, head.radius, spec, phase=0.7 * k)
    return b


def _leaf(azimuth_deg: float, length: float, tilt: float) -> Blade:
    return Blade(azimuth=math.radians(azimuth_deg), length=length, tilt=tilt, bend=30.0, width=0.011,
                 colors=(C['hierba_oscura'], C['hierba_salvia']), twist=20.0)


FLOWERS: dict[str, FlowerSpec] = {
    # margarita blanca de botón amarillo: 2 flores y 2 hojas
    'SM_FlowerA': FlowerSpec(
        heads=(FlowerHead(30.0, 0.31, 7.0, 0.06), FlowerHead(215.0, 0.25, 14.0, 0.052)),
        petals=5, petal=C['blanco_nube'], center=C['laton'], center_sides=5, stem_sides=3,
        leaves=(_leaf(120.0, 0.16, 44.0), _leaf(300.0, 0.14, 52.0))),
    # florecillas lilas de 4 pétalos: 3 flores, tallos planos y 2 hojas
    'SM_FlowerB': FlowerSpec(
        heads=(FlowerHead(80.0, 0.29, 6.0, 0.048), FlowerHead(200.0, 0.24, 15.0, 0.044),
               FlowerHead(320.0, 0.2, 20.0, 0.042)),
        petals=4, petal=C['malva_ocaso'], center=C['laton'], center_sides=4, stem_sides=2,
        leaves=(_leaf(20.0, 0.13, 50.0), _leaf(250.0, 0.14, 46.0))),
}


# ---------------------------------------------------------------------------
# Helecho
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Frond:
    azimuth: float      # grados
    length: float
    tilt: float         # grados desde la vertical en la base
    droop: float        # grados que cae hasta la punta
    width: float        # m de la pinna más larga
    segments: int


FERN_FRONDS: tuple[Frond, ...] = tuple(
    Frond(azimuth=k * 60.0 + (8.0 if k % 2 else -5.0), length=0.5 + 0.06 * ((k * 5) % 3) / 2,
          tilt=34.0 + 5.0 * (k % 3), droop=58.0 + 8.0 * (k % 2), width=0.11, segments=7)
    for k in range(6)
) + (
    Frond(azimuth=95.0, length=0.32, tilt=8.0, droop=36.0, width=0.065, segments=4),
    Frond(azimuth=275.0, length=0.29, tilt=11.0, droop=32.0, width=0.06, segments=4),
)


def _fern_color(t: float, left: bool) -> int:
    """Raquis «doblado»: una mitad más oscura que la otra y la punta clara."""
    if t >= 0.78:
        return C['hierba_clara']
    return C['verde_palma'] if left else C['hierba_oscura']


def add_frond(b: WindBuilder, frond: Frond) -> None:
    """Fronda en dientes de sierra: por tramo del raquis, una pinna a cada
    lado (triángulo apuntando hacia la punta), más larga en el centro."""
    out, side = _horizontal(math.radians(frond.azimuth))
    step = frond.length / frond.segments
    spine = [Vector((0.0, 0.0, 0.0))]
    for k in range(frond.segments):
        t = (k + 0.5) / frond.segments
        angle = math.radians(frond.tilt + frond.droop * t ** 1.3)
        spine.append(spine[-1] + (out * math.sin(angle) + Z_AXIS * math.cos(angle)) * step)
    ids = [b.vert(p, k / frond.segments) for k, p in enumerate(spine)]
    for k in range(frond.segments):
        t = (k + 0.5) / frond.segments
        reach = frond.width * math.sin(math.pi * (0.12 + 0.88 * t))
        along = spine[k].lerp(spine[k + 1], 0.85)
        weight = min(1.0, t + 0.1)
        for sign in (-1.0, 1.0):
            color = _fern_color(t, sign < 0)
            pinna = b.vert(along + side * (sign * reach) - Z_AXIS * (reach * 0.3), weight)
            b.face((ids[k], ids[k + 1], pinna) if sign > 0 else (ids[k], pinna, ids[k + 1]), color)


def build_fern() -> WindBuilder:
    b = WindBuilder()
    for frond in FERN_FRONDS:
        add_frond(b, frond)
    return b


# ---------------------------------------------------------------------------
# Tréboles
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Trefoil:
    x: float
    y: float
    height: float       # m del centro de la hoja
    turn: float         # grados
    size: float         # m de cada folíolo
    colors: tuple[int, int]


CLOVER_LEAVES: tuple[Trefoil, ...] = (
    Trefoil(0.0, 0.0, 0.05, 10.0, 0.066, (C['hierba_oscura'], C['verde_palma'])),
    Trefoil(0.12, 0.06, 0.035, 70.0, 0.056, (C['hierba_salvia'], C['hierba_oscura'])),
    Trefoil(-0.1, 0.1, 0.04, 140.0, 0.06, (C['hierba_oscura'], C['verde_palma'])),
    Trefoil(-0.03, -0.13, 0.03, 200.0, 0.052, (C['hierba_salvia'], C['hierba_oscura'])),
)
CLOVER_FLOWER = (0.06, -0.05, 0.075)    # x, y, altura de la cabezuela


def add_trefoil(b: WindBuilder, leaf: Trefoil) -> None:
    """Tallo plano y tres folíolos en cometa, cada uno partido en dos tonos."""
    center = Vector((leaf.x, leaf.y, leaf.height))
    foot = Vector((leaf.x * 0.6, leaf.y * 0.6, 0.0))
    _out, side = _horizontal(math.radians(leaf.turn))
    add_card(b, foot, center, 0.003, C['hierba_oscura'], 0.0, 0.2, side)
    hub = b.vert(center, 0.2)
    for k in range(3):
        d, s = _horizontal(math.radians(leaf.turn + 120.0 * k))
        tip = b.vert(center + d * leaf.size + Z_AXIS * 0.008, CLOVER_TIP_WEIGHT)
        left = b.vert(center + d * (leaf.size * 0.62) + s * (leaf.size * 0.42) + Z_AXIS * 0.004, 0.3)
        right = b.vert(center + d * (leaf.size * 0.62) - s * (leaf.size * 0.42) + Z_AXIS * 0.004, 0.3)
        b.face((hub, right, tip), leaf.colors[0])
        b.face((hub, tip, left), leaf.colors[1])


def add_clover_flower(b: WindBuilder) -> None:
    x, y, h = CLOVER_FLOWER
    top = Vector((x, y, h))
    add_card(b, Vector((x * 0.7, y * 0.7, 0.0)), top, 0.003, C['hierba_salvia'], 0.0, CLOVER_TIP_WEIGHT,
             Vector((0.0, 1.0, 0.0)))
    w = CLOVER_TIP_WEIGHT
    r = 0.019
    ring = [b.vert(top + Vector((math.cos(a) * r, math.sin(a) * r, 0.006)), w)
            for a in (2.0 * math.pi * k / 3 for k in range(3))]
    apex = b.vert(top + Vector((0.0, 0.0, 0.022)), w)
    bottom = b.vert(top - Vector((0.0, 0.0, 0.004)), w)
    for k in range(3):
        b.face((ring[k], ring[(k + 1) % 3], apex), C['papel'])
        b.face((ring[(k + 1) % 3], ring[k], bottom), C['papel'])


def build_clover() -> WindBuilder:
    b = WindBuilder()
    for leaf in CLOVER_LEAVES:
        add_trefoil(b, leaf)
    add_clover_flower(b)
    return b

