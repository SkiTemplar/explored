"""Mallas piloto del estilo de Baliza Siete (GDD v3 §11, fase F2).

Tres piezas que fijan el lenguaje de formas antes de producir los kits:

- SM_PalmA (palmera, tope 1.500 tris): tronco curvado en «plátano» con
  segmentos en copa, raíz ensanchada, corona, 3 cocos y 7 hojas arqueadas con
  foliolos en diente de sierra.
- SM_Cuchillo (objeto de mano, tope 300): hoja con lomo, bisel y punta
  curvada; guarda de latón y mango de madera con pomo.
- SM_BarrilLluvia (estructura, tope 800): 12 duelas con alturas irregulares,
  panza, 2 aros de hierro, agua dentro y grifo.

Todo es determinista (sin aleatoriedad): volver a ejecutar regenera los
mismos FBX. Uso: `uv run style-pilotos [--out Art/Export/Style]`.
"""

from __future__ import annotations

import argparse
import json
import math
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

import bpy  # noqa: F401  (debe importarse antes que mathutils)

from mathutils import Matrix, Vector

from explored_style import meshkit
from explored_style.meshkit import MeshBuilder, cross, lathe
from explored_style.palette import REPO_ROOT, load_palette

EXPORT_DIR = REPO_ROOT / 'Art' / 'Export' / 'Style'

_PAL = load_palette()
C = {c.name: c.index for c in _PAL.colors}

Z_AXIS = Vector((0.0, 0.0, 1.0))
Y_AXIS = Vector((0.0, 1.0, 0.0))


@dataclass(frozen=True)
class PilotSpec:
    name: str
    budget: str
    build: Callable[[], MeshBuilder]


# ---------------------------------------------------------------------------
# SM_PalmA
# ---------------------------------------------------------------------------
_TRUNK_P = (Vector((0.0, 0.0, 0.0)), Vector((0.95, 0.0, 2.3)), Vector((1.4, 0.0, 5.0)))
_TRUNK_SEGMENTS = 10
_TRUNK_SIDES = 8


def _bezier(t: float) -> Vector:
    p0, p1, p2 = _TRUNK_P
    return p0 * (1 - t) ** 2 + p1 * (2 * (1 - t) * t) + p2 * (t * t)


def _bezier_tangent(t: float) -> Vector:
    p0, p1, p2 = _TRUNK_P
    return ((p1 - p0) * (2 * (1 - t)) + (p2 - p1) * (2 * t)).normalized()


def _trunk_frame(t: float) -> tuple[Vector, Vector, Vector]:
    """(u, v, n) del anillo en t: horizontal en la raíz (apoya plano en el
    suelo) y perpendicular al tronco a partir del primer segmento."""
    blend = min(1.0, t / 0.12)
    n = Z_AXIS.lerp(_bezier_tangent(t), blend).normalized()
    u = cross(Y_AXIS, n).normalized()
    return u, cross(n, u), n


def _trunk_ring(b: MeshBuilder, t: float, radius: float) -> list[int]:
    u, v, _ = _trunk_frame(t)
    return b.ring(_bezier(t), u, v, radius, _TRUNK_SIDES, phase=math.pi / _TRUNK_SIDES)


def _trunk_radius(t: float) -> float:
    return 0.24 + (0.15 - 0.24) * t


def _palm_trunk(b: MeshBuilder) -> None:
    root = _trunk_ring(b, 0.0, 0.33)
    b.cap(root, _bezier(0.0), C['madera_oscura'], flip=True)
    t0, ledge = 0.045, 0.007
    prev = _trunk_ring(b, t0, _trunk_radius(t0) * 0.82)
    b.bridge(root, prev, C['madera_oscura'])
    span = (1.0 - t0) / _TRUNK_SEGMENTS
    for k in range(_TRUNK_SEGMENTS):
        t_top = t0 + (k + 1) * span - ledge
        cup = _trunk_ring(b, t_top, _trunk_radius(t_top))
        b.bridge(prev, cup, C['madera_media'])
        t_next = t0 + (k + 1) * span
        nxt = _trunk_ring(b, t_next, _trunk_radius(t_next) * 0.8)
        b.bridge(cup, nxt, C['madera_oscura'])
        prev = nxt
    _, _, n = _trunk_frame(1.0)
    top = _bezier(1.0)
    u, v, _ = _trunk_frame(1.0)
    crown = b.ring(top + n * 0.16, u, v, 0.2, _TRUNK_SIDES, phase=math.pi / _TRUNK_SIDES)
    b.bridge(prev, crown, C['hierba_oscura'])
    b.cap(crown, top + n * 0.34, C['verde_palma'])


def _palm_coconuts(b: MeshBuilder) -> None:
    _, _, n = _trunk_frame(1.0)
    u, v, _ = _trunk_frame(1.0)
    center = _bezier(1.0) + n * 0.02
    r = 0.13
    profile = [(r * 0.62, -r * 0.8), (r, -r * 0.1), (r * 0.7, r * 0.72)]
    for k, ang in enumerate((math.radians(a) for a in (200.0, 320.0, 80.0))):
        radial = u * math.cos(ang) + v * math.sin(ang)
        pos = center + radial * 0.2 - n * (0.06 + 0.03 * k)
        axis = radial.to_track_quat('Z', 'Y').to_matrix().to_4x4()
        lathe(b, profile, 6, [C['madera_oscura']] * 2, origin=pos, axis=axis,
              cap_bottom=C['madera_oscura'], cap_top=C['madera_oscura'])


@dataclass(frozen=True)
class Frond:
    azimuth_deg: float
    length: float
    lift: float      # pendiente inicial (sube al salir de la corona)
    droop: float     # caída hacia la punta
    width: float     # semiancho máximo de la hoja
    teeth: int = 5


_FRONDS = (
    Frond(5.0, 2.8, 0.6, 1.25, 0.42),
    Frond(55.0, 2.45, 0.9, 1.1, 0.38),
    Frond(105.0, 2.9, 0.5, 1.2, 0.44),
    Frond(158.0, 2.7, 0.65, 1.3, 0.42),
    Frond(210.0, 2.6, 0.75, 1.15, 0.4),
    Frond(260.0, 2.85, 0.55, 1.2, 0.44),
    Frond(312.0, 2.25, 1.15, 0.95, 0.36),
)


def _frond_stations(f: Frond) -> list[tuple[float, float]]:
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


def _palm_frond(b: MeshBuilder, f: Frond) -> None:
    _, _, n = _trunk_frame(1.0)
    ang = math.radians(f.azimuth_deg)
    a = Vector((math.cos(ang), math.sin(ang), 0.0))
    side = cross(Z_AXIS, a)
    root = _bezier(1.0) + n * 0.22 + a * 0.1

    def spine(s: float) -> Vector:
        return root + a * (f.length * s * (1.0 - 0.18 * s)) + Z_AXIS * (f.length * (f.lift * s - f.droop * s * s))

    top, under = C['verde_palma'], C['hierba_oscura']
    rows = []
    for s, w in _frond_stations(f):
        p = spine(s)
        fold = Z_AXIS * (0.38 * w)
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


def build_palm() -> MeshBuilder:
    b = MeshBuilder()
    _palm_trunk(b)
    _palm_coconuts(b)
    for frond in _FRONDS:
        _palm_frond(b, frond)
    return b


# ---------------------------------------------------------------------------
# SM_Cuchillo (tumbado sobre el plano XY, punta hacia +X, pivote bajo la guarda)
# ---------------------------------------------------------------------------
_BLADE_X = (0.0, 0.035, 0.07, 0.1, 0.122, 0.14)
_BLADE_SPINE_Y = (0.012, 0.012, 0.0115, 0.0105, 0.009, 0.0065)
_BLADE_EDGE_Y = (-0.014, -0.0145, -0.0135, -0.011, -0.0065, -0.001)
_BLADE_TIP = Vector((0.162, 0.004, 0.0))
_BLADE_HALF_T = 0.0036


def _knife_blade(b: MeshBuilder) -> None:
    flat, bevel, spine = C['acero'], C['papel'], C['roca_media']
    rings = []
    for x, ys, ye in zip(_BLADE_X, _BLADE_SPINE_Y, _BLADE_EDGE_Y, strict=True):
        yb = ye + 0.4 * (ys - ye)
        rings.append([b.vert((x, ys, _BLADE_HALF_T)), b.vert((x, yb, _BLADE_HALF_T * 0.8)), b.vert((x, ye, 0.0)),
                      b.vert((x, yb, -_BLADE_HALF_T * 0.8)), b.vert((x, ys, -_BLADE_HALF_T))])
    colors = [flat, bevel, bevel, flat, spine]
    for r0, r1 in zip(rings, rings[1:], strict=False):
        b.bridge(r0, r1, colors)
    b.cap(rings[0], (_BLADE_X[0], 0.0, 0.0), spine, flip=True)
    tip, last = b.vert(_BLADE_TIP), rings[-1]
    for k in range(len(last)):
        b.face((last[k], last[(k + 1) % len(last)], tip), colors[k])


def build_knife() -> MeshBuilder:
    b = MeshBuilder()
    _knife_blade(b)
    to_y = Matrix.Rotation(-math.pi / 2, 4, 'X')
    guard = [(0.0055, -0.024), (0.0085, -0.019), (0.0085, 0.019), (0.0055, 0.024)]
    lathe(b, guard, 6, [C['laton']] * 3, origin=Vector((-0.004, 0.0, 0.0)), axis=to_y,
          cap_bottom=C['laton'], cap_top=C['laton'])
    to_minus_x = Matrix.Rotation(-math.pi / 2, 4, 'Y')
    handle = [(0.0095, 0.0), (0.0115, 0.012), (0.0128, 0.05), (0.0112, 0.085), (0.0138, 0.093),
              (0.0138, 0.104), (0.0085, 0.113)]
    wood, brass = C['madera_oscura'], C['laton']
    lathe(b, handle, 8, [wood, wood, wood, brass, brass, brass], origin=Vector((-0.009, 0.0, 0.0)),
          axis=to_minus_x, phase=math.pi / 8, cap_bottom=wood, cap_top=brass)
    return b


# ---------------------------------------------------------------------------
# SM_BarrilLluvia
# ---------------------------------------------------------------------------
_STAVES = 12
_STAVE_Z = (0.0, 0.2, 0.47, 0.75)
_STAVE_TOP = 0.95
_STAVE_TOP_JITTER = (0.0, 0.018, -0.008, 0.026, 0.004, -0.012, 0.014, 0.0, 0.022, -0.006, 0.01, 0.03)
_STAVE_T = 0.035
_STAVE_GAP = 0.011  # rad de hueco a cada lado de la duela


def _barrel_radius(z: float) -> float:
    """Panza: radio máximo a media altura, menor en los extremos."""
    half = _STAVE_TOP / 2
    return 0.295 + 0.055 * (1.0 - ((z - half) / half) ** 2)


def _polyline_radius(z: float) -> float:
    """Radio del perfil poligonal real de las duelas (interpolado entre
    estaciones), para que los aros abracen la malla y no la curva ideal."""
    zs = (*_STAVE_Z, _STAVE_TOP)
    for z0, z1 in zip(zs, zs[1:], strict=False):
        if z0 <= z <= z1:
            f = (z - z0) / (z1 - z0)
            return _barrel_radius(z0) + (_barrel_radius(z1) - _barrel_radius(z0)) * f
    return _barrel_radius(z)


def _polar(r: float, a: float, z: float) -> Vector:
    return Vector((r * math.cos(a), r * math.sin(a), z))


def _barrel_stave(b: MeshBuilder, k: int) -> None:
    a0 = 2 * math.pi * k / _STAVES + _STAVE_GAP
    a1 = 2 * math.pi * (k + 1) / _STAVES - _STAVE_GAP
    color = C['madera_clara'] if k % 2 == 0 else C['madera_media']
    zs = (*_STAVE_Z, _STAVE_TOP + _STAVE_TOP_JITTER[k])
    rows = []
    for z in zs:
        r = _barrel_radius(z)
        rows.append([b.vert(_polar(r, a0, z)), b.vert(_polar(r, a1, z)),
                     b.vert(_polar(r - _STAVE_T, a1, z)), b.vert(_polar(r - _STAVE_T, a0, z))])
    for r0, r1 in zip(rows, rows[1:], strict=False):
        b.face((r0[0], r0[1], r1[1], r1[0]), color)
        b.face((r0[2], r0[3], r1[3], r1[2]), color)
        b.face((r0[3], r0[0], r1[0], r1[3]), color)
        b.face((r0[1], r0[2], r1[2], r1[1]), color)
    o0, o1, i1, i0 = rows[-1]
    b.face((o0, o1, i1, i0), C['madera_clara'])
    o0, o1, i1, i0 = rows[0]
    b.face((o0, i0, i1, o1), color)


def _barrel_hoop(b: MeshBuilder, z0: float, z1: float) -> None:
    r_in0, r_in1 = _polyline_radius(z0) - 0.006, _polyline_radius(z1) - 0.006
    r_out0, r_out1 = r_in0 + 0.026, r_in1 + 0.026
    profile = [(r_in0, z0), (r_out0, z0), (r_out1, z1), (r_in1, z1)]
    iron = C['hierro']
    lathe(b, profile, _STAVES, [iron] * 4, closed=True)


def build_barrel() -> MeshBuilder:
    b = MeshBuilder()
    for k in range(_STAVES):
        _barrel_stave(b, k)
    _barrel_hoop(b, 0.11, 0.18)
    _barrel_hoop(b, 0.78, 0.85)
    r_water = _barrel_radius(0.84) - _STAVE_T - 0.004
    lathe(b, [(r_water, 0.82), (r_water, 0.845)], _STAVES, [C['agua_media']],
          phase=math.pi / _STAVES, cap_bottom=C['agua_media'], cap_top=C['agua_media'])
    # grifo hacia -Y (frente de la lámina): caño de madera y llave de latón
    spout_z, front = 0.3, -math.pi / 2
    r_body = _polyline_radius(spout_z)
    to_front = Matrix.Rotation(math.pi / 2, 4, 'X')
    origin = _polar(r_body - 0.02, front, spout_z)
    lathe(b, [(0.04, 0.0), (0.04, 0.075), (0.03, 0.13)], 6, [C['madera_oscura']] * 2, origin=origin,
          axis=to_front, cap_bottom=C['madera_oscura'], cap_top=C['hierro'])
    key = _polar(r_body + 0.06, front, spout_z + 0.02)
    lathe(b, [(0.017, 0.0), (0.017, 0.07), (0.028, 0.085)], 6, [C['laton']] * 2, origin=key,
          cap_bottom=C['laton'], cap_top=C['laton'])
    return b


PILOTS: tuple[PilotSpec, ...] = (
    PilotSpec('SM_PalmA', 'palmera', build_palm),
    PilotSpec('SM_Cuchillo', 'objeto_mano', build_knife),
    PilotSpec('SM_BarrilLluvia', 'estructura', build_barrel),
)


def generate(spec: PilotSpec) -> bpy.types.Object:
    return meshkit.build_object(spec.name, spec.build())


def generate_all(out_dir: Path) -> list[dict]:
    """Genera y exporta los pilotos; devuelve las entradas del manifiesto."""
    out_dir.mkdir(parents=True, exist_ok=True)
    entries = []
    for spec in PILOTS:
        meshkit.reset_scene()
        obj = generate(spec)
        path = out_dir / f'{spec.name}.fbx'
        meshkit.export_fbx(obj, str(path))
        dims = obj.dimensions
        entries.append({
            'malla': spec.name, 'fbx': path.name, 'presupuesto': spec.budget,
            'tris': meshkit.triangle_count(obj),
            'dimensiones_m': [round(d, 3) for d in dims],
            'colores': sorted({_PAL.colors[i].name for i in meshkit.face_colors(obj)}),
            'origen': 'generado por Tools/Style/explored_style/pilots.py (sin fuentes externas)',
            'licencia': 'propia del proyecto Explored',
        })
        print(f'[pilotos] {spec.name}: {entries[-1]["tris"]} tris -> {path}')
    manifest = {'generador': 'uv run style-pilotos', 'mallas': entries}
    (out_dir / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n',
                                           encoding='utf-8')
    return entries


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog='style-pilotos', description='Genera y exporta las mallas piloto.')
    parser.add_argument('--out', type=Path, default=EXPORT_DIR)
    args = parser.parse_args(argv)
    generate_all(args.out)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
