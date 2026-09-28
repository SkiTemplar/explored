"""
items_frutas.py — comida vegetal y huevos de items.json: cocos, plátano,
mango, limón, lima, maracuyá, piña, batata, taro, yuca, setas, huevos,
fruta seca, fruta fermentada y comida quemada.

Las versiones cocinadas (batata asada, taro hervido, yuca cocida, huevo a
la brasa) son el MISMO modelo que la cruda con otra paleta de color de
vértice: el constructor recibe `cooked` en la variante.

Objetos sueltos: pivote en la base (z = 0), centrados en XY, en su postura
de reposo (los alargados tumbados a lo largo de +X). Escala real en metros.
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _items as I  # noqa: E402
import _materials as M  # noqa: E402
import common as C  # noqa: E402
import kit_construccion as K  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

GROUP = I.GROUP
PAL = I.PAL

FPAL = {
    'coco_green': (0.10, 0.30, 0.025),
    'coco_green_hi': (0.30, 0.42, 0.05),
    'coco_calyx': (0.26, 0.17, 0.05),
    'coco_nut': (0.20, 0.09, 0.03),
    'coco_fiber': (0.36, 0.19, 0.07),
    'coco_eye': (0.035, 0.018, 0.01),
    'banana': (0.80, 0.55, 0.03),
    'banana_green': (0.36, 0.45, 0.04),
    'banana_tip': (0.09, 0.05, 0.02),
    'mango_green': (0.22, 0.40, 0.04),
    'mango_yellow': (0.85, 0.50, 0.03),
    'mango_red': (0.62, 0.08, 0.03),
    'lemon': (0.86, 0.62, 0.03),
    'lemon_pale': (0.80, 0.70, 0.18),
    'lime': (0.16, 0.42, 0.03),
    'lime_pale': (0.42, 0.55, 0.08),
    'passion': (0.16, 0.025, 0.11),
    'passion_spot': (0.42, 0.24, 0.36),
    'stem': (0.14, 0.20, 0.04),
    'stem_dry': (0.20, 0.12, 0.05),
    'pine_eye': (0.80, 0.42, 0.04),
    'pine_rim': (0.24, 0.20, 0.04),
    'pine_leaf': (0.05, 0.20, 0.07),
    'pine_leaf_tip': (0.22, 0.32, 0.10),
    'batata': (0.36, 0.06, 0.07),
    'batata_dark': (0.20, 0.03, 0.04),
    'batata_flesh': (0.90, 0.38, 0.04),
    'char': (0.025, 0.016, 0.012),
    'roast': (0.16, 0.06, 0.025),
    'taro': (0.26, 0.14, 0.07),
    'taro_ring': (0.10, 0.05, 0.022),
    'taro_bud': (0.50, 0.14, 0.26),
    'taro_boiled': (0.62, 0.50, 0.62),
    'taro_fleck': (0.32, 0.16, 0.38),
    'yuca_bark': (0.24, 0.12, 0.05),
    'yuca_ring': (0.40, 0.23, 0.10),
    'yuca_flesh': (0.84, 0.76, 0.56),
    'yuca_pink': (0.62, 0.30, 0.28),
    'yuca_cooked': (0.84, 0.64, 0.28),
    'yuca_cooked_dk': (0.62, 0.42, 0.14),
    'mush_cap': (0.48, 0.15, 0.03),
    'mush_cap_edge': (0.70, 0.40, 0.14),
    'mush_gill': (0.80, 0.66, 0.46),
    'mush_stem': (0.84, 0.76, 0.58),
    'egg': (0.50, 0.70, 0.56),
    'egg_speck': (0.26, 0.12, 0.05),
    'egg_roast': (0.46, 0.26, 0.10),
    'egg_soot': (0.06, 0.035, 0.02),
    'dried': (0.60, 0.24, 0.03),
    'dried_edge': (0.30, 0.09, 0.02),
    'ferment': (0.56, 0.36, 0.05),
    'ferment_bruise': (0.17, 0.07, 0.02),
    'ferment_purple': (0.22, 0.06, 0.12),
    'foam': (0.86, 0.80, 0.52),
    'burnt': (0.022, 0.015, 0.012),
    'burnt_ridge': (0.10, 0.05, 0.025),
    'ember': (0.70, 0.14, 0.01),
}


VARIANTS = [
    dict(name='Item_CocoVerde', item_id='coco_verde', seed=4601, builder='coco_verde', tri_budget=(300, 4000)),
    dict(name='Item_CocoMaduro', item_id='coco_maduro', seed=4602, builder='coco_maduro', tri_budget=(300, 4000)),
    dict(name='Item_Platano', item_id='platano', seed=4603, builder='platano', tri_budget=(100, 2500)),
    dict(name='Item_MangoFruta', item_id='mango_fruta', seed=4604, builder='mango', tri_budget=(300, 3500)),
    dict(name='Item_Limon', item_id='limon', seed=4605, builder='limon', tri_budget=(300, 3500)),
    dict(name='Item_LimaSilvestre', item_id='lima_silvestre', seed=4606, builder='lima', tri_budget=(200, 3000)),
    dict(name='Item_Maracuya', item_id='maracuya', seed=4607, builder='maracuya', tri_budget=(200, 3000)),
    dict(name='Item_Pina', item_id='pina', seed=4608, builder='pina', tri_budget=(800, 7000)),
    dict(name='Item_Batata', item_id='batata', seed=4609, builder='batata', tri_budget=(300, 4000)),
    dict(name='Item_BatataAsada', item_id='batata_asada', seed=4609, builder='batata',
         tri_budget=(300, 4000), cooked=True),
    dict(name='Item_Taro', item_id='taro', seed=4611, builder='taro', tri_budget=(300, 4000)),
    dict(name='Item_TaroHervido', item_id='taro_hervido', seed=4611, builder='taro',
         tri_budget=(300, 4000), cooked=True),
    dict(name='Item_Yuca', item_id='yuca', seed=4613, builder='yuca', tri_budget=(300, 4000)),
    dict(name='Item_YucaCocida', item_id='yuca_cocida', seed=4613, builder='yuca',
         tri_budget=(300, 4000), cooked=True),
    dict(name='Item_SetaComestible', item_id='seta_comestible', seed=4615, builder='seta', tri_budget=(300, 4000)),
    dict(name='Item_Huevo', item_id='huevo', seed=4616, builder='huevo', tri_budget=(200, 3000)),
    dict(name='Item_HuevoBrasa', item_id='huevo_brasa', seed=4616, builder='huevo',
         tri_budget=(200, 3000), cooked=True),
    dict(name='Item_FrutaSeca', item_id='fruta_seca', seed=4618, builder='fruta_seca', tri_budget=(200, 4000)),
    dict(name='Item_FrutaFermentada', item_id='fruta_fermentada', seed=4619, builder='fruta_fermentada',
         tri_budget=(200, 4000)),
    dict(name='Item_ComidaQuemada', item_id='comida_quemada', seed=4620, builder='comida_quemada',
         tri_budget=(150, 3000)),
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
def _fruit(name, prof, segs=24, per_seg=3, mat='M_Leaf', sx=1.0, sy=1.0):
    """Sólido de revolución a lo largo de +Z desde un perfil (r, z) de
    control, remuestreado suave. El perfil debe empezar y acabar en r = 0."""
    o = I.lathe(name, I.smooth_profile(prof, per_seg), segs=segs, sx=sx, sy=sy)
    M.assign(o, [mat])
    return o


def _ellipsoid_prof(length, radius, n=8, tip0=0.0, tip1=0.0, bulge=0.0, power=0.9):
    """Perfil de un ovoide de largo `length` y radio `radius`; tip0/tip1
    añaden pezón en los polos; bulge > 0 engorda la mitad superior (huevo)."""
    pts = [(0.0, 0.0)]
    for i in range(1, n):
        t = i / n
        r = radius * math.sin(t * math.pi) ** power * (1.0 + bulge * (t - 0.5))
        pts.append((r, length * t))
    pts.append((0.0, length))
    if tip0:
        pts[0] = (0.0, -tip0)
    if tip1:
        pts[-1] = (0.0, length + tip1)
    return pts


def _displace(o, amp, scale, seed=0.0, zmin=-1e9, zmax=1e9):
    """Empuja cada vértice a lo largo de su dirección radial (XY) con ruido."""
    for vv in o.data.vertices:
        if not (zmin <= vv.co.z <= zmax):
            continue
        d = Vector((vv.co.x, vv.co.y, 0.0))
        if d.length < 1e-5:
            continue
        vv.co += d.normalized() * amp * I.noise3(vv.co, scale, seed)
    o.data.update()


def _stem(p, base, direction, length, r0, r1, rgb, rnd, name='Stem'):
    d = Vector(direction).normalized()
    pts = [Vector(base) + d * length * t for t in (0.0, 0.5, 1.0)]
    o = I.sweep(name, pts, [r0, (r0 + r1) / 2, r1], segs=6)
    M.assign(o, ['M_Wood'])
    I.tint(o, rgb, rnd, 0.02)
    p.add(o, 'none')
    return o


def _lay(p, angle=math.pi / 2):
    """Tumba sobre el costado (Z -> X)."""
    p.transform(Matrix.Rotation(angle, 4, 'Y'))


def _finish_smooth(p, name):
    obj = p.finish(name)
    I.smooth_all(obj)
    return obj


# ---------------------------------------------------------------------------
# COCOS
# ---------------------------------------------------------------------------
@_register('coco_verde')
def _b_coco_verde(v, rnd, name):
    """Coco verde con cáscara (24 cm): ovoide de tres caras suaves, cáliz
    pardo de tres sépalos en el pedúnculo y punta roma en el otro extremo."""
    p = K.Parts()
    L, R = 0.24, 0.095
    prof = [(0.0, 0.0), (0.03, 0.004), (0.07, 0.03), (R, 0.09), (R * 0.98, 0.15), (0.075, 0.205),
            (0.035, 0.232), (0.0, L)]
    o = _fruit('Husk', prof, segs=30, per_seg=3)
    for vv in o.data.vertices:
        a = math.atan2(vv.co.y, vv.co.x)
        t = vv.co.z / L
        k = 1.0 + 0.07 * math.cos(3 * a) * math.sin(t * math.pi) ** 0.6
        vv.co.x *= k
        vv.co.y *= k
    o.data.update()

    def col(co):
        a = math.atan2(co.y, co.x)
        ridge = max(0.0, math.cos(3 * a)) ** 4
        t = co.z / L
        c = I.lerp3(FPAL['coco_green'], FPAL['coco_green_hi'], ridge * 0.6 + max(0.0, t - 0.75) * 2.0)
        return I.lerp3(c, FPAL['coco_green_hi'], 0.25 * max(0.0, I.noise3(co, 40, 2.0)))
    I.color_fn(o, col, rnd, 0.012)
    p.add(o, 'none')
    # cáliz: tres sépalos aplastados sobre el polo del pedúnculo (+Z)
    for k in range(3):
        a = k * 2 * math.pi / 3 + 0.3
        c = (math.cos(a) * 0.018, math.sin(a) * 0.018, L - 0.006)
        s = C.make_blob(f'Sepal{k}', c, 1.0, v['seed'] + k, subdivisions=2, noise_strength=0.08,
                        scale=(0.022, 0.016, 0.007), relax_iterations=1)
        s.data.transform(Matrix.Translation(Vector(c)) @ Matrix.Rotation(a, 4, 'Z') @ Matrix.Translation(-Vector(c)))
        M.assign(s, ['M_Wood'])
        I.tint(s, FPAL['coco_calyx'], rnd, 0.02)
        p.add(s, 'none')
    _stem(p, (0, 0, L - 0.004), (0.1, 0, 1), 0.02, 0.008, 0.006, FPAL['stem_dry'], rnd)
    _lay(p, math.pi / 2 - 0.12)
    return _finish_smooth(p, name)


@_register('coco_maduro')
def _b_coco_maduro(v, rnd, name):
    """Coco maduro pelado (13 cm): nuez parda con fibra en vetas, tres
    aristas suaves y los tres «ojos» oscuros en un polo."""
    p = K.Parts()
    L, R = 0.13, 0.058
    prof = [(0.0, 0.0), (0.028, 0.006), (R, 0.045), (R * 0.98, 0.08), (0.04, 0.118), (0.0, L)]
    o = _fruit('Nut', prof, segs=28, per_seg=3, mat='M_Wood')
    for vv in o.data.vertices:
        a = math.atan2(vv.co.y, vv.co.x)
        k = 1.0 + 0.05 * math.cos(3 * a)
        vv.co.x *= k
        vv.co.y *= k
    _displace(o, 0.0015, 90, 3.0)

    def col(co):
        a = math.atan2(co.y, co.x)
        streak = 0.5 + 0.5 * math.sin(a * 23 + I.noise3(co, 30, 1.0) * 3)
        c = I.lerp3(FPAL['coco_nut'], FPAL['coco_fiber'], streak ** 3)
        c = I.lerp3(c, (0.08, 0.035, 0.012), max(0.0, 0.5 - streak) * 1.2)
        return I.lerp3(c, FPAL['coco_fiber'], 0.4 * max(0.0, math.cos(3 * a)) ** 6)
    I.color_fn(o, col, rnd, 0.015)
    p.add(o, 'none')
    # ojos: hoyuelos oscuros en triángulo alrededor del polo +Z
    for k in range(3):
        a = k * 2 * math.pi / 3 + math.pi / 3
        rr = 0.013
        z = L - 0.006
        e = C.make_blob(f'Eye{k}', (math.cos(a) * rr, math.sin(a) * rr, z), 0.0055, v['seed'] + k,
                        subdivisions=2, noise_strength=0.05, scale=(1.0, 1.0, 0.7))
        M.assign(e, ['M_Wood'])
        I.tint(e, FPAL['coco_eye'], rnd, 0.005)
        p.add(e, 'none')
    # mechón de fibra en el polo
    for k in range(5):
        a = rnd.uniform(0, 2 * math.pi)
        base = Vector((math.cos(a) * 0.004, math.sin(a) * 0.004, L - 0.002))
        d = Vector((math.cos(a) * 0.5, math.sin(a) * 0.5, 1.0)).normalized()
        f = I.sweep(f'Fib{k}', [base, base + d * 0.012, base + d * 0.022 + Vector((0, 0, -0.004))],
                    [0.0016, 0.0012, 0.0006], segs=4)
        M.assign(f, ['M_Wood'])
        I.tint(f, FPAL['coco_fiber'], rnd, 0.03)
        p.add(f, 'none')
    _lay(p, -math.pi / 2 + 0.35)
    return _finish_smooth(p, name)


# ---------------------------------------------------------------------------
# FRUTAS
# ---------------------------------------------------------------------------
@_register('platano')
def _b_platano(v, rnd, name):
    """Plátano maduro (19 cm) tumbado boca arriba: sección de cinco caras
    (aristas marcadas), pedúnculo verdoso y punta negra."""
    p = K.Parts()
    n = 22
    L = 0.19
    pts, radii = [], []
    for i in range(n + 1):
        t = i / n
        a = (t - 0.5) * 1.5
        pts.append((math.sin(a) * L / 1.5, 0.0, (1 - math.cos(a)) * L / 1.5))
        if t < 0.12:
            r = 0.0055
        else:
            u = (t - 0.12) / 0.88
            r = 0.006 + 0.012 * math.sin(min(1.0, u * 1.08) * math.pi) ** 0.55
        radii.append(r)
    radii[-1] = 0.004
    o = I.sweep('Banana', pts, radii, segs=5)
    M.assign(o, ['M_Leaf'])
    xs = [q[0] for q in pts]
    x0, x1 = xs[0], xs[-1]

    def col(co):
        t = (co.x - x0) / (x1 - x0)
        if t < 0.1:
            return FPAL['banana_green']
        c = I.lerp3(FPAL['banana_green'], FPAL['banana'], min(1.0, (t - 0.1) / 0.1))
        if t > 0.96:
            c = FPAL['banana_tip']
        spot = max(0.0, I.noise3(co, 180, 5.0) - 0.45) * 3
        return I.lerp3(c, FPAL['banana_tip'], min(0.8, spot))
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    return p.finish(name)


@_register('mango')
def _b_mango(v, rnd, name):
    """Mango (12 cm) tumbado: óvalo arriñonado, verde junto al pedúnculo,
    amarillo y con rubor rojo en la cara de sol."""
    p = K.Parts()
    L = 0.12
    prof = [(0.0, 0.0), (0.02, 0.004), (0.038, 0.03), (0.042, 0.065), (0.036, 0.098), (0.02, 0.116), (0.0, L)]
    o = _fruit('Mango', prof, segs=24, per_seg=3, sy=0.82)
    for vv in o.data.vertices:
        t = vv.co.z / L
        vv.co.x += 0.012 * math.sin(t * math.pi) - 0.006 * t
    o.data.update()

    def col(co):
        t = co.z / L
        c = I.lerp3(FPAL['mango_yellow'], FPAL['mango_green'], max(0.0, t - 0.55) / 0.45)
        blush = max(0.0, co.x / 0.045) * math.sin(t * math.pi) + 0.25 * I.noise3(co, 50, 1.0)
        return I.lerp3(c, FPAL['mango_red'], max(0.0, min(0.85, blush)))
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    _stem(p, (-0.004, 0, L - 0.003), (-0.2, 0, 1), 0.014, 0.0028, 0.002, FPAL['stem_dry'], rnd)
    _lay(p, math.pi / 2)
    return _finish_smooth(p, name)


def _citrus(p, rnd, v, L, R, tips, rgb, rgb_pale, leaf):
    prof = _ellipsoid_prof(L, R, n=8, tip0=tips[0], tip1=tips[1])
    o = _fruit('Peel', prof, segs=24, per_seg=3)
    _displace(o, R * 0.02, 160, 1.0)

    def col(co):
        c = I.lerp3(rgb, rgb_pale, max(0.0, -co.x / R) * 0.5)
        return I.lerp3(c, rgb_pale, 0.3 * max(0.0, I.noise3(co, 300, 2.0)))
    I.color_fn(o, col, rnd, 0.012)
    p.add(o, 'none')
    top = L + tips[1]
    _stem(p, (0, 0, top - 0.002), (0.25, 0, 1), 0.008, 0.0022, 0.0016, FPAL['stem'], rnd)
    if leaf:
        lf = I.leaf('Leaf', leaf, leaf * 0.42, 0.25, rnd, segments=6, fold=0.3, thick=0.0012)
        lf.data.transform(Matrix.Translation((0.0, 0.0, top + 0.004)) @ Matrix.Rotation(-0.35, 4, 'X')
                          @ Matrix.Rotation(0.4, 4, 'Z'))
        lf.data.update()
        p.add(lf, 'none')


@_register('limon')
def _b_limon(v, rnd, name):
    """Limón (8,5 cm) amarillo vivo con pezón en los dos polos, piel de
    poros y una hoja en el rabillo."""
    p = K.Parts()
    _citrus(p, rnd, v, 0.08, 0.029, (0.004, 0.005), FPAL['lemon'], FPAL['lemon_pale'], leaf=0.065)
    _lay(p, math.pi / 2)
    return _finish_smooth(p, name)


@_register('lima')
def _b_lima(v, rnd, name):
    """Lima silvestre (5 cm): casi esférica, verde intenso con la cara de
    apoyo más pálida."""
    p = K.Parts()
    _citrus(p, rnd, v, 0.048, 0.024, (0.0, 0.002), FPAL['lime'], FPAL['lime_pale'], leaf=0.0)
    _lay(p, math.pi / 2 - 0.3)
    return _finish_smooth(p, name)


@_register('maracuya')
def _b_maracuya(v, rnd, name):
    """Maracuyá (7 cm) morada y algo arrugada, con motas claras y el
    pedúnculo verde."""
    p = K.Parts()
    L = 0.07
    o = _fruit('Passion', _ellipsoid_prof(L, 0.032, n=8), segs=22, per_seg=3)
    _displace(o, 0.0018, 70, 4.0)

    def col(co):
        spot = max(0.0, I.noise3(co, 520, 3.0) - 0.4) * 5
        return I.lerp3(FPAL['passion'], FPAL['passion_spot'], min(0.9, spot))
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    _stem(p, (0, 0, L - 0.002), (0.3, 0.1, 1), 0.02, 0.0025, 0.002, FPAL['stem'], rnd)
    _lay(p, math.pi / 2 - 0.25)
    return _finish_smooth(p, name)


@_register('pina')
def _b_pina(v, rnd, name):
    """Piña (32 cm con la corona) de pie: cuerpo de «ojos» hexagonales en
    doble espiral, dorado con el reborde verde, y corona de hojas rígidas
    azuladas."""
    p = K.Parts()
    H, R = 0.17, 0.062
    prof = [(0.0, 0.0), (0.035, 0.002), (R * 0.95, 0.03), (R, 0.08), (R * 0.93, 0.13), (0.035, 0.168), (0.0, H)]
    segs = 48
    o = _fruit('Body', prof, segs=segs, per_seg=6)
    n_sp, pitch = 8, 0.021

    def cell(co):
        a = (math.atan2(co.y, co.x) / (2 * math.pi)) * n_sp
        u = a + co.z / pitch
        w = a - co.z / pitch
        return (math.cos(u * 2 * math.pi) + math.cos(w * 2 * math.pi)) / 2  # 1 centro del ojo, -1 surco
    for vv in o.data.vertices:
        d = Vector((vv.co.x, vv.co.y, 0.0))
        if d.length < 1e-4:
            continue
        taper = max(0.0, math.sin(max(0.0, min(1.0, vv.co.z / H)) * math.pi)) ** 0.5
        vv.co += d.normalized() * 0.004 * cell(vv.co) * taper
    o.data.update()

    def col(co):
        c = cell(co)
        base = I.lerp3(FPAL['pine_rim'], FPAL['pine_eye'], max(0.0, min(1.0, (c + 0.3) / 1.0)))
        return I.lerp3(base, (0.10, 0.05, 0.02), max(0.0, c - 0.85) * 5)  # pinchito oscuro
    I.color_fn(o, col, rnd, 0.015)
    p.add(o, 'none')
    # corona: tres pisos de hojas rígidas y estrechas
    k = 0
    for tier, (count, tilt, ln) in enumerate(((7, 0.75, 0.10), (6, 0.45, 0.14), (5, 0.18, 0.15))):
        for j in range(count):
            az = j / count * 2 * math.pi + tier * 0.5 + rnd.uniform(-0.15, 0.15)
            L = ln * rnd.uniform(0.9, 1.1)
            lf = I.leaf(f'Crown{k}', L, 0.022, 0.18 + tilt * 0.3, rnd, segments=6, fold=0.5, thick=0.0015,
                        rgb0=FPAL['pine_leaf'], rgb1=FPAL['pine_leaf_tip'])
            lf.data.transform(Matrix.Translation((0.0, 0.0, H - 0.008))
                              @ Matrix.Rotation(az - math.pi / 2, 4, 'Z')
                              @ Matrix.Rotation(math.pi / 2 - tilt, 4, 'X'))
            lf.data.update()
            p.add(lf, 'none')
            k += 1
    return _finish_smooth(p, name)


# ---------------------------------------------------------------------------
# TUBÉRCULOS (crudo / cocinado = misma malla, otra paleta)
# ---------------------------------------------------------------------------
@_register('batata')
def _b_batata(v, rnd, name):
    """Batata (19 cm): huso abultado y algo curvo con raicillas en las
    puntas. Cruda: piel granate. Asada: piel tostada con manchas de carbón
    y una grieta a lo largo que enseña la pulpa naranja."""
    p = K.Parts()
    cooked = v.get('cooked', False)
    L = 0.17
    n = 16
    pts, radii = [], []
    for i in range(n + 1):
        t = i / n
        pts.append((0.012 * math.sin(t * math.pi), 0.0, L * t))
        radii.append(0.004 + 0.036 * math.sin(t * math.pi) ** 0.75 * (1 + 0.15 * math.sin(t * 7 + 1)))
    o = I.sweep('Tuber', pts, radii, segs=14)
    M.assign(o, ['M_Wood'])
    _displace(o, 0.003, 60, 2.0)

    def col(co):
        if not cooked:
            streak = max(0.0, I.noise3(Vector((co.x * 3, co.y * 3, co.z * 0.6)), 60, 1.0))
            return I.lerp3(FPAL['batata'], FPAL['batata_dark'], min(1.0, streak * 2))
        # grieta: franja estrecha en la cara +X (arriba tras tumbarla)
        crack = abs(co.y + 0.004 * math.sin(co.z * 90)) < 0.007 and co.x > 0.02 and 0.2 * L < co.z < 0.8 * L
        if crack:
            return FPAL['batata_flesh']
        c = I.lerp3(FPAL['roast'], FPAL['char'], max(0.0, I.noise3(co, 45, 4.0) + 0.1) * 1.6)
        return c
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    # raicillas en las dos puntas
    for k, (z, s) in enumerate(((0.0, -1), (L, 1))):
        for j in range(2):
            a = rnd.uniform(0, 2 * math.pi)
            b = Vector((pts[0 if s < 0 else -1][0], 0.0, z))
            d = Vector((math.cos(a) * 0.4, math.sin(a) * 0.4, s)).normalized()
            ln = rnd.uniform(0.02, 0.035)
            r = I.sweep(f'Root{k}{j}', [b, b + d * ln * 0.5, b + d * ln + Vector((0, 0, -0.004))],
                        [0.0022, 0.0014, 0.0006], segs=4)
            M.assign(r, ['M_Wood'])
            I.tint(r, FPAL['char'] if cooked else FPAL['batata_dark'], rnd, 0.01)
            p.add(r, 'none')
    _lay(p, -math.pi / 2)
    return _finish_smooth(p, name)


@_register('taro')
def _b_taro(v, rnd, name):
    """Taro (10 cm): cormo redondo con anillos de fibra y la yema rosada del
    tallo cortado. Hervido: pelado, lila claro con motas moradas."""
    p = K.Parts()
    cooked = v.get('cooked', False)
    H, R = 0.10, 0.045
    prof = [(0.0, 0.0), (0.025, 0.003), (R, 0.03), (R * 1.02, 0.05), (R * 0.85, 0.078), (0.018, 0.098),
            (0.0, H)]
    o = _fruit('Corm', prof, segs=24, per_seg=4, mat='M_Wood')
    pitch = 0.011
    if not cooked:
        # anillos: rebordes cada `pitch` (cicatrices de las hojas)
        for vv in o.data.vertices:
            d = Vector((vv.co.x, vv.co.y, 0.0))
            if d.length > 1e-4:
                ring = (vv.co.z / pitch) % 1.0
                vv.co += d.normalized() * 0.0018 * max(0.0, 1 - abs(ring - 0.5) * 4)
        o.data.update()
    _displace(o, 0.002, 50, 6.0)

    def col(co):
        if cooked:
            fleck = max(0.0, I.noise3(co, 400, 3.0) - 0.3) * 4
            return I.lerp3(FPAL['taro_boiled'], FPAL['taro_fleck'], min(0.9, fleck))
        ring = (co.z / pitch) % 1.0
        c = I.lerp3(FPAL['taro'], FPAL['taro_ring'], max(0.0, 1 - abs(ring - 0.5) * 3.5))
        return I.lerp3(c, FPAL['taro_bud'], max(0.0, co.z / H - 0.9) * 8)
    I.color_fn(o, col, rnd, 0.012)
    p.add(o, 'none')
    if not cooked:
        # muñón del tallo cortado
        _stem(p, (0, 0, H - 0.004), (0.15, 0, 1), 0.016, 0.009, 0.008, FPAL['taro_bud'], rnd)
        for j in range(4):
            a = rnd.uniform(0, 2 * math.pi)
            b = Vector((math.cos(a) * 0.01, math.sin(a) * 0.01, 0.004))
            d = Vector((math.cos(a), math.sin(a), -0.6)).normalized()
            r = I.sweep(f'Root{j}', [b, b + d * 0.012, b + d * 0.02 + Vector((0, 0, 0.004))],
                        [0.0018, 0.0012, 0.0005], segs=4)
            M.assign(r, ['M_Wood'])
            I.tint(r, FPAL['taro_ring'], rnd, 0.01)
            p.add(r, 'none')
    _lay(p, math.pi / 2 - 0.35)
    return _finish_smooth(p, name)


@_register('yuca')
def _b_yuca(v, rnd, name):
    """Raíz de yuca (32 cm): larga, ahusada y algo curva, con la corteza
    parda anillada y el corte blanco con su anillo rosado. Cocida: pelada,
    amarillo cera."""
    p = K.Parts()
    cooked = v.get('cooked', False)
    L = 0.32
    n = 18
    pts = I.crooked(L, rnd, n=n, bend=0.04, wobble=0.004)
    radii = [(0.03 - 0.022 * (i / n) ** 1.2) * (1 + 0.06 * math.sin(i * 1.7)) for i in range(n + 1)]
    radii[-1] = 0.003
    o = I.sweep('Root', pts, radii, segs=12, cap=True)
    M.assign(o, ['M_Wood'])
    _displace(o, 0.0015, 70, 2.0)

    def col(co):
        if cooked:
            return I.lerp3(FPAL['yuca_cooked'], FPAL['yuca_cooked_dk'],
                           max(0.0, I.noise3(co, 50, 1.0)) * 1.4)
        ring = max(0.0, math.sin(co.z * 260 + I.noise3(co, 30, 2.0) * 2)) ** 6
        c = I.lerp3(FPAL['yuca_bark'], FPAL['yuca_ring'], ring)
        return I.lerp3(c, FPAL['yuca_ring'], max(0.0, I.noise3(co, 120, 5.0)) * 0.5)
    I.color_fn(o, col, rnd, 0.012)
    p.add(o, 'none')
    # tapa del corte (z = 0): disco con pulpa y anillo de corteza interior
    r0 = radii[0]
    cap = I.lathe('Cut', [(0.0, -0.0015), (r0 * 0.6, -0.0015), (r0 * 0.97, -0.0008), (r0 * 0.97, 0.001),
                          (0.0, 0.001)], segs=12)
    M.assign(cap, ['M_Wood'])

    def ccol(vv):
        rr = math.hypot(vv.co.x, vv.co.y) / r0
        if cooked:
            c = I.lerp3(FPAL['yuca_cooked'], (0.90, 0.78, 0.45), 1 - rr)
        else:
            c = FPAL['yuca_flesh'] if rr < 0.8 else FPAL['yuca_pink']
        return tuple(c) + (0.0,)
    C.set_vertex_colors(cap, ccol)
    cap.data.transform(Matrix.Translation(Vector(pts[0])))
    p.add(cap, 'none')
    _lay(p, math.pi / 2)
    return _finish_smooth(p, name)


# ---------------------------------------------------------------------------
# SETAS Y HUEVOS
# ---------------------------------------------------------------------------
def _mushroom(p, rnd, seed, base, scale, lean):
    cap_r, cap_h, st_h = 0.034 * scale, 0.02 * scale, 0.045 * scale
    stem = _fruit('Stipe', [(0.0, 0.0), (0.011 * scale, 0.0), (0.0095 * scale, st_h * 0.5),
                            (0.008 * scale, st_h), (0.0, st_h)], segs=12, per_seg=2, mat='M_Wood')
    I.tint(stem, FPAL['mush_stem'], rnd, 0.02)
    cap = _fruit('Cap', [(0.0, st_h - 0.004 * scale), (cap_r * 0.35, st_h - 0.002 * scale),
                         (cap_r * 0.9, st_h + 0.001 * scale), (cap_r, st_h + 0.005 * scale),
                         (cap_r * 0.85, st_h + cap_h * 0.75), (cap_r * 0.4, st_h + cap_h),
                         (0.0, st_h + cap_h * 1.05)], segs=20, per_seg=3, mat='M_Wood')
    _displace(cap, 0.0015 * scale, 60, seed)
    gill_z = st_h + 0.0035 * scale

    def ccol(co):
        if co.z < gill_z:
            return FPAL['mush_gill']
        t = math.hypot(co.x, co.y) / cap_r
        c = I.lerp3(FPAL['mush_cap'], FPAL['mush_cap_edge'], max(0.0, t - 0.8) / 0.2)
        return I.lerp3(c, FPAL['mush_cap_edge'], max(0.0, I.noise3(co, 200 / scale, seed) - 0.45) * 1.5)
    I.color_fn(cap, ccol, rnd, 0.01)
    m = Matrix.Translation(Vector(base)) @ Matrix.Rotation(lean[0], 4, 'X') @ Matrix.Rotation(lean[1], 4, 'Y')
    for o in (stem, cap):
        o.data.transform(m)
        o.data.update()
        p.add(o, 'none')


@_register('seta')
def _b_seta(v, rnd, name):
    """Dos setas comestibles unidas por el pie (9 cm): sombrero anaranjado
    con el borde claro, láminas crema y pie blanquecino."""
    p = K.Parts()
    _mushroom(p, rnd, 1.0, (0.0, 0.0, 0.0), 1.0, (0.0, 0.12))
    _mushroom(p, rnd, 2.0, (0.018, -0.016, 0.0), 0.68, (0.3, 0.4))
    return _finish_smooth(p, name)


@_register('huevo')
def _b_huevo(v, rnd, name):
    """Huevo de ave marina (6,5 cm) tumbado: verde agua con motas pardas.
    A la brasa: cáscara tostada con hollín."""
    p = K.Parts()
    cooked = v.get('cooked', False)
    L = 0.065
    o = _fruit('Egg', _ellipsoid_prof(L, 0.024, n=10, bulge=-0.3, power=0.6), segs=22, per_seg=3, mat='M_Stone')

    def col(co):
        if cooked:
            soot = max(0.0, I.noise3(co, 90, 2.0) + (0.2 - co.x / 0.03)) * 1.2
            return I.lerp3(FPAL['egg_roast'], FPAL['egg_soot'], min(0.9, soot))
        speck = max(0.0, I.noise3(co, 500, 1.0) - 0.3) * 5 + max(0.0, I.noise3(co, 160, 7.0) - 0.45) * 3
        return I.lerp3(FPAL['egg'], FPAL['egg_speck'], min(0.9, speck * (0.5 + co.z / L)))
    I.color_fn(o, col, rnd, 0.008)
    p.add(o, 'none')
    _lay(p, math.pi / 2)
    return _finish_smooth(p, name)


# ---------------------------------------------------------------------------
# PREPARADOS
# ---------------------------------------------------------------------------
@_register('fruta_seca')
def _b_fruta_seca(v, rnd, name):
    """Montoncito de tiras de fruta seca (mango y plátano secados al sol):
    cinco tiras arrugadas y algo curvadas, ámbar con el borde tostado."""
    p = K.Parts()
    for k in range(5):
        ln = rnd.uniform(0.07, 0.09)
        w = rnd.uniform(0.022, 0.03)
        o = I.soft_box(f'Strip{k}', (ln, w, 0.005), roundness=0.6, cuts=5)
        for vv in o.data.vertices:
            x = vv.co.x / ln
            vv.co.z += 0.012 * (x * x * 4 - 0.3) * (1 if k % 2 else 0.6)
            vv.co += Vector((0, 0, 0.0015 * I.noise3(vv.co, 150, k)))
            vv.co.y *= 1 + 0.15 * math.sin(x * 9 + k)
        o.data.update()
        M.assign(o, ['M_Leaf'])
        rgb = FPAL['dried'] if k % 2 else (0.72, 0.42, 0.06)

        def col(co, ln=ln, w=w, rgb=rgb):
            e = max(abs(co.x) / (ln / 2), abs(co.y) / (w / 2))
            return I.lerp3(rgb, FPAL['dried_edge'], max(0.0, e - 0.7) / 0.3)
        I.color_fn(o, col, rnd, 0.02)
        a = rnd.uniform(0, math.pi)
        o.data.transform(Matrix.Translation((rnd.uniform(-0.012, 0.012), rnd.uniform(-0.012, 0.012), 0.006 * k))
                         @ Matrix.Rotation(a, 4, 'Z') @ Matrix.Rotation(rnd.uniform(-0.1, 0.1), 4, 'X'))
        o.data.update()
        p.add(o, 'none')
    return _finish_smooth(p, name)


@_register('fruta_fermentada')
def _b_fruta_fermentada(v, rnd, name):
    """Fruta pasada y fermentada (10 cm): mango aplastado y blando, con
    magulladuras pardas, manchas moradas y espuma en una raja."""
    p = K.Parts()
    o = C.make_blob('Mush', (0, 0, 0), 1.0, v['seed'], subdivisions=4, noise_strength=0.12, noise_scale=1.4,
                    scale=(0.052, 0.04, 0.028), relax_iterations=2)
    for vv in o.data.vertices:
        if vv.co.z < -0.012:
            vv.co.z = -0.012 - (vv.co.z + 0.012) * 0.25  # asentada, aplastada contra el suelo
    o.data.update()
    M.assign(o, ['M_Leaf'])

    def col(co):
        c = I.lerp3(FPAL['ferment'], FPAL['ferment_bruise'], max(0.0, I.noise3(co, 45, 1.0) + 0.15) * 1.6)
        return I.lerp3(c, FPAL['ferment_purple'], max(0.0, I.noise3(co, 110, 6.0) - 0.3) * 3)
    I.color_fn(o, col, rnd, 0.015)
    p.add(o, 'none')
    # espuma: burbujitas en la raja de arriba
    for k in range(7):
        c = (rnd.uniform(-0.018, 0.018), rnd.uniform(-0.005, 0.005), 0.024)
        b = C.make_blob(f'Foam{k}', c, rnd.uniform(0.003, 0.005), v['seed'] + k, subdivisions=1,
                        noise_strength=0.0)
        M.assign(b, ['M_Leaf'])
        I.tint(b, FPAL['foam'], rnd, 0.03)
        p.add(b, 'none')
    _stem(p, (-0.045, 0, 0.004), (-1, 0, 0.3), 0.012, 0.0025, 0.002, FPAL['stem_dry'], rnd)
    return _finish_smooth(p, name)


@_register('comida_quemada')
def _b_comida_quemada(v, rnd, name):
    """Comida quemada: pegote carbonizado (8 cm) y dos migas, negro con
    crestas pardas y grietas de brasa naranja."""
    p = K.Parts()
    for k, (c, s) in enumerate((((0, 0, 0), (0.042, 0.032, 0.022)),
                                ((0.05, 0.02, -0.01), (0.012, 0.01, 0.008)),
                                ((-0.04, -0.026, -0.012), (0.01, 0.008, 0.007)))):
        o = C.make_blob(f'Char{k}', c, 1.0, v['seed'] + k, subdivisions=3 if k == 0 else 2,
                        noise_strength=0.3, noise_scale=2.2, scale=s, relax_iterations=1)
        M.assign(o, ['M_Wood'])

        def col(co, k=k):
            n = I.noise3(co, 160, 3.0 + k)
            if abs(n) < 0.025 and I.noise3(co, 40, 11.0 + k) > -0.1:
                return FPAL['ember']
            return I.lerp3(FPAL['burnt'], FPAL['burnt_ridge'], max(0.0, I.noise3(co, 60, 9.0)) * 1.5)
        I.color_fn(o, col, rnd, 0.005)
        p.add(o, 'none')
    return _finish_smooth(p, name)
