"""
_albatros_shape.py — geometría compartida del kit «Albatros» (albatros.py).

Hidrocanoa chárter de los años 70: casco-fuselaje con redan, ala alta con
flotadores de punta, dos motores radiales en el ala y cola con deriva y
estabilizadores. El avión entero (SM_Albatros_Full) y sus cuatro restos
muestrean LAS MISMAS secciones, así que los restos encajan con el avión.

Coordenadas del avión: metros, morro hacia +X (x = 5,5), cola en x = -5,5,
Z = 0 en la quilla del redan. Los restos se inclinan y se apoyan en el suelo
después (rotate_mesh + ground_and_center).
"""

import math
import os
import sys

import bpy

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _prims as P  # noqa: E402
import common as C  # noqa: E402

# Librea de los 70 desgastada (colores lineales).
WHITE = (0.84, 0.83, 0.78)
RED = (0.62, 0.06, 0.04)
RED_DARK = (0.40, 0.05, 0.04)
BARE = (0.60, 0.61, 0.63)
RUST = (0.42, 0.17, 0.06)
DARK = (0.09, 0.09, 0.10)
GREY = (0.30, 0.31, 0.33)
INTERIOR = (0.05, 0.05, 0.06)
GLASS = (0.16, 0.30, 0.36)
YELLOW = (0.95, 0.66, 0.05)
BRONZE = (0.30, 0.20, 0.10)

# ---------------------------------------------------------------------------
# Fuselaje (casco de hidrocanoa)
# ---------------------------------------------------------------------------
STATIONS = [-5.5, -5.0, -4.4, -3.8, -3.2, -2.5, -1.9, -1.4, -1.0, -0.6, -0.2, -0.02,
            0.02, 0.45, 0.85, 1.25, 1.65, 2.05, 2.45, 2.85, 3.25, 3.6, 4.0, 4.4, 4.8,
            5.15, 5.45]
X_BREAK = -2.5           # donde el fuselaje se parte en dos restos
WINDOW_SPANS = [(0.45, 0.85), (1.25, 1.65), (2.05, 2.45), (2.85, 3.25)]
COCKPIT = (3.6, 4.4)     # parabrisas en la cubierta inclinada del morro
DOOR = (-1.0, -0.2)      # puerta de babor (abierta en el resto)


def hull_top(x):
    if x > 3.6:
        return 2.05 - 0.95 * ((x - 3.6) / 1.9) ** 1.6
    if x < -1.5:
        return 2.05 - 0.45 * (-1.5 - x) / 4.0
    return 2.05


def hull_keel(x):
    if x > 3.0:
        return 0.55 * ((x - 3.0) / 2.5) ** 2
    if x < 0.0:
        return 0.18 + 0.95 * (-x / 5.5) ** 1.3
    return 0.0


def hull_half_width(x):
    if x > 3.0:
        return 0.85 * (1.0 - 0.75 * ((x - 3.0) / 2.5) ** 2)
    if x < -1.0:
        return 0.85 * (1.0 - 0.80 * ((-1.0 - x) / 4.5) ** 1.3)
    return 0.85


def hull_centre_z(x):
    return 0.5 * (hull_top(x) + hull_keel(x))


# categoría de cada segmento del anillo (16 puntos, estribor y luego babor)
RING_CATS = ['top', 'top', 'window', 'cheat', 'side', 'chine', 'bottom', 'bottom',
             'bottom', 'bottom', 'chine', 'side', 'cheat', 'window', 'top', 'top']


def hull_ring(x):
    """Sección cerrada: crujía superior, estribor hasta la quilla y babor de
    vuelta. El pantoque (chine) queda como arista viva."""
    zt, zk, w = hull_top(x), hull_keel(x), hull_half_width(x)
    zc = zk + min(0.45, 0.28 * (zt - zk))
    h = zt - zc
    half = [(0.60 * w, zt - 0.05), (0.93 * w, zt - 0.22 * h), (w, zt - 0.45 * h),
            (w, zt - 0.56 * h), (0.98 * w, zc + 0.10), (1.04 * w, zc),
            (0.55 * w, zk + 0.35 * (zc - zk))]
    pts = [Vector((x, 0.0, zt))]
    pts += [Vector((x, y, z)) for y, z in half]
    pts.append(Vector((x, 0.0, zk)))
    pts += [Vector((x, -y, z)) for y, z in reversed(half)]
    return pts


def nose_tip_ring(x_tip=5.58):
    """Anillo pequeño que redondea el morro antes de la tapa."""
    base = hull_ring(STATIONS[-1])
    c = Vector((x_tip, 0.0, hull_centre_z(STATIONS[-1]) + 0.04))
    return [c + (Vector((x_tip, p.y, p.z)) - c) * 0.45 for p in base]


def in_span(x0, x1, spans):
    xm = 0.5 * (x0 + x1)
    return any(a <= xm <= b for a, b in spans)


def dent(points, centre_fn, rnd, prob=0.3, depth=0.07):
    """Abolladuras: empuja hacia el eje algunos puntos del anillo."""
    out = []
    for p in points:
        if rnd.random() < prob:
            c = Vector((p.x, 0.0, centre_fn(p.x)))
            p = p + (c - p).normalized() * rnd.uniform(0.25, 1.0) * depth
        out.append(p)
    return out


def shell(name, rings, thickness, centre_fn, break_at_start):
    """Chapa con grosor: anillos exteriores + interiores unidos por el borde
    roto (abierto en un extremo y tapado en el otro). Tags: ('outer', tramo,
    segmento) | ('inner',) | ('rim',) | ('cap_out',) | ('cap_in',)."""
    inner = []
    for ring in rings:
        ir = []
        for p in ring:
            c = Vector((p.x, 0.0, centre_fn(p.x)))
            ir.append(p + (c - p).normalized() * thickness)
        inner.append(ir)
    n = len(rings)
    seq = inner[::-1] + rings if break_at_start else rings + inner[::-1]
    obj, raw = P.loft(name, seq)
    tags = []
    for t in raw:
        if t[0] == 'cap0':
            tags.append(('cap_in',) if break_at_start else ('cap_out',))
        elif t[0] == 'cap1':
            tags.append(('cap_out',) if break_at_start else ('cap_in',))
        else:
            r = t[1]
            if r == n - 1:
                tags.append(('rim',))
            elif (r >= n) == break_at_start:
                tags.append(('outer', r - n if break_at_start else r, t[2]))
            else:
                tags.append(('inner',))
    return obj, tags


# ---------------------------------------------------------------------------
# Ala, flotador y palas
# ---------------------------------------------------------------------------
WING_SPAN = 6.6
WING_Z = 2.10
AILERON = (3.8, 6.1)
AIRFOIL = [(1.0, 0.0), (0.8, 0.03), (0.6, 0.055), (0.4, 0.07), (0.22, 0.072),
           (0.08, 0.055), (0.02, 0.03), (0.0, 0.0), (0.02, -0.02), (0.08, -0.03),
           (0.22, -0.035), (0.4, -0.032), (0.6, -0.022), (0.8, -0.01)]
AIRFOIL_UPPER = range(0, 7)   # segmentos del extradós


def wing_chord(y):
    return 1.9 - 0.4 * abs(y) / WING_SPAN


def wing_le_x(y):
    return 2.3 - 0.25 * abs(y) / WING_SPAN


def wing_z(y):
    return WING_Z + 0.03 * abs(y)


def wing_ring(y, cut=1.0, scale=1.0):
    """Perfil alar en la estación y; `cut` < 1 recorta el borde de salida
    (hueco del alerón); `scale` encoge el perfil hacia su centro (punta)."""
    c, le, z = wing_chord(y), wing_le_x(y), wing_z(y)
    ctr = Vector((le - 0.4 * c, y, z))
    pts = []
    for xn, zn in AIRFOIL:
        xn = min(xn, cut)
        if xn == cut and cut < 1.0 and zn == 0.0:
            zn = 0.012
        p = Vector((le - xn * c, y, z + zn * c))
        pts.append(ctr + (p - ctr) * scale)
    return pts


def aileron_ring(y, angle_deg):
    c, le, z = wing_chord(y), wing_le_x(y), wing_z(y)
    hinge = Vector((le - 0.77 * c, y, z + 0.012 * c))
    pts = [Vector((le - 0.77 * c, y, z + 0.036 * c)), Vector((le - c, y, z)),
           Vector((le - 0.77 * c, y, z - 0.014 * c))]
    rot = Matrix.Rotation(math.radians(angle_deg), 3, 'Y')
    return [hinge + rot @ (p - hinge) for p in pts]


def float_rings(y, x0=0.45, x1=2.65, zc=1.05, segs=10):
    """Flotador de punta: huso con proa roma y fondo en V."""
    rings = []
    profile = [(0.0, 0.05), (0.08, 0.15), (0.25, 0.22), (0.5, 0.23), (0.75, 0.19),
               (0.92, 0.11), (1.0, 0.03)]
    for f, r in profile:
        x = x1 - (x1 - x0) * f
        ring = []
        for k in range(segs):
            a = 2.0 * math.pi * k / segs
            dy, dz = math.cos(a) * r, math.sin(a) * r
            if dz < 0.0:
                dz *= 1.0 - 0.35 * abs(math.cos(a))
            ring.append(Vector((x, y + dy, zc + dz)))
        rings.append(ring)
    return rings


def blade(name, radius=0.95, root=0.12, bend=0.0, bend_dir=(-1.0, 0.0, 0.0),
          bend_from=0.45, n=7):
    """Pala de hélice a lo largo de +Z (disco en el plano YZ, eje X) con
    torsión y, si bend != 0, doblada a partir de `bend_from` hacia
    bend_dir (rad acumulados a la punta). Tags del loft."""
    bd = Vector(bend_dir).normalized()
    zax = Vector((0.0, 0.0, 1.0))
    pos = Vector((0.0, 0.0, root))
    seg = (radius - root) / (n - 1)
    rings = []
    for k in range(n):
        r = root + seg * k
        f = max(0.0, (r - bend_from * radius) / ((1.0 - bend_from) * radius))
        th = bend * f
        tan = (zax * math.cos(th) + bd * math.sin(th)).normalized()
        if k:
            pos = pos + tan * seg
        chord = 0.20 - 0.08 * (r - root) / (radius - root)
        if k == 0:
            chord = 0.10
        twist = math.radians(35.0 - 23.0 * (r - root) / (radius - root))
        cax = Vector((0.0, 1.0, 0.0))
        cax = (cax - tan * cax.dot(tan)).normalized()
        cax = (cax * math.cos(twist) + tan.cross(cax) * math.sin(twist)).normalized()
        tax = tan.cross(cax).normalized()
        half_t = 0.018 if k else 0.035
        sec = [(0.5, 0.0), (0.1, 1.0), (-0.5, 0.3), (-0.5, -0.3), (0.1, -1.0)]
        rings.append([pos + cax * (a * chord) + tax * (b * half_t) for a, b in sec])
    return P.loft(name, rings)


# ---------------------------------------------------------------------------
# Matrícula, desgaste y transformaciones
# ---------------------------------------------------------------------------

def text_mesh(name, body, size, extrude=0.004):
    """Texto en malla (fuente integrada de Blender), en el plano XY mirando
    a +Z y centrado en el origen."""
    curve = bpy.data.curves.new(name + '_Curve', 'FONT')
    curve.body = body
    curve.size = size
    curve.extrude = extrude
    curve.resolution_u = 2
    curve.align_x = 'CENTER'
    curve.align_y = 'CENTER'
    tmp = bpy.data.objects.new(name + '_Tmp', curve)
    C.link_object(tmp)
    bpy.context.view_layer.update()
    deps = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(tmp.evaluated_get(deps))
    bpy.data.objects.remove(tmp)
    bpy.data.curves.remove(curve)
    obj = bpy.data.objects.new(name, me)
    C.link_object(obj)
    C.merge_by_distance(obj, dist=1e-4)
    return obj


# Escala global del desgaste: albatros.build() la fija por variante (el
# avión intacto apenas tiene desgaste; los restos, mucho).
WEAR = [1.0]


def _mix(a, b, t):
    return tuple(a[i] * (1.0 - t) + b[i] * t for i in range(3))


def worn(base, rnd, bare=0.12, rust=0.05):
    """Color de una cara de chapa pintada: casi siempre la pintura con un
    leve cambio de tono y, a veces, pintura saltada (mezcla con aluminio) o
    manchada de óxido. Mezclas a medias, no parches puros: con caras grandes
    un parche puro se leía como un tablero de ajedrez."""
    r = rnd.random()
    rust, bare = rust * WEAR[0], bare * WEAR[0]
    if r < rust:
        return _mix(base, RUST, 0.45)
    if r < rust + bare:
        return _mix(base, BARE, 0.55)
    k = rnd.uniform(0.92, 1.0)
    return (base[0] * k, base[1] * k, base[2] * k)


def rotate_mesh(obj, angle_deg, axis, pivot=(0.0, 0.0, 0.0)):
    pv = Vector(pivot)
    m = Matrix.Translation(pv) @ Matrix.Rotation(math.radians(angle_deg), 4, axis) \
        @ Matrix.Translation(-pv)
    obj.data.transform(m)
    obj.data.update()
    return obj


def ground_and_center(obj):
    """Origen en la base: z mínima a 0 y centro XY del bbox en el origen."""
    vs = [v.co for v in obj.data.vertices]
    cx = 0.5 * (min(v.x for v in vs) + max(v.x for v in vs))
    cy = 0.5 * (min(v.y for v in vs) + max(v.y for v in vs))
    zmin = min(v.z for v in vs)
    return P.translate_mesh(obj, (-cx, -cy, -zmin))
