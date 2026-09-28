"""
items_cocina.py — agua, platos cocinados y los últimos mariscos de
items.json: agua sin tratar, hervida y de mar (cada una en su recipiente),
sopa, estofado y guiso improvisado, cangrejo asado en su caparazón, pulpo y
cangrejo de los cocoteros.

El agua se lee por el recipiente y el color: la SIN TRATAR va en medio coco
(turbia, verde parda, con una hojita y motas flotando), la HERVIDA en un
cuenco de barro (clara, azul limpio, con burbujitas en el borde) y la DE MAR
en un canuto de bambú (turquesa con espuma y costra de sal en el labio).

El cangrejo asado es el MISMO modelo que el cangrejo crudo de
items_pescados.py con otra paleta (escarlata vivo con quemaduras de brasa)
servido sobre un trozo de hoja de plátano.

Objetos sueltos: pivote en la base (z = 0), centrados en XY, en su postura
de reposo. Escala real en metros.
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
import items_pescados as F  # noqa: E402
import kit_construccion as K  # noqa: E402
from mathutils import Vector  # noqa: E402

GROUP = I.GROUP
PAL = I.PAL

KPAL = {
    'murky': (0.16, 0.20, 0.06),
    'murky_edge': (0.34, 0.30, 0.12),
    'clear': (0.18, 0.52, 0.72),
    'clear_hi': (0.62, 0.86, 0.95),
    'sea': (0.02, 0.50, 0.52),
    'sea_hi': (0.30, 0.78, 0.72),
    'foam': (0.92, 0.94, 0.90),
    'salt': (0.95, 0.93, 0.86),
    'broth': (0.86, 0.48, 0.10),
    'broth_hi': (0.98, 0.72, 0.26),
    'stew': (0.62, 0.26, 0.06),
    'stew_hi': (0.82, 0.44, 0.10),
    'guiso': (0.42, 0.30, 0.08),
    'guiso_hi': (0.56, 0.46, 0.14),
    'fish_meat': (0.95, 0.88, 0.72),
    'fish_meat_sh': (0.80, 0.62, 0.40),
    'taro': (0.70, 0.56, 0.78),
    'taro_dot': (0.40, 0.22, 0.48),
    'yam': (0.96, 0.52, 0.10),
    'herb': (0.14, 0.46, 0.06),
    'herb_hi': (0.40, 0.66, 0.10),
    'wood_bowl': (0.36, 0.17, 0.06),
    'wood_bowl_hi': (0.56, 0.30, 0.10),
    'wood_bowl_in': (0.62, 0.38, 0.16),
    'crab_cooked': (0.95, 0.20, 0.03),
    'crab_cooked_hi': (1.00, 0.55, 0.10),
    'crab_cooked_belly': (0.98, 0.72, 0.42),
    'octo': (0.56, 0.12, 0.10),
    'octo_dark': (0.30, 0.05, 0.08),
    'octo_spot': (0.92, 0.62, 0.48),
    'octo_under': (0.96, 0.78, 0.66),
    'octo_sucker': (0.88, 0.50, 0.46),
    'cc_shell': (0.20, 0.10, 0.50),
    'cc_shell_hi': (0.42, 0.22, 0.70),
    'cc_leg': (0.34, 0.14, 0.52),
    'cc_leg_tip': (0.90, 0.40, 0.10),
    'cc_abdomen': (0.62, 0.44, 0.58),
    'cc_abdomen_band': (0.40, 0.24, 0.42),
}

VARIANTS = [
    dict(name='Item_AguaSinTratar', item_id='agua_sin_tratar', seed=5101, builder='agua_sin_tratar',
         tri_budget=(300, 6000)),
    dict(name='Item_AguaHervida', item_id='agua_hervida', seed=5102, builder='agua_hervida',
         tri_budget=(300, 6000)),
    dict(name='Item_AguaMar', item_id='agua_mar', seed=5103, builder='agua_mar', tri_budget=(300, 6000)),
    dict(name='Item_SopaPescado', item_id='sopa_pescado', seed=5104, builder='sopa_pescado',
         tri_budget=(500, 8000)),
    dict(name='Item_EstofadoPescado', item_id='estofado_pescado', seed=5105, builder='estofado_pescado',
         tri_budget=(500, 9000)),
    dict(name='Item_GuisoImprovisado', item_id='guiso_improvisado', seed=5106, builder='guiso_improvisado',
         tri_budget=(500, 8000)),
    # misma semilla y constructor que el cangrejo crudo: solo cambia la paleta
    dict(name='Item_CangrejoCaparazon', item_id='cangrejo_caparazon', seed=4717, builder='cangrejo_caparazon',
         tri_budget=(800, 9000), preview_rot_z=-math.pi / 2),
    dict(name='Item_Pulpo', item_id='pulpo', seed=5108, builder='pulpo', tri_budget=(800, 9000)),
    dict(name='Item_CangrejoCocotero', item_id='cangrejo_cocotero', seed=5109, builder='cangrejo_cocotero',
         tri_budget=(1000, 12000), preview_rot_z=-math.pi / 2),
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
def _coconut_inner_r(R, h, z, wall=0.009):
    """Radio interior del medio coco de CT._coconut_half a la altura z."""
    ca = max(-1.0, min(1.0, 1.0 - z / h))
    a = math.acos(ca)
    t = a / (math.pi / 2)
    return R * math.sin(a) * (1.0 - 0.08 * (1 - t)) - wall


def _profile_r(outer, z):
    """Radio de un perfil (r, z) ascendente a la altura z (interpolado)."""
    for (r0, z0), (r1, z1) in pairwise(outer):
        if z0 <= z <= z1 and z1 > z0:
            return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    return outer[-1][0]


def _liquid(p, rnd, name, r, z, colfn, j=0.01, sx=1.0, sy=1.0):
    """Superficie de líquido: disco fino con anillos para el degradado del
    menisco. Su borde se mete un pelo en la pared (sin rendija visible)."""
    prof = [(0.0, z), (r * 0.3, z), (r * 0.6, z), (r * 0.85, z), (r, z + 0.0008), (r, z - 0.004),
            (0.0, z - 0.004)]
    o = I.lathe(name, prof, segs=28, sx=sx, sy=sy)
    M.assign(o, ['M_Leaf'])
    I.color_fn(o, lambda co: colfn(co, math.hypot(co.x / sx, co.y / sy) / max(r, 1e-5)), rnd, j)
    p.add(o, 'none')
    return o


def _chunk(p, rnd, name, pos, size, col0, col1, seed, flat=0.7, mat='M_Leaf', noise=0.3):
    """Tropezón (trozo de pescado, taro, batata) medio hundido en el caldo."""
    o = C.make_blob(name, pos, 1.0, seed, subdivisions=2, noise_strength=noise,
                    scale=(size[0], size[1], size[2] * flat), relax_iterations=1)
    M.assign(o, [mat])
    I.color_fn(o, lambda co: I.lerp3(col0, col1, 0.5 + 0.5 * I.noise3(co, 90, seed * 0.1)), rnd, 0.02)
    p.add(o, 'none')
    return o


def _herb(p, rnd, name, pos, ang, L=0.022, W=0.009):
    """Hojita de hierba aromática flotando (dos triángulos con grosor)."""
    lf = I.leaf(name, L, W, 0.05, rnd, segments=3, fold=0.3, thick=0.0012,
                rgb0=KPAL['herb'], rgb1=KPAL['herb_hi'])
    from mathutils import Matrix
    lf.data.transform(Matrix.Rotation(ang, 4, 'Z'))
    lf.data.transform(Matrix.Translation(pos))
    p.add(lf, 'none')
    return lf


def _clay_bowl(p, rnd, name, outer, wall, seed, band=True):
    """Cuenco de barro cocido (torno) con manchas de cocción y una banda
    de engobe claro bajo el labio."""
    prof = CT._vessel_prof(outer, wall)
    o = I.lathe(name, prof, segs=36)
    M.assign(o, ['M_Stone'])
    top = outer[-1][1]
    from mathutils import noise as mnoise

    def col(co):
        rr = math.hypot(co.x, co.y)
        if co.z > outer[0][1] + wall * 0.5 and rr < _profile_r(outer, co.z) - wall * 0.6:
            return I.lerp3(CT.CPAL['clay'], CT.CPAL['clay_fire'], 0.35)
        fire = max(0.0, mnoise.noise(Vector((co.x * 16 + seed, co.y * 16, co.z * 12))) * 1.6)
        c = I.lerp3(CT.CPAL['clay'], CT.CPAL['clay_fire'], min(1.0, fire) * 0.7)
        if band and top - 0.016 < co.z < top - 0.009:
            c = I.lerp3(c, CT.CPAL['clay_slip'], 0.85)
        return c
    I.color_fn(o, col, rnd, 0.015)
    p.add(o, 'none')
    return o


# ---------------------------------------------------------------------------
# AGUA
# ---------------------------------------------------------------------------
@_register('agua_sin_tratar')
def _b_agua_sin_tratar(v, rnd, name):
    """Medio coco (Ø 14 cm) con agua de charca: turbia, verde parda, con una
    hojita seca y motas flotando."""
    p = K.Parts()
    R, h = 0.068, 0.062
    p.add(CT._coconut_half('Bowl', rnd, R, h), 'none')
    zw = 0.05
    r = _coconut_inner_r(R, h, zw) + 0.001

    def wc(co, s):
        c = I.lerp3(KPAL['murky'], KPAL['murky_edge'], max(0.0, s - 0.6) / 0.4)
        return I.lerp3(c, (0.08, 0.06, 0.02), max(0.0, I.noise3(co, 160, 3.0) - 0.3) * 1.5)
    _liquid(p, rnd, 'Water', r, zw, wc)
    lf = I.leaf('Debris', 0.03, 0.012, 0.02, rnd, segments=3, fold=0.2, thick=0.001,
                rgb0=(0.36, 0.20, 0.05), rgb1=(0.55, 0.36, 0.10))
    from mathutils import Matrix
    lf.data.transform(Matrix.Rotation(0.7, 4, 'Z'))
    lf.data.transform(Matrix.Translation((-0.012, -0.018, zw + 0.0012)))
    p.add(lf, 'none')
    for k in range(3):
        a = rnd.uniform(0, 2 * math.pi)
        d = rnd.uniform(0.01, 0.035)
        _chunk(p, rnd, f'Speck{k}', (math.cos(a) * d, math.sin(a) * d, zw + 0.0006), (0.0035, 0.0025, 0.0015),
               (0.22, 0.14, 0.05), (0.12, 0.10, 0.03), v['seed'] + k, mat='M_Wood')
    return p.finish(name)


@_register('agua_hervida')
def _b_agua_hervida(v, rnd, name):
    """Cuenco de barro (Ø 13 cm) con agua hervida clara, azul limpio, y un
    collar de burbujitas junto a la pared."""
    p = K.Parts()
    outer = [(0.0, 0.0)] + I.smooth_profile([(0.035, 0.0), (0.05, 0.008), (0.062, 0.03), (0.066, 0.055),
                                              (0.064, 0.07)], per_seg=2)
    wall = 0.006
    _clay_bowl(p, rnd, 'Cup', outer, wall, v['seed'])
    zw = 0.058
    r = _profile_r(outer, zw) - wall + 0.001

    def wc(co, s):
        c = I.lerp3(KPAL['clear_hi'], KPAL['clear'], min(1.0, s * 1.2))
        return I.lerp3(c, KPAL['clear_hi'], max(0.0, s - 0.85) / 0.15 * 0.8)
    _liquid(p, rnd, 'Water', r, zw, wc)
    for k in range(9):
        a = k / 9 * 2 * math.pi + rnd.uniform(-0.2, 0.2)
        rr = r - 0.004
        b = C.make_blob(f'Bubble{k}', (math.cos(a) * rr, math.sin(a) * rr, zw + 0.0008),
                        rnd.uniform(0.0018, 0.003), v['seed'] + k, subdivisions=1, noise_strength=0.0,
                        scale=(1, 1, 0.6))
        M.assign(b, ['M_Leaf'])
        I.tint(b, (0.90, 0.97, 1.0), rnd, 0.01)
        p.add(b, 'none')
    return p.finish(name)


@_register('agua_mar')
def _b_agua_mar(v, rnd, name):
    """Canuto de bambú (11 cm) lleno de agua de mar turquesa, con espuma en
    el borde y costra blanca de sal en el labio y los chorretones."""
    p = K.Parts()
    R, H = 0.042, 0.105
    wall = 0.005
    outer = [(0.0, 0.0), (R * 0.97, 0.0), (R, 0.004)]
    for z in (0.012, 0.016, 0.02, 0.05, 0.08, 0.095):
        outer.append((R * (1.07 if abs(z - 0.016) < 1e-4 else 1.0), z))
    outer.append((R, H))
    prof = CT._vessel_prof(outer, wall)
    o = I.lathe('Tube', prof, segs=24)
    M.assign(o, ['M_Wood'])
    drips = [(rnd.uniform(0, 2 * math.pi), rnd.uniform(0.2, 0.45)) for _ in range(4)]

    def col(co):
        rr = math.hypot(co.x, co.y)
        a = math.atan2(co.y, co.x)
        if rr < R - wall * 0.6 and co.z > wall:
            return PAL['bamboo_cut']
        c = I.lerp3(PAL['bamboo'][0], PAL['bamboo'][2], 0.5 + 0.5 * math.sin(a * 5 + co.z * 30))
        if 0.012 < co.z < 0.02:
            c = PAL['bamboo_node']
        if co.z > H - 0.003:
            c = KPAL['salt']
        for da, dl in drips:
            dd = abs(math.atan2(math.sin(a - da), math.cos(a - da)))
            if dd < 0.14 and co.z > H * (1 - dl):
                c = I.lerp3(c, KPAL['salt'], 0.8)
        if co.z < 0.002:
            c = PAL['bamboo_cut']
        return c
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    zw = H - 0.008
    r = R - wall + 0.001

    def wc(co, s):
        c = I.lerp3(KPAL['sea_hi'], KPAL['sea'], min(1.0, s * 1.3))
        foam = max(0.0, s - 0.75) / 0.25 + max(0.0, I.noise3(co, 220, 5.0) - 0.35) * 2
        return I.lerp3(c, KPAL['foam'], min(1.0, foam))
    _liquid(p, rnd, 'Water', r, zw, wc)
    for k in range(5):
        a = rnd.uniform(0, 2 * math.pi)
        b = C.make_blob(f'Salt{k}', (math.cos(a) * R, math.sin(a) * R, H - 0.001), rnd.uniform(0.002, 0.0035),
                        v['seed'] + k, subdivisions=1, noise_strength=0.3)
        M.assign(b, ['M_Stone'])
        I.tint(b, KPAL['salt'], rnd, 0.02)
        p.add(b, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# PLATOS
# ---------------------------------------------------------------------------
@_register('sopa_pescado')
def _b_sopa_pescado(v, rnd, name):
    """Cuenco de barro ancho (Ø 17 cm) con caldo dorado: tacos de pescado
    blanco, hierbas y una cola de pescado asomando."""
    p = K.Parts()
    outer = [(0.0, 0.0)] + I.smooth_profile([(0.04, 0.0), (0.06, 0.01), (0.078, 0.032), (0.085, 0.055),
                                              (0.086, 0.062)], per_seg=2)
    wall = 0.006
    _clay_bowl(p, rnd, 'Bowl', outer, wall, v['seed'])
    zw = 0.05
    r = _profile_r(outer, zw) - wall + 0.001

    def wc(co, s):
        c = I.lerp3(KPAL['broth_hi'], KPAL['broth'], min(1.0, s * 1.1))
        # ojos de grasa del caldo
        return I.lerp3(c, (1.0, 0.86, 0.46), max(0.0, I.noise3(co, 140, 2.0) - 0.4) * 2.2)
    _liquid(p, rnd, 'Broth', r, zw, wc)
    for k in range(5):
        a = k / 5 * 2 * math.pi + rnd.uniform(-0.3, 0.3)
        d = rnd.uniform(0.015, 0.045)
        _chunk(p, rnd, f'Fish{k}', (math.cos(a) * d, math.sin(a) * d, zw), (0.013, 0.010, 0.010),
               KPAL['fish_meat'], KPAL['fish_meat_sh'], v['seed'] + k)
    for k in range(6):
        a = rnd.uniform(0, 2 * math.pi)
        d = rnd.uniform(0.01, 0.055)
        _herb(p, rnd, f'Herb{k}', (math.cos(a) * d, math.sin(a) * d, zw + 0.0012), rnd.uniform(0, 6.28))
    # cola de pescado asomando por el borde: aleta en abanico con radios
    base = [(0.035, -0.004, zw + 0.002), (0.035, 0.004, zw + 0.002)]
    tips = []
    n = 7
    base = []
    for i in range(n):
        s = -1 + 2 * i / (n - 1)
        base.append((0.03, s * 0.004, zw + 0.004))
        tips.append((0.058 + 0.006 * (1 - abs(s)) * -1, s * 0.022, zw + 0.03 + 0.004 * s * s))
    F._fin(p, 'Tail', base, tips, (0.62, 0.30, 0.08), (0.90, 0.56, 0.18), rows=2,
           fold=(0.0015, Vector((1, 0, 0))), thick=0.0025, rim=(0.55, 0.26, 0.06))
    stub = C.make_blob('TailStub', (0.028, 0.0, zw + 0.002), 1.0, v['seed'], subdivisions=2, noise_strength=0.0,
                       scale=(0.012, 0.0065, 0.006))
    M.assign(stub, ['M_Leaf'])
    I.tint(stub, (0.80, 0.52, 0.20), rnd, 0.02)
    p.add(stub, 'none')
    return p.finish(name)


@_register('estofado_pescado')
def _b_estofado_pescado(v, rnd, name):
    """Cuenco ovalado de madera tallada (kumete, 22 cm) con estofado espeso
    de pescado: tacos de pescado, taro lila, batata naranja, hierbas y una
    cuchara de madera apoyada."""
    p = K.Parts()
    sx, sy = 1.25, 0.9
    outer = [(0.0, 0.0)] + I.smooth_profile([(0.045, 0.0), (0.065, 0.012), (0.08, 0.035), (0.085, 0.055),
                                              (0.084, 0.064)], per_seg=2)
    wall = 0.008
    prof = CT._vessel_prof(outer, wall)
    bowl = I.lathe('Bowl', prof, segs=36, sx=sx, sy=sy)
    M.assign(bowl, ['M_Wood'])

    def bc(co):
        rr = math.hypot(co.x / sx, co.y / sy)
        if co.z > wall * 0.8 and rr < _profile_r(outer, co.z) - wall * 0.6:
            return KPAL['wood_bowl_in']
        grain = 0.5 + 0.5 * math.sin(co.x * 90 + 3 * I.noise3(co, 25))
        return I.lerp3(KPAL['wood_bowl'], KPAL['wood_bowl_hi'], grain * 0.8)
    I.color_fn(bowl, bc, rnd, 0.01)
    p.add(bowl, 'none')
    # orejetas talladas en los extremos del óvalo
    for side in (1, -1):
        lug = C.make_blob(f'Lug{side}', (side * 0.084 * sx + side * 0.008, 0.0, 0.056), 1.0, v['seed'] + side,
                          subdivisions=2, noise_strength=0.0, scale=(0.016, 0.022, 0.008))
        M.assign(lug, ['M_Wood'])
        I.tint(lug, KPAL['wood_bowl_hi'], rnd, 0.02)
        p.add(lug, 'none')
    zw = 0.052
    r = _profile_r(outer, zw) - wall + 0.0012

    def wc(co, s):
        c = I.lerp3(KPAL['stew_hi'], KPAL['stew'], min(1.0, s))
        return I.lerp3(c, (0.40, 0.14, 0.03), max(0.0, I.noise3(co, 110, 7.0) - 0.2))
    surf = _liquid(p, rnd, 'Stew', r, zw, wc, sx=sx, sy=sy)
    # estofado espeso: la superficie abulta hacia el centro
    for vv in surf.data.vertices:
        s = math.hypot(vv.co.x / sx, vv.co.y / sy) / r
        if vv.co.z > zw - 0.001:
            vv.co.z += 0.008 * max(0.0, 1 - s * s)
    surf.data.update()
    for k in range(10):
        a = rnd.uniform(0, 2 * math.pi)
        d = rnd.uniform(0.0, 0.055)
        x, y = math.cos(a) * d * sx, math.sin(a) * d * sy
        zz = zw + 0.008 * max(0.0, 1 - (d / r) ** 2)
        kind = k % 3
        if kind == 0:
            _chunk(p, rnd, f'Fish{k}', (x, y, zz), (0.014, 0.011, 0.011), KPAL['fish_meat'], KPAL['fish_meat_sh'],
                   v['seed'] + k)
        elif kind == 1:
            _chunk(p, rnd, f'Taro{k}', (x, y, zz), (0.011, 0.011, 0.011), KPAL['taro'], KPAL['taro_dot'],
                   v['seed'] + k, flat=0.8, noise=0.15)
        else:
            _chunk(p, rnd, f'Yam{k}', (x, y, zz), (0.011, 0.009, 0.01), KPAL['yam'], (0.85, 0.34, 0.05),
                   v['seed'] + k, flat=0.8, noise=0.15)
    for k in range(5):
        a = rnd.uniform(0, 2 * math.pi)
        d = rnd.uniform(0.0, 0.05)
        _herb(p, rnd, f'Herb{k}', (math.cos(a) * d * sx, math.sin(a) * d * sy, zw + 0.008 * (1 - (d / r) ** 2)
                                   + 0.002), rnd.uniform(0, 6.28))
    # cuchara de madera: mango apoyado en el borde, pala hundida en el guiso
    pa = Vector((0.012, 0.018, zw + 0.006))
    rim = Vector((-0.004, -0.084 * sy - 0.001, 0.0715))
    dirn = (rim - pa).normalized()
    pb = rim + dirn * 0.075
    spoon = I.sweep('Handle', [pa, pa.lerp(rim, 0.5), rim, rim.lerp(pb, 0.5), pb], [0.0035, 0.004, 0.0045, 0.005, 0.0055],
                    segs=6)
    M.assign(spoon, ['M_Wood'])
    I.tint(spoon, PAL['wood_light'][0], rnd, 0.02)
    p.add(spoon, 'none')
    scoop = C.make_blob('Scoop', pa - dirn * 0.014 + Vector((0, 0, -0.002)), 1.0, v['seed'], subdivisions=2,
                        noise_strength=0.0, scale=(0.014, 0.02, 0.005))
    M.assign(scoop, ['M_Wood'])
    I.tint(scoop, PAL['wood_light'][0], rnd, 0.02)
    p.add(scoop, 'none')
    return p.finish(name)


@_register('guiso_improvisado')
def _b_guiso_improvisado(v, rnd, name):
    """Medio coco (Ø 14 cm) con un guiso de lo que había: caldo pardo
    verdoso, rodajas de batata, trozos de taro y hojas de verdura."""
    p = K.Parts()
    R, h = 0.068, 0.062
    p.add(CT._coconut_half('Bowl', rnd, R, h, jag=0.004, seed=3), 'none')
    zw = 0.048
    r = _coconut_inner_r(R, h, zw) + 0.001

    def wc(co, s):
        c = I.lerp3(KPAL['guiso_hi'], KPAL['guiso'], min(1.0, s))
        return I.lerp3(c, (0.22, 0.30, 0.06), max(0.0, I.noise3(co, 120, 1.0) - 0.15) * 1.2)
    _liquid(p, rnd, 'Stew', r, zw, wc)
    from mathutils import Matrix
    for k in range(3):
        a = k / 3 * 2 * math.pi + 0.4
        d = 0.022
        # rodaja de batata: disco naranja con piel granate
        sl = I.lathe(f'Slice{k}', [(0.0, 0.0), (0.012, 0.0), (0.0125, 0.003), (0.012, 0.006), (0.0, 0.006)], segs=12)
        sl.data.transform(Matrix.Rotation(rnd.uniform(-0.25, 0.25), 4, 'X'))
        sl.data.transform(Matrix.Translation((math.cos(a) * d, math.sin(a) * d, zw - 0.003)))
        M.assign(sl, ['M_Leaf'])
        c0 = (math.cos(a) * d, math.sin(a) * d)
        I.color_fn(sl, lambda co, c0=c0: (0.45, 0.06, 0.08)
                   if math.hypot(co.x - c0[0], co.y - c0[1]) > 0.0112 else KPAL['yam'], rnd, 0.02)
        p.add(sl, 'none')
    for k in range(3):
        a = k / 3 * 2 * math.pi + 1.4
        _chunk(p, rnd, f'Taro{k}', (math.cos(a) * 0.028, math.sin(a) * 0.028, zw), (0.01, 0.009, 0.009),
               KPAL['taro'], KPAL['taro_dot'], v['seed'] + k, noise=0.15)
    for k in range(3):
        a = rnd.uniform(0, 2 * math.pi)
        lf = I.leaf(f'Green{k}', 0.04, 0.02, 0.1, rnd, segments=4, fold=0.2, thick=0.001,
                    rgb0=(0.10, 0.30, 0.05), rgb1=(0.26, 0.44, 0.08))
        lf.data.transform(Matrix.Rotation(a, 4, 'Z'))
        lf.data.transform(Matrix.Translation((rnd.uniform(-0.01, 0.01), rnd.uniform(-0.01, 0.01), zw + 0.004)))
        p.add(lf, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# MARISCOS
# ---------------------------------------------------------------------------
@_register('cangrejo_caparazon')
def _b_cangrejo_caparazon(v, rnd, name):
    """Cangrejo asado en su caparazón: la malla del cangrejo crudo con la
    paleta cocida (escarlata y naranja, vientre crema, quemaduras de brasa)
    servido sobre un trozo de hoja de plátano."""
    vv = dict(v, crab_pal=(KPAL['crab_cooked'], KPAL['crab_cooked_hi'], KPAL['crab_cooked_belly']), char=0.8)
    crab = F._BUILDERS['cangrejo'](vv, rnd, 'Crab')
    zs = [p.co.z for p in crab.data.vertices]
    from mathutils import Matrix
    crab.data.transform(Matrix.Translation((0, 0, -min(zs) + 0.0035)))
    p = K.Parts()
    p.add(crab, 'none')
    D._leaf_bed(p, rnd, 0.20, 0.17, curl=0.15)
    return p.finish(name)


def _arm_path(root, heading, length, r0, curl, side, n=18, lift=0.02):
    """Brazo de pulpo tendido en el suelo: sale del cuerpo en `root` con
    rumbo `heading` y se enrosca en espiral al final (`curl` vueltas,
    hacia `side`)."""
    pts, radii = [], []
    pos = Vector((root.x, root.y, 0.0))
    th = heading
    ds = length / n
    for i in range(n + 1):
        t = i / n
        r = r0 * (1 - 0.85 * t) + 0.0012
        z = r + lift * max(0.0, 1 - t / 0.25) ** 2
        pts.append(Vector((pos.x, pos.y, z)))
        radii.append(r)
        k = max(0.0, (t - 0.45) / 0.55)
        th += side * curl * 2 * math.pi * k * 2.2 / n
        pos = pos + Vector((math.cos(th), math.sin(th), 0.0)) * ds * (1 - 0.35 * k)
    return pts, radii


@_register('pulpo')
def _b_pulpo(v, rnd, name):
    """Pulpo (45 cm de brazo a brazo): manto bulboso rojo granate con
    motas claras, ojos saltones y ocho brazos tendidos que se enroscan en
    la punta; la cara de las ventosas, rosa crema."""
    p = K.Parts()
    seed = v['seed']
    head = C.make_blob('Head', (0.0, 0.0, 0.028), 1.0, seed, subdivisions=3, noise_strength=0.05,
                       scale=(0.04, 0.038, 0.028))
    mantle = C.make_blob('Mantle', (-0.055, 0.0, 0.05), 1.0, seed + 1, subdivisions=3, noise_strength=0.08,
                         scale=(0.065, 0.047, 0.042), relax_iterations=1)
    for o in (head, mantle):
        M.assign(o, ['M_Leaf'])

        def mc(co):
            c = I.lerp3(KPAL['octo_dark'], KPAL['octo'], 0.5 + 0.5 * I.noise3(co, 30, 2.0))
            c = I.lerp3(c, KPAL['octo_spot'], F._spots(co.x * 12, co.y * 12 + co.z * 8, 5, 5, 0.18, 3) * 0.7)
            if co.z < 0.012:
                c = I.lerp3(c, KPAL['octo_under'], 0.6)
            return c
        I.color_fn(o, mc, rnd, 0.01)
        p.add(o, 'none')
    # ojos saltones a los lados de la cabeza, pupila en barra horizontal
    for side in (1, -1):
        c = Vector((0.012, side * 0.028, 0.045))
        bump = C.make_blob(f'EyeBump{side}', c, 0.012, seed + 5, subdivisions=2, noise_strength=0.0,
                           scale=(1.0, 0.9, 0.9))
        M.assign(bump, ['M_Leaf'])
        I.tint(bump, KPAL['octo'], rnd, 0.01)
        p.add(bump, 'none')
        e = C.make_blob(f'Eye{side}', c + Vector((0.003, side * 0.006, 0.002)), 0.0075, seed + 6, subdivisions=2,
                        noise_strength=0.0, scale=(1.0, 0.7, 1.0))
        M.assign(e, ['M_Leaf'])

        def ec(co, c=c, side=side):
            d = co - (c + Vector((0.003, side * 0.006, 0.002)))
            if abs(d.z) < 0.0018 and abs(d.x) < 0.005:
                return F.FPAL['eye_pupil']
            return (0.92, 0.72, 0.20)
        I.color_fn(e, ec)
        p.add(e, 'none')
    # ocho brazos repartidos hacia delante y los lados (el manto va detrás)
    segs = 8
    for i in range(8):
        a = -2.4 + i * (4.8 / 7) + rnd.uniform(-0.12, 0.12)
        root = Vector((0.012 + math.cos(a) * 0.022, math.sin(a) * 0.022, 0.0))
        side = 1 if (i % 2) else -1
        pts, radii = _arm_path(root, a, rnd.uniform(0.17, 0.22), 0.011, rnd.uniform(0.45, 0.8), side)
        arm = I.sweep(f'Arm{i}', pts, radii, segs=segs)
        M.assign(arm, ['M_Leaf'])
        n = len(pts)

        def ac(vv, pts=pts, radii=radii, n=n):
            ring, k = divmod(vv.index, segs)
            ring = min(ring, n - 1)
            under = vv.co.z < pts[ring].z - radii[ring] * 0.25
            if under:
                return (KPAL['octo_sucker'] if ring % 2 == 0 else KPAL['octo_under']) + (0.0,)
            c = I.lerp3(KPAL['octo'], KPAL['octo_dark'], 0.5 + 0.5 * I.noise3(vv.co, 40, 1.0))
            c = I.lerp3(c, KPAL['octo_spot'], 0.35 * (ring % 3 == 0))
            return tuple(I.lerp3(c, KPAL['octo_under'], ring / n * 0.3)) + (0.0,)
        C.set_vertex_colors(arm, ac)
        p.add(arm, 'none')
    obj = p.finish(name)
    I.smooth_all(obj)
    return obj


@_register('cangrejo_cocotero')
def _b_cangrejo_cocotero(v, rnd, name):
    """Cangrejo de los cocoteros (50 cm de pata a pata): caparazón violeta
    azulado, pinza izquierda enorme, patas largas del violeta al naranja,
    abdomen blando plegado bajo la trasera y antenas rojas."""
    p = K.Parts()
    seed = v['seed']
    shell = C.make_blob('Shell', (0.02, 0.0, 0.055), 1.0, seed, subdivisions=4, noise_strength=0.04,
                        scale=(0.066, 0.052, 0.036))
    M.assign(shell, ['M_Leaf'])

    def sc(co):
        if co.z < 0.045:
            return KPAL['cc_abdomen']
        c = I.lerp3(KPAL['cc_shell'], KPAL['cc_shell_hi'], 0.5 + 0.5 * I.noise3(co, 35, 3.0))
        # surco cervical en V de la placa
        if abs(co.x - 0.02 - abs(co.y) * 0.6) < 0.004:
            c = I.lerp3(c, (0.08, 0.03, 0.2), 0.7)
        return c
    I.color_fn(shell, sc, rnd, 0.01)
    p.add(shell, 'none')
    abd = C.make_blob('Abdomen', (-0.055, 0.0, 0.04), 1.0, seed + 1, subdivisions=3, noise_strength=0.03,
                      scale=(0.05, 0.044, 0.036), relax_iterations=1)
    M.assign(abd, ['M_Leaf'])
    I.color_fn(abd, lambda co: KPAL['cc_abdomen_band'] if (int((co.x + 0.1) * 110) % 2) else KPAL['cc_abdomen'],
               rnd, 0.01)
    p.add(abd, 'none')
    leg0, leg1 = KPAL['cc_leg'], KPAL['cc_leg_tip']
    # tres pares de patas andadoras largas
    for side in (1, -1):
        for i in range(3):
            x = 0.035 - i * 0.025
            root = Vector((x, side * 0.044, 0.05))
            out = Vector((0.55 - i * 0.55, side * 1.0, 0.0)).normalized()
            knee = root + out * 0.058 + Vector((0, 0, 0.03))
            ankle = knee + out * 0.058 + Vector((0, 0, -0.045))
            foot = ankle + out * 0.026 + Vector((0, 0, -ankle.z + 0.001))
            pts = [root, root.lerp(knee, 0.5) + Vector((0, 0, 0.005)), knee, knee.lerp(ankle, 0.5), ankle,
                   ankle.lerp(foot, 0.5), foot]
            F._limb(p, f'Leg{side}{i}', pts, [0.012, 0.012, 0.0105, 0.0095, 0.0075, 0.005, 0.0015], leg0, leg1, rnd,
                    segs=8)
        # pinzas: la izquierda (+Y) mucho mayor
        big = 1.35 if side > 0 else 0.85
        root = Vector((0.07, side * 0.026, 0.05))
        elbow = root + Vector((0.035, side * 0.035, 0.012)) * big
        wrist = elbow + Vector((0.04, -side * 0.01, -0.02)) * big
        F._limb(p, f'Arm{side}', [root, root.lerp(elbow, 0.5), elbow, wrist],
                [0.009 * big, 0.01 * big, 0.011 * big, 0.012 * big], leg0, I.lerp3(leg0, leg1, 0.4), rnd)
        palm_c = wrist + Vector((0.026, 0, -0.004)) * big
        palm = C.make_blob(f'Palm{side}', palm_c, 1.0, seed + 10 + side, subdivisions=3, noise_strength=0.02,
                           scale=(0.032 * big, 0.016 * big, 0.022 * big))
        M.assign(palm, ['M_Leaf'])
        I.color_fn(palm, lambda co, c=palm_c: I.lerp3(leg0, leg1, 0.3 + 0.4 * (co.z - c.z + 0.02) / 0.04), rnd, 0.01)
        p.add(palm, 'none')
        tip0 = palm_c + Vector((0.026, 0, 0)) * big
        for k, dz in enumerate((0.008, -0.008)):
            f0 = tip0 + Vector((0.0, 0.0, dz * big))
            f1 = f0 + Vector((0.03, -side * 0.004, -dz * 0.7)) * big
            F._limb(p, f'Finger{side}{k}', [f0, f0.lerp(f1, 0.5), f1],
                    [0.008 * big, 0.006 * big, 0.0015], leg1, (0.98, 0.85, 0.62), rnd)
        # ojos en pedúnculo y antenas largas
        st = Vector((0.08, side * 0.012, 0.074))
        se = st + Vector((0.006, side * 0.003, 0.014))
        F._limb(p, f'Stalk{side}', [st, se], [0.003, 0.003], leg0, leg0, rnd)
        e = C.make_blob(f'Eye{side}', se + Vector((0, 0, 0.003)), 0.005, seed + 20, subdivisions=2,
                        noise_strength=0.0)
        M.assign(e, ['M_Leaf'])
        I.color_fn(e, lambda co: F.FPAL['eye_pupil'])
        p.add(e, 'none')
        a0 = Vector((0.084, side * 0.006, 0.064))
        ant = [a0 + Vector((0.03 * t * 3, side * 0.03 * t * t * 4, 0.04 * math.sin(t * 2.4))) for t in
               (0.0, 0.2, 0.4, 0.6, 0.8, 1.0)]
        F._limb(p, f'Ant{side}', ant, [0.0022, 0.002, 0.0017, 0.0014, 0.001, 0.0006], (0.75, 0.16, 0.06),
                (0.95, 0.45, 0.12), rnd, segs=5)
    return F._finish(p, name)
