"""
albatros.py — hidrocanoa «Albatros» (chárter de los años 70 que se estrella
en el prólogo) y sus 4 restos narrativos, esparcidos por el archipiélago para
la baliza de rescate.

Pasada de arte 2026-10-17: se rehace el kit entero (la versión anterior eran
cilindros y cajas desnudas). Casco-fuselaje de hidrocanoa con redan,
ventanillas, cabina y puerta; ala alta con flotadores de punta y alerones;
dos motores radiales; cola con deriva, timón, estabilizadores y matrícula
F-OALB. Librea blanca y roja desgastada (parches de aluminio desnudo y
óxido), metal oscuro en motor y bancada.

- SM_Albatros_Full: el avión intacto (Z = 0 en la quilla).
- SM_Albatros_Fuselage_Wreck: fuselaje partido por detrás de la puerta, con
  chapa abollada, puerta abierta y muñón del ala en el techo (~8 m).
- SM_Albatros_Wing_Wreck: ala de estribor arrancada en el encastre, con
  flotador, alerón caído y el muñón de la góndola del motor (~6 m).
- SM_Albatros_Tail_Wreck: cono de cola con deriva, timón, estabilizadores
  (uno doblado) y matrícula (~3 m).
- SM_Albatros_Engine_Wreck: motor radial de 9 cilindros sobre su bancada y
  cortafuegos, con la hélice de 3 palas doblada (~1,8 m).

Los restos llevan el origen en la base (Z = 0 suelo, centrados en XY). Color
por vértice escrito por cara sobre M_Metal / M_Glass, como el resto del kit.
"""

import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _albatros_shape as A  # noqa: E402
import _prims as P  # noqa: E402
import _shapes as S  # noqa: E402
import common as C  # noqa: E402

CATEGORY = 'albatros'

VARIANTS = [
    dict(name='Albatros_Full', seed=1001, builder='full', tri_budget=(1000, 4000), wear=0.12,
         needs_collision=True, collision_complex=True,
         bbox_cm=dict(x=(1000.0, 1250.0), y=(1200.0, 1400.0), z=(250.0, 420.0)),
         z_range_cm=(0.0, 420.0)),
    dict(name='Albatros_Fuselage_Wreck', seed=1002, builder='fuselage_wreck',
         tri_budget=(1000, 4000), needs_collision=True, collision_complex=True,
         bbox_cm=dict(x=(700.0, 900.0)), z_range_cm=(0.0, 400.0)),
    dict(name='Albatros_Wing_Wreck', seed=1003, builder='wing_wreck',
         tri_budget=(1000, 4000), needs_collision=True,
         bbox_cm=dict(y=(520.0, 680.0)), z_range_cm=(0.0, 400.0)),
    dict(name='Albatros_Tail_Wreck', seed=1004, builder='tail_wreck',
         tri_budget=(1000, 4000), needs_collision=True,
         bbox_cm=dict(x=(260.0, 380.0)), z_range_cm=(0.0, 400.0)),
    dict(name='Albatros_Engine_Wreck', seed=1005, builder='engine_wreck',
         tri_budget=(1000, 4000), needs_collision=True,
         z_range_cm=(0.0, 220.0)),
]

SLOTS = ['M_Metal', 'M_Glass']
REGISTRATION = 'F-OALB'
ROUND = 60.0
HARD = 38.0

_BUILDERS = {}


def _register(key):
    def deco(fn):
        _BUILDERS[key] = fn
        return fn
    return deco


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    A.WEAR[0] = variant.get('wear', 1.0)
    return _BUILDERS[variant['builder']](variant, rnd)


def _solid(obj, rgb, rnd, angle=HARD, slot='M_Metal', bevel=0.0, jitter=0.02):
    P.solid(obj, SLOTS, slot, rgb, rnd=rnd, jitter=jitter)
    if bevel > 0.0:
        S.bevel_obj(obj, width=bevel, segments=1, limit_angle_deg=35.0)
    return P.smooth(obj, angle)


def _finish(parts, name):
    obj = C.join_objects(parts, name)
    C.add_basic_uv(obj)
    return obj


# ---------------------------------------------------------------------------
# Fuselaje
# ---------------------------------------------------------------------------

def _fuselage_face(xs, tag, rnd, wear, door_open):
    """Color de una cara exterior del fuselaje según su tramo y segmento."""
    span, seg = tag[1], tag[2]
    x0, x1 = xs[span], xs[span + 1]
    cat = A.RING_CATS[seg]
    port = seg >= 8
    cockpit = A.in_span(x0, x1, [A.COCKPIT])
    if cockpit and (cat == 'window' or seg in (1, 14)):
        return 'M_Glass', A.GLASS
    if A.in_span(x0, x1, [(4.0, 4.4)]) and seg in (0, 15):
        return 'M_Glass', A.GLASS
    if cat == 'window' and A.in_span(x0, x1, A.WINDOW_SPANS):
        return 'M_Glass', A.GLASS
    if door_open and port and cat in ('window', 'cheat', 'side') and A.in_span(x0, x1, [A.DOOR]):
        return 'M_Metal', A.INTERIOR
    base = dict(top=A.WHITE, window=A.WHITE, cheat=A.RED, side=A.WHITE,
                chine=A.RED, bottom=A.RED_DARK)[cat]
    if 0.5 * (x0 + x1) > 5.1:
        base = A.RED
    return 'M_Metal', A.worn(base, rnd, bare=0.12 * wear, rust=0.05 * wear)


def _paint_fuselage(obj, tags, xs, rnd, wear, door_open):
    def face(poly):
        tag = tags[poly.index]
        if tag[0] == 'outer':
            return _fuselage_face(xs, tag, rnd, wear, door_open)
        if tag[0] == 'cap_out':
            return 'M_Metal', A.RED
        if tag[0] == 'rim':
            return 'M_Metal', A.BARE
        return 'M_Metal', A.INTERIOR
    P.paint(obj, SLOTS, face)
    return P.smooth(obj, 40.0)


def _door(rnd, open_deg):
    ring = A.hull_ring(-0.6)
    z_lo, z_hi = ring[4].z + 0.02, ring[2].z - 0.02
    w = A.hull_half_width(-0.6)
    x0, x1 = A.DOOR
    pts = [(x1, z_lo), (x0, z_lo), (x0, z_hi), (x1, z_hi)]
    door, _ = P.slab('Door', pts, 0.04, P.xz(-(w + 0.025)))
    A.rotate_mesh(door, open_deg, 'Z', pivot=(x1, -(w + 0.025), 0.0))
    return _solid(door, A.worn(A.WHITE, rnd, bare=0.3), rnd, bevel=0.012)


# ---------------------------------------------------------------------------
# Superficies: ala, cola, flotadores
# ---------------------------------------------------------------------------

def _wing_loft(name, ys, rnd, tip_red_from=6.05, torn_root=False, cuts=(A.AILERON,),
               dents=0.0):
    """Ala por estaciones de envergadura: recorta el borde de salida en los
    tramos `cuts` (hueco de alerón y flap), punta redondeada y, en el resto,
    abolladuras de `dents` metros en el extradós."""
    rings = []
    for i, y in enumerate(ys):
        ay = abs(y)
        cut = 0.75 if any(a <= ay <= b for a, b in cuts) else 1.0
        scale = 0.55 if ay >= A.WING_SPAN - 1e-6 else 1.0
        ring = A.wing_ring(y, cut=cut, scale=scale)
        if dents and 0 < i < len(ys) - 3:
            ring = [p - Vector((0.0, 0.0, rnd.uniform(0.0, dents))) if k in (2, 3, 4)
                    and rnd.random() < 0.4 else p for k, p in enumerate(ring)]
        rings.append(ring)
    if torn_root:
        rings[0] = [p + Vector((0.0, rnd.uniform(-0.18, 0.18), rnd.uniform(-0.02, 0.02)))
                    for p in rings[0]]
    obj, tags = P.loft(name, rings)

    def face(poly):
        tag = tags[poly.index]
        if tag[0] == 'cap0' and torn_root:
            return 'M_Metal', A.INTERIOR
        if tag[0] != 'side':
            return 'M_Metal', A.RED
        y_mid = abs(0.5 * (ys[tag[1]] + ys[tag[1] + 1]))
        upper = tag[2] in A.AIRFOIL_UPPER
        if y_mid >= tip_red_from:
            base = A.RED
        elif upper:
            base = A.RED if tag[2] in (5, 6) and y_mid > 1.0 else A.WHITE
        else:
            base = (0.62, 0.62, 0.62)
        return 'M_Metal', A.worn(base, rnd, bare=0.10, rust=0.04)
    P.paint(obj, SLOTS, face)
    return P.smooth(obj, HARD)


def _aileron(y0, y1, angle, rnd):
    obj, _ = P.loft('Aileron', [A.aileron_ring(y0, angle), A.aileron_ring(y1, angle)])
    return _solid(obj, A.worn(A.WHITE, rnd, bare=0.2), rnd)


def _float(y, rnd):
    obj, tags = P.loft('Float', A.float_rings(y))
    P.paint(obj, SLOTS, lambda p: ('M_Metal', A.worn(
        A.RED if tags[p.index][0] == 'side' and tags[p.index][2] in range(4, 8) else A.WHITE,
        rnd, bare=0.12, rust=0.05)))
    parts = [P.smooth(obj, 50.0)]
    wz = A.wing_z(y) - 0.035 * A.wing_chord(y)
    pylon, _ = P.slab('Pylon', [(1.15, 1.22), (1.95, 1.22), (1.80, wz + 0.02), (1.30, wz + 0.02)],
                      0.06, P.xz(y))
    parts.append(_solid(pylon, A.WHITE, rnd, bevel=0.01))
    strut, _ = P.sweep('FloatStrut', [(2.05, y, 1.22), (1.6, y - 0.9 * math.copysign(1, y), wz + 0.03)],
                       0.025, sides=6)
    parts.append(_solid(strut, A.GREY, rnd, angle=ROUND))
    return parts


def _cut_profile(prof, cut):
    """Recorta un perfil 2D en xn = cut y quita los puntos alineados con sus
    vecinos: en las tapas (n-gonos) un punto alineado acaba en un triángulo
    de área cero al triangular el FBX."""
    pts = [(min(a, cut), b) for a, b in prof]
    changed = True
    while changed and len(pts) > 3:
        changed = False
        for i in range(len(pts)):
            a, b, c = pts[i - 1], pts[i], pts[(i + 1) % len(pts)]
            cross = (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])
            if abs(cross) < 1e-9 or a == b:
                pts.pop(i)
                changed = True
                break
    return pts


FIN_PROFILE = _cut_profile(
    [(1.0, 0.0), (0.88, 0.6), (0.75, 1.0), (0.45, 1.0), (0.12, 1.0), (0.03, 0.6),
     (0.0, 0.0), (0.03, -0.6), (0.12, -1.0), (0.45, -1.0), (0.75, -1.0), (0.88, -0.6)], 0.72)
STAB_PROFILE = _cut_profile(A.AIRFOIL, 0.72)


def _foil_v(le_x, chord, z, scale=1.0, half_t=0.045):
    """Perfil vertical de la deriva (simétrico, caras planas en el centro
    para que la matrícula asiente), recortado donde empieza el timón."""
    ctr = Vector((le_x - 0.4 * chord, 0.0, z))
    return [ctr + (Vector((le_x - xn * chord, yn * half_t, z)) - ctr) * scale
            for xn, yn in FIN_PROFILE]


def _tail_surfaces(rnd, rudder_deg, elev_deg, bent_deg=0.0):
    """Deriva + timón + estabilizadores + timones de profundidad + matrícula,
    en coordenadas del avión."""
    parts = []
    fin_st = [(1.55, -3.70, 1.60), (2.40, -4.05, 1.35), (3.00, -4.40, 1.10), (3.20, -4.55, 0.95)]
    rings = []
    for i, (z, le, c) in enumerate(fin_st):
        rings.append(_foil_v(le, c, z, scale=0.6 if i == len(fin_st) - 1 else 1.0))
    fin, tags = P.loft('Fin', rings)
    P.paint(fin, SLOTS, lambda p: ('M_Metal', A.worn(
        A.RED if (tags[p.index][0] == 'side' and tags[p.index][1] == 2) or tags[p.index][0] == 'cap1'
        else A.WHITE, rnd, bare=0.10, rust=0.04)))
    parts.append(P.smooth(fin, HARD))

    rud = []
    for z, le, c in ((1.75, -3.7 - (0.2 / 0.85) * 0.35, 1.55), (3.1, -4.48, 1.02)):
        hinge = Vector((le - 0.72 * c, 0.0, z))
        pts = [Vector((le - 0.72 * c, 0.04, z)), Vector((le - c, 0.0, z)),
               Vector((le - 0.72 * c, -0.04, z))]
        rud.append([hinge + (Matrix.Rotation(math.radians(rudder_deg), 3, 'Z') @ (p - hinge))
                    for p in pts])
    rudder, _ = P.loft('Rudder', rud)
    parts.append(_solid(rudder, A.worn(A.RED, rnd, bare=0.15), rnd))

    for side in (-1.0, 1.0):
        st = []
        for y, le, c in ((0.12, -4.30, 0.95), (1.45, -4.55, 0.62)):
            zz = 1.78
            ring = [Vector((le - xn * c, side * y, zz + zn * c * 0.75))
                    for xn, zn in STAB_PROFILE]
            st.append(ring)
        if side > 0 and bent_deg:
            st[1] = [p + Vector((rnd.uniform(-0.05, 0.05), rnd.uniform(-0.12, 0.05), 0.0))
                     for p in st[1]]
        stab, tags = P.loft('Stab', st)
        P.paint(stab, SLOTS, lambda p: ('M_Metal', A.worn(A.WHITE, rnd, bare=0.12, rust=0.05)))
        elev_rings = []
        for y, le, c in ((0.14, -4.30, 0.95), (1.40, -4.55, 0.62)):
            hinge = Vector((le - 0.72 * c, side * y, 1.78))
            pts = [Vector((le - 0.72 * c, side * y, 1.78 + 0.03 * c)),
                   Vector((le - c, side * y, 1.78)),
                   Vector((le - 0.72 * c, side * y, 1.78 - 0.02 * c))]
            elev_rings.append([hinge + (Matrix.Rotation(math.radians(elev_deg), 3, 'Y') @ (p - hinge))
                               for p in pts])
        elev, _ = P.loft('Elevator', elev_rings)
        P.solid(elev, SLOTS, 'M_Metal', A.worn(A.RED, rnd, bare=0.15), rnd=rnd)
        pair = [P.smooth(stab, HARD), P.smooth(elev, HARD)]
        if side > 0 and bent_deg:
            for o in pair:
                A.rotate_mesh(o, bent_deg, 'X', pivot=(-4.4, 0.12, 1.78))
        parts += pair

    zc, c_mid = 2.45, 1.33
    xc = -3.70 - 0.35 * (zc - 1.55) / 0.85 - 0.42 * c_mid
    for side in (-1.0, 1.0):
        reg = A.text_mesh('Registration', REGISTRATION, 0.20)
        A.rotate_mesh(reg, 90.0, 'X')
        if side > 0:
            A.rotate_mesh(reg, 180.0, 'Z')
        P.translate_mesh(reg, (xc, side * 0.049, zc))
        parts.append(_solid(reg, A.DARK, rnd, jitter=0.0))
    return parts


# ---------------------------------------------------------------------------
# Motor radial y hélice
# ---------------------------------------------------------------------------

def _propeller(rnd, x, centre, bends):
    """Buje + 3 palas en el plano YZ a la cota x; bends = [(rad, dir, r)]."""
    parts = []
    hub, _ = P.sweep('Hub', [(x - 0.10, centre.y, centre.z), (x + 0.06, centre.y, centre.z),
                             (x + 0.16, centre.y, centre.z)], [0.13, 0.12, 0.03], sides=10)
    parts.append(_solid(hub, A.GREY, rnd, angle=ROUND))
    for k, (bend, bdir, radius) in enumerate(bends):
        bl, tags = A.blade('Blade', radius=radius, bend=bend, bend_dir=bdir)
        n_sp = max(t[1] for t in tags if t[0] == 'side')
        P.paint(bl, SLOTS, lambda p, t=tags, n=n_sp: (
            'M_Metal', A.YELLOW if (t[p.index][0] == 'cap1' or
                                    (t[p.index][0] == 'side' and t[p.index][1] == n)) else A.DARK),
            rnd=rnd, jitter=0.015)
        A.rotate_mesh(bl, 120.0 * k + 15.0, 'X')
        P.translate_mesh(bl, (x, centre.y, centre.z))
        parts.append(P.smooth(bl, 50.0))
    return parts


def _radial_engine(rnd):
    """Motor radial de 9 cilindros con su bancada, eje +X, en el origen."""
    parts = []
    case, _ = P.sweep('Crankcase', [(-0.28, 0, 0), (-0.05, 0, 0), (0.10, 0, 0), (0.30, 0, 0)],
                      [0.24, 0.30, 0.27, 0.13], sides=14)
    parts.append(_solid(case, A.GREY, rnd, angle=ROUND))
    for k in range(9):
        a = 2.0 * math.pi * k / 9.0
        d = Vector((0.0, math.cos(a), math.sin(a)))
        c0 = Vector((-0.06, 0.0, 0.0))
        rs = [0.26, 0.31, 0.36, 0.41, 0.46, 0.51, 0.56, 0.62]
        rad = [0.085, 0.10, 0.085, 0.10, 0.085, 0.10, 0.09, 0.06]
        cyl, tags = P.sweep('Cylinder', [c0 + d * r for r in rs], rad, sides=8,
                            up=(1.0, 0.0, 0.0))
        P.paint(cyl, SLOTS, lambda p, t=tags: ('M_Metal', A.BRONZE if t[p.index][0] != 'side'
                                               or t[p.index][1] >= 5 else A.DARK),
                rnd=rnd, jitter=0.02)
        parts.append(P.smooth(cyl, 50.0))
    ring = [(-0.24, 0.47 * math.cos(2 * math.pi * i / 14), 0.47 * math.sin(2 * math.pi * i / 14))
            for i in range(14)]
    exhaust, _ = P.sweep('Exhaust', ring, 0.04, sides=5, closed=True, up=(1.0, 0.0, 0.0))
    parts.append(_solid(exhaust, A.RUST, rnd, angle=ROUND))
    mount = [(-0.40, 0.33 * math.cos(2 * math.pi * i / 10), 0.33 * math.sin(2 * math.pi * i / 10))
             for i in range(10)]
    mring, _ = P.sweep('MountRing', mount, 0.03, sides=4, closed=True, up=(1.0, 0.0, 0.0))
    parts.append(_solid(mring, A.DARK, rnd, angle=ROUND))
    for sy, sz in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        a = Vector((-0.40, sy * 0.233, sz * 0.233))
        b = Vector((-0.95, sy * 0.42, sz * 0.42))
        tube, _ = P.sweep('MountTube', [a, b], 0.028, sides=6)
        parts.append(_solid(tube, A.DARK, rnd, angle=ROUND))
    wall = []
    for i in range(10):
        a = 2.0 * math.pi * (i + 0.5) / 10
        r = 0.50 * rnd.uniform(0.85, 1.08)
        wall.append((r * math.cos(a), r * math.sin(a)))
    fw, _ = P.slab('Firewall', wall, 0.035, lambda a, b, d: Vector((-0.98 + d, a, b)))
    parts.append(_solid(fw, A.GREY, rnd, bevel=0.01))
    return parts


def _cowl_band(rnd, a0=0.6, a1=3.9, n=10):
    """Resto del capó: banda curvada abollada, roja y blanca."""
    rings = []
    for i in range(n):
        a = a0 + (a1 - a0) * i / (n - 1)
        r = 0.68 + rnd.uniform(-0.05, 0.03)
        cs, sn = math.cos(a), math.sin(a)
        sec = [(-0.16, r), (0.20, r - 0.05), (0.20, r - 0.03), (-0.16, r + 0.02)]
        rings.append([Vector((x, rr * cs, rr * sn)) for x, rr in sec])
    obj, tags = P.loft('CowlBand', rings)
    P.paint(obj, SLOTS, lambda p: ('M_Metal', A.worn(A.RED, rnd, bare=0.25, rust=0.1)))
    return P.smooth(obj, HARD)


# ---------------------------------------------------------------------------
# Builders
# ---------------------------------------------------------------------------

@_register('full')
def _build_full(variant, rnd):
    xs = A.STATIONS + [5.58]
    rings = [A.hull_ring(x) for x in A.STATIONS] + [A.nose_tip_ring()]
    tail_tip = [Vector((-5.62, p.y * 0.45, A.hull_centre_z(-5.5) + (p.z - A.hull_centre_z(-5.5)) * 0.45))
                for p in rings[0]]
    rings = [tail_tip] + rings
    xs = [-5.62] + xs
    obj, raw = P.loft('Fuselage', rings)
    tags = [('outer', t[1], t[2]) if t[0] == 'side' else ('cap_out',) for t in raw]
    parts = [_paint_fuselage(obj, tags, xs, rnd, wear=0.15, door_open=False)]

    ys = [6.6, 6.45, 6.11, 6.1, 5.3, 4.5, 3.8, 3.79, 3.2, 2.4, 1.6, 0.0]
    ys = [-y for y in ys] + ys[::-1][1:]
    parts.append(_wing_loft('Wing', ys, rnd))
    for side in (-1.0, 1.0):
        parts.append(_aileron(side * 3.83, side * 6.07, 0.0, rnd))
        parts += _float(side * 5.4, rnd)
        y = side * 2.2
        zc = A.wing_z(2.2) + 0.02
        cowl, tags = P.sweep('Nacelle', [(3.05, y, zc), (2.90, y, zc), (2.45, y, zc),
                                         (1.60, y, zc - 0.04), (0.80, y, zc - 0.02)],
                             [0.30, 0.40, 0.42, 0.33, 0.10], sides=12)
        P.paint(cowl, SLOTS, lambda p, t=tags: ('M_Metal', A.DARK if t[p.index][0] == 'cap0' else (
            A.RED if t[p.index][0] == 'side' and t[p.index][1] == 0 else A.WHITE)), rnd=rnd,
            jitter=0.015)
        parts.append(P.smooth(cowl, 50.0))
        parts += _propeller(rnd, 3.12, Vector((0.0, y, zc)), [(0.0, (-1, 0, 0), 0.95)] * 3)
    parts += _tail_surfaces(rnd, rudder_deg=0.0, elev_deg=0.0)
    return _finish(parts, 'SM_' + variant['name'])


@_register('fuselage_wreck')
def _build_fuselage_wreck(variant, rnd):
    xs = [x for x in A.STATIONS if x >= A.X_BREAK] + [5.58]
    rings = [A.hull_ring(x) for x in xs[:-1]] + [A.nose_tip_ring()]
    rings[0] = [p + Vector((rnd.uniform(-0.35, 0.30), 0.0, 0.0)) for p in rings[0]]
    rings[0] = A.dent(rings[0], A.hull_centre_z, rnd, prob=0.6, depth=0.12)
    for i in range(1, len(rings) - 1):
        crash = 1.0 if xs[i] > 4.4 or i < 3 else 0.0
        rings[i] = A.dent(rings[i], A.hull_centre_z, rnd, prob=0.15 + 0.45 * crash,
                          depth=0.035 + 0.08 * crash)
    shell, tags = A.shell('Fuselage', rings, 0.045, A.hull_centre_z, break_at_start=True)
    parts = [_paint_fuselage(shell, tags, xs, rnd, wear=1.0, door_open=True)]
    parts.append(_door(rnd, 105.0))

    ys = [-1.35, -0.85, 0.0, 0.85, 1.35]
    stub = [A.wing_ring(y) for y in ys]
    for i in (0, -1):
        stub[i] = [p + Vector((0.0, rnd.uniform(-0.15, 0.15), rnd.uniform(-0.03, 0.03)))
                   for p in stub[i]]
    wing, wtags = P.loft('WingStub', stub)
    P.paint(wing, SLOTS, lambda p: ('M_Metal', A.INTERIOR if wtags[p.index][0] != 'side'
                                    else A.worn(A.WHITE, rnd, bare=0.15, rust=0.06)))
    parts.append(P.smooth(wing, HARD))

    obj = _finish(parts, 'SM_' + variant['name'])
    A.rotate_mesh(obj, -14.0, 'X')
    A.rotate_mesh(obj, 4.0, 'Y')
    A.rotate_mesh(obj, 8.0, 'Z')
    return A.ground_and_center(obj)


@_register('tail_wreck')
def _build_tail_wreck(variant, rnd):
    xs = [x for x in A.STATIONS if x <= A.X_BREAK]
    rings = [A.hull_ring(x) for x in xs]
    cz = A.hull_centre_z(-5.5)
    tip = [Vector((-5.62, p.y * 0.45, cz + (p.z - cz) * 0.45)) for p in rings[0]]
    rings = [tip] + rings
    xs = [-5.62] + xs
    rings[-1] = [p + Vector((rnd.uniform(-0.30, 0.35), 0.0, 0.0)) for p in rings[-1]]
    rings[-1] = A.dent(rings[-1], A.hull_centre_z, rnd, prob=0.6, depth=0.10)
    for i in range(1, len(rings) - 1):
        rings[i] = A.dent(rings[i], A.hull_centre_z, rnd, prob=0.3, depth=0.05)
    shell, tags = A.shell('TailCone', rings, 0.04, A.hull_centre_z, break_at_start=False)
    parts = [_paint_fuselage(shell, tags, xs, rnd, wear=1.0, door_open=False)]
    parts += _tail_surfaces(rnd, rudder_deg=-18.0, elev_deg=12.0, bent_deg=-24.0)
    obj = _finish(parts, 'SM_' + variant['name'])
    A.rotate_mesh(obj, 20.0, 'X')
    A.rotate_mesh(obj, -6.0, 'Y')
    return A.ground_and_center(obj)


@_register('wing_wreck')
def _build_wing_wreck(variant, rnd):
    ys = [0.9, 1.15, 1.16, 1.6, 2.0, 2.2, 2.4, 2.8, 3.2, 3.55, 3.56, 3.79, 3.8, 4.0, 4.2,
          4.6, 5.0, 5.4, 5.8, 6.1, 6.11, 6.3, 6.45, 6.6]
    flap = (1.16, 3.55)
    parts = [_wing_loft('Wing', ys, rnd, torn_root=True, cuts=(A.AILERON, flap), dents=0.025)]
    parts.append(_aileron(3.83, 6.07, 24.0, rnd))
    parts.append(_aileron(1.19, 3.52, 38.0, rnd))  # flap colgando, medio arrancado
    parts += _float(5.4, rnd)
    for xn in (0.25, 0.62):
        c, le = A.wing_chord(0.9), A.wing_le_x(0.9)
        spar = C.make_box('Spar', (0.06, 0.55, 0.12 - 0.05 * xn),
                            center=(le - xn * c, 0.75, A.wing_z(0.9) + 0.01))
        parts.append(_solid(spar, A.BARE, rnd))
    zc = A.wing_z(2.2) + 0.02
    nac, ntags = P.sweep('NacelleStub', [(2.70, 2.2, zc), (2.45, 2.2, zc), (1.60, 2.2, zc - 0.04),
                                         (0.80, 2.2, zc - 0.02)], [0.36, 0.42, 0.33, 0.10], sides=12)
    P.paint(nac, SLOTS, lambda p: ('M_Metal', A.INTERIOR if ntags[p.index][0] == 'cap0'
                                   else A.worn(A.WHITE, rnd, bare=0.2, rust=0.08)))
    parts.append(P.smooth(nac, 50.0))
    parts += _torn_skin(rnd, 0.9)
    light, _ = P.sweep('NavLight', [(1.30, 6.55, A.wing_z(6.6)), (1.10, 6.62, A.wing_z(6.6))],
                       [0.05, 0.02], sides=6)
    parts.append(_solid(light, (0.10, 0.60, 0.20), rnd, slot='M_Glass', jitter=0.0))
    obj = _finish(parts, 'SM_' + variant['name'])
    A.rotate_mesh(obj, 14.8, 'X')
    A.rotate_mesh(obj, 12.0, 'Z')
    return A.ground_and_center(obj)


def _torn_skin(rnd, y_root, n=6):
    """Jirones de chapa arrancada que asoman del encastre roto del ala."""
    ring = A.wing_ring(y_root)
    parts = []
    for k in rnd.sample(range(1, 13), n):
        p0, p1 = ring[k], ring[k + 1]
        up = Vector((0.0, 0.0, 0.012 if k < 7 else -0.012))
        reach = Vector((rnd.uniform(-0.08, 0.08), -rnd.uniform(0.18, 0.40),
                        rnd.uniform(0.05, 0.22) * (1 if k < 7 else -1)))
        p3, p2 = p0 + reach, p1 + reach * rnd.uniform(0.5, 0.9)
        shard, _ = P.loft('Shard', [[p0, p1, p1 + up, p0 + up], [p3, p2, p2 + up, p3 + up]])
        parts.append(_solid(shard, A.BARE if rnd.random() < 0.5 else A.WHITE, rnd))
    return parts


@_register('engine_wreck')
def _build_engine_wreck(variant, rnd):
    parts = _radial_engine(rnd)
    parts.append(_cowl_band(rnd))
    bends = [(1.25, (-1.0, 0.0, 0.0), 0.92), (0.45, (-0.6, 0.8, 0.0), 0.95),
             (-0.7, (-1.0, 0.0, 0.0), 0.78)]
    parts += _propeller(rnd, 0.40, Vector((0.0, 0.0, 0.0)), bends)
    obj = _finish(parts, 'SM_' + variant['name'])
    A.rotate_mesh(obj, -28.0, 'Y')
    A.rotate_mesh(obj, 12.0, 'X')
    A.rotate_mesh(obj, 25.0, 'Z')
    return A.ground_and_center(obj)
