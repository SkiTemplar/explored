"""
items_botica.py — plantas sueltas de inventario y remedios (items.json):
hijuelo de plátano (para plantar), aloe arrancado y pasta medicinal.

* Hijuelo: cormo con raíces cortadas, pseudotallo verde con manchas
  vinosas y dos hojas en espada sin abrir del todo (se lee como «esqueje
  de platanera», no como una planta adulta).
* Aloe: roseta de hojas carnosas verde jade con motas claras, sobre su
  cepellón de raíces.
* Pasta medicinal: valva de concha a modo de cuenco con un montón de
  pasta verde batida y un trozo de hoja de aloe cortada al lado.

Objetos sueltos: pivote en la base (z = 0), centrados en XY, de pie en su
postura natural. Escala real en metros.
"""

import math
import os
import sys
from itertools import pairwise

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
sys.path.insert(0, os.path.dirname(__file__))
import _items as I  # noqa: E402
import _materials as M  # noqa: E402
import common as C  # noqa: E402
import items_contenedores as CT  # noqa: E402
import items_despojos as D  # noqa: E402
import kit_construccion as K  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

GROUP = I.GROUP
PAL = I.PAL

BPAL = {
    'corm': (0.62, 0.46, 0.26),
    'corm_dark': (0.36, 0.22, 0.10),
    'root_cut': (0.90, 0.84, 0.66),
    'stem': (0.20, 0.42, 0.08),
    'stem_hi': (0.46, 0.60, 0.16),
    'stem_blotch': (0.34, 0.08, 0.10),
    'aloe': (0.12, 0.36, 0.14),
    'aloe_hi': (0.30, 0.52, 0.20),
    'aloe_spot': (0.66, 0.80, 0.56),
    'aloe_tip': (0.52, 0.30, 0.12),
    'aloe_gel': (0.78, 0.90, 0.62),
    'soil': (0.22, 0.12, 0.05),
    'soil_hi': (0.34, 0.20, 0.08),
    'shell_out': (0.72, 0.46, 0.26),
    'shell_rib': (0.46, 0.24, 0.12),
    'shell_in': (0.96, 0.86, 0.80),
    'shell_in_hi': (0.90, 0.72, 0.74),
    'paste': (0.20, 0.44, 0.08),
    'paste_hi': (0.46, 0.66, 0.18),
    'paste_fleck': (0.08, 0.20, 0.04),
}

VARIANTS = [
    dict(name='Item_HijueloPlatano', item_id='hijuelo_platano', seed=5301, builder='hijuelo', tri_budget=(600, 8000)),
    dict(name='Item_PlantaMedicinalAloe', item_id='planta_medicinal_aloe', seed=5302, builder='aloe',
         tri_budget=(600, 8000)),
    dict(name='Item_PastaMedicinal', item_id='pasta_medicinal', seed=5303, builder='pasta', tri_budget=(400, 6000)),
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
def _fleshy_leaf(name, base, out_ang, L, W, T, rise, rnd, n=9, segs=8, droop=0.35):
    """Hoja carnosa de aloe: loft de secciones en «D» (cara de arriba casi
    plana y algo cóncava, dorso abombado) a lo largo de una curva que sale
    de `base` subiendo y se abre hacia fuera en el ángulo `out_ang`.
    Índice de vértice: anillo * segs + k. Devuelve (obj, n_anillos)."""
    out = Vector((math.cos(out_ang), math.sin(out_ang), 0.0))
    up = Vector((0.0, 0.0, 1.0))
    side = up.cross(out).normalized()
    pts = []
    for i in range(n + 1):
        t = i / n
        # sube y se abre; al final cae un poco (droop)
        pts.append(Vector(base) + up * (L * rise * math.sin(t * math.pi * 0.5) - L * droop * t ** 3)
                   + out * (L * (1 - rise * 0.6) * t ** 1.2))
    rings = []
    for i, p in enumerate(pts):
        t = i / n
        tan = (pts[min(i + 1, n)] - pts[max(i - 1, 0)]).normalized()
        nrm = tan.cross(side).normalized()  # hacia la cara de arriba
        if nrm.z < 0:
            nrm = -nrm
        w = W / 2 * (1 - t) ** 0.9 + 0.0008
        d = T * (1 - t) ** 0.8 + 0.0006
        ring = []
        for k in range(segs):
            a = 2 * math.pi * k / segs
            s, c = math.sin(a), math.cos(a)
            h = d * (0.2 * s if s > 0 else s)  # cara de arriba casi plana
            ring.append(p + side * (c * w) + nrm * h)
        rings.append(ring)
    o = C.ring_loft(name, rings, cap_start=True, cap_end=True)
    return o, len(rings)


def _paint_aloe(o, segs, n_rings, rnd, seed):
    spots = {}

    def f(v):
        ring = min(v.index // segs, n_rings - 1)
        t = ring / (n_rings - 1)
        c = I.lerp3(BPAL['aloe'], BPAL['aloe_hi'], 0.3 + 0.4 * t)
        if (ring * 7 + v.index * 13 + seed) % 5 == 0 and 0.1 < t < 0.8:
            c = BPAL['aloe_spot']
        c = I.lerp3(c, BPAL['aloe_tip'], max(0.0, t - 0.8) / 0.2)
        if t < 0.08:
            c = I.lerp3(c, (0.70, 0.78, 0.50), 0.6)
        spots[v.index] = c
        return tuple(c) + (0.0,)
    C.set_vertex_colors(o, f)


# ---------------------------------------------------------------------------
# HIJUELO
# ---------------------------------------------------------------------------
@_register('hijuelo')
def _b_hijuelo(v, rnd, name):
    """Hijuelo de plátano (60 cm): cormo pardo con raíces cortadas,
    pseudotallo verde con manchas vinosas y dos hojas estrechas en espada,
    una aún enrollada en cigarro."""
    p = K.Parts()
    seed = v['seed']
    corm = C.make_blob('Corm', (0.0, 0.0, 0.055), 1.0, seed, subdivisions=3, noise_strength=0.18,
                       scale=(0.075, 0.07, 0.058), relax_iterations=1)
    M.assign(corm, ['M_Wood'])

    def cc(co):
        c = I.lerp3(BPAL['corm_dark'], BPAL['corm'], 0.5 + 0.5 * I.noise3(co, 30, 1.0))
        # anillos de cicatrices de hojas viejas
        if (co.z * 90) % 1.0 < 0.18 and co.z > 0.05:
            c = I.lerp3(c, BPAL['corm_dark'], 0.6)
        return c
    I.color_fn(corm, cc, rnd, 0.01)
    p.add(corm, 'none')
    # raíces cortadas: muñones que salen de la base del cormo
    for k in range(9):
        a = k / 9 * 2 * math.pi + rnd.uniform(-0.2, 0.2)
        r0 = Vector((math.cos(a) * 0.05, math.sin(a) * 0.05, 0.025 + rnd.uniform(-0.01, 0.01)))
        r1 = r0 + Vector((math.cos(a) * 0.045, math.sin(a) * 0.045, 0.0))
        r1.z = 0.0065
        root = I.sweep(f'Root{k}', [r0, r0.lerp(r1, 0.5) + Vector((0, 0, -0.004)), r1], [0.009, 0.0075, 0.0065],
                       segs=6)
        M.assign(root, ['M_Wood'])
        I.color_fn(root, lambda co, r1=r1: BPAL['root_cut'] if (co - r1).length < 0.0045 else
                   I.lerp3(BPAL['corm'], BPAL['corm_dark'], 0.3), rnd, 0.01)
        p.add(root, 'none')
    # pseudotallo
    zs = [0.08, 0.14, 0.22, 0.30, 0.38, 0.42]
    rs = [0.05, 0.044, 0.037, 0.031, 0.026, 0.022]
    prof = [(0.0, 0.07)] + list(zip(rs, zs, strict=True)) + [(0.0, zs[-1])]
    stem = I.lathe('Stem', prof, segs=18)
    M.assign(stem, ['M_Leaf'])

    def stc(co):
        a = math.atan2(co.y, co.x)
        c = I.lerp3(BPAL['stem'], BPAL['stem_hi'], 0.5 + 0.5 * math.sin(a * 9))
        blot = I.noise3(co, 28, 3.0)
        c = I.lerp3(c, BPAL['stem_blotch'], min(1.0, max(0.0, blot) * 2.5))
        if co.z < 0.1:
            c = I.lerp3(c, BPAL['corm'], (0.1 - co.z) / 0.03)
        return c
    I.color_fn(stem, stc, rnd, 0.01)
    p.add(stem, 'none')
    # hojas en espada: salen de lo alto del pseudotallo, se arquean hacia fuera
    for k, (ang, L, W, tilt) in enumerate(((0.4, 0.30, 0.075, 0.35), (3.3, 0.24, 0.06, 0.5))):
        lf = I.leaf(f'Leaf{k}', L, W, 0.35, rnd, segments=8, fold=0.35, thick=0.0018,
                    rgb0=D.DPAL['banana_leaf'], rgb1=D.DPAL['banana_leaf_hi'])
        # la hoja crece en +Y: de pie (+Z), inclinada hacia fuera
        lf.data.transform(Matrix.Rotation(math.pi / 2 - tilt, 4, 'X'))
        lf.data.transform(Matrix.Rotation(ang, 4, 'Z'))
        lf.data.transform(Matrix.Translation((0.0, 0.0, zs[-1] - 0.03)))
        p.add(lf, 'none')
        # pecíolo que la une al tallo
        pet = I.sweep(f'Pet{k}', [(0.0, 0.0, zs[-1] - 0.06), (0.0, 0.0, zs[-1] - 0.02)], [0.009, 0.006], segs=6)
        M.assign(pet, ['M_Leaf'])
        I.tint(pet, BPAL['stem_hi'], rnd, 0.02)
        p.add(pet, 'none')
    # hoja nueva enrollada en cigarro
    cig = I.sweep('Cigar', [(0.0, 0.0, zs[-1] - 0.02), (0.004, 0.0, zs[-1] + 0.08), (0.012, 0.004, zs[-1] + 0.15)],
                  [0.012, 0.009, 0.0025], segs=8)
    M.assign(cig, ['M_Leaf'])
    I.color_fn(cig, lambda co: I.lerp3((0.40, 0.62, 0.14), (0.62, 0.74, 0.22), (co.z - zs[-1]) / 0.15), rnd, 0.01)
    p.add(cig, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# ALOE
# ---------------------------------------------------------------------------
@_register('aloe')
def _b_aloe(v, rnd, name):
    """Aloe arrancado (32 cm): roseta de catorce hojas carnosas verde jade
    con motas claras y puntas cobrizas, en tres pisos, sobre un cepellón de
    tierra con raicillas."""
    p = K.Parts()
    seed = v['seed']
    ball = C.make_blob('Soil', (0.0, 0.0, 0.03), 1.0, seed, subdivisions=3, noise_strength=0.3,
                       scale=(0.055, 0.055, 0.032), relax_iterations=1)
    M.assign(ball, ['M_Stone'])
    I.color_fn(ball, lambda co: I.lerp3(BPAL['soil'], BPAL['soil_hi'], 0.5 + 0.5 * I.noise3(co, 60)), rnd, 0.02)
    p.add(ball, 'none')
    for k in range(7):
        a = k / 7 * 2 * math.pi + 0.3
        r0 = Vector((math.cos(a) * 0.04, math.sin(a) * 0.04, 0.012))
        r1 = Vector((math.cos(a + 0.4) * 0.085, math.sin(a + 0.4) * 0.085, 0.002))
        rt = I.sweep(f'Root{k}', [r0, r0.lerp(r1, 0.5) + Vector((0, 0, 0.004)), r1], [0.0028, 0.002, 0.0008], segs=5)
        M.assign(rt, ['M_Wood'])
        I.tint(rt, (0.62, 0.46, 0.24), rnd, 0.02)
        p.add(rt, 'none')
    segs = 8
    tiers = [(6, 0.25, 0.055, 0.02, 0.5, 0.0), (5, 0.21, 0.05, 0.018, 0.7, 0.5), (3, 0.15, 0.04, 0.015, 0.9, 1.1)]
    for ti, (count, L, W, T, rise, off) in enumerate(tiers):
        for k in range(count):
            a = k / count * 2 * math.pi + off + rnd.uniform(-0.12, 0.12)
            base = (math.cos(a) * 0.012, math.sin(a) * 0.012, 0.05 + ti * 0.008)
            o, nr = _fleshy_leaf(f'Leaf{ti}{k}', base, a, L * rnd.uniform(0.9, 1.05), W, T, rise, rnd, segs=segs,
                                 droop=0.18 if ti == 0 else 0.05)
            M.assign(o, ['M_Leaf'])
            _paint_aloe(o, segs, nr, rnd, seed + ti * 10 + k)
            p.add(o, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# PASTA MEDICINAL
# ---------------------------------------------------------------------------
@_register('pasta')
def _b_pasta(v, rnd, name):
    """Pasta medicinal: valva de concha (Ø 12 cm) usada de cuenco, con un
    montón de pasta verde batida en remolino, y un trozo de hoja de aloe
    abierto que enseña el gel."""
    p = K.Parts()
    seed = v['seed']
    outer = [(0.0, 0.0)] + I.smooth_profile([(0.02, 0.0), (0.038, 0.007), (0.052, 0.017), (0.058, 0.026)],
                                             per_seg=2)
    prof = CT._vessel_prof(outer, 0.004)
    shell = I.lathe('Shell', prof, segs=48)
    ribs = 14
    for q in shell.data.vertices:
        a = math.atan2(q.co.y, q.co.x)
        rr = math.hypot(q.co.x, q.co.y)
        if rr < 1e-5:
            continue
        k = 1.0 + 0.02 * math.cos(a * ribs) * min(1.0, rr / 0.03)
        q.co.x *= k
        q.co.y *= k
        if q.co.z > 0.02:
            q.co.z += 0.0015 * math.cos(a * ribs)
    shell.data.update()
    M.assign(shell, ['M_Stone'])

    def sc(co):
        a = math.atan2(co.y, co.x)
        rr = math.hypot(co.x, co.y)
        rib = 0.5 + 0.5 * math.cos(a * ribs)
        if co.z > 0.004 and rr < _prof_r(outer, co.z) - 0.0025:
            return I.lerp3(BPAL['shell_in'], BPAL['shell_in_hi'], 0.4 + 0.4 * I.noise3(co, 60))
        c = I.lerp3(BPAL['shell_rib'], BPAL['shell_out'], rib)
        # bandas de crecimiento concéntricas
        if (rr * 160) % 1.0 < 0.2:
            c = I.lerp3(c, BPAL['shell_rib'], 0.5)
        return c
    I.color_fn(shell, sc, rnd, 0.01)
    p.add(shell, 'none')
    # pasta: montón en remolino
    zb = 0.012
    mprof = [(0.0, zb)] + [(0.041 * (1 - (k / 6)) ** 0.8 + 0.001, zb + 0.026 * math.sin((k / 6) * math.pi / 2) ** 1.4)
                           for k in range(7)]
    mprof[-1] = (0.0, zb + 0.028)
    mound = I.lathe('Paste', mprof, segs=28)
    for q in mound.data.vertices:
        a = math.atan2(q.co.y, q.co.x)
        q.co.z += 0.0025 * math.sin(a * 3 + q.co.z * 300)
    mound.data.update()
    M.assign(mound, ['M_Leaf'])

    def pc(co):
        a = math.atan2(co.y, co.x)
        swirl = 0.5 + 0.5 * math.sin(a + co.z * 420)
        c = I.lerp3(BPAL['paste'], BPAL['paste_hi'], swirl * 0.8)
        if I.noise3(co, 300, 4.0) > 0.45:
            c = BPAL['paste_fleck']
        return c
    I.color_fn(mound, pc, rnd, 0.01)
    p.add(mound, 'none')
    # trozo de hoja de aloe cortado a lo largo, junto a la concha
    segs = 8
    o, nr = _fleshy_leaf('AloeCut', (0.07, -0.02, 0.006), -0.3, 0.1, 0.03, 0.009, 0.0, rnd, segs=segs, droop=0.0)
    M.assign(o, ['M_Leaf'])
    _paint_aloe(o, segs, nr, rnd, seed)
    p.add(o, 'none')
    gel = C.make_blob('Gel', (0.0, 0.0, 0.0), 1.0, seed, subdivisions=2, noise_strength=0.1,
                      scale=(0.03, 0.008, 0.0022))
    gel.data.transform(Matrix.Rotation(-0.3, 4, 'Z'))
    gel.data.transform(Matrix.Translation((0.07 + math.cos(-0.3) * 0.035, -0.02 + math.sin(-0.3) * 0.035, 0.0075)))
    M.assign(gel, ['M_Leaf'])
    I.tint(gel, BPAL['aloe_gel'], rnd, 0.02)
    p.add(gel, 'none')
    return p.finish(name)


def _prof_r(outer, z):
    for (r0, z0), (r1, z1) in pairwise(outer):
        if z0 <= z <= z1 and z1 > z0:
            return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    return outer[-1][0]
