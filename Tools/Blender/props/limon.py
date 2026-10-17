"""
limon.py — «Limón», el barco final del juego (kit de 2 mallas).

- SM_Limon: balandro de madera de ~7,3 m terminado. Casco amarillo limón con
  franja crema, obra viva verde hoja, cubierta de tablones, caseta, mástil con
  cangreja y foque crema, timón de rueda, farol de popa y salvavidas en el
  espejo. Origen en crujía a la altura de la flotación (Z = 0), proa +X.
- SM_Limon_Frame: el mismo casco a medio construir sobre una grada en la
  arena: quilla y roda, cuadernas, algunas tablas, vagras, baos, caballetes
  y puntales. Origen en el suelo (Z = 0), proa +X.

La forma del casco vive en _limon_shape.py y la comparten las dos mallas. El
color va por vértice (escrito por cara) sobre los materiales estables del kit
de props, como el resto de Tools/Blender/props.
"""

import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _limon_shape as H  # noqa: E402
import _shapes as S  # noqa: E402
import common as C  # noqa: E402

CATEGORY = 'boats'

VARIANTS = [
    dict(name='Limon', seed=2601, builder='boat', tri_budget=(1500, 4000),
         needs_collision=True, collision_complex=True,
         bbox_cm=dict(x=(650.0, 750.0), y=(200.0, 240.0), z=(600.0, 720.0)),
         z_range_cm=(-60.0, 700.0)),
    dict(name='Limon_Frame', seed=2602, builder='frame', tri_budget=(800, 3000),
         needs_collision=True, collision_complex=True,
         bbox_cm=dict(x=(600.0, 750.0), y=(200.0, 340.0), z=(150.0, 260.0)),
         z_range_cm=(0.0, 260.0)),
]

SLOTS_BOAT = ['M_Wood', 'M_Fabric', 'M_Metal', 'M_Glass']
SLOTS_FRAME = ['M_Wood', 'M_Fabric', 'M_Metal']

# Colores lineales (el atributo «Col» se exporta en lineal).
LEMON = (0.98, 0.70, 0.04)
CREAM = (0.90, 0.80, 0.56)
CREAM_PAINT = (0.86, 0.78, 0.58)
LEAF = (0.12, 0.36, 0.06)
CORAL = (0.86, 0.20, 0.10)
DECK_TONES = [(0.62, 0.42, 0.20), (0.55, 0.36, 0.16), (0.67, 0.47, 0.24)]
WOOD_RAIL = (0.50, 0.31, 0.14)
WOOD_MID = (0.56, 0.37, 0.17)
WOOD_DARK = (0.40, 0.24, 0.11)
WOOD_FRESH = (0.76, 0.56, 0.31)
WOOD_PALE = (0.70, 0.52, 0.30)
SAIL = (0.88, 0.80, 0.60)
ROPE = (0.64, 0.46, 0.20)
IRON = (0.09, 0.09, 0.10)
BRASS = (0.80, 0.52, 0.14)
GLASS_AMBER = (1.00, 0.62, 0.18)
GLASS_PORT = (0.50, 0.76, 0.86)

N_STATIONS = 18
N_DECK = 6
X_MAST = 0.85
ROUND = 65.0   # ángulo de sombreado suave de piezas redondas
HARD = 35.0    # ángulo de cajas, tablas y casco

_BUILDERS = {}


def _register(key):
    def deco(fn):
        _BUILDERS[key] = fn
        return fn
    return deco


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    return _BUILDERS[variant['builder']](variant, rnd)


def _part(obj, slots, slot, rgb, rnd, angle=HARD, bevel=0.0, jitter=0.02):
    H.solid(obj, slots, slot, rgb, rnd=rnd, jitter=jitter)
    if bevel > 0.0:
        S.bevel_obj(obj, width=bevel, segments=1, limit_angle_deg=35.0)
    return H.smooth(obj, angle)


def _finish(parts, name):
    obj = C.join_objects(parts, name)
    C.add_basic_uv(obj)
    return obj


def _keel_stem_path(z_drop=0.0, stem_out=0.015, top_extra=0.08):
    """Quilla de popa a proa y roda lanzada hasta sobrepasar la regala."""
    path = []
    for i in range(11):
        t = i / 10.0
        k = H.keel(t)
        path.append((H.x_at(t, k), 0.0, k - z_drop))
    k1, s1 = H.keel(1.0), H.sheer(1.0)
    for frac in (0.25, 0.5, 0.75, 1.0):
        z = k1 + (s1 - k1) * frac
        path.append((H.x_at(1.0, z) + stem_out, 0.0, z))
    z = s1 + top_extra
    path.append((H.x_at(1.0, z) + stem_out, 0.0, z))
    return path


# ---------------------------------------------------------------------------
# SM_Limon — casco
# ---------------------------------------------------------------------------

def _outer_side(t, side):
    """Puntos del costado de regala a quilla (sin la quilla) y la categoría
    de cada segmento hacia el siguiente punto, bajando."""
    us = [(1.0, 'band'), (H.u_below_sheer(t, 0.07), 'stripe'),
          (H.u_below_sheer(t, 0.18), 'topside')]
    us.append((0.5 + 0.45 * (us[2][0] - 0.5), 'topside'))
    us += [(0.5, 'bottom'), (0.36, 'bottom'), (0.22, 'bottom'), (0.10, 'bottom')]
    return [(H.hull_point(t, u, side), cat) for u, cat in us]


def _hull_ring(t):
    """Anillo cerrado de la sección: cubierta (estribor a babor), regala de
    babor, costado de babor, quilla, costado de estribor y regala de
    estribor. Devuelve (puntos, categoría del segmento i -> i+1)."""
    s, bi = H.sheer(t), H.inner_half_beam(t)
    pts, cats = [], []
    for j in range(N_DECK + 1):
        y = bi * (1.0 - 2.0 * j / N_DECK)
        z = H.deck_z(t, y)
        pts.append(Vector((H.x_at(t, z), y, z)))
        cats.append(('deck', j))
    cats[-1] = ('inner',)
    pts.append(Vector((H.x_at(t, s), -bi, s)))
    cats.append(('rail',))
    port = _outer_side(t, -1)
    for p, cat in port:
        pts.append(p)
        cats.append((cat,))
    pts.append(H.hull_point(t, 0.0, 1))
    cats.append(('bottom',))
    stbd = _outer_side(t, 1)
    for i in range(len(stbd) - 1, -1, -1):
        pts.append(stbd[i][0])
        cats.append((stbd[i - 1][1],) if i > 0 else ('rail',))
    pts.append(Vector((H.x_at(t, s), bi, s)))
    cats.append(('inner',))
    return pts, cats


def _hull(rnd):
    rings, cats = [], None
    for i in range(N_STATIONS):
        pts, cats = _hull_ring(i / (N_STATIONS - 1))
        rings.append(pts)
    obj, tags = H.loft('Hull', rings)
    colors = dict(inner=CREAM_PAINT, rail=WOOD_RAIL, band=LEMON, stripe=CREAM,
                  topside=LEMON, bottom=LEAF)

    def face(poly):
        tag = tags[poly.index]
        if tag[0] != 'side':
            return 'M_Wood', LEMON
        span, seg = tag[1], tag[2]
        cat = cats[seg]
        if cat[0] == 'deck':
            j = cat[1]
            return 'M_Wood', DECK_TONES[(j * 2 + (span + 2 * j) // 4) % 3]
        return 'M_Wood', colors[cat[0]]

    H.paint(obj, SLOTS_BOAT, face, rnd=rnd, jitter=0.012)
    S.bevel_obj(obj, width=0.014, segments=1, limit_angle_deg=40.0)
    return H.smooth(obj, HARD)


def _keel_parts(rnd):
    keel, _ = H.sweep('KeelStrip', _keel_stem_path(z_drop=0.012), 0.07, sides=4,
                      roll=math.pi / 4.0, up=(0.0, 0.0, 1.0))
    _part(keel, SLOTS_BOAT, 'M_Wood', WOOD_MID, rnd, bevel=0.01)

    top = []
    for i in range(7):
        x = -1.75 + 3.15 * i / 6.0
        top.append((x, H.keel(H.t_of_x(x)) + 0.05))
    bottom = [(1.15, -0.27), (0.85, -0.47), (0.45, -0.52), (-1.20, -0.52), (-1.55, -0.40)]
    fin, _ = H.slab('Fin', top + bottom, 0.10, H.xz())
    _part(fin, SLOTS_BOAT, 'M_Wood', LEAF, rnd, bevel=0.02)

    k0, s0 = H.keel(0.0), H.sheer(0.0)

    def x_tr(z):
        return H.X_STERN + H.TRANSOM_RAKE * max(0.0, (z - k0)) / (s0 - k0)
    rudder_pts = [(x_tr(0.66) - 0.02, 0.66), (x_tr(0.66) - 0.15, 0.66),
                  (x_tr(0.25) - 0.17, 0.25), (x_tr(-0.05) - 0.36, -0.05),
                  (x_tr(-0.38) - 0.34, -0.40), (H.X_STERN - 0.08, -0.45),
                  (x_tr(-0.20) - 0.02, -0.20)]
    rudder, _ = H.slab('Rudder', rudder_pts, 0.06, H.xz())
    _part(rudder, SLOTS_BOAT, 'M_Wood', WOOD_DARK, rnd, bevel=0.015)

    pintles = []
    for z in (0.05, 0.45):
        p = C.make_box('Pintle', (0.20, 0.09, 0.04), center=(x_tr(z) - 0.06, 0.0, z))
        pintles.append(_part(p, SLOTS_BOAT, 'M_Metal', IRON, rnd))
    return [keel, fin, rudder] + pintles


# ---------------------------------------------------------------------------
# SM_Limon — caseta, timón, farol y cubierta
# ---------------------------------------------------------------------------

def _cabin(rnd):
    x0, x1, hw, hw_top, zb, zt = -0.85, 0.50, 0.62, 0.56, 0.30, 0.84

    def ring(x, front_rake):
        sec = [(-hw, zb), (-hw_top, zt), (hw_top, zt), (hw, zb)]
        return [Vector((x + front_rake * (z - zb) / (zt - zb), y, z)) for y, z in sec]
    body, _ = H.loft('Cabin', [ring(x0, 0.0), ring(x1, -0.10)])
    _part(body, SLOTS_BOAT, 'M_Wood', CREAM_PAINT, rnd, bevel=0.015)

    w = hw_top + 0.07

    def roof_ring(x, dx_top):
        top = [(-w + 2.0 * w * i / 6.0) for i in range(7)]
        pts = [Vector((x + dx_top, y, zt + 0.075 * (1.0 - (y / w) ** 2))) for y in top]
        pts += [Vector((x + dx_top, y, zt - 0.045 + 0.075 * (1.0 - (y / w) ** 2)))
                for y in reversed(top)]
        return pts
    roof, _ = H.loft('Roof', [roof_ring(x0 - 0.07, 0.0), roof_ring(x1 + 0.07, -0.10)])
    _part(roof, SLOTS_BOAT, 'M_Wood', WOOD_MID, rnd, bevel=0.012)

    door = C.make_box('Door', (0.04, 0.38, 0.40), center=(x0 - 0.01, 0.0, 0.60))
    knob = C.make_box('Knob', (0.04, 0.035, 0.035), center=(x0 - 0.035, 0.12, 0.60))
    parts = [body, roof, _part(door, SLOTS_BOAT, 'M_Wood', WOOD_DARK, rnd, bevel=0.01),
             _part(knob, SLOTS_BOAT, 'M_Metal', BRASS, rnd)]

    y_wall = hw + (hw_top - hw) * (0.6 - zb) / (zt - zb)
    for x in (-0.50, 0.12):
        for side in (-1.0, 1.0):
            port, tags = H.sweep('Porthole', [(x, side * (y_wall - 0.03), 0.6),
                                              (x, side * (y_wall + 0.025), 0.6)],
                                 0.075, sides=8, up=(0.0, 0.0, 1.0))
            H.paint(port, SLOTS_BOAT, lambda p, t=tags: (
                ('M_Glass', GLASS_PORT) if t[p.index][0] == 'cap1' else ('M_Metal', BRASS)))
            parts.append(H.smooth(port, HARD))
    return parts


def _wheel(rnd):
    xw = -2.55
    dz = H.deck_z(H.t_of_x(xw))
    zc, r_rim = dz + 0.78, 0.30
    ped = C.make_box('Pedestal', (0.16, 0.16, zc - dz + 0.12),
                     center=(xw + 0.14, 0.0, dz - 0.04 + (zc - dz + 0.12) / 2.0))
    parts = [_part(ped, SLOTS_BOAT, 'M_Wood', WOOD_MID, rnd, bevel=0.015)]
    ring = [(xw, r_rim * math.cos(2 * math.pi * i / 12), zc + r_rim * math.sin(2 * math.pi * i / 12))
            for i in range(12)]
    rim, _ = H.sweep('Rim', ring, 0.03, sides=5, closed=True, up=(1.0, 0.0, 0.0))
    parts.append(_part(rim, SLOTS_BOAT, 'M_Wood', WOOD_RAIL, rnd, angle=ROUND))
    for k in range(6):
        a = math.radians(30.0 + 60.0 * k)
        d = Vector((0.0, math.cos(a), math.sin(a)))
        c = Vector((xw, 0.0, zc))
        spoke, _ = H.sweep('Spoke', [c, c + d * (r_rim * 0.9), c + d * (r_rim + 0.07),
                                     c + d * (r_rim + 0.14)],
                           [0.02, 0.02, 0.034, 0.022], sides=5, up=(1.0, 0.0, 0.0))
        parts.append(_part(spoke, SLOTS_BOAT, 'M_Wood', WOOD_MID, rnd, angle=ROUND))
    hub, _ = H.sweep('Hub', [(xw - 0.05, 0.0, zc), (xw + 0.10, 0.0, zc)], 0.06, sides=8)
    parts.append(_part(hub, SLOTS_BOAT, 'M_Metal', BRASS, rnd, angle=ROUND))
    return parts


def _lantern(rnd):
    xl, yl = -2.90, -0.62
    z0 = H.deck_z(H.t_of_x(xl), yl) - 0.02
    post, _ = H.sweep('LanternPost', [(xl, yl, z0), (xl, yl, 1.10)], 0.035, sides=6)
    parts = [_part(post, SLOTS_BOAT, 'M_Wood', WOOD_MID, rnd, angle=ROUND)]
    specs = [('Base', 1.08, 1.13, 0.085, 0.085, 'M_Metal', IRON),
             ('Glass', 1.13, 1.29, 0.068, 0.068, 'M_Glass', GLASS_AMBER),
             ('Cap', 1.29, 1.37, 0.095, 0.025, 'M_Metal', IRON)]
    for name, za, zb, ra, rb, slot, rgb in specs:
        obj, _ = H.sweep('Lantern' + name, [(xl, yl, za), (xl, yl, zb)], [ra, rb], sides=6)
        parts.append(_part(obj, SLOTS_BOAT, slot, rgb, rnd, angle=HARD, jitter=0.0))
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.5
        bx, by = xl + 0.072 * math.cos(a), yl + 0.072 * math.sin(a)
        bar = C.make_box('LanternBar', (0.018, 0.018, 0.17), center=(bx, by, 1.21))
        parts.append(_part(bar, SLOTS_BOAT, 'M_Metal', IRON, rnd))
    loop = [(xl + 0.04 * math.cos(2 * math.pi * i / 8), yl,
             1.41 + 0.04 * math.sin(2 * math.pi * i / 8)) for i in range(8)]
    handle, _ = H.sweep('LanternHandle', loop, 0.009, sides=4, closed=True, up=(0.0, 1.0, 0.0))
    parts.append(_part(handle, SLOTS_BOAT, 'M_Metal', IRON, rnd, angle=ROUND))
    return parts


def _deck_props(rnd):
    parts = []
    xb, yb = -1.45, 0.50
    zb = H.deck_z(H.t_of_x(xb), yb) - 0.02
    hs = [0.0, 0.06, 0.10, 0.40, 0.44, 0.50]
    rs = [0.17, 0.185, 0.195, 0.195, 0.185, 0.17]
    barrel, tags = H.sweep('Barrel', [(xb, yb, zb + h) for h in hs], rs, sides=10)
    H.paint(barrel, SLOTS_BOAT, lambda p, t=tags: (
        ('M_Metal', IRON) if t[p.index][0] == 'side' and t[p.index][1] in (1, 3)
        else ('M_Wood', WOOD_PALE)), rnd=rnd, jitter=0.015)
    parts.append(H.smooth(barrel, ROUND))

    xc, yc = -1.35, -0.48
    zc = H.deck_z(H.t_of_x(xc), yc)
    crate = C.make_box('Crate', (0.40, 0.40, 0.34), center=(xc, yc, zc + 0.15))
    parts.append(_part(crate, SLOTS_BOAT, 'M_Wood', WOOD_FRESH, rnd, bevel=0.02))

    # salvavidas en el espejo (estribor), inclinado con el lanzamiento
    k0, s0 = H.keel(0.0), H.sheer(0.0)
    up = Vector((H.TRANSOM_RAKE, 0.0, s0 - k0)).normalized()
    out = Vector((-up.z, 0.0, up.x))  # hacia popa
    zc = 0.32
    centre = Vector((H.x_at(0.0, zc), 0.42, zc)) + out * 0.05
    ring = [centre + (Vector((0.0, 1.0, 0.0)) * math.cos(a) + up * math.sin(a)) * 0.17
            for a in (2 * math.pi * i / 12 for i in range(12))]
    buoy, tags = H.sweep('LifeRing', ring, 0.05, sides=5, closed=True, up=tuple(out))
    H.paint(buoy, SLOTS_BOAT, lambda p, t=tags: (
        'M_Fabric', CORAL if (t[p.index][1] // 3) % 2 == 0 else CREAM))
    parts.append(H.smooth(buoy, ROUND))
    return parts


# ---------------------------------------------------------------------------
# SM_Limon — aparejo
# ---------------------------------------------------------------------------

def _sail(name, tack, clew, head_luff, head_leech, belly, nu, nv, band_rows=()):
    def pfn(u, v):
        a = tack.lerp(head_luff, v)
        b = clew.lerp(head_leech, v)
        p = a.lerp(b, u)
        p.y += belly * math.sin(math.pi * (0.9 * u + 0.05)) * math.sin(math.pi * v) ** 0.8
        return p
    obj, tags = H.cloth(name, pfn, nu, nv, 0.014, (0.0, -1.0, 0.0))

    def face(poly):
        tag = tags[poly.index]
        if tag[0] != 'edge' and tag[2] in band_rows:
            return 'M_Fabric', LEMON
        return 'M_Fabric', SAIL if (tag[0] == 'edge' or tag[1] % 2 == 0) else (0.84, 0.75, 0.54)
    H.paint(obj, SLOTS_BOAT, face)
    return H.smooth(obj, HARD)


def _rope(a, b, rnd):
    obj, _ = H.sweep('Rope', [a, b], 0.011, sides=4)
    return _part(obj, SLOTS_BOAT, 'M_Fabric', ROPE, rnd, angle=ROUND)


def _rig(rnd):
    xm = X_MAST
    parts = []
    mast, _ = H.sweep('Mast', [(xm, 0.0, 0.05), (xm, 0.0, 3.0), (xm, 0.0, 6.24)],
                      [0.085, 0.078, 0.058], sides=8)
    truck, _ = H.sweep('Truck', [(xm, 0.0, 6.22), (xm, 0.0, 6.31)], [0.075, 0.06], sides=8)
    boom, _ = H.sweep('Boom', [(xm - 0.07, 0.0, 1.38), (-1.95, 0.0, 1.50)], [0.05, 0.04], sides=6)
    gaff, _ = H.sweep('Gaff', [(xm - 0.07, 0.0, 4.52), (-1.42, 0.0, 5.84)], [0.045, 0.034], sides=6)
    t_bs = H.t_of_x(2.55)
    bowsprit, _ = H.sweep('Bowsprit', [(2.55, 0.0, H.deck_z(t_bs) + 0.06), (3.82, 0.0, 0.99)],
                          [0.06, 0.04], sides=6)
    for obj, rgb in ((mast, WOOD_MID), (truck, WOOD_DARK), (boom, WOOD_RAIL),
                     (gaff, WOOD_RAIL), (bowsprit, WOOD_MID)):
        parts.append(_part(obj, SLOTS_BOAT, 'M_Wood', rgb, rnd, angle=ROUND))

    parts.append(_sail('MainSail', Vector((xm - 0.10, 0.0, 1.45)), Vector((-1.86, 0.0, 1.56)),
                       Vector((xm - 0.10, 0.0, 4.50)), Vector((-1.32, 0.0, 5.76)),
                       0.24, 5, 6, band_rows=(1,)))
    fs0, fs1 = Vector((xm + 0.06, 0.0, 5.0)), Vector((3.78, 0.0, 1.0))
    head = fs0.lerp(fs1, 0.12)
    clew = Vector((1.45, 0.0, 1.30))
    parts.append(_sail('Jib', fs0.lerp(fs1, 0.90), clew, head,
                       head + (clew - head).normalized() * 0.10, 0.16, 4, 5))

    t_sh = H.t_of_x(xm - 0.25)
    b_sh, s_sh = H.half_beam(t_sh), H.sheer(t_sh)
    lines = [(fs0, fs1), (Vector((xm - 0.04, 0.0, 6.18)), Vector((-1.92, 0.0, 1.52))),
             (Vector((xm - 0.04, 0.0, 6.12)), Vector((-0.30, 0.0, 5.20))),
             (Vector((-1.86, 0.0, 1.48)), Vector((-1.90, 0.0, H.deck_z(H.t_of_x(-1.9)) + 0.02))),
             (clew, Vector((1.25, 0.78, H.deck_z(H.t_of_x(1.25), 0.78) + 0.02)))]
    for side in (-1.0, 1.0):
        lines.append((Vector((xm, side * 0.05, 4.86)),
                      Vector((xm - 0.25, side * (b_sh + 0.01), s_sh - 0.02))))
        plate = C.make_box('Chainplate', (0.06, 0.025, 0.24),
                           center=(xm - 0.25, side * (b_sh + 0.012), s_sh - 0.10))
        parts.append(_part(plate, SLOTS_BOAT, 'M_Metal', IRON, rnd))
    parts += [_rope(a, b, rnd) for a, b in lines]

    pennant = [((xm - 0.02, 6.20), (xm - 0.02, 6.33), (0.45, 6.30), (0.45, 6.23), 'M_Fabric', LEAF),
               ((0.45, 6.23), (0.45, 6.30), (0.02, 6.265), None, 'M_Fabric', LEMON)]
    for *pts, slot, rgb in pennant:
        obj, _ = H.slab('Pennant', [p for p in pts if p is not None], 0.012, H.xz())
        parts.append(_part(obj, SLOTS_BOAT, slot, rgb, rnd, jitter=0.0))
    return parts


@_register('boat')
def _build_boat(variant, rnd):
    parts = [_hull(rnd)]
    parts += _keel_parts(rnd)
    parts += _cabin(rnd)
    parts += _wheel(rnd)
    parts += _lantern(rnd)
    parts += _deck_props(rnd)
    parts += _rig(rnd)
    return _finish(parts, 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# SM_Limon_Frame — el barco en grada
# ---------------------------------------------------------------------------

def _plank(t0, t1, u0, u1, side, thick):
    n = max(3, int((t1 - t0) / 0.10) + 1)
    rings = []
    for i in range(n):
        t = t0 + (t1 - t0) * i / (n - 1)
        a, b = H.hull_point(t, u0, side), H.hull_point(t, u1, side)
        na, nb = H.hull_normal(t, u0, side), H.hull_normal(t, u1, side)
        rings.append([a, b, b + nb * thick, a + na * thick])
    obj, _ = H.loft('Plank', rings)
    return obj


def _rib(t, half):
    """Cuaderna: sigue la piel por dentro (desplazada `half` hacia dentro)
    de regala a regala pasando por la quilla; las cabezas asoman 6 cm."""
    us = [1.0, 0.75, 0.5, 0.33, 0.16]
    path = []
    for side, seq in ((-1, us), (1, list(reversed(us)))):
        if side == 1:
            path.append(H.hull_point(t, 0.0, 1) + Vector((0.0, 0.0, half)))
        for u in seq:
            path.append(H.hull_point(t, u, side) - H.hull_normal(t, u, side) * half)
    head = Vector((0.0, 0.0, 0.06))
    path = [path[0] + head] + path + [path[-1] + head]
    obj, _ = H.sweep('Rib', path, half * math.sqrt(2.0), sides=4, roll=math.pi / 4.0,
                     up=(1.0, 0.0, 0.0))
    return obj


def _trestle(x, top_z, ground_z, rnd):
    parts = []
    beam = C.make_box('TrestleBeam', (0.14, 1.10, 0.12), center=(x, 0.0, top_z - 0.06))
    parts.append(_part(beam, SLOTS_FRAME, 'M_Wood', WOOD_MID, rnd, bevel=0.012))
    for sy in (-1.0, 1.0):
        for sx in (-1.0, 1.0):
            leg, _ = H.sweep('Leg', [(x + sx * 0.04, sy * 0.42, top_z - 0.08),
                                     (x + sx * 0.20, sy * 0.60, ground_z)],
                             0.045 * math.sqrt(2.0), sides=4, roll=math.pi / 4.0)
            parts.append(_part(leg, SLOTS_FRAME, 'M_Wood', WOOD_DARK, rnd))
        mid = ground_z + 0.45 * (top_z - ground_z)
        brace = C.make_box('Brace', (0.36, 0.04, 0.07), center=(x, sy * 0.53, mid))
        parts.append(_part(brace, SLOTS_FRAME, 'M_Wood', WOOD_MID, rnd))
    for sy in (-1.0, 1.0):
        dog = C.make_box('Dog', (0.05, 0.10, 0.10), center=(x, sy * 0.09, top_z + 0.02))
        parts.append(_part(dog, SLOTS_FRAME, 'M_Metal', IRON, rnd))
    return parts


@_register('frame')
def _build_frame(variant, rnd):
    keel_half = 0.06
    z_off = 0.45 + 0.38 + 2.0 * keel_half - 0.02
    ground = -z_off
    parts = []

    keel, _ = H.sweep('Keel', _keel_stem_path(z_drop=keel_half - 0.01, stem_out=0.02),
                      keel_half * math.sqrt(2.0), sides=4, roll=math.pi / 4.0)
    parts.append(_part(keel, SLOTS_FRAME, 'M_Wood', WOOD_DARK, rnd, bevel=0.012))

    for i in range(9):
        t = 0.10 + 0.80 * i / 8.0
        parts.append(_part(_rib(t, 0.035), SLOTS_FRAME, 'M_Wood',
                           WOOD_FRESH if i % 2 else WOOD_PALE, rnd))

    transom_pts = [H.hull_point(0.0, u, -1) for u in (1.0, 0.75, 0.5, 0.3, 0.12)]
    transom_pts += [H.hull_point(0.0, 0.0, 1)]
    transom_pts += [H.hull_point(0.0, u, 1) for u in (0.12, 0.3, 0.5, 0.75, 1.0)]
    transom, _ = H.loft('Transom', [transom_pts, [p + Vector((0.05, 0.0, 0.0)) for p in transom_pts]])
    parts.append(_part(transom, SLOTS_FRAME, 'M_Wood', WOOD_PALE, rnd, bevel=0.012))

    strakes = [(0.04, 0.93, 0.0, 0.17, -1, WOOD_PALE), (0.04, 0.93, 0.0, 0.17, 1, WOOD_PALE),
               (0.06, 0.88, 0.17, 0.31, -1, WOOD_FRESH), (0.20, 0.76, 0.31, 0.45, -1, WOOD_PALE),
               (0.12, 0.62, 0.17, 0.31, 1, WOOD_FRESH),
               (0.0, 0.96, 0.90, 1.0, 1, LEMON), (0.0, 0.96, 0.90, 1.0, -1, WOOD_FRESH),
               (0.05, 0.90, 0.66, 0.71, 1, WOOD_MID), (0.05, 0.90, 0.66, 0.71, -1, WOOD_MID)]
    for t0, t1, u0, u1, side, rgb in strakes:
        parts.append(_part(_plank(t0, t1, u0, u1, side, 0.03), SLOTS_FRAME, 'M_Wood',
                           rgb, rnd))

    for t in (0.30, 0.55, 0.75):
        bi = H.inner_half_beam(t) + 0.02
        z = H.sheer(t) - 0.20
        beam = C.make_box('DeckBeam', (0.07, 2.0 * bi, 0.07), center=(H.x_station(t), 0.0, z))
        parts.append(_part(beam, SLOTS_FRAME, 'M_Wood', WOOD_MID, rnd, bevel=0.01))

    for t in (0.22, 0.48, 0.74):
        top = H.keel(t) - 2.0 * keel_half + 0.01
        parts += _trestle(H.x_station(t), top, ground + 0.10, rnd)

    for sy in (-1.0, 1.0):
        skid = C.make_box('Skid', (5.4, 0.16, 0.10), center=(-0.05, sy * 0.60, ground + 0.05))
        parts.append(_part(skid, SLOTS_FRAME, 'M_Wood', WOOD_DARK, rnd, bevel=0.015))

    for t in (0.32, 0.66):
        for side in (-1.0, 1.0):
            p = H.hull_point(t, 0.62, side) + H.hull_normal(t, 0.62, side) * 0.03
            foot = Vector((p.x, side * 1.45, ground + 0.06))
            shore, _ = H.sweep('Shore', [p, foot], [0.04, 0.045], sides=6)
            parts.append(_part(shore, SLOTS_FRAME, 'M_Wood', WOOD_PALE, rnd, angle=ROUND))
            pad = C.make_box('ShorePad', (0.22, 0.18, 0.06), center=(foot.x, foot.y, ground + 0.03))
            parts.append(_part(pad, SLOTS_FRAME, 'M_Wood', WOOD_DARK, rnd, bevel=0.01))
            lash, _ = H.sweep('Lashing', [p + Vector((0.0, 0.0, 0.05)), p - Vector((0.0, 0.0, 0.05))],
                              0.05, sides=6)
            parts.append(_part(lash, SLOTS_FRAME, 'M_Fabric', ROPE, rnd, angle=ROUND))

    obj = _finish(parts, 'SM_' + variant['name'])
    return H.translate_mesh(obj, (0.0, 0.0, z_off))
