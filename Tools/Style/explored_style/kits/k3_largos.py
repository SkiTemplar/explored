"""Piezas largas o pesadas del kit K3: cañas (tope 300), tronco, remo y
lente de Fresnel (tope 800).

Las piezas largas yacen a lo largo de X con el centro en el origen; la lente
se tiene de pie sobre su cuna con la cara hacia -Y.
"""

from __future__ import annotations

import math

from mathutils import Vector

from explored_style.kits.k3_base import AX_NEG_Y, AX_X, C, box, ccw, end_face, move_verts
from explored_style.meshkit import MeshBuilder, lathe, slab, sweep_path

# ---------------------------------------------------------------------------
# Caña de bambú: 1,4 m con tres nudos, bocas huecas, un extremo cortado en
# bisel y un brote con dos hojas.
# ---------------------------------------------------------------------------
_BAMBU_L = 1.4
_BAMBU_R = 0.026


def _node_profile(length: float, radius: float, nodes: tuple[float, ...], bulge: float = 1.14,
                  half: float = 0.012) -> list[tuple[float, float]]:
    """Perfil (radio, h) de un tallo con nudos abultados y bocas anulares."""
    pts = [(radius * 0.65, 0.0), (radius, 0.0)]
    for h in nodes:
        pts += [(radius, h - half), (radius * bulge, h), (radius, h + half)]
    return pts + [(radius, length), (radius * 0.65, length)]


def build_cana_bambu() -> MeshBuilder:
    b = MeshBuilder()
    nodes = (0.36, 0.71, 1.06)
    profile = _node_profile(_BAMBU_L, _BAMBU_R, nodes)
    cane, node, cut = C['verde_lima'], C['hierba_clara'], C['fibra']
    colors = [cut, cane] + [node, node, cane] * len(nodes) + [cut]
    start = Vector((-_BAMBU_L / 2, 0.0, _BAMBU_R * 1.14))
    rings = lathe(b, profile, 8, colors, origin=start, axis=AX_X, phase=math.pi / 8,
                  cap_bottom=C['madera_oscura'], cap_top=C['madera_oscura'], tips=(0.04, _BAMBU_L - 0.04))
    axis_z = start.z
    for ring in rings[-2:]:
        move_verts(b, ring, lambda _k, co: (0.07 * (co.z - axis_z) / _BAMBU_R, 0.0, 0.0))
    leaf, vein = C['hierba_clara'], C['hierba_oscura']
    base = Vector((start.x + nodes[1] + 0.01, 0.0, axis_z + _BAMBU_R * 0.9))
    for direction, length, lift in (((0.75, 0.66), 0.24, 0.06), ((-0.35, 0.94), 0.2, 0.045)):
        d = Vector((direction[0], direction[1], 0.0))
        side = Vector((-d.y, d.x, 0.0)) * 0.03
        tip = base + d * length
        tip.z = 0.012
        mid = base + d * (length * 0.4) + Vector((0.0, 0.0, lift))
        slab(b, ccw([base, mid + side, tip, mid - side], (0, 0, 1)), (0.0, 0.0, 0.004), leaf, rim=vein)
    return b


# ---------------------------------------------------------------------------
# Caña de pescar: vara de bambú curvada con nudos, puño encordado, sedal y
# flotador rojo y blanco apoyado en la arena.
# ---------------------------------------------------------------------------
_ROD_L = 1.7


def _rod_point(t: float) -> Vector:
    return Vector((-_ROD_L / 2 + _ROD_L * t, 0.0, 0.018 + 0.22 * t ** 2.4))


def _rod_radius(t: float) -> float:
    return 0.016 + (0.0055 - 0.016) * t


def build_cana_pescar() -> MeshBuilder:
    b = MeshBuilder()
    ts, radii, colors = [0.0], [_rod_radius(0.0)], []
    for node in (0.4, 0.6, 0.8):
        for dt, bulge in ((-0.01, 1.0), (0.0, 1.18), (0.01, 1.0)):
            ts.append(node + dt)
            radii.append(_rod_radius(node + dt) * bulge)
        colors += [C['verde_lima'], C['hierba_clara'], C['hierba_clara']]
    ts.append(1.0)
    radii.append(_rod_radius(1.0))
    colors.append(C['verde_lima'])
    pts = [_rod_point(t) for t in ts]
    sweep_path(b, pts, radii, 6, colors, cap_start=C['madera_clara'], cap_end=C['verde_lima'])
    grip_t = (0.02, 0.08, 0.14, 0.2)
    sweep_path(b, [_rod_point(t) for t in grip_t], [_rod_radius(t) * 1.35 for t in grip_t], 6,
          [C['fibra'], C['madera_oscura'], C['fibra']], cap_start=C['fibra'], cap_end=C['fibra'])
    tip = _rod_point(1.0)
    float_at = Vector((tip.x + 0.14, 0.03, 0.0))
    line = [tip, Vector((tip.x + 0.1, 0.015, tip.z * 0.45)), float_at + Vector((0.0, 0.0, 0.046))]
    sweep_path(b, line, [0.0022] * 3, 3, C['tinta'], cap_start=C['tinta'], cap_end=C['tinta'])
    bobber = [(0.004, 0.0), (0.016, 0.01), (0.019, 0.02), (0.015, 0.03), (0.004, 0.04)]
    lathe(b, bobber, 6, [C['rojo_faro'], C['rojo_faro'], C['papel'], C['papel']], origin=float_at,
          cap_bottom=C['rojo_faro'], cap_top=C['papel'], tips=(0.0, 0.05))
    return b


# ---------------------------------------------------------------------------
# Tronco de palma: 2,5 m tumbado, con segmentos en copa como SM_PalmA, raíz
# ensanchada y cortes con anillos.
# ---------------------------------------------------------------------------
_LOG_L = 2.5
_LOG_SIDES = 8
_LOG_SEGMENTS = 12


def _log_radius(t: float) -> float:
    return 0.19 + (0.145 - 0.19) * t


def _log_center(t: float, radius: float) -> Vector:
    return Vector((-_LOG_L / 2 + _LOG_L * t, 0.07 * math.sin(math.pi * t), radius))


def build_tronco() -> MeshBuilder:
    b = MeshBuilder()
    stations: list[tuple[float, float]] = [(0.0, 0.225), (0.03, 0.2)]
    colors: list[int] = [C['madera_oscura']]
    t0 = 0.03
    weights = [1.0 + 0.22 * math.sin(2.7 * k + 0.4) for k in range(_LOG_SEGMENTS)]
    scale = (0.985 - t0) / sum(weights)
    t_next = t0
    for k, weight in enumerate(weights):
        t_next += weight * scale
        t_cup = t_next - 0.011
        swell = 1.0 + 0.035 * math.sin(1.9 * k)
        stations += [(t_cup, _log_radius(t_cup) * swell), (t_next, _log_radius(t_next) * 0.93)]
        colors += [C['madera_media'], C['madera_oscura']]
    stations.append((1.0, _log_radius(1.0) * 0.95))
    colors.append(C['madera_media'])
    # la raíz ensanchada comparte la altura del segundo anillo: así el corte
    # queda perpendicular al eje y no inclinado por la diferencia de radios
    pts = [_log_center(t, r if t > 0.0 else stations[1][1]) for t, r in stations]
    rings = sweep_path(b, pts, [r for _, r in stations], _LOG_SIDES, colors, phase=math.pi / _LOG_SIDES)
    for idx, ring in enumerate(rings[1:-1], start=1):
        move_verts(b, ring, lambda k, _co, i=idx: (0.0, 0.0, 0.006 * math.sin(2.3 * k + 1.7 * i)))
    wood = [C['madera_clara'], C['madera_media']]
    end_face(b, rings[0], (0.72, 0.42), wood, C['madera_clara'], at_start=True)
    end_face(b, rings[-1], (0.72, 0.42), wood, C['madera_clara'], at_start=False)
    return b


# ---------------------------------------------------------------------------
# Remo: pala lenticular con nervio, caña redonda, empuñadura en T, puño de
# cuerda y ligadura de fibra en el cuello.
# ---------------------------------------------------------------------------
_OAR_BLADE = ((0.12, 0.017, 0.017), (0.18, 0.014, 0.045), (0.26, 0.012, 0.072), (0.36, 0.011, 0.088),
              (0.47, 0.011, 0.092), (0.58, 0.0105, 0.087), (0.66, 0.0095, 0.07), (0.71, 0.008, 0.045))


def build_remo() -> MeshBuilder:
    b = MeshBuilder()
    shaft = (-0.7, 0.019, 0.019)
    stations = (shaft, *_OAR_BLADE)
    pts = [(x, 0.0, 0.02) for x, _, _ in stations]
    radii = [(ru, rv) for _, ru, rv in stations]
    top = [C['madera_clara'] if k in (0, 1, 6, 7) else C['madera_media'] for k in range(8)]
    colors = [C['madera_media'], *([top] * (len(stations) - 2))]
    sweep_path(b, pts, radii, 8, colors, cap_start=C['madera_media'], cap_end=C['madera_clara'],
          tips=(None, (0.75, 0.0, 0.02)))
    grip = C['madera_oscura']
    sweep_path(b, [(-0.72, -0.07, 0.02), (-0.72, 0.07, 0.02)], [0.018, 0.018], 6, grip, up=(1.0, 0.0, 0.0),
          cap_start=grip, cap_end=grip, tips=((-0.72, -0.085, 0.02), (-0.72, 0.085, 0.02)))
    wrap = [(-0.66, 0.0, 0.02), (-0.6, 0.0, 0.02), (-0.54, 0.0, 0.02), (-0.48, 0.0, 0.02)]
    sweep_path(b, wrap, [0.0225] * 4, 8, [C['fibra'], C['madera_oscura'], C['fibra']], cap_start=C['fibra'],
          cap_end=C['fibra'])
    sweep_path(b, [(0.07, 0.0, 0.02), (0.11, 0.0, 0.02), (0.15, 0.0, 0.02)], [0.0215, 0.023, 0.0205], 8,
          C['fibra'], cap_start=C['fibra'], cap_end=C['fibra'])
    return b


# ---------------------------------------------------------------------------
# Lente de Fresnel: Ø 0,5 m, aro de latón, tres anillos escalonados y cúpula
# central, de pie en una cuna de madera.
# ---------------------------------------------------------------------------
_LENS_R = 0.25
_LENS_SIDES = 24
_CRADLE_H = 0.05


def _lens_profile() -> tuple[list[tuple[float, float]], list[int]]:
    brass, glass, step = C['laton'], C['cristal'], C['agua_somera']
    pts = [(0.2, 0.0), (_LENS_R, 0.0), (_LENS_R, 0.05), (0.215, 0.05), (0.21, 0.038), (0.21, 0.012)]
    colors = [brass] * 5
    lo, hi = 0.012, 0.03
    edges = (0.21, 0.165, 0.12, 0.075)
    for r_in in edges[1:]:
        pts += [(r_in, hi), (r_in, lo)]
        colors += [glass, step]
    pts.append((0.04, 0.034))
    colors.append(glass)
    return pts, colors


def build_lente() -> MeshBuilder:
    b = MeshBuilder()
    center_z = _LENS_R + _CRADLE_H - 0.02
    pts, colors = _lens_profile()
    lathe(b, pts, _LENS_SIDES, colors, origin=Vector((0.0, 0.025, center_z)), axis=AX_NEG_Y,
          phase=math.pi / _LENS_SIDES, cap_bottom=C['cristal'], cap_top=C['cristal'], tips=(0.0, 0.044))
    box(b, (-0.17, -0.06, 0.0), (0.17, 0.11, _CRADLE_H), C['madera_oscura'])
    for x in (-0.13, 0.13):
        box(b, (x - 0.018, -0.04, _CRADLE_H - 0.005), (x + 0.018, 0.09, _CRADLE_H + 0.03), C['hierro'])
    return b
