"""
items_rescatados.py — objetos que el jugador rescata del Albatros y de la
playa en las primeras horas (items.json, biblia §3.3): brújula de bolsillo,
cable eléctrico, cerillas, chapa del fuselaje, cinta americana, tubo de
aluminio y madera de naufragio.

Objetos sueltos: pivote en la base (z = 0) y centrados en XY, tumbados como
quedan en el suelo (a lo largo de +X los alargados). Escala real. La chapa y
el tubo llevan la librea del Albatros (crema con franja roja y filete dorado,
ver albatros.py) para que se lean como «trozos del avión».
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _materials as M  # noqa: E402
import _items as I  # noqa: E402
import kit_construccion as K  # noqa: E402

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

GROUP = I.GROUP
PAL = I.PAL

# librea del Albatros en lineal (el crema/rojo/dorado de albatros.py)
LIVERY = {
    'cream': (0.60, 0.50, 0.32),
    'red': (0.56, 0.09, 0.03),
    'gold': (0.72, 0.44, 0.06),
    'alu': (0.62, 0.60, 0.54),
    'alu_dark': (0.36, 0.33, 0.28),
    'rust': (0.30, 0.10, 0.03),
}
BRASS = (0.62, 0.40, 0.10)
BRASS_DARK = (0.34, 0.19, 0.04)

VARIANTS = [
    dict(name='Item_Brujula', item_id='brujula', seed=4401, builder='brujula', tri_budget=(300, 3500)),
    dict(name='Item_CableElectrico', item_id='cable_electrico', seed=4402, builder='cable_electrico',
         tri_budget=(300, 4000)),
    dict(name='Item_Cerillas', item_id='cerillas', seed=4403, builder='cerillas', tri_budget=(200, 3500)),
    dict(name='Item_ChapaFuselaje', item_id='chapa_fuselaje', seed=4404, builder='chapa_fuselaje',
         tri_budget=(300, 4000)),
    dict(name='Item_CintaAmericana', item_id='cinta_americana', seed=4405, builder='cinta_americana',
         tri_budget=(200, 3000)),
    dict(name='Item_TuboAluminio', item_id='tubo_aluminio', seed=4406, builder='tubo_aluminio',
         tri_budget=(200, 3000)),
    dict(name='Item_MaderaNaufragio', item_id='madera_naufragio', seed=4407, builder='madera_naufragio',
         tri_budget=(300, 4000)),
]
for _v in VARIANTS:
    _v['group'] = GROUP
    _v['needs_collision'] = True
    _v['interactable'] = True

_BUILDERS = {}


def _register(key):
    def deco(fn):
        _BUILDERS[key] = fn
        return fn
    return deco


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    obj = _BUILDERS[variant['builder']](variant, rnd, 'SM_' + variant['name'])
    return I.ground_centered(obj)


def _grid_sheet(name, nu, nv, fn):
    """Lámina de nu x nv quads; fn(i, j) -> Vector. Sin grosor."""
    bm = bmesh.new()
    vs = [[bm.verts.new(fn(i, j)) for j in range(nv + 1)] for i in range(nu + 1)]
    for i in range(nu):
        for j in range(nv):
            bm.faces.new((vs[i][j], vs[i + 1][j], vs[i + 1][j + 1], vs[i][j + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    return o


def _rivet(p, rnd, pos, normal, r=0.004, rgb=None, name='Rivet'):
    o = C.make_blob(name, (0, 0, 0), 1.0, rnd.randint(0, 9999), subdivisions=1, noise_strength=0.0,
                    scale=(r, r, r * 0.55))
    C.orient_and_place_zaxis(o, Vector(pos), Vector(normal))
    M.assign(o, ['M_Metal'])
    I.tint(o, rgb or LIVERY['alu'], rnd, 0.02)
    p.add(o, 'none')
    return o


# ---------------------------------------------------------------------------
# BRÚJULA
# ---------------------------------------------------------------------------
@_register('brujula')
def _b_brujula(v, rnd, name):
    """Brújula de bolsillo de latón con la tapa abierta (espejo dentro),
    esfera crema con rosa de los vientos y aguja roja/blanca, argolla."""
    p = K.Parts()
    R = 0.026
    case = I.lathe('Case', [(0.0, 0.0), (R - 0.002, 0.0), (R, 0.002), (R + 0.0005, 0.009), (R - 0.001, 0.0125),
                            (R - 0.0045, 0.0128), (R - 0.0048, 0.0098), (0.0, 0.0098)], segs=28)
    M.assign(case, ['M_Metal'])
    # latón con el bisel superior pulido (más claro) y el fondo oscurecido
    I.color_fn(case, lambda co: I.lerp3(BRASS_DARK, BRASS, min(1.0, co.z / 0.009))
               if co.z < 0.0115 else I.lerp3(BRASS, (0.85, 0.66, 0.26), 0.6), rnd, 0.02)
    p.add(case, 'none')
    # esfera: disco con anillos para pintar la rosa y las marcas
    dial_z = 0.0101
    dial = I.lathe('Dial', [(0.0, dial_z), (0.006, dial_z), (0.012, dial_z), (0.0165, dial_z), (0.0195, dial_z),
                            (0.0212, dial_z)], segs=32)
    M.assign(dial, ['M_Paper'])
    face = (0.82, 0.74, 0.52)
    ink = (0.05, 0.06, 0.12)

    def dial_col(vv):
        ring = I.lathe_ring(vv.index, 32)
        k = (vv.index - 1) % 32
        if ring >= 4:
            # corona de marcas: 32 rayitas, las cardinales rojas
            if k % 8 == 0:
                return (0.55, 0.05, 0.02, 0.0)
            return (ink if k % 2 == 0 else face) + (0.0,)
        if ring == 2:
            # estrella de 4 puntas de la rosa
            return (I.lerp3(face, (0.45, 0.30, 0.08), 0.8) if k % 8 == 0 else face) + (0.0,)
        return face + (0.0,)
    C.set_vertex_colors(dial, dial_col)
    p.add(dial, 'none')
    # aguja: rombo plano con relieve; mitad norte roja, sur crema
    for half, sgn, rgb in (('N', 1, (0.72, 0.06, 0.02)), ('S', -1, (0.86, 0.82, 0.70))):
        bm = bmesh.new()
        h = dial_z + 0.0012
        tip = bm.verts.new((0.0, sgn * 0.0175, h))
        l_ = bm.verts.new((-0.0032, 0.0, h))
        r_ = bm.verts.new((0.0032, 0.0, h))
        top = bm.verts.new((0.0, 0.0, h + 0.0014))
        bm.faces.new((tip, top, l_) if sgn > 0 else (tip, l_, top))
        bm.faces.new((tip, r_, top) if sgn > 0 else (tip, top, r_))
        bm.faces.new((tip, l_, r_) if sgn > 0 else (tip, r_, l_))
        bm.faces.new((l_, top, r_) if sgn > 0 else (l_, r_, top))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        me = bpy.data.meshes.new('Needle' + half)
        bm.to_mesh(me)
        bm.free()
        nd = bpy.data.objects.new('Needle' + half, me)
        C.link_object(nd)
        nd.data.transform(Matrix.Rotation(math.radians(-18), 4, 'Z'))
        M.assign(nd, ['M_Metal'])
        I.tint(nd, rgb, rnd, 0.01)
        p.add(nd, 'none')
    hub = C.make_cylinder('Hub', 0.0022, 0.003, segments=10, center=(0, 0, dial_z + 0.0022))
    M.assign(hub, ['M_Metal'])
    I.tint(hub, BRASS, rnd, 0.01)
    p.add(hub, 'none')
    # tapa abierta ~105º sobre la bisagra de +Y, espejo hacia la cámara
    lid = I.lathe('Lid', [(0.0, 0.0), (R - 0.0045, 0.0), (R - 0.0045, 0.0006), (R, 0.0006), (R + 0.0004, 0.004),
                          (R - 0.004, 0.0072), (0.0, 0.008)], segs=28)
    M.assign(lid, ['M_Metal'])
    mirror = (0.62, 0.66, 0.60)

    def lid_col(co):
        if co.z < 0.0003 and math.hypot(co.x, co.y) < R - 0.0048:
            # espejo con un reflejo cálido en diagonal
            return I.lerp3(mirror, (0.90, 0.84, 0.66), max(0.0, 1 - abs(co.x + co.y) / 0.01))
        # tapa grabada: círculo de puntos más oscuro
        return I.lerp3(BRASS, BRASS_DARK, 0.5 if 0.012 < math.hypot(co.x, co.y) < 0.016 and co.z > 0.005 else 0.0)
    I.color_fn(lid, lid_col, rnd, 0.015)
    lid.data.transform(Matrix.Translation((0.0, -R, 0.0)))
    lid.data.transform(Matrix.Rotation(math.radians(-105), 4, 'X'))
    lid.data.transform(Matrix.Translation((0.0, R + 0.0008, 0.0118)))
    p.add(lid, 'none')
    hinge = C.make_cylinder('Hinge', 0.0022, 0.012, segments=8)
    hinge.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
    hinge.data.transform(Matrix.Translation((0.0, R + 0.0008, 0.0118)))
    M.assign(hinge, ['M_Metal'])
    I.tint(hinge, BRASS_DARK, rnd, 0.01)
    p.add(hinge, 'none')
    # argolla en -Y (al lado opuesto de la bisagra, como un reloj de bolsillo)
    stem = C.make_cylinder('Stem', 0.0028, 0.006, segments=10)
    stem.data.transform(Matrix.Rotation(math.pi / 2, 4, 'X'))
    stem.data.transform(Matrix.Translation((0.0, -R - 0.0025, 0.0055)))
    M.assign(stem, ['M_Metal'])
    I.tint(stem, BRASS, rnd, 0.01)
    p.add(stem, 'none')
    ring_pts = [(math.cos(a) * 0.0075, -R - 0.0055 - 0.0075 - math.sin(a) * 0.0075, 0.0055)
                for a in [2 * math.pi * k / 18 for k in range(19)]]
    ring = I.sweep('Bail', ring_pts, 0.0013, segs=6, cap=False)
    M.assign(ring, ['M_Metal'])
    I.tint(ring, BRASS, rnd, 0.01)
    p.add(ring, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# CABLE ELÉCTRICO
# ---------------------------------------------------------------------------
@_register('cable_electrico')
def _b_cable_electrico(v, rnd, name):
    """Rollo de cable rojo arrancado del cuadro del avión: cinco vueltas
    flojas, un cabo suelto y las puntas peladas con el cobre abierto."""
    p = K.Parts()
    cr = 0.0042
    segs = 6
    turns = 4.6
    n = int(turns * 22)
    ph = rnd.uniform(0, 6.28)
    coil = []
    for i in range(n + 1):
        t = i / n
        a = ph + t * turns * 2 * math.pi
        r = 0.058 + 0.016 * t + 0.006 * math.sin(3 * a + ph)
        z = cr + 0.0045 * (1 + math.sin(a * 0.5 + t * 9))
        coil.append(Vector((math.cos(a) * r, math.sin(a) * r * 0.9, z)))
    # cabos: el de entrada sale hacia fuera y el final cruza por encima
    a0 = ph
    out0 = Vector((math.cos(a0), math.sin(a0) * 0.9, 0.0))
    tan0 = Vector((-math.sin(a0), math.cos(a0) * 0.9, 0.0)).normalized()
    lead = [coil[0] - tan0 * (0.022 * k) + out0 * (0.0035 * k * k) for k in range(5, 0, -1)]
    lead = [Vector((q.x, q.y, cr)) for q in lead]
    last = coil[-1]
    tail = [last + Vector((-last.x * 0.25 * k, -last.y * 0.25 * k, 0.004 * math.sin(k * 0.9))) for k in range(1, 7)]
    tail = [Vector((q.x + 0.015 * k, q.y + 0.01 * k, max(cr, q.z))) for k, q in enumerate(tail)]
    pts = lead + coil + tail
    wire = I.sweep('Wire', pts, cr, segs=segs)
    M.assign(wire, ['M_Fabric'])
    red = (0.62, 0.05, 0.02)
    red_d = (0.38, 0.03, 0.01)
    nrings = len(pts)
    copper = (0.72, 0.30, 0.08)

    def wcol(vv):
        ring, k = divmod(vv.index, segs)
        if ring <= 0 or ring >= nrings - 1:
            return copper + (0.0,)
        # rayita blanca de rotulado cada ~6 anillos y brillo lateral
        c = red if k % 3 else red_d
        if ring % 9 == 0 and k in (1, 2):
            c = (0.85, 0.80, 0.70)
        return c + (0.0,)
    C.set_vertex_colors(wire, wcol)
    p.add(wire, 'none')
    # puntas peladas: hilos de cobre abiertos en abanico
    for end, nxt in ((pts[0], pts[1]), (pts[-1], pts[-2])):
        d = (end - nxt).normalized()
        side = d.cross(Vector((0, 0, 1))).normalized()
        for s in range(4):
            off = side * (s - 1.5) * 0.0012
            q = [end + off, end + d * 0.007 + off * 2.2 + Vector((0, 0, 0.0005 * s)),
                 end + d * 0.014 + off * 4.0 + Vector((0, 0, 0.001 * s))]
            st = I.sweep(f'Strand{s}', q, [0.0008, 0.0007, 0.0005], segs=4)
            M.assign(st, ['M_Metal'])
            I.tint(st, copper, rnd, 0.04)
            p.add(st, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# CERILLAS
# ---------------------------------------------------------------------------
def _match(p, rnd, base, direction, length, burnt=False, name='Match'):
    d = Vector(direction).normalized()
    stick = C.make_box(name, (0.0024, 0.0024, length))
    stick.data.transform(Matrix.Translation((0, 0, length / 2)))
    C.orient_and_place_zaxis(stick, Vector(base), d)
    M.assign(stick, ['M_Wood'])
    wood = (0.70, 0.52, 0.26)
    if burnt:
        I.color_fn(stick, lambda co: I.lerp3(wood, PAL['char'],
                                              max(0.0, ((co - Vector(base)).dot(d) - length * 0.55) / (length * 0.3))),
                   rnd, 0.02)
    else:
        I.tint(stick, wood, rnd, 0.02)
    p.add(stick, 'none')
    head = C.make_blob(name + 'Head', (0, 0, 0), 1.0, rnd.randint(0, 999), subdivisions=2, noise_strength=0.05,
                       scale=(0.0021, 0.0021, 0.0034))
    C.orient_and_place_zaxis(head, Vector(base) + d * (length + 0.0012), d)
    M.assign(head, ['M_Stone'])
    I.tint(head, (0.05, 0.03, 0.025) if burnt else (0.62, 0.05, 0.03), rnd, 0.02)
    p.add(head, 'none')


@_register('cerillas')
def _b_cerillas(v, rnd, name):
    """Caja de cerillas del kit del avión: funda amarilla con franja roja y
    raspador pardo, cajón medio abierto con las cabezas rojas a la vista y
    dos cerillas sueltas (una ya gastada)."""
    p = K.Parts()
    L, W, T = 0.056, 0.037, 0.016
    wall = 0.0012
    # funda: tubo abierto a lo largo de X (sección de rectángulo redondeado)
    sec_o = I.rrect(W, T, 0.0015, corner_segs=2, side_pts=5)
    sec_i = I.rrect(W - 2 * wall, T - 2 * wall, 0.0008, corner_segs=2, side_pts=5)
    xs = [-L / 2 + L * i / 10 for i in range(11)]
    outer = I.loft_x('SleeveO', xs, lambda x: sec_o, cap=False)
    inner = I.loft_x('SleeveI', xs, lambda x: sec_i, cap=False)
    inner.data.flip_normals()
    rims = []
    for x in (xs[0], xs[-1]):
        ro = [Vector((x, y, z)) for y, z in sec_o]
        ri = [Vector((x, y, z)) for y, z in sec_i]
        rims.append(C.ring_loft('Rim', [ro, ri]))
    sleeve = C.join_objects([outer, inner] + rims, 'Sleeve')
    sleeve.data.transform(Matrix.Translation((0, 0, T / 2)))
    M.assign(sleeve, ['M_Paper'])
    yellow = (0.86, 0.56, 0.04)
    red = (0.62, 0.06, 0.02)
    navy = (0.03, 0.06, 0.22)
    striker = (0.26, 0.09, 0.04)

    def s_col(co):
        if abs(co.y) > W / 2 - 0.0009 and 0.002 < co.z < T - 0.002:
            return striker
        if co.z > T - 0.0015:
            if abs(co.x + 0.004) < 0.0075:
                return red
            if abs(co.x + 0.004) < 0.0105:
                return navy
            return yellow
        if co.z < 0.0015:
            return I.lerp3(yellow, (0.55, 0.36, 0.05), 0.5)
        return yellow
    I.color_fn(sleeve, s_col, rnd, 0.01)
    p.add(sleeve, 'none')
    # cajón asomando 2,2 cm por +X
    pull = 0.022
    tx0, tx1 = L / 2 - 0.01, L / 2 + pull
    tw, tt = W - 2 * wall - 0.0006, T - 2 * wall - 0.0006
    tray_parts = []
    bottom = C.make_box('TrayB', (tx1 - tx0, tw, 0.0008), center=((tx0 + tx1) / 2, 0, wall + 0.0005))
    tray_parts.append(bottom)
    for sy in (-1, 1):
        tray_parts.append(C.make_box('TrayS', (tx1 - tx0, 0.0008, tt), center=((tx0 + tx1) / 2, sy * (tw / 2 - 0.0004),
                                                                                  wall + tt / 2 + 0.0003)))
    tray_parts.append(C.make_box('TrayE', (0.0008, tw, tt), center=(tx1 - 0.0004, 0, wall + tt / 2 + 0.0003)))
    tray = C.join_objects(tray_parts, 'Tray')
    M.assign(tray, ['M_Paper'])
    I.tint(tray, (0.66, 0.52, 0.30), rnd, 0.02)
    p.add(tray, 'none')
    # cabezas de las cerillas que asoman en el cajón (en dos capas)
    for row in range(2):
        for k in range(7):
            y = -tw / 2 + 0.003 + k * (tw - 0.006) / 6
            z = wall + 0.0028 + row * 0.0034
            x_head = tx1 - 0.004 - (0.0015 if (k + row) % 2 else 0.0)
            stick = C.make_box('Stk', (0.012, 0.0022, 0.0022), center=(x_head - 0.006, y, z))
            M.assign(stick, ['M_Wood'])
            I.tint(stick, (0.72, 0.54, 0.28), rnd, 0.02)
            p.add(stick, 'none')
            head = C.make_blob('Hd', (x_head + 0.0006, y, z), 1.0, rnd.randint(0, 999), subdivisions=1,
                               noise_strength=0.04, scale=(0.0028, 0.0021, 0.0021))
            M.assign(head, ['M_Stone'])
            I.tint(head, (0.64, 0.05, 0.03), rnd, 0.03)
            p.add(head, 'none')
    # dos cerillas sueltas en el suelo, delante
    _match(p, rnd, (-0.02, -0.032, 0.0012), (1.0, -0.25, 0.0), 0.046, name='Loose0')
    _match(p, rnd, (0.028, -0.024, 0.0012), (0.55, -0.9, 0.0), 0.046, burnt=True, name='Loose1')
    return p.finish(name)


# ---------------------------------------------------------------------------
# CHAPA DEL FUSELAJE
# ---------------------------------------------------------------------------
@_register('chapa_fuselaje')
def _b_chapa_fuselaje(v, rnd, name):
    """Chapa del fuselaje arrancada: curvada como el casco, un borde
    desgarrado en dientes, una esquina doblada hacia arriba, dos filas de
    remaches y la librea del Albatros (crema, franja roja con filete dorado)
    desconchada hasta el aluminio."""
    p = K.Parts()
    Lx, Ls, R = 0.56, 0.40, 0.36
    nu, nv = 22, 18
    tear = [0.03 * abs(math.sin(j * 1.7 + 0.3)) + 0.03 * rnd.random() for j in range(nv + 1)]
    tear2 = [0.012 * rnd.random() for _ in range(nu + 1)]

    def pos(i, j):
        u = i / nu
        s = (j / nv - 0.5) * Ls
        x = -Lx / 2 + Lx * u
        # borde +X desgarrado en dientes, borde +S algo mordido
        if i == nu:
            x -= tear[j]
        elif i == nu - 1:
            x -= tear[j] * 0.5
        if j == nv:
            s -= tear2[i]
        y = R * math.sin(s / R)
        z = R * math.cos(s / R) - R * math.cos(Ls / 2 / R)
        # esquina (-X, +S) doblada hacia arriba
        fold = max(0.0, (0.09 - (x + Lx / 2)) / 0.09) * max(0.0, (s - 0.08) / (Ls / 2 - 0.08))
        z += 0.09 * fold ** 1.3
        # abolladuras suaves
        z += 0.006 * math.sin(x * 23 + 1.0) * math.sin(s * 19)
        return Vector((x, y, z))
    sheet = _grid_sheet('Sheet', nu, nv, pos)
    I.solidify(sheet, 0.003)
    # pintura de la librea: material NO metálico (con M_Metal el crema se
    # lee gris azulado por el reflejo); el aluminio desnudo lo dan los
    # remaches y el color de los desconchones
    M.assign(sheet, ['M_Paper'])
    stripe0, stripe1 = -0.02, 0.07

    def col(co):
        s = R * math.asin(max(-1.0, min(1.0, co.y / R)))
        n = I.noise3(co, 22.0, 3.1) + 0.5 * I.noise3(co, 55.0, 7.7)
        paint = LIVERY['cream']
        if stripe0 < s < stripe1:
            paint = LIVERY['red']
        elif stripe1 < s < stripe1 + 0.014:
            paint = LIVERY['gold']
        # desconchones hasta el aluminio y mugre de óxido junto al desgarro
        if n > 0.5:
            paint = LIVERY['alu']
        elif n > 0.42:
            paint = I.lerp3(paint, LIVERY['alu_dark'], 0.5)
        edge = max(0.0, (co.x - (Lx / 2 - 0.07)) / 0.07)
        return I.lerp3(paint, LIVERY['rust'], edge * 0.45)
    I.color_fn(sheet, col, rnd, 0.015)
    # la cara interior (hacia abajo) va de imprimación verde-amarilla:
    # color por esquina (CORNER) solo en los polígonos que miran abajo
    me = sheet.data
    attr = me.color_attributes['Col']
    primer = (0.30, 0.34, 0.12, 0.0)
    for poly in me.polygons:
        if poly.normal.z < -0.3:
            for li in poly.loop_indices:
                attr.data[li].color = primer
    p.add(sheet, 'none')
    # dos filas de remaches (costura de cuaderna) en X constante y en S constante
    for k in range(7):
        x = -0.17
        s = -0.16 + k * 0.045
        q = pos(int((x + Lx / 2) / Lx * nu + 0.5), int((s / Ls + 0.5) * nv + 0.5))
        nrm = Vector((0.0, math.sin(s / R), math.cos(s / R)))
        _rivet(p, rnd, q + nrm * 0.0015, nrm, r=0.0042, name=f'RivA{k}')
    for k in range(8):
        x = -0.12 + k * 0.045
        s = -0.14
        yv = R * math.sin(s / R)
        zv = R * math.cos(s / R) - R * math.cos(Ls / 2 / R) + 0.006 * math.sin(x * 23 + 1.0) * math.sin(s * 19)
        nrm = Vector((0.0, math.sin(s / R), math.cos(s / R)))
        _rivet(p, rnd, Vector((x, yv, zv)) + nrm * 0.0015, nrm, r=0.0042, name=f'RivB{k}')
    return p.finish(name)


# ---------------------------------------------------------------------------
# CINTA AMERICANA
# ---------------------------------------------------------------------------
@_register('cinta_americana')
def _b_cinta_americana(v, rnd, name):
    """Rollo de cinta americana verde oliva (kit de reparación del avión)
    con el canuto de cartón y una lengüeta despegada."""
    p = K.Parts()
    H = 0.048
    prof = [(0.0335, 0.0005), (0.0366, 0.0), (0.040, 0.0004), (0.044, 0.0004), (0.0478, 0.0008),
            (0.0486, 0.004), (0.0488, H * 0.5), (0.0486, H - 0.004), (0.0478, H - 0.0008), (0.044, H - 0.0004),
            (0.040, H - 0.0004), (0.0366, H), (0.0335, H - 0.0005), (0.0333, H * 0.5), (0.0335, 0.0005)]
    roll = I.lathe('Roll', prof, segs=30)
    C.merge_by_distance(roll, 1e-5)
    M.assign(roll, ['M_Fabric'])
    olive = (0.18, 0.21, 0.07)
    olive_l = (0.30, 0.32, 0.12)
    card = (0.56, 0.38, 0.18)

    def col(co):
        r = math.hypot(co.x, co.y)
        if r < 0.0368:
            return I.lerp3(card, (0.40, 0.26, 0.11), 0.5 + 0.5 * math.sin(co.z * 400))
        if co.z < 0.0012 or co.z > H - 0.0012:
            # canto: anillos de las capas de cinta
            return I.lerp3(olive, olive_l, 0.5 + 0.5 * math.sin(r * 2600))
        # cara exterior: tejido con brillo en los bordes
        return I.lerp3(olive, olive_l, 0.35 * max(0.0, abs(co.z - H / 2) / (H / 2) - 0.6) / 0.4)
    I.color_fn(roll, col, rnd, 0.01)
    p.add(roll, 'none')
    # lengüeta: sigue la tangente del rollo y cae un poco, borde rasgado
    a0 = math.radians(-70)
    R = 0.0489
    base = Vector((math.cos(a0) * R, math.sin(a0) * R, 0.0))
    tan = Vector((-math.sin(a0), math.cos(a0), 0.0))
    outw = Vector((math.cos(a0), math.sin(a0), 0.0))
    nu = 8
    jag = [0.004 * rnd.random() for _ in range(3)]

    def tpos(i, j):
        t = i / nu
        z = 0.001 + (H - 0.002) * j / 2
        L = 0.065 * t - (jag[j] if i == nu else 0.0)
        drop = 0.010 * t * t
        return base + tan * L + outw * (0.022 * t * t) + Vector((0, 0, z - drop * (z / H)))
    tongue = _grid_sheet('Tongue', nu, 2, tpos)
    I.solidify(tongue, 0.0009)
    M.assign(tongue, ['M_Fabric'])
    I.tint(tongue, olive, rnd, 0.01)
    p.add(tongue, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# TUBO DE ALUMINIO
# ---------------------------------------------------------------------------
@_register('tubo_aluminio')
def _b_tubo_aluminio(v, rnd, name):
    """Tirante de ala arrancado: tubo de aluminio de 0,8 m algo combado,
    con un extremo aplastado y agujereado (donde iba el perno) y el otro
    roto; restos de pintura crema y banda roja."""
    p = K.Parts()
    L, ro, ri = 0.80, 0.0155, 0.0128
    n = 26
    segs = 14
    levels = [i / n for i in range(n + 1)]

    def ring(t, r):
        x = -L / 2 + L * t
        # extremo -X aplastado (herraje del tirante)
        flat = max(0.0, (0.09 - L * t) / 0.09)
        sy = 1.0 + 0.55 * flat
        sz = 1.0 - 0.75 * flat
        # abolladura hacia el centro
        dent = 0.25 * math.exp(-((t - 0.58) / 0.04) ** 2)
        bow = 0.012 * math.sin(math.pi * t)
        pts = []
        for k in range(segs):
            a = 2 * math.pi * k / segs
            rr = r * (1.0 - dent * max(0.0, math.sin(a)) ** 2)
            # extremo +X roto: dientes en el borde
            dx = -(0.006 * abs(math.sin(k * 2.3)) + 0.003 * (k % 3)) if t == 1.0 else 0.0
            pts.append(Vector((x + dx, math.cos(a) * rr * sy + bow, math.sin(a) * rr * sz)))
        return pts
    outer = C.ring_loft('TubeO', [ring(t, ro) for t in levels])
    inner = C.ring_loft('TubeI', [ring(t, ri) for t in levels])
    inner.data.flip_normals()
    lips = [C.ring_loft('Lip', [ring(t, ro), ring(t, ri)]) for t in (0.0, 1.0)]
    tube = C.join_objects([outer, inner] + lips, 'Tube')
    # pintado casi entero: no metálico para que el crema no se lea gris
    M.assign(tube, ['M_Paper'])

    def col(co):
        t = (co.x + L / 2) / L
        n_ = I.noise3(co, 30.0, 1.7)
        c = LIVERY['cream']
        if 0.30 < t < 0.42:
            c = LIVERY['red']
        elif 0.42 < t < 0.44:
            c = LIVERY['gold']
        if n_ > (0.5 if 0.30 < t < 0.44 else 0.28) or t > 0.93:
            c = LIVERY['alu']
        if t < 0.1:
            c = I.lerp3(c, LIVERY['alu_dark'], 0.4)
        if math.hypot(co.y, co.z) < ri + 0.0006 and 0.02 < t < 0.98:
            c = LIVERY['alu_dark']
        return c
    I.color_fn(tube, col, rnd, 0.015)
    tube.data.transform(Matrix.Translation((0, 0, ro)))
    p.add(tube, 'none')
    # perno con tuerca atravesando el herraje aplastado
    bolt = C.make_cylinder('Bolt', 0.0035, 0.03, segments=8, center=(-L / 2 + 0.035, 0.0, ro))
    M.assign(bolt, ['M_Metal'])
    I.tint(bolt, (0.40, 0.22, 0.08), rnd, 0.02)
    p.add(bolt, 'none')
    for zz in (ro + 0.0075, ro - 0.0075):
        nut = C.make_cylinder('Nut', 0.0062, 0.004, segments=6, center=(-L / 2 + 0.035, 0.0, zz))
        M.assign(nut, ['M_Metal'])
        I.tint(nut, (0.46, 0.26, 0.09), rnd, 0.02)
        p.add(nut, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# MADERA DE NAUFRAGIO
# ---------------------------------------------------------------------------
@_register('madera_naufragio')
def _b_madera_naufragio(v, rnd, name):
    """Tablón de un barco hundido: madera blanqueada por el sol y la sal,
    restos de pintura azul, dos clavos de hierro con chorretones de óxido,
    un extremo partido en astillas y bellotas de mar en una esquina."""
    p = K.Parts()
    L, W, T = 0.86, 0.15, 0.032
    n = 30
    sec = I.rrect(W, T, 0.006, corner_segs=2, side_pts=6)
    npts = len(sec)
    xs = [-L / 2 + L * i / n for i in range(n + 1)]
    splinter = [0.05 * abs(math.sin(k * 1.9 + 0.4)) + 0.03 * rnd.random() for k in range(npts)]

    def section(x):
        t = (x + L / 2) / L
        warp = 0.008 * math.sin(t * math.pi)
        out = []
        for k, (y, z) in enumerate(sec):
            wear = 1.0 - 0.06 * max(0.0, abs(t - 0.5) * 2 - 0.8) / 0.2
            out.append((y * wear, z * wear + warp + 0.002 * math.sin(y * 90)))
        return out
    plank = I.loft_x('Plank', xs, section, cap=True)
    # extremo +X partido: el último anillo se retrasa en dientes
    me = plank.data
    for vv in me.vertices:
        ring, k = divmod(vv.index, npts)
        if ring == n:
            vv.co.x -= splinter[k]
        elif ring == n - 1:
            vv.co.x -= splinter[k] * 0.4
    me.update()
    plank.data.transform(Matrix.Translation((0, 0, T / 2)))
    M.assign(plank, ['M_Wood'])
    wood = (0.34, 0.22, 0.12)
    wood_l = (0.50, 0.36, 0.20)
    wood_d = (0.16, 0.09, 0.04)
    blue = (0.04, 0.20, 0.40)
    nails = [(-0.30, 0.035), (0.27, -0.04)]

    def col(co):
        grain = 0.5 + 0.5 * math.sin(co.y * 260 + math.sin(co.x * 9) * 3.0)
        c = I.lerp3(wood, wood_l, grain * 0.8)
        # grieta oscura a lo largo
        if abs(co.y - 0.02 - 0.01 * math.sin(co.x * 7)) < 0.004:
            c = wood_d
        n_ = I.noise3(co, 5.0, 2.2)
        if co.z > T * 0.7 and -0.3 < co.x < 0.28:
            c = I.lerp3(c, I.lerp3(blue, (0.12, 0.32, 0.50), 0.5 + 0.5 * I.noise3(co, 30.0, 5.0)),
                        max(0.0, min(1.0, (n_ + 0.05) / 0.12)))
        for nx, ny in nails:
            d = math.hypot(co.x - nx - 0.012, (co.y - ny) * 2.5)
            if d < 0.03 and co.x > nx - 0.004:
                c = I.lerp3(c, LIVERY['rust'], max(0.0, 1 - d / 0.03) * 0.8)
        # extremo partido: madera fresca más clara
        if co.x > L / 2 - 0.09 and co.z < T * 0.95:
            c = I.lerp3(c, (0.62, 0.44, 0.24), 0.5)
        return c
    I.color_fn(plank, col, rnd, 0.015)
    p.add(plank, 'none')
    for k, (nx, ny) in enumerate(nails):
        head = C.make_cylinder(f'Nail{k}', 0.0065, 0.004, segments=8, center=(nx, ny, T + 0.001), radius2=0.0055)
        M.assign(head, ['M_Metal'])
        I.tint(head, (0.22, 0.08, 0.03), rnd, 0.02)
        p.add(head, 'none')
    # bellotas de mar en la esquina -X, -Y
    for k in range(7):
        bx = -L / 2 + 0.02 + rnd.uniform(0, 0.07)
        by = -W / 2 + 0.012 + rnd.uniform(0, 0.04)
        r = rnd.uniform(0.004, 0.0075)
        cone = I.lathe(f'Barn{k}', [(r, 0.0), (r * 0.95, r * 0.6), (r * 0.5, r * 1.1), (r * 0.35, r * 1.05),
                                    (0.0, r * 0.8)], segs=7)
        cone.data.transform(Matrix.Translation((bx, by, T - 0.0005)))
        M.assign(cone, ['M_Stone'])
        I.color_fn(cone, lambda co, z0=T: I.lerp3((0.72, 0.64, 0.48), (0.40, 0.28, 0.20),
                                                  max(0.0, (co.z - z0) / 0.009)), rnd, 0.03)
        p.add(cone, 'none')
    return p.finish(name)
