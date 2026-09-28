"""
items_orilla.py — materiales sueltos de playa, arrecife y sotobosque que
aún usaban un marcador (items.json, biblia §3.1 «Plantas», «Minerales» y
«Mar»): hoja de plátano, musgo, algodón silvestre, arena, sal marina,
azufre, caracola (el pū polinesio), esponja de mar y alga de fibra; y la
tierra suelta que sale de cavar (biblia 02 §2.7), paralela a la arena.

Objetos sueltos: pivote en la base (z = 0), centrados en XY, en su postura
de reposo (los alargados a lo largo de +X). Escala real en metros. Los
materiales «a granel» (arena, tierra suelta, sal) se representan como el montoncito que
el jugador recoge del suelo.
"""

import math
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import bpy  # noqa: E402

import _items as I  # noqa: E402
import _materials as M  # noqa: E402
import common as C  # noqa: E402
import kit_construccion as K  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

GROUP = I.GROUP
PAL = I.PAL

OPAL = {
    'banana_leaf': (0.07, 0.30, 0.04),
    'banana_leaf_hi': (0.26, 0.46, 0.07),
    'banana_rib': (0.55, 0.58, 0.20),
    'banana_dry': (0.46, 0.32, 0.10),
    'moss': (0.10, 0.28, 0.035),
    'moss_hi': (0.34, 0.46, 0.06),
    'soil': (0.16, 0.09, 0.04),
    'soil_dry': (0.33, 0.21, 0.11),
    'soil_pebble': (0.40, 0.37, 0.32),
    'soil_root': (0.36, 0.24, 0.12),
    'cotton': (0.93, 0.91, 0.84),
    'cotton_shade': (0.80, 0.76, 0.66),
    'boll': (0.30, 0.17, 0.06),
    'boll_in': (0.56, 0.40, 0.18),
    'twig': (0.22, 0.12, 0.05),
    'sand': (0.80, 0.64, 0.38),
    'sand_dark': (0.62, 0.46, 0.24),
    'salt': (0.94, 0.90, 0.82),
    'salt_pink': (0.90, 0.74, 0.70),
    'salt_crust': (0.70, 0.54, 0.40),
    'sulfur': (0.86, 0.66, 0.04),
    'sulfur_hi': (0.96, 0.86, 0.22),
    'sulfur_rust': (0.52, 0.26, 0.04),
    'conch': (0.86, 0.74, 0.54),
    'conch_mark': (0.40, 0.18, 0.07),
    'conch_lip': (0.88, 0.42, 0.14),
    'conch_in': (0.94, 0.66, 0.48),
    'sponge': (0.82, 0.50, 0.12),
    'sponge_dark': (0.46, 0.22, 0.05),
    'kelp': (0.30, 0.26, 0.05),
    'kelp_hi': (0.52, 0.40, 0.08),
    'kelp_dark': (0.16, 0.12, 0.03),
}

VARIANTS = [
    dict(name='Item_HojaPlatano', item_id='hoja_platano', seed=4901, builder='hoja_platano',
         tri_budget=(400, 6000)),
    dict(name='Item_Musgo', item_id='musgo', seed=4902, builder='musgo', tri_budget=(400, 6000)),
    dict(name='Item_AlgodonSilvestre', item_id='algodon_silvestre', seed=4903, builder='algodon_silvestre',
         tri_budget=(400, 6000)),
    dict(name='Item_Arena', item_id='arena', seed=4904, builder='arena', tri_budget=(300, 5000)),
    dict(name='Item_TierraSuelta', item_id='tierra_suelta', seed=4910, builder='tierra_suelta',
         tri_budget=(300, 5000)),
    dict(name='Item_SalMarina', item_id='sal_marina', seed=4905, builder='sal_marina', tri_budget=(400, 6000)),
    dict(name='Item_Azufre', item_id='azufre', seed=4906, builder='azufre', tri_budget=(300, 5000)),
    dict(name='Item_Caracola', item_id='caracola', seed=4907, builder='caracola', tri_budget=(600, 8000),
         preview_rot_z=2.4),
    dict(name='Item_EsponjaMar', item_id='esponja_mar', seed=4908, builder='esponja_mar', tri_budget=(400, 6000)),
    dict(name='Item_AlgaFibra', item_id='alga_fibra', seed=4909, builder='alga_fibra', tri_budget=(400, 6000)),
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


def _mesh(name, bm):
    import bmesh
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    return o


# ---------------------------------------------------------------------------
# HOJA DE PLÁTANO
# ---------------------------------------------------------------------------
@_register('hoja_platano')
def _b_hoja_platano(v, rnd, name):
    """Hoja de plátano cortada (1 m): limbo ancho con nervios paralelos en
    diagonal, los desgarros típicos que la parten en tiras hacia el borde,
    nervio central grueso y claro y un trozo de pecíolo. Tumbada, con los
    bordes algo caídos y el extremo ondulado."""
    import bmesh
    p = K.Parts()
    L, W = 0.95, 0.20            # largo del limbo y media anchura máxima
    nu, nv = 30, 6
    tears = {}
    for side in (-1, 1):
        tears[side] = set(rnd.sample(range(4, nu - 2), 6))
    for side in (-1, 1):
        bm = bmesh.new()
        grid = []
        for i in range(nu + 1):
            t = i / nu
            w = W * (math.sin(min(1.0, t * 1.05 + 0.04) * math.pi) ** 0.55) * (1 - 0.15 * t)
            row = []
            for j in range(nv + 1):
                s = j / nv
                # los nervios salen del central inclinados hacia la punta
                y = t * L + s * w * 0.35
                x = side * s * w
                z = -0.05 * s * s * (0.6 + 0.4 * math.sin(t * math.pi)) + 0.012 * s * math.sin(t * 40 + side)
                z += -0.06 * (t - 0.5) ** 2 + 0.015
                row.append(bm.verts.new((x, y, z)))
            grid.append(row)
        for i in range(nu):
            for j in range(nv):
                # desgarro: se salta la tira exterior entre dos nervios
                if i in tears[side] and j >= 2:
                    continue
                f = (grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1])
                bm.faces.new(f if side > 0 else f[::-1])
        bmesh.ops.delete(bm, geom=[vv for vv in bm.verts if not vv.link_faces], context='VERTS')
        o = _mesh(f'Blade{side}', bm)
        I.solidify(o, 0.002)
        M.assign(o, ['M_Leaf'])

        def col(vv, side=side):
            co = vv.co
            t = co.y / L
            s = min(1.0, abs(co.x) / W)
            vein = 0.5 + 0.5 * math.cos((co.y - abs(co.x) * 0.35 / 1.0) * 230)
            c = I.lerp3(OPAL['banana_leaf'], OPAL['banana_leaf_hi'], 0.25 + vein * 0.3 + 0.2 * (1 - s))
            dry = max(0.0, s - 0.88) / 0.12 + max(0.0, t - 0.94) / 0.06
            return I.lerp3(c, OPAL['banana_dry'], min(1.0, dry) * 0.8) + (0.0,)
        C.set_vertex_colors(o, col)
        p.add(o, 'none')
    # nervio central + pecíolo
    n = 16
    pts = []
    for i in range(n + 1):
        t = i / n
        y = -0.12 + (L + 0.12) * t
        tt = y / L
        pts.append((0.0, y, 0.015 - 0.06 * (tt - 0.5) ** 2 + 0.004 + (0.02 * (1 - t * 8) if t < 0.12 else 0.0)))
    radii = [0.011 * (1 - t) ** 0.8 + 0.0015 for t in (i / n for i in range(n + 1))]
    rib = I.sweep('Rib', pts, radii, segs=8)
    for vv in rib.data.vertices:
        vv.co.z = 0.015 + (vv.co.z - 0.015) * 0.75
    rib.data.update()
    M.assign(rib, ['M_Leaf'])
    I.color_fn(rib, lambda co: PAL['bamboo_cut'] if co.y < -0.118 else
               I.lerp3(OPAL['banana_rib'], (0.36, 0.48, 0.12), max(0.0, co.y / L)), rnd, 0.01)
    p.add(rib, 'none')
    p.transform(Matrix.Rotation(-math.pi / 2, 4, 'Z'))   # a lo largo de +X
    return p.finish(name)


# ---------------------------------------------------------------------------
# MUSGO
# ---------------------------------------------------------------------------
@_register('musgo')
def _b_musgo(v, rnd, name):
    """Almohadilla de musgo arrancada (16 cm): cojín de matas redondeadas
    verde vivo con puntas amarillentas, algunas cápsulas en tallito rojizo
    y la costra de tierra oscura por debajo."""
    p = K.Parts()
    base = C.make_blob('Base', (0, 0, 0), 1.0, v['seed'], subdivisions=4, noise_scale=1.4, noise_strength=0.18,
                       relax_iterations=1)
    for vv in base.data.vertices:
        x, y, z = vv.co
        z = max(z, -0.2)
        vv.co = Vector((x * 0.075, y * 0.058, (z + 0.2) * 0.028))
    base.data.update()
    M.assign(base, ['M_Leaf'])

    def col(co):
        if co.z < 0.007:
            return I.lerp3(OPAL['soil'], OPAL['moss'], max(0.0, (co.z - 0.003) / 0.004))
        return I.lerp3(OPAL['moss'], OPAL['moss_hi'], max(0.0, I.noise3(co, 90)) * 1.4)
    I.color_fn(base, col, rnd, 0.015)
    p.add(base, 'none')
    # matas: cúpulas pequeñas sobre la superficie
    for k in range(34):
        a = rnd.uniform(0, 2 * math.pi)
        rr = math.sqrt(rnd.uniform(0, 1)) * 0.85
        x, y = math.cos(a) * rr * 0.075, math.sin(a) * rr * 0.058
        top = 0.028 * (1.2 - rr * rr) * 0.85 + 0.004
        r = rnd.uniform(0.008, 0.014)
        tuft = C.make_blob(f'T{k}', (x, y, top), 1.0, v['seed'] + k, subdivisions=2, noise_scale=3.0,
                           noise_strength=0.45, scale=(r, r, r * 0.75))
        M.assign(tuft, ['M_Leaf'])
        tone = rnd.uniform(0, 1)
        I.color_fn(tuft, lambda co, tone=tone, z0=top: I.lerp3(I.lerp3(OPAL['moss'], OPAL['moss_hi'], tone * 0.7),
                                                             (0.52, 0.56, 0.10), max(0.0, co.z - z0) / 0.012 * 0.6),
                   rnd, 0.02)
        p.add(tuft, 'none')
    # cápsulas (esporófitos)
    for k in range(6):
        a = rnd.uniform(0, 2 * math.pi)
        rr = rnd.uniform(0.1, 0.55)
        x, y = math.cos(a) * rr * 0.075, math.sin(a) * rr * 0.058
        z0 = 0.028 * (1.2 - rr * rr) * 0.85 + 0.004
        h = rnd.uniform(0.018, 0.028)
        lean = Vector((rnd.uniform(-0.3, 0.3), rnd.uniform(-0.3, 0.3), 1)).normalized()
        pts = [Vector((x, y, z0)) + lean * h * t for t in (0.0, 0.5, 1.0)]
        st = I.sweep(f'S{k}', pts, [0.0008, 0.0007, 0.0006], segs=4)
        M.assign(st, ['M_Leaf'])
        I.tint(st, (0.46, 0.14, 0.05), rnd, 0.02)
        p.add(st, 'none')
        cap = C.make_blob(f'C{k}', tuple(pts[-1] + lean * 0.002), 1.0, v['seed'] + 40 + k, subdivisions=1,
                          noise_strength=0.0, scale=(0.0018, 0.0018, 0.0032))
        M.assign(cap, ['M_Leaf'])
        I.tint(cap, (0.56, 0.30, 0.06), rnd, 0.02)
        p.add(cap, 'none')
    obj = p.finish(name)
    I.smooth_all(obj)
    return obj


# ---------------------------------------------------------------------------
# ALGODÓN
# ---------------------------------------------------------------------------
@_register('algodon_silvestre')
def _b_algodon_silvestre(v, rnd, name):
    """Ramita de algodonero silvestre (22 cm) con tres cápsulas reventadas:
    cuatro valvas pardas abiertas en estrella y el copo blanco cálido que
    rebosa en cuatro lóbulos."""
    p = K.Parts()
    L = 0.20
    stem, pts = I.stick('Twig', L, 0.0035, 0.0018, rnd, n=8, bend=0.06, wobble=0.01)
    M.assign(stem, ['M_Wood'])
    I.tint(stem, OPAL['twig'], rnd, 0.02)
    p.add(stem, 'none')
    spots = [(0.95, 0.0, 0.0), (0.55, 1.2, 0.05), (0.3, -1.4, 0.04)]
    for k, (t, ang, _off) in enumerate(spots):
        c = Vector(pts[int(t * 8)])
        d = Vector((math.cos(ang), math.sin(ang), 0.6)).normalized() if k else Vector((0, 0, 1))
        R = 0.022 if k == 0 else 0.018
        cen = c + d * (0.012 + R * 0.4)
        if k:
            br = I.sweep(f'Br{k}', [c, c + d * 0.012], [0.0016, 0.0013], segs=5)
            M.assign(br, ['M_Wood'])
            I.tint(br, OPAL['twig'], rnd, 0.02)
            p.add(br, 'none')
        # cuatro lóbulos de algodón
        for j in range(4):
            a = j / 4 * 2 * math.pi + k
            lob = C.make_blob(f'Cot{k}{j}', (0, 0, 0), 1.0, v['seed'] + k * 10 + j, subdivisions=2,
                              noise_scale=2.4, noise_strength=0.2, relax_iterations=1,
                              scale=(R * 0.62, R * 0.62, R * 0.7))
            lob.data.transform(Matrix.Translation((math.cos(a) * R * 0.42, math.sin(a) * R * 0.42, R * 0.25)))
            C.orient_and_place_zaxis(lob, cen, d)
            M.assign(lob, ['M_Fabric'])
            I.color_fn(lob, lambda co: I.lerp3(OPAL['cotton_shade'], OPAL['cotton'], 0.5 + 0.5 * I.noise3(co, 180)),
                       rnd, 0.01)
            p.add(lob, 'none')
            # valva parda detrás de cada lóbulo
            vp = []
            for q in range(4):
                u = q / 3
                rad = R * (0.25 + 0.75 * u)
                vp.append((math.cos(a + 0.4) * rad, math.sin(a + 0.4) * rad, -R * 0.35 + R * 0.25 * u))
            valve = I.sweep(f'Val{k}{j}', vp, [0.0015, 0.004, 0.0045, 0.0012], segs=5)
            C.orient_and_place_zaxis(valve, cen, d)
            M.assign(valve, ['M_Wood'])
            I.color_fn(valve, lambda co: OPAL['boll'], rnd, 0.02)
            p.add(valve, 'none')
    I.lay_along_x(p)
    return p.finish(name)


# ---------------------------------------------------------------------------
# ARENA, SAL, AZUFRE
# ---------------------------------------------------------------------------
def _mound(name, R, H, seed, rings=10, segs=28, ripple=0.0):
    """Montón a granel: cono suave con pie ancho y ruido."""
    prof = [(0.0, H)]
    for i in range(1, rings + 1):
        t = i / rings
        prof.append((R * t, H * (1 - t ** 1.4) ** 1.8))
    o = I.lathe(name, prof, segs=segs)
    r = random.Random(seed)
    s0 = r.uniform(0, 50)
    for vv in o.data.vertices:
        co = vv.co
        rr = math.hypot(co.x, co.y) / R
        k = 1.0 + 0.18 * I.noise3(co, 9 / R * 0.1, s0)
        vv.co.x *= k
        vv.co.y *= k
        vv.co.z += H * 0.08 * I.noise3(co, 30, s0) * (1 - rr) + ripple * math.sin(co.x / R * 14) * rr * (1 - rr)
    o.data.update()
    return o


@_register('arena')
def _b_arena(v, rnd, name):
    """Montoncito de arena de playa (15 cm): coral molido crema dorado con
    granos más oscuros, un fragmento de concha rosada y otro de coral."""
    p = K.Parts()
    o = _mound('Sand', 0.075, 0.045, v['seed'], rings=12, segs=32, ripple=0.002)
    M.assign(o, ['M_Stone'])
    I.color_fn(o, lambda co: I.lerp3(OPAL['sand'], OPAL['sand_dark'], max(0.0, I.noise3(co, 260)) * 1.2
                                     + max(0.0, 0.004 - co.z) / 0.004 * 0.3), rnd, 0.025)
    p.add(o, 'none')
    for k, (pos, rgb, sc) in enumerate((((0.03, -0.02, 0.022), (0.86, 0.52, 0.46), (0.009, 0.007, 0.002)),
                                        ((-0.035, 0.018, 0.02), (0.92, 0.84, 0.74), (0.011, 0.004, 0.003)),
                                        ((0.005, 0.04, 0.018), (0.72, 0.30, 0.20), (0.006, 0.006, 0.002)))):
        bit = C.make_blob(f'Bit{k}', pos, 1.0, v['seed'] + k, subdivisions=1, noise_strength=0.2, scale=sc)
        M.assign(bit, ['M_Stone'])
        I.tint(bit, rgb, rnd, 0.02)
        p.add(bit, 'none')
    return p.finish(name)


@_register('tierra_suelta')
def _b_tierra_suelta(v, rnd, name):
    """Puñado de tierra suelta recién cavada (15 cm): montón más alto y
    grumoso que el de arena, pardo oscuro con terrones secos más claros
    arriba, dos chinas y una raicilla que asoma."""
    p = K.Parts()
    o = _mound('Soil', 0.07, 0.055, v['seed'], rings=12, segs=32, ripple=0.004)
    M.assign(o, ['M_Stone'])
    I.color_fn(o, lambda co: I.lerp3(OPAL['soil'], OPAL['soil_dry'], max(0.0, I.noise3(co, 180)) * 0.9
                                     + max(0.0, co.z - 0.03) / 0.025 * 0.35), rnd, 0.03)
    p.add(o, 'none')
    r = random.Random(v['seed'])
    for k in range(5):
        a = r.uniform(0, 2 * math.pi)
        rr = r.uniform(0.2, 0.75)
        pos = (math.cos(a) * rr * 0.06, math.sin(a) * rr * 0.06, 0.055 * (1 - rr ** 1.4) ** 1.8 + 0.004)
        clod = C.make_blob(f'Clod{k}', pos, 1.0, v['seed'] + 10 + k, subdivisions=1, noise_strength=0.3,
                           scale=(0.012, 0.010, 0.008))
        M.assign(clod, ['M_Stone'])
        I.tint(clod, OPAL['soil_dry'], rnd, 0.03)
        p.add(clod, 'none')
    for k, (pos, sc) in enumerate((((0.04, -0.03, 0.012), (0.008, 0.006, 0.005)),
                                   ((-0.045, 0.02, 0.01), (0.006, 0.005, 0.004)))):
        pebble = C.make_blob(f'Pebble{k}', pos, 1.0, v['seed'] + 20 + k, subdivisions=1, noise_strength=0.15, scale=sc)
        M.assign(pebble, ['M_Stone'])
        I.tint(pebble, OPAL['soil_pebble'], rnd, 0.02)
        p.add(pebble, 'none')
    root = I.soft_box('Root', (0.05, 0.003, 0.003), roundness=0.5, cuts=2)
    root.data.transform(Matrix.Rotation(0.5, 4, 'Y') @ Matrix.Rotation(0.8, 4, 'Z'))
    root.data.transform(Matrix.Translation((0.012, 0.01, 0.045)))
    M.assign(root, ['M_Wood'])
    I.tint(root, OPAL['soil_root'], rnd, 0.02)
    p.add(root, 'none')
    return p.finish(name)


@_register('sal_marina')
def _b_sal_marina(v, rnd, name):
    """Sal marina recogida de una charca de roca (10 cm): costra plana
    rota con un montón de cristales cúbicos blancos y rosados encima."""
    p = K.Parts()
    crust = C.make_blob('Crust', (0, 0, 0), 1.0, v['seed'], subdivisions=3, noise_scale=1.2, noise_strength=0.2)
    for vv in crust.data.vertices:
        x, y, z = vv.co
        vv.co = Vector((x * 0.055, y * 0.045, (math.copysign(abs(z) ** 1.8, z) + 1) * 0.004))
    crust.data.update()
    M.assign(crust, ['M_Stone'])
    I.color_fn(crust, lambda co: I.lerp3(OPAL['salt_crust'], OPAL['salt'], min(1.0, max(0.0, co.z - 0.003) / 0.003)
                                     * (0.75 + 0.25 * I.noise3(co, 80))), rnd, 0.02)
    p.add(crust, 'none')
    r = random.Random(v['seed'])
    for k in range(34):
        a = r.uniform(0, 2 * math.pi)
        rr = math.sqrt(r.uniform(0, 1))
        s = r.uniform(0.006, 0.011) * (1.25 - rr * 0.5)
        x, y = math.cos(a) * rr * 0.035, math.sin(a) * rr * 0.028
        z = 0.007 + (1 - rr * rr) * 0.016 + s * 0.3
        cube = I.soft_box(f'X{k}', (s, s * r.uniform(0.8, 1.1), s * r.uniform(0.7, 1.0)), roundness=0.3, cuts=2)
        cube.data.transform(Matrix.Rotation(r.uniform(-0.6, 0.6), 4, 'X') @ Matrix.Rotation(r.uniform(-0.6, 0.6), 4, 'Y')
                            @ Matrix.Rotation(r.uniform(0, 1.6), 4, 'Z'))
        cube.data.transform(Matrix.Translation((x, y, z)))
        M.assign(cube, ['M_Stone'])
        tone = r.uniform(0, 1)
        I.tint(cube, I.lerp3(OPAL['salt'], OPAL['salt_pink'], tone * tone), rnd, 0.015)
        p.add(cube, 'none')
    return p.finish(name)


@_register('azufre')
def _b_azufre(v, rnd, name):
    """Terrón de azufre de fumarola (10 cm): amarillo limón con facetas de
    fractura, drusas de cristalitos puntiagudos más claros y costra ocre
    rojiza en la base."""
    p = K.Parts()
    o = C.make_blob('Lump', (0, 0, 0), 1.0, v['seed'], subdivisions=4, noise_scale=1.5, noise_strength=0.22,
                    relax_iterations=1)
    r = random.Random(v['seed'])
    planes = [(Vector((r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-0.2, 1))).normalized(), r.uniform(0.6, 0.8))
              for _ in range(6)]
    for vv in o.data.vertices:
        co = vv.co.copy()
        for n_, d in planes:
            h = co.dot(n_)
            if h > d:
                co -= n_ * (h - d)
        co.z = max(co.z, -0.6)
        vv.co = Vector((co.x * 0.05, co.y * 0.042, (co.z + 0.6) * 0.03))
    o.data.update()
    M.assign(o, ['M_Stone'])

    def col(co):
        if co.z < 0.006:
            return I.lerp3(OPAL['sulfur_rust'], OPAL['sulfur'], co.z / 0.006)
        return I.lerp3(OPAL['sulfur'], OPAL['sulfur_hi'], max(0.0, I.noise3(co, 70)) * 1.3)
    I.color_fn(o, col, rnd, 0.02)
    p.add(o, 'none')
    # drusa de cristales
    for k in range(16):
        a = r.uniform(0, 2 * math.pi)
        rr = r.uniform(0.0, 0.6)
        base = Vector((math.cos(a) * rr * 0.04, math.sin(a) * rr * 0.03, 0.0))
        # apoyo en la superficie: busca el vértice más alto cercano
        best = max(o.data.vertices, key=lambda vv: vv.co.z - 8 * (Vector((vv.co.x, vv.co.y, 0)) - base).length)
        pos = best.co.copy() - Vector((0, 0, 0.002))
        d = (Vector((pos.x, pos.y, 0.0)) * 8 + Vector((0, 0, 1))).normalized()
        d = (d + Vector((r.uniform(-0.3, 0.3), r.uniform(-0.3, 0.3), 0))).normalized()
        h = r.uniform(0.010, 0.022)
        cr = C.make_cylinder(f'Cr{k}', h * 0.28, h, segments=4, center=(0, 0, h / 2), radius2=0.0004)
        C.orient_and_place_zaxis(cr, pos, d)
        M.assign(cr, ['M_Stone'])
        I.tint(cr, OPAL['sulfur_hi'], rnd, 0.02)
        p.add(cr, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# CARACOLA (pū)
# ---------------------------------------------------------------------------
@_register('caracola')
def _b_caracola(v, rnd, name):
    """Caracola de tritón (20 cm), el pū que los navegantes soplaban:
    espira alta de vueltas crecientes con cordones y nódulos, crema con
    medias lunas pardas, boca ovalada con el labio claro dentado que se
    abre hacia dentro a un interior naranja, y la punta limada (el agujero
    por donde se sopla). Tumbada sobre el costado, con la boca hacia arriba."""
    p = K.Parts()
    turns = 4.6
    per = 18
    n = int(turns * per)
    th_end = turns * 2 * math.pi
    b = math.log(1.75) / (2 * math.pi)       # crecimiento por vuelta
    R_end = 0.034
    segs = 16

    def R(th):
        return R_end * math.exp(b * (th - th_end))

    centers, radii, stretch = [], [], []
    for i in range(n + 1):
        th = i / n * th_end
        rr = R(th)
        centers.append(Vector((math.cos(th) * rr, math.sin(th) * rr, -rr * 4.6)))
        radii.append(rr * 0.86 + 0.0008)
        # la última vuelta se estira a lo largo del eje: boca ovalada
        stretch.append(1.0 + 0.55 * max(0.0, (th - (th_end - 2 * math.pi)) / (2 * math.pi)) ** 1.5)
    rings = []
    prev_n = None
    frames = []
    for i, c in enumerate(centers):
        t = (centers[min(i + 1, n)] - centers[max(i - 1, 0)]).normalized()
        if prev_n is None:
            nn = (Vector((0, 0, 1)) - t * t.z).normalized()
        else:
            nn = (prev_n - t * prev_n.dot(t)).normalized()
        prev_n = nn
        bb = t.cross(nn)
        frames.append((t, nn, bb))
        ring = []
        for k in range(segs):
            a = 2 * math.pi * k / segs
            d = nn * math.cos(a) + bb * math.sin(a)
            # cordones espirales (4) y nódulos cada cuarto de vuelta
            bump = 0.07 * max(0.0, math.cos(a * 4)) + 0.10 * max(0.0, math.cos(i / per * 2 * math.pi * 3)) ** 6 \
                * max(0.0, math.cos(a - 0.6))
            q = d * radii[i] * (1 + bump)
            q.z *= stretch[i]
            ring.append(c + q)
        rings.append(ring)
    # labio: se ensancha, vuelve sobre sí y se mete hacia dentro de la boca
    t, nn, bb = frames[-1]
    c = centers[-1]
    lip_rings = []
    for fwd, sc in ((0.004, 1.10), (0.006, 1.16), (0.004, 1.02), (-0.004, 0.86), (-0.02, 0.62)):
        ring = []
        for k in range(segs):
            a = 2 * math.pi * k / segs
            q = (nn * math.cos(a) + bb * math.sin(a)) * radii[-1] * sc
            q.z *= stretch[-1]
            ring.append(c + t * fwd + q)
        lip_rings.append(ring)
    rings += lip_rings
    o = C.ring_loft('Shell', rings, cap_start=True, cap_end=True)
    M.assign(o, ['M_Stone'])
    body_rings = n + 1

    def col(vv):
        ring, k = divmod(vv.index, segs)
        if ring >= body_rings:
            j = ring - body_rings
            if j <= 2:
                # labio crema con dientes pardos
                return I.lerp3((0.96, 0.88, 0.72), OPAL['conch_mark'],
                               0.8 * max(0.0, math.cos(k / segs * 2 * math.pi * 7)) ** 3) + (0.0,)
            return I.lerp3(OPAL['conch_lip'], (0.60, 0.20, 0.06), (j - 3) / 1.0) + (0.0,)
        th = ring / n * th_end
        a = k / segs * 2 * math.pi
        m = math.sin(th * 2.0 + 0.8) * math.cos(a * 2 - 0.5)
        c = I.lerp3(OPAL['conch'], OPAL['conch_mark'], min(1.0, max(0.0, m + 0.05) * 2.2))
        c = I.lerp3(c, (0.96, 0.90, 0.76), 0.35 * max(0.0, math.cos(a * 4)))
        # la punta, gastada y rosada
        return I.lerp3(c, (0.80, 0.56, 0.46), max(0.0, 1 - th / 5.0)) + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    # boquilla limada en la punta
    tip = centers[0]
    mouth = C.make_cylinder('Mouth', 0.0032, 0.004, segments=10, center=(tip.x, tip.y, tip.z + 0.0015))
    M.assign(mouth, ['M_Stone'])
    I.tint(mouth, (0.30, 0.12, 0.05), rnd, 0.01)
    p.add(mouth, 'none')
    # tumbada: el eje de la concha pasa a horizontal y la boca mira arriba
    p.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
    t_w = Matrix.Rotation(math.pi / 2, 4, 'Y') @ bb.to_4d()
    ang = math.atan2(t_w.y, t_w.z)
    p.transform(Matrix.Rotation(ang - 0.35, 4, 'X'))
    return p.finish(name)


# ---------------------------------------------------------------------------
# ESPONJA, ALGA
# ---------------------------------------------------------------------------
@_register('esponja_mar')
def _b_esponja_mar(v, rnd, name):
    """Esponja de baño natural (12 cm) seca al sol: bola aplastada ocre
    anaranjada, superficie de poros hundidos y tres ósculos grandes."""
    p = K.Parts()
    o = C.make_blob('Sponge', (0, 0, 0), 1.0, v['seed'], subdivisions=4, noise_scale=0.9, noise_strength=0.26,
                    relax_iterations=1)
    r = random.Random(v['seed'])
    oscula = [Vector((r.uniform(-0.5, 0.5), r.uniform(-0.5, 0.5), 1.0)).normalized() for _ in range(3)]
    pore = {}
    for vv in o.data.vertices:
        co = vv.co.copy()
        d = co.normalized()
        cell = I.noise3(co, 11.0, 2.0)
        dent = max(0.0, cell - 0.2) * 0.22
        big = 0.0
        for oc in oscula:
            ang = d.angle(oc)
            if ang < 0.2:
                big = max(big, (1 - ang / 0.2) * 0.35)
        pore[vv.index] = min(1.0, max(0.0, cell - 0.08) * 3.2 + big * 3)
        co = co * (1.0 - dent - big)
        co.z = max(co.z, -0.55)
        vv.co = Vector((co.x * 0.062, co.y * 0.054, (co.z + 0.55) * 0.036))
    o.data.update()
    M.assign(o, ['M_Fabric'])

    def col(vv):
        c = I.lerp3(OPAL['sponge'], (0.92, 0.66, 0.20), max(0.0, I.noise3(vv.co, 150)) * 1.5)
        return I.lerp3(c, OPAL['sponge_dark'], pore[vv.index]) + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    return p.finish(name)


@_register('alga_fibra')
def _b_alga_fibra(v, rnd, name):
    """Manojo de alga parda de fibra (34 cm): seis cintas onduladas con el
    nervio central, dobladas en dos y atadas con una tira de la misma alga;
    ámbar oliva con las puntas doradas."""
    p = K.Parts()
    for k in range(6):
        L = rnd.uniform(0.28, 0.34)
        W = rnd.uniform(0.014, 0.022)
        y0 = (k - 2.5) * 0.009
        n = 22
        rows = []
        for i in range(n + 1):
            t = i / n
            x = L * (t - 0.35)
            y = y0 * (1 + 1.8 * abs(t - 0.35)) + 0.004 * math.sin(t * 13 + k)
            z = 0.004 + 0.003 * k * 0.3 + 0.004 * math.sin(t * 17 + k * 2) * t
            w = W * (0.5 + 0.5 * math.sin(min(1.0, t * 1.1 + 0.05) * math.pi) ** 0.5)
            ruffle = 0.003 * math.sin(t * 60 + k)
            rows.append([Vector((x, y - w, z + ruffle)), Vector((x, y, z + 0.0015)), Vector((x, y + w, z - ruffle))])
        import bmesh
        bm = bmesh.new()
        vs = [[bm.verts.new(q) for q in row] for row in rows]
        for i in range(n):
            for j in range(2):
                bm.faces.new((vs[i][j], vs[i + 1][j], vs[i + 1][j + 1], vs[i][j + 1]))
        o = _mesh(f'Kelp{k}', bm)
        I.solidify(o, 0.0015)
        M.assign(o, ['M_Leaf'])
        Lk = L

        def col(co, Lk=Lk, y0=y0):
            t = co.x / Lk + 0.35
            mid = max(0.0, 1 - abs(co.y - y0 * (1 + 1.8 * abs(t - 0.35))) / 0.003)
            c = I.lerp3(OPAL['kelp'], OPAL['kelp_hi'], max(0.0, t - 0.55) / 0.45)
            return I.lerp3(c, OPAL['kelp_dark'], mid * 0.6)
        I.color_fn(o, col, rnd, 0.02)
        p.add(o, 'none')
    # atadura en el doblez
    wrap = I.helix_wrap('Wrap', -0.012, 0.012, 0.03, 0.003, rnd, turns=3, segs=5)
    for vv in wrap.data.vertices:
        vv.co.x *= 0.35
    wrap.data.update()
    wrap.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
    wrap.data.transform(Matrix.Translation((0.0, 0.0, 0.008)))
    M.assign(wrap, ['M_Leaf'])
    I.tint(wrap, OPAL['kelp_dark'], rnd, 0.02)
    p.add(wrap, 'none')
    return p.finish(name)
