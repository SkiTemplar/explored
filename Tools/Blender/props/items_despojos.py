"""
items_despojos.py — materiales de origen animal que salen de cazar, pescar
y limpiar la presa (items.json, biblia §3.1 «Animal» y §3.4): tendón, piel
de tiburón en bruto y curtida, grasa, plumas, espina, piel y vísceras de
pescado, lombriz de cebo y aceite de pescado.

Objetos sueltos: pivote en la base (z = 0), centrados en XY, en su postura
de reposo (los alargados a lo largo de +X). Escala real en metros. La piel
de tiburón nunca se pinta gris: pizarra verdosa por el lomo y crema por el
vientre; la de pescado, azul y plata con reflejo cálido.
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _materials as M  # noqa: E402
import _items as I  # noqa: E402
import kit_construccion as K  # noqa: E402

import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

GROUP = I.GROUP
PAL = I.PAL

DPAL = {
    'sinew': (0.58, 0.32, 0.10),
    'sinew_hi': (0.80, 0.58, 0.28),
    'shark_back': (0.035, 0.10, 0.12),
    'shark_mid': (0.10, 0.20, 0.20),
    'shark_belly': (0.78, 0.72, 0.58),
    'flesh': (0.66, 0.32, 0.26),
    'flesh_pale': (0.78, 0.52, 0.42),
    'leather': (0.40, 0.21, 0.08),
    'leather_hi': (0.60, 0.36, 0.14),
    'leather_edge': (0.72, 0.52, 0.28),
    'fat': (0.92, 0.82, 0.50),
    'fat_shade': (0.78, 0.62, 0.36),
    'fat_meat': (0.66, 0.14, 0.09),
    'feather_white': (0.88, 0.86, 0.78),
    'feather_black': (0.04, 0.035, 0.05),
    'feather_red': (0.72, 0.05, 0.03),
    'feather_brown': (0.36, 0.20, 0.08),
    'feather_bar': (0.14, 0.07, 0.03),
    'quill': (0.82, 0.74, 0.54),
    'bone': (0.84, 0.76, 0.56),
    'bone_dark': (0.62, 0.48, 0.28),
    'fish_back': (0.02, 0.10, 0.30),
    'fish_silver': (0.66, 0.72, 0.74),
    'fish_gold': (0.80, 0.56, 0.16),
    'fish_flesh': (0.84, 0.56, 0.46),
    'gut': (0.72, 0.36, 0.34),
    'gut_dark': (0.50, 0.18, 0.18),
    'liver': (0.34, 0.08, 0.06),
    'roe': (0.90, 0.42, 0.06),
    'bladder': (0.86, 0.80, 0.70),
    'worm': (0.62, 0.26, 0.26),
    'worm_dark': (0.44, 0.15, 0.16),
    'clitellum': (0.78, 0.40, 0.30),
    'oil': (0.62, 0.36, 0.04),
    'banana_leaf': (0.10, 0.34, 0.05),
    'banana_leaf_hi': (0.30, 0.48, 0.08),
}

VARIANTS = [
    dict(name='Item_Tendon', item_id='tendon', seed=4801, builder='tendon', tri_budget=(300, 5000)),
    dict(name='Item_PielBruto', item_id='piel_bruto', seed=4802, builder='piel_bruto', tri_budget=(400, 5000)),
    dict(name='Item_CueroCurtido', item_id='cuero_curtido', seed=4803, builder='cuero_curtido',
         tri_budget=(400, 6000), preview_rot_z=0.5),
    dict(name='Item_Grasa', item_id='grasa', seed=4804, builder='grasa', tri_budget=(300, 5000)),
    dict(name='Item_Plumas', item_id='plumas', seed=4805, builder='plumas', tri_budget=(300, 5000)),
    dict(name='Item_EspinaPescado', item_id='espina_pescado', seed=4806, builder='espina_pescado',
         tri_budget=(300, 5000)),
    dict(name='Item_PielPescado', item_id='piel_pescado', seed=4807, builder='piel_pescado',
         tri_budget=(300, 5000)),
    dict(name='Item_ViscerasPescado', item_id='visceras_pescado', seed=4808, builder='visceras_pescado',
         tri_budget=(400, 6000)),
    dict(name='Item_Lombriz', item_id='lombriz', seed=4809, builder='lombriz', tri_budget=(200, 3000)),
    dict(name='Item_AceitePescado', item_id='aceite_pescado', seed=4810, builder='aceite_pescado',
         tri_budget=(400, 6000)),
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


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------
def _sheet(name, L, half_w, lift, nu=18, nv=8, thick=0.004):
    """Lámina (piel, cuero) a lo largo de X: `half_w(u)` da la media anchura
    para u en -1..1 y `lift(u, s)` la altura (s en -1..1 a lo ancho). Grosor
    real con solidify. Devuelve (obj, n_capa) para distinguir las dos caras
    (el vértice i y el i + n son pareja; la cara de arriba es la de mayor z)."""
    import bmesh
    bm = bmesh.new()
    grid = []
    for i in range(nu + 1):
        u = i / nu * 2 - 1
        w = half_w(u)
        row = []
        for j in range(nv + 1):
            s = j / nv * 2 - 1
            row.append(bm.verts.new((u * L / 2, s * w, lift(u, s))))
        grid.append(row)
    for i in range(nu):
        for j in range(nv):
            bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    n = len(o.data.vertices)
    I.solidify(o, thick)
    return o, n


def _top_layer(o, n):
    """Índices de la capa de arriba de una lámina de _sheet()."""
    vs = o.data.vertices
    return {i if vs[i].co.z >= vs[i + n].co.z else i + n for i in range(n)}


def _leaf_bed(p, rnd, L, W, name='Bed', curl=0.25):
    """Trozo de hoja de plátano de apoyo (para grasa y vísceras): lámina
    con nervios paralelos claros y el nervio central."""
    o, n = _sheet(name, L, lambda u: W / 2 * (1 - 0.25 * u * u) * (1 + 0.04 * math.sin(u * 9)),
                  lambda u, s: curl * W * 0.25 * s * s + 0.002, nu=10, nv=8, thick=0.0025)
    M.assign(o, ['M_Leaf'])

    def col(v):
        co = v.co
        vein = 0.5 + 0.5 * math.cos((co.x * 0.5 + abs(co.y)) * 160)
        c = I.lerp3(DPAL['banana_leaf'], DPAL['banana_leaf_hi'], vein * 0.35)
        if abs(co.y) < W * 0.04:
            c = I.lerp3(c, (0.55, 0.60, 0.22), 0.7)
        return c + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    return o


# ---------------------------------------------------------------------------
# TENDÓN
# ---------------------------------------------------------------------------
@_register('tendon')
def _b_tendon(v, rnd, name):
    """Madeja de tendón seco (26 cm): cinco hebras ámbar que se retuercen
    juntas en una S suave, deshilachadas en un extremo y atadas con una
    vuelta de la propia hebra en el otro."""
    p = K.Parts()
    L, n = 0.26, 22
    axis = [Vector((L * (i / n - 0.5), 0.022 * math.sin(i / n * 2 * math.pi), 0.0)) for i in range(n + 1)]
    for k in range(5):
        a0 = k / 5 * 2 * math.pi
        rr = 0.0058 if k else 0.0
        pts = []
        for i, c in enumerate(axis):
            t = i / n
            a = a0 + t * 3.5 * math.pi
            fray = max(0.0, t - 0.8) / 0.2 * 0.012  # abanico del extremo suelto
            off = rr + fray
            pts.append(c + Vector((0.0, math.cos(a) * off, math.sin(a) * off * 0.6 + 0.004)))
        radii = [0.0028 * (1 - 0.5 * max(0.0, (i / n - 0.85) / 0.15)) for i in range(n + 1)]
        s = I.sweep(f'Strand{k}', pts, radii, segs=6)
        M.assign(s, ['M_Fabric'])
        base = I.lerp3(DPAL['sinew'], DPAL['sinew_hi'], k / 5)
        I.color_fn(s, lambda co, b=base: I.lerp3(b, DPAL['sinew_hi'], 0.3 + 0.3 * math.sin(co.x * 180)), rnd, 0.02)
        p.add(s, 'none')
    # ligadura cerca del extremo atado
    wrap = I.helix_wrap('Wrap', -0.012, 0.004, 0.0085, 0.0018, rnd, turns=4, segs=5)
    wrap.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
    wrap.data.transform(Matrix.Translation((axis[2].x, axis[2].y, 0.004)))
    M.assign(wrap, ['M_Fabric'])
    I.tint(wrap, I.lerp3(DPAL['sinew'], (0.3, 0.15, 0.05), 0.4), rnd, 0.02)
    p.add(wrap, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# PIELES
# ---------------------------------------------------------------------------
def _shark_hide_shape(u):
    body = 0.5 + 0.5 * math.cos(u * math.pi * 0.92)
    lobe = 0.55 * math.exp(-((u - 0.25) / 0.14) ** 2)      # aletas pectorales
    tail = 0.25 * math.exp(-((u + 0.85) / 0.1) ** 2)       # arranque de la cola
    return 0.13 * (0.25 + body * 0.75 + lobe + tail)


@_register('piel_bruto')
def _b_piel_bruto(v, rnd, name):
    """Piel de tiburón recién desollada (50 cm), abierta por el vientre y
    tendida: lomo pizarra verdoso al centro, vientre crema en los bordes,
    que se abarquillan y enseñan la carne rosada de la cara interior."""
    p = K.Parts()
    L = 0.50

    def lift(u, s):
        curl = 0.035 * abs(s) ** 3 * (0.8 + 0.4 * math.sin(u * 5 + 1))
        return curl + 0.004 * math.sin(u * 11) * math.cos(s * 3) + 0.004

    o, n = _sheet('Hide', L, _shark_hide_shape, lift, nu=30, nv=12, thick=0.005)
    M.assign(o, ['M_Fabric'])
    top = _top_layer(o, n)

    def col(vv):
        co = vv.co
        if vv.index not in top:
            return I.lerp3(DPAL['flesh'], DPAL['flesh_pale'], 0.5 + 0.5 * I.noise3(co, 30)) + (0.0,)
        w = max(1e-4, _shark_hide_shape(co.x / (L / 2)))
        s = min(1.0, abs(co.y) / w)
        c = I.lerp3(DPAL['shark_back'], DPAL['shark_mid'], min(1.0, s / 0.55))
        c = I.lerp3(c, DPAL['shark_belly'], max(0.0, s - 0.6) / 0.4)
        speck = max(0.0, I.noise3(co, 120) - 0.35)
        return I.lerp3(c, DPAL['shark_belly'], speck * 0.5) + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    return p.finish(name)


@_register('cuero_curtido')
def _b_cuero_curtido(v, rnd, name):
    """Piel de tiburón curtida (32 cm) enrollada y atada con dos vueltas de
    cordel: cuero pardo rojizo, el canto de la espiral más claro."""
    p = K.Parts()
    L = 0.30
    turns, r0, r1, th, npts = 2.3, 0.010, 0.036, 0.003, 44

    def spiral(x):
        wob = 0.002 * math.sin(x * 40)
        outer, inner = [], []
        for i in range(npts + 1):
            t = i / npts
            a = t * turns * 2 * math.pi
            r = r0 + (r1 - r0) * t + wob
            outer.append((math.cos(a) * (r + th / 2), math.sin(a) * (r + th / 2)))
            inner.append((math.cos(a) * (r - th / 2), math.sin(a) * (r - th / 2)))
        return outer + inner[::-1]

    xs = [L * (i / 12 - 0.5) for i in range(13)]
    roll = I.loft_x('Roll', xs, lambda x: spiral(x))
    M.assign(roll, ['M_Fabric'])
    half = xs[-1]

    def col(co):
        c = I.lerp3(DPAL['leather'], DPAL['leather_hi'], 0.5 + 0.5 * I.noise3(co, 40))
        if abs(abs(co.x) - half) < 1e-4:
            return DPAL['leather_edge']
        return c
    I.color_fn(roll, col, rnd, 0.01)
    p.add(roll, 'none')
    def outer_r(a):
        # radio de la última vuelta de la espiral en el ángulo a (el cordel
        # abraza el rollo, que no es redondo: la solapa hace escalón)
        a_end = turns * 2 * math.pi
        aa = a_end - ((a_end - a) % (2 * math.pi))
        return r0 + (r1 - r0) * aa / a_end + th / 2

    for k, x in enumerate((-0.085, 0.09)):
        pts = [(x + 0.002 * math.sin(a * 3), math.cos(a) * (outer_r(a) + 0.0026), math.sin(a) * (outer_r(a) + 0.0026))
               for a in (2 * math.pi * i / 32 for i in range(33))]
        tie = I.sweep(f'Tie{k}', pts, 0.0024, segs=5)
        M.assign(tie, ['M_Fabric'])
        I.cord_stripes(tie, PAL['cord'][0], I.lerp3(PAL['cord'][0], PAL['coir'][2], 0.6), 5, period=2)
        p.add(tie, 'none')
    # nudo arriba y chicotes
    ka = math.pi / 2
    knot = C.make_blob('Knot', (0.09, 0.0, outer_r(ka) + 0.004), 0.0055, v['seed'], subdivisions=2,
                       noise_strength=0.2)
    M.assign(knot, ['M_Fabric'])
    I.tint(knot, PAL['cord'][1], rnd, 0.02)
    p.add(knot, 'none')
    return p.finish(name)


@_register('piel_pescado')
def _b_piel_pescado(v, rnd, name):
    """Tira de piel de pescado (26 cm) despegada del filete, algo retorcida:
    lomo azul, flancos plata con escamas en rombo y un reflejo dorado; por
    dentro, carne rosada pálida."""
    p = K.Parts()
    L = 0.26

    def half_w(u):
        return 0.045 * (0.35 + 0.65 * math.cos(u * math.pi / 2) ** 0.7) * (1 + 0.05 * math.sin(u * 13))

    def lift(u, s):
        return 0.012 * (u + 1) ** 2 * max(0.0, s) + 0.006 * abs(s) ** 2 + 0.003

    o, n = _sheet('Skin', L, half_w, lift, nu=44, nv=16, thick=0.0025)
    # leve torsión a lo largo
    for vv in o.data.vertices:
        a = vv.co.x / L * 0.6
        y, z = vv.co.y, vv.co.z
        vv.co.y, vv.co.z = y * math.cos(a) - z * math.sin(a) * 0.3, z + y * math.sin(a) * 0.3
    o.data.update()
    M.assign(o, ['M_Fabric'])
    top = _top_layer(o, n)

    def col(vv):
        co = vv.co
        if vv.index not in top:
            return I.lerp3(DPAL['fish_flesh'], DPAL['flesh_pale'], 0.5 + 0.5 * I.noise3(co, 40)) + (0.0,)
        s = (co.y / max(1e-4, half_w(co.x / (L / 2))) + 1) / 2   # 0 vientre .. 1 lomo
        c = I.lerp3(DPAL['fish_silver'], DPAL['fish_back'], min(1.0, max(0.0, s - 0.5) / 0.3))
        c = I.lerp3(c, DPAL['fish_gold'], 0.7 * math.exp(-((s - 0.42) / 0.08) ** 2))
        scale = min(1.0, 3 * abs(math.sin((co.x + co.y) * 180)) * abs(math.sin((co.x - co.y) * 180)))
        return I.lerp3(c, I.lerp3(c, (0.02, 0.06, 0.12), 0.6), (1 - scale) * 0.45) + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# GRASA, PLUMAS
# ---------------------------------------------------------------------------
@_register('grasa')
def _b_grasa(v, rnd, name):
    """Taco de grasa (12 cm) sobre un trozo de hoja de plátano: blanco
    marfil, la capa de piel y carne roja en la base y dos cortes limpios."""
    p = K.Parts()
    _leaf_bed(p, rnd, 0.20, 0.13)
    o = C.make_blob('Fat', (0, 0, 0), 1.0, v['seed'], subdivisions=4, noise_scale=1.3, noise_strength=0.22,
                    relax_iterations=1)
    for vv in o.data.vertices:
        x, y, z = vv.co
        z = max(z, -0.55)                        # base plana (apoyada)
        z = z + 0.18 * x                         # cuña: más alta por un lado
        vv.co = Vector((x * 0.058, y * 0.040, (z + 0.55) * 0.030 + 0.003))
    o.data.update()
    M.assign(o, ['M_Fabric'])

    def col(co):
        h = co.z - 0.003
        rind = 0.012 + 0.003 * math.sin(co.x * 90)
        if h < rind:
            # capa de carne roja y piel parda debajo
            return I.lerp3(DPAL['fat_meat'], (0.30, 0.12, 0.07), max(0.0, 1 - h / 0.003))
        c = I.lerp3(DPAL['fat_shade'], DPAL['fat'], min(1.0, (h - rind) / 0.02))
        # vetas rosadas de carne dentro de la grasa
        vein = max(0.0, 1.0 - abs(h - 0.022 - 0.005 * math.sin(co.x * 70 + co.y * 40)) / 0.0022)
        return I.lerp3(c, DPAL['flesh_pale'], vein * 0.75)
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    return p.finish(name)


def _feather(p, rnd, name, L, W, base, dirn, colfn, curve=0.12, split=None):
    """Pluma tumbada: cañón (cálamo + raquis) y vexilo a ambos lados, más
    ancho por un lado. `colfn(t, side)` da el color (t 0 base .. 1 punta)."""
    import bmesh
    d = Vector(dirn).normalized()
    side_v = Vector((-d.y, d.x, 0.0))
    n = 12
    rach = []
    for i in range(n + 1):
        t = i / n
        rach.append(Vector(base) + d * L * t + side_v * curve * L * t * t + Vector((0, 0, 0.004 + 0.01 * math.sin(t * math.pi))))
    bm = bmesh.new()
    rows = []
    for i in range(n + 1):
        t = i / n
        if t < 0.22:
            wl = wr = 0.0006
        else:
            tt = (t - 0.22) / 0.78
            prof = math.sin(min(1.0, tt * 1.15) * math.pi * 0.5 + 0.2) * (1 - tt ** 3)
            wl, wr = W * 0.55 * prof + 0.0006, W * 0.35 * prof + 0.0006
        tang = (rach[min(i + 1, n)] - rach[max(i - 1, 0)]).normalized()
        sv = Vector((-tang.y, tang.x, 0.0)).normalized()
        c = rach[i]
        droop = Vector((0, 0, -0.002))
        rows.append([bm.verts.new(c + sv * wl + droop - tang * wl * 0.4),
                     bm.verts.new(c),
                     bm.verts.new(c - sv * wr + droop - tang * wr * 0.4)])
    for i in range(n):
        if split is not None and i == split:
            continue
        for k in range(2):
            bm.faces.new((rows[i][k], rows[i][k + 1], rows[i + 1][k + 1], rows[i + 1][k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    I.solidify(o, 0.0012)
    M.assign(o, ['M_Fabric'])
    b0 = Vector(base)

    def col(co):
        rel = Vector(co) - b0
        t = max(0.0, min(1.0, rel.dot(d) / L))
        s = rel.dot(side_v) - curve * L * t * t
        return colfn(t, s)
    I.color_fn(o, col, rnd, 0.015)
    p.add(o, 'none')
    shaft = I.sweep(name + 'Q', rach, [0.0014 * (1 - 0.8 * i / n) + 0.0003 for i in range(n + 1)], segs=5)
    M.assign(shaft, ['M_Fabric'])
    I.tint(shaft, DPAL['quill'], rnd, 0.02)
    p.add(shaft, 'none')


@_register('plumas')
def _b_plumas(v, rnd, name):
    """Manojo de cinco plumas de ave marina (20-30 cm) en abanico, atadas
    por los cañones: blancas con la punta negra (piquero), una larga roja
    (rabijunco) y una parda barrada."""
    p = K.Parts()

    def white_black(t, s):
        return I.lerp3(DPAL['feather_white'], DPAL['feather_black'], max(0.0, t - 0.7) / 0.18)

    def red(t, s):
        return I.lerp3(DPAL['feather_white'], DPAL['feather_red'], min(1.0, t / 0.35))

    def barred(t, s):
        bar = 0.5 + 0.5 * math.sin(t * 38 + s * 90)
        return I.lerp3(DPAL['feather_brown'], DPAL['feather_bar'], bar * 0.8)

    def black(t, s):
        return I.lerp3((0.20, 0.16, 0.14), DPAL['feather_black'], min(1.0, t / 0.3))

    specs = [(0.23, 0.035, -0.55, white_black, None), (0.30, 0.018, -0.2, red, None),
             (0.21, 0.040, 0.12, barred, 6), (0.24, 0.036, 0.42, white_black, None),
             (0.19, 0.034, 0.72, black, 8)]
    for k, (L, W, ang, cf, split) in enumerate(specs):
        _feather(p, rnd, f'F{k}', L, W, (0.0, 0.0, 0.002 * k), (math.cos(ang), math.sin(ang), 0.0), cf,
                 curve=0.06 if k % 2 else -0.05, split=split)
    wrap = I.helix_wrap('Wrap', -0.006, 0.006, 0.0055, 0.0016, rnd, turns=3, segs=5)
    wrap.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
    wrap.data.transform(Matrix.Translation((0.03, 0.0, 0.006)))
    M.assign(wrap, ['M_Fabric'])
    I.tint(wrap, PAL['cord_red'], rnd, 0.02)
    p.add(wrap, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# PESCADO: espina, vísceras
# ---------------------------------------------------------------------------
@_register('espina_pescado')
def _b_espina_pescado(v, rnd, name):
    """La espina de pescado de toda la vida (16 cm): columna con siete pares
    de costillas curvas, cráneo triangular con la cuenca del ojo y la cola
    en abanico. Hueso crema; tumbada."""
    p = K.Parts()
    L = 0.13
    n = 14
    spine_pts = [(L * (i / n - 0.5), 0.0, 0.004 + 0.001 * math.sin(i)) for i in range(n + 1)]
    spine = I.sweep('Spine', spine_pts, [0.0022 + 0.0012 * math.sin(i / n * math.pi) for i in range(n + 1)], segs=6)
    bones = [spine]
    # vértebras: pequeños engrosamientos
    for i in range(1, n):
        vb = C.make_cylinder(f'V{i}', 0.0034, 0.0035, segments=8, center=(0, 0, 0))
        vb.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
        vb.data.transform(Matrix.Translation(spine_pts[i]))
        bones.append(vb)
    # costillas
    for k in range(7):
        x = L * (0.32 - k * 0.09)
        ln = 0.030 * math.sin((k + 1.5) / 8.5 * math.pi) + 0.012
        for sgn in (-1, 1):
            pts = []
            for j in range(6):
                t = j / 5
                pts.append((x - 0.012 * t * t, sgn * ln * t, 0.004 - 0.001 * t))
            r = I.sweep(f'R{k}{sgn}', pts, [0.0013 * (1 - 0.6 * j / 5) for j in range(6)], segs=5)
            bones.append(r)
    # cráneo: cuña con la boca abierta hacia +X
    head = C.make_blob('Skull', (L / 2 + 0.018, 0.0, 0.006), 1.0, v['seed'], subdivisions=3, noise_strength=0.05,
                       scale=(0.024, 0.016, 0.006), relax_iterations=1)
    for vv in head.data.vertices:
        t = max(0.0, (vv.co.x - L / 2) / 0.04)
        vv.co.y *= 1.0 - 0.55 * t
    head.data.update()
    bones.append(head)
    # cola: dos lóbulos
    for sgn in (-1, 1):
        pts = [(-L / 2 + 0.002, 0.0, 0.004), (-L / 2 - 0.012, sgn * 0.008, 0.004), (-L / 2 - 0.026, sgn * 0.02, 0.004)]
        lobe = I.sweep(f'Tail{sgn}', pts, [0.002, 0.0045, 0.0015], segs=6)
        for vv in lobe.data.vertices:
            vv.co.z = 0.004 + (vv.co.z - 0.004) * 0.35
        lobe.data.update()
        bones.append(lobe)
    for b in bones:
        M.assign(b, ['M_Stone'])
        I.color_fn(b, lambda co: I.lerp3(DPAL['bone'], DPAL['bone_dark'], max(0.0, 0.5 + 0.5 * I.noise3(co, 90) - 0.5)),
                   rnd, 0.015)
        p.add(b, 'none')
    # ojo: cuenca oscura
    eye = C.make_blob('Eye', (L / 2 + 0.016, 0.006, 0.0105), 0.0035, v['seed'] + 3, subdivisions=1,
                      noise_strength=0.0)
    M.assign(eye, ['M_Stone'])
    I.tint(eye, (0.10, 0.05, 0.03), rnd, 0.0)
    p.add(eye, 'none')
    return p.finish(name)


@_register('visceras_pescado')
def _b_visceras_pescado(v, rnd, name):
    """Vísceras de pescado sobre un trozo de hoja de plátano (cebo): tripa
    rosada enroscada, hígado granate, una bolsa de huevas naranja y la
    vejiga natatoria nacarada."""
    p = K.Parts()
    _leaf_bed(p, rnd, 0.20, 0.12, curl=0.35)
    # tripa: tubo que serpentea
    pts = []
    for i in range(31):
        t = i / 30
        a = t * 3.2 * math.pi
        r = 0.035 * (1 - 0.55 * t)
        pts.append((math.cos(a) * r - 0.01, math.sin(a) * r * 0.8, 0.011 + 0.004 * t + 0.002 * math.sin(a * 2)))
    gut = I.sweep('Gut', pts, [0.0065 * (1 - 0.3 * i / 30) * (1 + 0.15 * math.sin(i * 1.7)) for i in range(31)],
                  segs=8)
    M.assign(gut, ['M_Leaf'])
    I.color_fn(gut, lambda co: I.lerp3(DPAL['gut'], DPAL['gut_dark'], 0.5 + 0.5 * I.noise3(co, 60)), rnd, 0.02)
    p.add(gut, 'none')
    liver = C.make_blob('Liver', (0.045, -0.018, 0.012), 1.0, v['seed'], subdivisions=3, noise_strength=0.12,
                        scale=(0.03, 0.02, 0.008), relax_iterations=2)
    M.assign(liver, ['M_Leaf'])
    I.color_fn(liver, lambda co: I.lerp3(DPAL['liver'], (0.52, 0.16, 0.10), 0.5 + 0.5 * I.noise3(co, 50)), rnd, 0.01)
    p.add(liver, 'none')
    roe = C.make_blob('Roe', (0.04, 0.026, 0.013), 1.0, v['seed'] + 1, subdivisions=3, noise_strength=0.08,
                      scale=(0.026, 0.012, 0.01), relax_iterations=1)
    M.assign(roe, ['M_Leaf'])
    I.color_fn(roe, lambda co: I.lerp3(DPAL['roe'], (0.98, 0.64, 0.20), max(0.0, I.noise3(co, 400))), rnd, 0.01)
    p.add(roe, 'none')
    bl = C.make_blob('Bladder', (-0.058, 0.01, 0.012), 1.0, v['seed'] + 2, subdivisions=3, noise_strength=0.03,
                     scale=(0.022, 0.011, 0.009), relax_iterations=1)
    M.assign(bl, ['M_Leaf'])
    I.color_fn(bl, lambda co: I.lerp3(DPAL['bladder'], (0.92, 0.72, 0.66), 0.5 + 0.5 * I.noise3(co, 80)), rnd, 0.01)
    p.add(bl, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# LOMBRIZ
# ---------------------------------------------------------------------------
@_register('lombriz')
def _b_lombriz(v, rnd, name):
    """Lombriz de tierra (13 cm) en S: anillos marcados, clitelo más claro
    y engrosado a un tercio de la cabeza, cola aplastada."""
    p = K.Parts()
    n = 48
    L = 0.12
    pts = []
    for i in range(n + 1):
        t = i / n
        pts.append((L * (t - 0.5), 0.018 * math.sin(t * 2.2 * math.pi + 0.4), 0.0032 + 0.0015 * math.sin(t * 9)))
    radii = []
    for i in range(n + 1):
        t = i / n
        r = 0.0032 * math.sin(min(1.0, t * 1.08 + 0.03) * math.pi) ** 0.35
        r *= 1.0 + 0.10 * math.cos(i * math.pi)             # anillos
        if 0.62 < t < 0.72:
            r *= 1.25                                        # clitelo
        radii.append(max(0.0008, r))
    o = I.sweep('Worm', pts, radii, segs=8)
    M.assign(o, ['M_Leaf'])

    def col(co):
        t = co.x / L + 0.5
        c = I.lerp3(DPAL['worm'], DPAL['worm_dark'], 0.4 + 0.3 * math.cos(t * n * math.pi))
        if 0.61 < t < 0.73:
            c = DPAL['clitellum']
        return I.lerp3(c, (0.80, 0.52, 0.46), max(0.0, 0.0028 - co.z) / 0.0028 * 0.5)
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    obj = p.finish(name)
    I.smooth_all(obj)
    return obj


# ---------------------------------------------------------------------------
# ACEITE
# ---------------------------------------------------------------------------
@_register('aceite_pescado')
def _b_aceite_pescado(v, rnd, name):
    """Canuto de bambú (19 cm) con aceite de pescado: tapón de hoja
    envuelta y atada con cordel, chorretones de aceite ámbar por fuera y
    un asa de cordel para colgarlo."""
    p = K.Parts()
    R, H = 0.028, 0.16
    prof = [(0.0, 0.0), (R * 0.97, 0.0), (R, 0.004), (R, 0.012), (R * 1.08, 0.016), (R, 0.02)]
    for z in (0.05, 0.09, 0.13):
        prof.append((R, z))
    prof += [(R, H - 0.006), (R * 0.98, H), (0.0, H)]
    o = I.lathe('Tube', prof, segs=18)
    M.assign(o, ['M_Wood'])
    drips = [(rnd.uniform(0, 2 * math.pi), rnd.uniform(0.35, 0.75)) for _ in range(3)]

    def col(co):
        a = math.atan2(co.y, co.x)
        c = I.lerp3(PAL['bamboo'][1], PAL['bamboo'][2], 0.5 + 0.5 * math.sin(a * 7 + co.z * 20))
        if 0.011 < co.z < 0.02:
            c = PAL['bamboo_node']
        for da, dl in drips:
            dd = abs(math.atan2(math.sin(a - da), math.cos(a - da)))
            if dd < 0.18 and co.z > H * (1 - dl):
                c = I.lerp3(c, DPAL['oil'], 0.8)
        if co.z < 0.002:
            c = PAL['bamboo_cut']
        return c
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    # tapón: hoja arrugada que rebosa por la boca
    plug = C.make_blob('Plug', (0.0, 0.0, H + 0.008), 1.0, v['seed'], subdivisions=3, noise_strength=0.25,
                       scale=(R * 1.25, R * 1.25, 0.016), relax_iterations=1)
    M.assign(plug, ['M_Leaf'])
    I.color_fn(plug, lambda co: I.lerp3(DPAL['banana_leaf'], (0.42, 0.40, 0.10), 0.5 + 0.5 * I.noise3(co, 70)),
               rnd, 0.02)
    p.add(plug, 'none')
    # faldón de la hoja caído sobre el borde + ligadura
    skirt = I.lathe('Skirt', [(R * 1.02, H - 0.022), (R * 1.07, H - 0.012), (R * 1.08, H + 0.002),
                              (R * 0.95, H + 0.008)], segs=18)
    for vv in skirt.data.vertices:
        a = math.atan2(vv.co.y, vv.co.x)
        if vv.co.z < H - 0.015:
            vv.co.z -= 0.006 * (0.5 + 0.5 * math.sin(a * 5))
    skirt.data.update()
    M.assign(skirt, ['M_Leaf'])
    I.color_fn(skirt, lambda co: I.lerp3(DPAL['banana_leaf'], DPAL['banana_leaf_hi'], 0.5 + 0.5 * math.sin(
        math.atan2(co.y, co.x) * 9)), rnd, 0.02)
    p.add(skirt, 'none')
    tie = I.helix_wrap('Tie', H - 0.012, H - 0.004, R * 1.1, 0.0018, rnd, turns=3, segs=5)
    M.assign(tie, ['M_Fabric'])
    I.cord_stripes(tie, PAL['cord'][0], I.lerp3(PAL['cord'][0], PAL['coir'][2], 0.6), 5, period=2)
    p.add(tie, 'none')
    # asa de cordel
    pts = []
    for i in range(17):
        t = i / 16
        a = math.pi * t
        pts.append((math.cos(a) * R * 1.12, 0.0, H - 0.008 + math.sin(a) * 0.045))
    handle = I.sweep('Handle', pts, 0.0022, segs=5)
    M.assign(handle, ['M_Fabric'])
    I.cord_stripes(handle, PAL['cord'][1], PAL['coir'][2], 5, period=2)
    p.add(handle, 'none')
    return p.finish(name)
