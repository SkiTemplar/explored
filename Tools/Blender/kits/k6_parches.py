"""Parches de césped del kit K6: manojos densos para sembrar una alfombra continua.

Cada parche junta 12-18 hojas afiladas (las mismas tarjetas de una cara que las
matas, `k6_formas.add_blade`) en una huella circular de 0,35-0,45 m de radio:

- un anillo exterior de hojas largas de tres tramos que nacen lejos del centro
  y se curvan hacia fuera (dan el radio y tapan el borde de la huella);
- un relleno interior de hojas cortas de dos tramos en espiral áurea, que
  tapan el centro para que desde arriba no quede un agujero.

La base de cada hoja es más oscura que la punta y el peso de viento sube de 0
en el pie a 1 en la punta (igual que las matas). Determinista: semillas fijas.
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass

from kits.k6_formas import GOLDEN, Blade, C, WindBuilder, add_blade


@dataclass(frozen=True)
class BladeGroup:
    """Grupo de hojas de un parche con sus rangos de forma."""

    count: int
    length: tuple[float, float]          # m a lo largo del nervio
    tilt: tuple[float, float]            # grados desde la vertical en la base
    bend: tuple[float, float]            # grados que se dobla más hasta la punta
    width: tuple[float, float]           # semiancho en la base (m)
    feet: tuple[float, float]            # m del centro al pie de la hoja
    colors: tuple[tuple[int, ...], ...]  # secuencias de color (una por tramo) que se alternan
    ring: bool                           # True: acimuts repartidos en anillo; False: espiral áurea

    @property
    def triangles(self) -> int:
        return sum(2 * (len(self.colors[k % len(self.colors)]) - 1) + 1 for k in range(self.count))


@dataclass(frozen=True)
class PatchSpec:
    seed: int
    outer: BladeGroup
    inner: BladeGroup

    @property
    def blades(self) -> int:
        return self.outer.count + self.inner.count

    @property
    def triangles(self) -> int:
        return self.outer.triangles + self.inner.triangles


def group_blades(group: BladeGroup, rng: random.Random, phase: float) -> list[Blade]:
    """Hojas de un grupo: el pie va en la dirección hacia la que se inclina la hoja."""
    blades = []
    step = 2.0 * math.pi / group.count if group.ring else GOLDEN
    for k in range(group.count):
        if group.ring:
            foot = rng.uniform(*group.feet)
        else:
            radial = math.sqrt((k + 0.5) / group.count)
            foot = group.feet[0] + (group.feet[1] - group.feet[0]) * radial
        blades.append(Blade(
            azimuth=phase + k * step + rng.uniform(-0.22, 0.22),
            length=rng.uniform(*group.length),
            tilt=rng.uniform(*group.tilt),
            bend=rng.uniform(*group.bend),
            width=rng.uniform(*group.width),
            colors=group.colors[k % len(group.colors)],
            offset=foot,
            twist=rng.uniform(-18.0, 18.0),
        ))
    return blades


def patch_blades(spec: PatchSpec) -> list[Blade]:
    rng = random.Random(spec.seed)
    phase = rng.uniform(0.0, 2.0 * math.pi)
    return group_blades(spec.outer, rng, phase) + group_blades(spec.inner, rng, phase + 0.5)


def build_patch(spec: PatchSpec) -> WindBuilder:
    b = WindBuilder()
    for blade in patch_blades(spec):
        add_blade(b, blade)
    return b


# Secuencias de color de la base a la punta: base oscura, punta clara.
_OSC, _SAL, _CLA, _LIM = C['hierba_oscura'], C['hierba_salvia'], C['hierba_clara'], C['verde_lima']

PATCHES: dict[str, PatchSpec] = {
    # A: erguido y lleno; anillo de 9 hojas largas poco abiertas y 8 cortas en roseta
    'SM_GrassPatch_A': PatchSpec(
        seed=811,
        outer=BladeGroup(9, length=(0.42, 0.49), tilt=(14.0, 26.0), bend=(24.0, 38.0), width=(0.054, 0.064),
                         feet=(0.07, 0.11), colors=((_OSC, _SAL, _CLA), (_SAL, _SAL, _CLA)), ring=True),
        inner=BladeGroup(8, length=(0.28, 0.34), tilt=(48.0, 68.0), bend=(10.0, 22.0), width=(0.064, 0.074),
                         feet=(-0.06, 0.1), colors=((_OSC, _SAL), (_SAL, _CLA)), ring=False)),
    # B: caído al exterior, al sol; 10 hojas que se tumban hacia fuera y 8 cortas más rectas
    'SM_GrassPatch_B': PatchSpec(
        seed=823,
        outer=BladeGroup(10, length=(0.37, 0.43), tilt=(38.0, 50.0), bend=(40.0, 58.0), width=(0.054, 0.064),
                         feet=(0.03, 0.07), colors=((_SAL, _CLA, _CLA), (_SAL, _SAL, _LIM)), ring=True),
        inner=BladeGroup(8, length=(0.29, 0.33), tilt=(10.0, 46.0), bend=(14.0, 28.0), width=(0.064, 0.074),
                         feet=(-0.06, 0.08), colors=((_SAL, _CLA), (_SAL, _LIM)), ring=False)),
    # C: ralo y mezclado: 5 hojas largas que curvan y 12 cortas repartidas por la huella
    'SM_GrassPatch_C': PatchSpec(
        seed=839,
        outer=BladeGroup(5, length=(0.48, 0.54), tilt=(14.0, 24.0), bend=(36.0, 50.0), width=(0.05, 0.058),
                         feet=(0.05, 0.1), colors=((_OSC, _SAL, _CLA),), ring=True),
        inner=BladeGroup(12, length=(0.22, 0.3), tilt=(46.0, 72.0), bend=(10.0, 24.0), width=(0.064, 0.076),
                         feet=(-0.08, 0.16), colors=((_OSC, _SAL), (_SAL, _CLA), (_OSC, _CLA)), ring=False)),
}
