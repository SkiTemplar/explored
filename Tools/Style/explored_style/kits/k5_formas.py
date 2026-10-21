"""Formas del kit K5 Cielo: nubes de cúmulo por lóbulos y discos de sol y luna.

Nube: unión de 3 a 7 lóbulos (cúpulas de revolución achatadas, cerradas,
con base plana). Los lóbulos que apoyan en la base (z = 0) llevan gris en la
tapa inferior y en el primer tramo, que mira hacia abajo; los lóbulos
elevados solo en la tapa inferior, que queda dentro de la nube. Cada nube se
escala de forma uniforme a su ancho objetivo y se centra en XY.

Discos: polígono plano en el plano XY (normal +Z, z = 0), Ø 1 m, pivote en el
centro. En el motor se orientan con la Z local hacia la cámara y se escalan
con la distancia: Ø = 2 · distancia · tan(diámetro angular / 2).
"""

from __future__ import annotations

import math
from collections.abc import Sequence
from dataclasses import dataclass

import bpy  # noqa: F401  (debe importarse antes que mathutils)

from mathutils import Matrix

from explored_style import meshkit
from explored_style.meshkit import MeshBuilder
from explored_style.palette import load_palette

C: dict[str, int] = {c.name: c.index for c in load_palette().colors}

# Tonos de la nube. La paleta no tiene blanco frío ni gris lila: se usan los
# más cercanos (propuesta de color en NUBE_PROPUESTA; paleta.json no se toca).
NUBE_ARRIBA = 'papel'
NUBE_ABAJO = 'acero'
NUBE_PROPUESTA = {
    'blanco_nube': {'hex': '#FAF7F2', 'sustituye': 'papel #F6EEDB (algo amarillento para nube)'},
    'gris_lila': {'hex': '#C7C3D6', 'sustituye': 'acero #B7C1C8 (gris azulado, sin matiz lila)'},
}

# Perfil unitario del lóbulo (radio, altura) en sentido antihorario: base
# metida (0,80), panza (1,0) y hombro; la tapa superior cierra en (0, 1).
LOBE_PROFILE: tuple[tuple[float, float], ...] = ((0.80, 0.0), (1.0, 0.22), (0.95, 0.52), (0.66, 0.84))
LOBE_JITTER = 0.07
GROUND_EPS = 1e-6


@dataclass(frozen=True)
class Lobe:
    """Cúpula achatada: centro de la base (x, y, z), radios y lados."""

    x: float
    y: float
    z: float
    rx: float
    ry: float
    rz: float
    sides: int = 8

    @property
    def on_ground(self) -> bool:
        return self.z <= GROUND_EPS

    @property
    def triangles(self) -> int:
        spans = len(LOBE_PROFILE) - 1
        return self.sides * (2 * spans + 2)


@dataclass(frozen=True)
class CloudShape:
    width: float
    lobes: tuple[Lobe, ...]
    seed: int

    @property
    def triangles(self) -> int:
        return sum(lobe.triangles for lobe in self.lobes)


def _lobe(lobe: Lobe, seed: int, top: int, bottom: int) -> MeshBuilder:
    """Lóbulo unitario con ruido radial determinista, ya escalado y situado."""
    b = MeshBuilder()
    first = bottom if lobe.on_ground else top
    phase = math.pi * meshkit.noise(seed, 7)
    colors: list[int | Sequence[int]] = [first, top, top]
    rings = meshkit.lathe(b, LOBE_PROFILE, lobe.sides, colors, phase=phase, cap_bottom=bottom, cap_top=top,
                          tips=(0.0, 1.0))
    for k, ring in enumerate(rings):
        for j, i in enumerate(ring):
            x, y, z = b.verts[i]
            s = 1.0 + LOBE_JITTER * meshkit.noise(seed, k, j)
            b.verts[i] = (x * s, y * s, z)
    out = MeshBuilder()
    place = Matrix.Translation((lobe.x, lobe.y, lobe.z)) @ Matrix.Diagonal((lobe.rx, lobe.ry, lobe.rz, 1.0))
    out.extend(b, place)
    return out


def build_cloud(shape: CloudShape) -> MeshBuilder:
    """Nube completa escalada a «shape.width» (eje X) y centrada en XY."""
    if not 3 <= len(shape.lobes) <= 7:
        raise ValueError(f'una nube lleva de 3 a 7 lóbulos (hay {len(shape.lobes)})')
    top, bottom = C[NUBE_ARRIBA], C[NUBE_ABAJO]
    raw = MeshBuilder()
    for k, lobe in enumerate(shape.lobes):
        raw.extend(_lobe(lobe, shape.seed * 31 + k, top, bottom))
    xs = [v[0] for v in raw.verts]
    ys = [v[1] for v in raw.verts]
    scale = shape.width / (max(xs) - min(xs))
    cx, cy = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2
    out = MeshBuilder()
    out.extend(raw, Matrix.Diagonal((scale, scale, scale, 1.0)) @ Matrix.Translation((-cx, -cy, 0.0)))
    return out


def build_disc(zones: Sequence[tuple[float, int | Sequence[int]]], sides: int, phase: float = 0.0,
               wobble: Sequence[float] = ()) -> MeshBuilder:
    """Disco plano (normal +Z) de anillos concéntricos. «zones» es
    [(radio exterior, color)] de dentro afuera; el color de una zona puede ser
    uno por cara. «wobble» desplaza en radio los vértices de los anillos
    interiores (fracción del radio, uno por vértice y anillo, en orden)."""
    if not zones:
        raise ValueError('build_disc: se necesita al menos una zona')
    b = MeshBuilder()
    center = b.vert((0.0, 0.0, 0.0))
    rings: list[list[int]] = []
    w = iter(wobble)
    for z, (radius, _) in enumerate(zones):
        last = z == len(zones) - 1
        ring = []
        for k in range(sides):
            a = phase + 2.0 * math.pi * k / sides
            r = radius * (1.0 if last else 1.0 + next(w, 0.0))
            ring.append(b.vert((r * math.cos(a), r * math.sin(a), 0.0)))
        rings.append(ring)
    first = zones[0][1]
    for k in range(sides):
        c = first if isinstance(first, int) else first[k]
        b.face((center, rings[0][k], rings[0][(k + 1) % sides]), c)
    for z in range(1, len(zones)):
        inner, outer = rings[z - 1], rings[z]
        color = zones[z][1]
        for k in range(sides):
            c = color if isinstance(color, int) else color[k]
            j = (k + 1) % sides
            b.face((outer[k], outer[j], inner[j], inner[k]), c)
    return b


def _ground(x: float, y: float, rx: float, ry: float, rz: float, sides: int = 8) -> Lobe:
    return Lobe(x, y, 0.0, rx, ry, rz, sides)


# Disposición en metros de cada nube antes de escalarla a su ancho: lóbulos
# casi esféricos (rz ≈ rx) que se solapan, con una corona elevada en las
# nubes grandes para la silueta de coliflor del cúmulo.
NUBES: dict[str, CloudShape] = {
    'SM_Nube_A': CloudShape(20.0, (
        _ground(-4.6, 0.8, 4.2, 3.8, 5.4, 10),
        _ground(0.3, -0.4, 5.6, 5.0, 9.0, 12),
        _ground(4.8, 0.6, 4.0, 3.6, 5.8, 10),
    ), seed=1),
    'SM_Nube_B': CloudShape(30.0, (
        _ground(-7.6, 0.6, 4.8, 4.4, 6.0, 9),
        _ground(-1.6, -0.8, 6.2, 5.6, 8.6, 10),
        _ground(5.4, 0.4, 5.2, 4.6, 6.8, 9),
        Lobe(0.4, 1.2, 4.0, 5.0, 4.4, 7.4, 9),
    ), seed=2),
    'SM_Nube_C': CloudShape(40.0, (
        _ground(-12.0, 0.6, 4.8, 4.4, 6.0),
        _ground(-5.5, -1.0, 6.4, 5.8, 9.0),
        _ground(3.0, -0.6, 7.0, 6.2, 10.0, 9),
        _ground(10.8, 0.6, 5.2, 4.6, 6.6),
        Lobe(-1.5, 1.4, 6.0, 6.0, 5.2, 9.0),
    ), seed=3),
    'SM_Nube_D': CloudShape(50.0, (
        _ground(-17.0, 0.4, 4.8, 4.4, 5.6, 7),
        _ground(-10.0, -1.0, 6.4, 5.8, 8.6),
        _ground(-1.0, -1.6, 7.8, 6.8, 10.6),
        _ground(8.5, 0.0, 7.0, 6.2, 9.4),
        _ground(16.0, 1.0, 4.8, 4.2, 6.0, 7),
        Lobe(-3.5, 1.4, 7.0, 6.4, 5.6, 9.6, 7),
    ), seed=4),
    'SM_Nube_E': CloudShape(60.0, (
        _ground(-23.0, 0.6, 4.8, 4.4, 5.4, 6),
        _ground(-15.5, -0.8, 6.6, 6.0, 8.4, 7),
        _ground(-5.5, -1.6, 8.4, 7.2, 11.0),
        _ground(5.5, -1.0, 8.0, 7.0, 10.4),
        _ground(15.0, 0.2, 6.6, 5.8, 8.2, 7),
        _ground(22.5, 1.0, 4.6, 4.2, 5.4, 6),
        Lobe(0.0, 1.6, 8.0, 7.0, 6.0, 11.0, 7),
    ), seed=5),
}


def cloud_builder(name: str) -> MeshBuilder:
    return build_cloud(NUBES[name])


SOL_LADOS = 16


def build_sol() -> MeshBuilder:
    """Sol: disco de latón con halo fino de ocaso (48 tris)."""
    zones: list[tuple[float, int | Sequence[int]]] = [(0.40, C['laton']), (0.5, C['cielo_ocaso'])]
    return build_disc(zones, SOL_LADOS)


LUNA_LADOS = 16
# Cara iluminada: círculo interior desplazado hacia la luz (arriba a la
# izquierda con la Y local hacia arriba); la media luna gris del lado contrario sugiere el volumen.
LUNA_LUZ_CENTRO = (-0.08, 0.09)
LUNA_LUZ_RADIO = 0.37
LUNA_SOMBRA_MIN = 0.045  # grosor (m, Ø 1 m) a partir del cual un vértice del anillo es sombra


def build_luna() -> MeshBuilder:
    """Luna: disco de papel con la media luna de sombra en acero (48 tris)."""
    b = MeshBuilder()
    cx, cy = LUNA_LUZ_CENTRO
    center = b.vert((cx, cy, 0.0))
    inner, outer, gaps = [], [], []
    for k in range(LUNA_LADOS):
        a = 2.0 * math.pi * k / LUNA_LADOS
        ix, iy = cx + LUNA_LUZ_RADIO * math.cos(a), cy + LUNA_LUZ_RADIO * math.sin(a)
        inner.append(b.vert((ix, iy, 0.0)))
        outer.append(b.vert((0.5 * math.cos(a), 0.5 * math.sin(a), 0.0)))
        gaps.append(0.5 - math.hypot(ix, iy))
    lit, dark = C['papel'], C['acero']
    shadow = [g >= LUNA_SOMBRA_MIN for g in gaps]
    for k in range(LUNA_LADOS):
        j = (k + 1) % LUNA_LADOS
        b.face((center, inner[k], inner[j]), lit)
        ok, oj, ij, ik = outer[k], outer[j], inner[j], inner[k]
        if shadow[k] == shadow[j]:
            b.face((ok, oj, ij, ik), dark if shadow[k] else lit)
        elif shadow[k]:  # la sombra acaba en punta en oj
            b.face((ok, oj, ik), dark)
            b.face((oj, ij, ik), lit)
        else:  # la sombra empieza en punta en ok
            b.face((ok, oj, ij), dark)
            b.face((ok, ij, ik), lit)
    return b
