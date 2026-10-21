"""Peces y gaviota del kit K3.

Un único cuerpo de pez sirve para el objeto (SM_Pez, tumbado de costado),
su variante asada (SM_PezAsado, con marcas de parrilla y espetón) y la
fauna (SM_FaunaPez, de pie, morro en +X, tope 200 tris). La gaviota
(SM_FaunaGaviota, tope 400) vuela con las alas abiertas en «M», morro en +X;
ambas se animan por código en Unreal (boids), sin esqueleto.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from mathutils import Matrix, Vector

from explored_style.kits.k3_base import C, ccw
from explored_style.meshkit import MeshBuilder, slab, sweep

_Y_AXIS = Vector((0.0, 1.0, 0.0))


@dataclass(frozen=True)
class FishLook:
    back: int
    flank: int
    belly: int
    fin: int
    eye: int
    pupil: int | None = None
    grill: int | None = None   # color de las marcas de parrilla (asado)
    big_eye: bool = False


# (s desde la cola, alto relativo, ancho relativo)
_FISH_STATIONS = ((0.0, 0.16, 0.3), (0.12, 0.36, 0.48), (0.35, 0.82, 0.86), (0.6, 1.0, 1.0),
                  (0.8, 0.82, 0.8), (0.93, 0.5, 0.55))
_GRILL_S = (0.42, 0.46, 0.62, 0.66)
_FISH_SIDES = 8


def _fish_shape(s: float) -> tuple[float, float]:
    for (s0, h0, w0), (s1, h1, w1) in zip(_FISH_STATIONS, _FISH_STATIONS[1:], strict=False):
        if s0 <= s <= s1:
            f = (s - s0) / (s1 - s0)
            return h0 + (h1 - h0) * f, w0 + (w1 - w0) * f
    return _FISH_STATIONS[-1][1], _FISH_STATIONS[-1][2]


def _ring_colors(look: FishLook, flank: int) -> list[int]:
    """Lomo arriba, vientre abajo y costados (vértice superior en el lomo)."""
    out = []
    for k in range(_FISH_SIDES):
        c = math.cos(2 * math.pi * (k + 0.5) / _FISH_SIDES)
        out.append(look.back if c > 0.5 else look.belly if c < -0.5 else flank)
    return out


def _fish_body(b: MeshBuilder, look: FishLook, length: float) -> tuple[float, float, float]:
    """Cuerpo de la cola (x0) al morro; devuelve (x de la cola, centro z, semialto)."""
    half_h, half_w = 0.16 * length, 0.075 * length
    x0, span = -0.36 * length, 0.81 * length
    s_list = sorted({s for s, _, _ in _FISH_STATIONS} | (set(_GRILL_S) if look.grill is not None else set()))
    pts, radii, colors = [], [], []
    for s in s_list:
        h, w = _fish_shape(s)
        pts.append(Vector((x0 + span * s, 0.0, half_h)))
        radii.append((half_h * h, half_w * w))
    for s0 in s_list[:-1]:
        stripe = look.grill is not None and s0 in (_GRILL_S[0], _GRILL_S[2])
        colors.append(_ring_colors(look, look.grill if stripe and look.grill is not None else look.flank))
    nose = Vector((x0 + span + 0.07 * length, 0.0, half_h * 0.9))
    sweep(b, pts, radii, _FISH_SIDES, colors, cap_start=look.fin, cap_end=look.back, tips=(None, nose))
    return x0, half_h, half_h


def _fish_fins(b: MeshBuilder, look: FishLook, length: float, x0: float, zc: float, half_h: float) -> None:
    thin = length * 0.014
    tail = [(x0 + 0.02 * length, zc + 0.12 * half_h), (x0 - 0.17 * length, zc + 1.0 * half_h),
            (x0 - 0.08 * length, zc), (x0 - 0.16 * length, zc - 0.85 * half_h),
            (x0 + 0.02 * length, zc - 0.12 * half_h)]
    slab(b, ccw([(x, 0.0, z) for x, z in tail], _Y_AXIS), _Y_AXIS * thin, look.fin,
         center=(x0 - 0.03 * length, 0.0, zc))
    span = 0.81 * length
    top = zc + half_h * 0.95
    dorsal = [(x0 + span * 0.4, 0.0, top - 0.01 * length), (x0 + span * 0.66, 0.0, top - 0.01 * length),
              (x0 + span * 0.45, 0.0, top + 0.5 * half_h)]
    slab(b, ccw(dorsal, _Y_AXIS), _Y_AXIS * thin, look.fin)
    for side in (1.0, -1.0):
        _, w = _fish_shape(0.74)
        y = side * 0.075 * length * w * 0.85
        root = Vector((x0 + span * 0.76, y, zc - 0.35 * half_h))
        fin = [root, root + Vector((-0.17 * length, side * 0.02 * length, -0.28 * half_h)),
               root + Vector((-0.1 * length, side * 0.015 * length, -0.6 * half_h))]
        normal = _Y_AXIS * side
        slab(b, ccw(fin, normal), normal * thin, look.fin)


def _disc(center: Vector, radius: float, n: int, phase: float = 0.0) -> list[Vector]:
    return [center + Vector((radius * math.cos(phase + 2 * math.pi * k / n), 0.0,
                             radius * math.sin(phase + 2 * math.pi * k / n))) for k in range(n)]


def _fish_eyes(b: MeshBuilder, look: FishLook, length: float, x0: float, zc: float, half_h: float) -> None:
    s = 0.86
    _, w = _fish_shape(s)
    for side in (1.0, -1.0):
        normal = _Y_AXIS * side
        at = Vector((x0 + 0.81 * length * s, side * 0.075 * length * w * 0.8, zc + 0.22 * half_h))
        if look.big_eye:
            slab(b, ccw(_disc(at, 0.045 * length, 6), normal), normal * 0.012 * length, look.eye)
            if look.pupil is not None:
                pupil = at + normal * 0.008 * length + Vector((0.006 * length, 0.0, 0.0))
                slab(b, ccw(_disc(pupil, 0.024 * length, 4, math.pi / 4), normal), normal * 0.01 * length,
                     look.pupil)
        else:
            slab(b, ccw(_disc(at, 0.03 * length, 4, math.pi / 4), normal), normal * 0.012 * length, look.eye)


def build_fish(look: FishLook, length: float) -> MeshBuilder:
    """Pez de pie, morro en +X y lomo en +Z."""
    b = MeshBuilder()
    x0, zc, half_h = _fish_body(b, look, length)
    _fish_fins(b, look, length, x0, zc, half_h)
    _fish_eyes(b, look, length, x0, zc, half_h)
    return b


PEZ_CRUDO = FishLook(back=C['agua_profunda'], flank=C['acero'], belly=C['papel'], fin=C['agua_media'],
                     eye=C['papel'], pupil=C['tinta'], big_eye=True)
PEZ_ASADO = FishLook(back=C['madera_oscura'], flank=C['cielo_ocaso'], belly=C['fibra'], fin=C['madera_oscura'],
                     eye=C['papel'], grill=C['madera_oscura'], big_eye=True)
PEZ_FAUNA = FishLook(back=C['agua_profunda'], flank=C['acero'], belly=C['papel'], fin=C['agua_media'],
                     eye=C['tinta'])
_ON_SIDE = Matrix.Rotation(math.pi / 2, 4, 'X')  # +Y (costado con el ojo) pasa a +Z


def _lying(fish: MeshBuilder) -> MeshBuilder:
    out = MeshBuilder()
    out.extend(fish, _ON_SIDE)
    return out


def build_pez() -> MeshBuilder:
    return _lying(build_fish(PEZ_CRUDO, 0.32))


def build_pez_asado() -> MeshBuilder:
    length = 0.32
    b = build_fish(PEZ_ASADO, length)
    zc = 0.16 * length
    stick = C['madera_clara']
    sweep(b, [(-0.62 * length, 0.0, zc), (0.78 * length, 0.0, zc * 0.95)], [0.0055, 0.0045], 5, stick,
          cap_start=stick, cap_end=stick, tips=(None, (0.86 * length, 0.0, zc * 0.95)))
    return _lying(b)


def build_fauna_pez() -> MeshBuilder:
    return build_fish(PEZ_FAUNA, 0.3)


# ---------------------------------------------------------------------------
# Gaviota en vuelo: cuerpo blanco, manto y alas gris acero, puntas negras,
# pico de latón; alas en «M» (diedro en el hombro, caída en la mano).
# ---------------------------------------------------------------------------
_GULL_BODY = ((-0.24, 0.0, 0.012, 0.025), (-0.15, 0.0, 0.045, 0.055), (-0.04, 0.0, 0.07, 0.07),
              (0.07, 0.01, 0.062, 0.062), (0.12, 0.035, 0.045, 0.043), (0.17, 0.06, 0.05, 0.047),
              (0.215, 0.066, 0.03, 0.028))
_GULL_WING = ((0.01, 0.03, 0.03, 0.014, 0.085), (0.0, 0.17, 0.085, 0.013, 0.09), (-0.03, 0.3, 0.1, 0.011, 0.075),
              (-0.07, 0.41, 0.075, 0.009, 0.058), (-0.14, 0.53, 0.03, 0.006, 0.038))
_GULL_ZC = 0.12


def _gull_body(b: MeshBuilder) -> None:
    white, grey = C['papel'], C['acero']
    pts = [(x, 0.0, _GULL_ZC + z) for x, z, _, _ in _GULL_BODY]
    radii = [(ru, rv) for _, _, ru, rv in _GULL_BODY]
    colors: list[int | list[int]] = []
    for k in range(len(pts) - 1):
        if 1 <= k <= 3:
            colors.append([grey if math.cos(2 * math.pi * (j + 0.5) / 8) > 0.5 else white for j in range(8)])
        else:
            colors.append(white)
    sweep(b, pts, radii, 8, colors, cap_start=white, cap_end=white)
    beak = C['laton']
    sweep(b, [(0.2, 0.0, _GULL_ZC + 0.06), (0.265, 0.0, _GULL_ZC + 0.052)], [0.014, 0.006], 4, beak,
          phase=math.pi / 4, cap_start=beak, cap_end=beak, tips=(None, (0.3, 0.0, _GULL_ZC + 0.045)))
    for side in (1.0, -1.0):
        normal = _Y_AXIS * side
        eye = Vector((0.185, side * 0.04, _GULL_ZC + 0.078))
        slab(b, ccw(_disc(eye, 0.009, 4, math.pi / 4), normal), normal * 0.006, C['tinta'])
    tail = [(-0.2, -0.035, _GULL_ZC), (-0.31, -0.065, _GULL_ZC - 0.008), (-0.325, 0.0, _GULL_ZC - 0.01),
            (-0.31, 0.065, _GULL_ZC - 0.008), (-0.2, 0.035, _GULL_ZC)]
    slab(b, ccw(tail, (0, 0, 1)), (0.0, 0.0, 0.008), white, rim=grey)


def _gull_wing(b: MeshBuilder, side: float) -> None:
    grey, white, black = C['acero'], C['papel'], C['tinta']
    pts = [(x, side * y, _GULL_ZC + z) for x, y, z, _, _ in _GULL_WING]
    radii = [(ru, rv) for _, _, _, ru, rv in _GULL_WING]
    two_tone = [grey if math.cos(math.pi / 6 + math.pi * j / 3 + math.pi / 6) > 0.0 else white for j in range(6)]
    colors: list[int | list[int]] = [two_tone, two_tone, two_tone, black]
    tip = (-0.2, side * 0.585, _GULL_ZC + 0.012)
    sweep(b, pts, radii, 6, colors, phase=math.pi / 6, cap_start=white, cap_end=black, tips=(None, tip))
    for k in range(2):
        base = Vector((-0.09 - 0.03 * k, side * (0.43 + 0.04 * k), _GULL_ZC + 0.065 - 0.015 * k))
        end = Vector((-0.2 - 0.025 * k, side * (0.5 + 0.03 * k), _GULL_ZC + 0.03 - 0.01 * k))
        across = Vector((0.0, side * 0.012, 0.0))
        feather = [base - across, end, base + across, (base + end) / 2 + Vector((0.012, 0.0, 0.0))]
        slab(b, ccw(feather, (0, 0, 1)), (0.0, 0.0, 0.006), black)


def build_fauna_gaviota() -> MeshBuilder:
    b = MeshBuilder()
    _gull_body(b)
    for side in (1.0, -1.0):
        _gull_wing(b, side)
    return b
