"""Parches de césped del kit K6: manojos densos de hojas finas para sembrar una alfombra continua.

Cada parche junta 33-36 hojas finas (proporción largo / ancho de 8:1 a 12:1; las mismas
tarjetas de una cara que las matas, `k6_formas.add_blade`) en una huella circular de
0,35-0,45 m de radio, en tres capas:

- un anillo exterior de hojas largas de tres tramos que nacen cerca del centro y se
  arquean hacia fuera (dan el radio y la silueta de pasto);
- hojas medias de dos tramos en espiral áurea, algo curvadas, repartidas por la huella;
- una capa baja de hojas cortas de un solo triángulo (espigas finas, casi rectas) que
  tapa el suelo entre las demás. Un triángulo por hoja es lo que más suelo tapa por
  triángulo; al ser cortas y estar debajo, no hace falta que se curven.

Ninguna hoja se tumba: la inclinación más la curvatura queda por debajo de 65° en todas,
así que de cerca se ve el canto y la curva de la hoja, nunca una cara plana y ancha al sol.
La base de cada hoja de varios tramos es más oscura que la punta; las cortas de un
triángulo llevan los verdes oscuros (hacen de sombra al pie). El peso de viento sube de 0
en el pie a 1 en la punta. Determinista: semillas fijas.
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass

from kits.k6_formas import GOLDEN, Blade, C, WindBuilder, add_blade

MAX_LEAN = 65.0             # grados: inclinación + curvatura máximas de cualquier hoja
MIN_ASPECT = 8.0            # largo / ancho en la base, como mínimo
HALF_WIDTH = (0.015, 0.03)  # m: semiancho en la base admitido


@dataclass(frozen=True)
class BladeGroup:
    """Grupo de hojas de un parche con sus rangos de forma."""

    count: int
    length: tuple[float, float]          # m a lo largo del nervio
    tilt: tuple[float, float]            # grados desde la vertical en la base
    bend: tuple[float, float]            # grados que se dobla más hasta la punta
    aspect: tuple[float, float]          # largo / ancho en la base (ancho = 2 x semiancho)
    feet: tuple[float, float]            # m del centro al pie de la hoja
    colors: tuple[tuple[int, ...], ...]  # secuencias de color (una por tramo) que se alternan
    ring: bool                           # True: acimuts repartidos en anillo; False: espiral áurea
    scatter: float = 0.0                 # grados que la hoja se aparta de inclinarse hacia fuera

    @property
    def triangles(self) -> int:
        return sum(2 * (len(self.colors[k % len(self.colors)]) - 1) + 1 for k in range(self.count))

    @property
    def max_lean(self) -> float:
        return self.tilt[1] + self.bend[1]


@dataclass(frozen=True)
class PatchSpec:
    seed: int
    outer: BladeGroup                    # largas de tres tramos en anillo
    middle: BladeGroup                   # medias de dos tramos
    inner: BladeGroup                    # cortas de un triángulo

    @property
    def groups(self) -> tuple[BladeGroup, BladeGroup, BladeGroup]:
        return self.outer, self.middle, self.inner

    @property
    def blades(self) -> int:
        return sum(g.count for g in self.groups)

    @property
    def triangles(self) -> int:
        return sum(g.triangles for g in self.groups)


def group_blades(group: BladeGroup, rng: random.Random, phase: float) -> list[Blade]:
    """Hojas de un grupo: el pie va hacia fuera y la hoja se inclina hacia fuera, apartándose
    hasta «scatter» grados a cada lado (rellena los huecos entre las hojas vecinas)."""
    blades = []
    step = 2.0 * math.pi / group.count if group.ring else GOLDEN
    for k in range(group.count):
        if group.ring:
            foot = rng.uniform(*group.feet)
        else:
            radial = math.sqrt((k + 0.5) / group.count)
            foot = group.feet[0] + (group.feet[1] - group.feet[0]) * radial
        length = rng.uniform(*group.length)
        half = length / (2.0 * rng.uniform(*group.aspect))
        foot_azimuth = phase + k * step + rng.uniform(-0.22, 0.22)
        blades.append(Blade(
            azimuth=foot_azimuth + math.radians(rng.uniform(-group.scatter, group.scatter)),
            length=length,
            tilt=rng.uniform(*group.tilt),
            bend=rng.uniform(*group.bend),
            width=min(max(half, HALF_WIDTH[0]), HALF_WIDTH[1]),
            colors=group.colors[k % len(group.colors)],
            offset=foot,
            twist=rng.uniform(-25.0, 25.0),
            foot_azimuth=foot_azimuth,
        ))
    return blades


def patch_blades(spec: PatchSpec) -> list[Blade]:
    rng = random.Random(spec.seed)
    phase = rng.uniform(0.0, 2.0 * math.pi)
    return (group_blades(spec.outer, rng, phase) + group_blades(spec.middle, rng, phase + 0.5)
            + group_blades(spec.inner, rng, phase + 1.3))


def build_patch(spec: PatchSpec) -> WindBuilder:
    b = WindBuilder()
    for blade in patch_blades(spec):
        add_blade(b, blade)
    return b


# Secuencias de color de la base a la punta: base oscura, punta clara.
_OSC, _SAL, _CLA, _LIM = C['hierba_oscura'], C['hierba_salvia'], C['hierba_clara'], C['verde_lima']

PATCHES: dict[str, PatchSpec] = {
    # A: erguido y lleno; 7 largas poco abiertas, 8 medias y 28 cortas, oscuras al pie
    'SM_GrassPatch_A': PatchSpec(
        seed=811,
        outer=BladeGroup(7, length=(0.48, 0.54), tilt=(14.0, 24.0), bend=(26.0, 38.0), aspect=(8.05, 8.4),
                         feet=(0.04, 0.09), colors=((_OSC, _SAL, _CLA), (_SAL, _SAL, _CLA)), ring=True),
        middle=BladeGroup(8, length=(0.32, 0.4), tilt=(8.0, 26.0), bend=(18.0, 32.0), aspect=(8.05, 8.4),
                          feet=(0.0, 0.2), colors=((_OSC, _SAL), (_SAL, _CLA)), ring=False, scatter=60.0),
        inner=BladeGroup(32, length=(0.3, 0.42), tilt=(12.0, 30.0), bend=(0.0, 8.0), aspect=(8.05, 8.3),
                         feet=(0.02, 0.26), colors=((_OSC,), (_SAL,), (_OSC,)), ring=False, scatter=120.0)),
    # B: abierto y al sol; 8 largas que se abren hacia fuera, 8 medias y 24 cortas
    'SM_GrassPatch_B': PatchSpec(
        seed=823,
        outer=BladeGroup(8, length=(0.44, 0.52), tilt=(24.0, 32.0), bend=(22.0, 32.0), aspect=(8.05, 8.4),
                         feet=(0.02, 0.06), colors=((_SAL, _CLA, _CLA), (_SAL, _SAL, _LIM)), ring=True),
        middle=BladeGroup(8, length=(0.32, 0.38), tilt=(12.0, 30.0), bend=(16.0, 30.0), aspect=(8.05, 8.4),
                          feet=(0.0, 0.2), colors=((_SAL, _CLA), (_SAL, _LIM)), ring=False, scatter=60.0),
        inner=BladeGroup(28, length=(0.3, 0.4), tilt=(12.0, 30.0), bend=(0.0, 8.0), aspect=(8.05, 8.3),
                         feet=(0.02, 0.26), colors=((_SAL,), (_SAL,), (_CLA,)), ring=False, scatter=120.0)),
    # C: mezclado: 6 largas que se curvan, 10 medias y 26 cortas
    'SM_GrassPatch_C': PatchSpec(
        seed=839,
        outer=BladeGroup(6, length=(0.5, 0.55), tilt=(12.0, 22.0), bend=(34.0, 42.0), aspect=(8.05, 8.4),
                         feet=(0.05, 0.1), colors=((_OSC, _SAL, _CLA),), ring=True),
        middle=BladeGroup(10, length=(0.3, 0.38), tilt=(6.0, 28.0), bend=(16.0, 34.0), aspect=(8.05, 8.4),
                          feet=(0.0, 0.2), colors=((_OSC, _SAL), (_SAL, _CLA), (_OSC, _CLA)), ring=False, scatter=60.0),
        inner=BladeGroup(30, length=(0.3, 0.4), tilt=(12.0, 30.0), bend=(0.0, 8.0), aspect=(8.05, 8.3),
                         feet=(0.02, 0.26), colors=((_OSC,), (_SAL,)), ring=False, scatter=120.0)),
}
