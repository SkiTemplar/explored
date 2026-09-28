"""
items_naturales.py — materiales naturales de las primeras horas
(items.json) que faltaban tras items_materiales.py: huesos, conchas,
yesca de hongo, corteza, cuerda, piedra plana, nódulo de obsidiana y
cáscara de coco rota.

Objetos sueltos: pivote en la base (z = 0), centrados en XY, en su postura
de reposo (los alargados a lo largo de +X). Escala real en metros.
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
import items_contenedores as IC  # noqa: E402
import items_materiales as IM  # noqa: E402
import kit_construccion as K  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils import noise as mnoise

GROUP = I.GROUP
PAL = I.PAL

NPAL = {
    'bone': (0.74, 0.64, 0.45),
    'bone_end': (0.60, 0.46, 0.28),
    'bone_stain': (0.36, 0.22, 0.10),
    'tridacna_out': (0.66, 0.52, 0.32),
    'tridacna_rib': (0.46, 0.36, 0.22),
    'tridacna_in': (0.80, 0.70, 0.52),
    'tridacna_lip': (0.30, 0.20, 0.42),
    'cockle': (0.78, 0.36, 0.14),
    'cockle_rib': (0.55, 0.18, 0.08),
    'cockle_in': (0.84, 0.62, 0.50),
    'fungus_top': [(0.13, 0.065, 0.025), (0.46, 0.27, 0.09)],
    'fungus_pore': (0.70, 0.58, 0.36),
    'bark_in': (0.58, 0.40, 0.18),
    'slate': (0.26, 0.12, 0.055),
    'slate_light': (0.50, 0.30, 0.14),
    'obsidian': (0.018, 0.014, 0.026),
    'obsidian_glint': (0.16, 0.11, 0.28),
    'obsidian_cortex': (0.36, 0.22, 0.10),
}

VARIANTS = [
    dict(name='Item_HuesoLargo', item_id='hueso_largo', seed=4401, builder='hueso_largo', tri_budget=(300, 4000)),
    dict(name='Item_HuesoPequeno', item_id='hueso_pequeno', seed=4402, builder='hueso_pequeno',
         tri_budget=(150, 2500)),
    dict(name='Item_ConchaGrande', item_id='concha_grande', seed=4403, builder='concha_grande',
         tri_budget=(300, 4000)),
    dict(name='Item_ConchaPequena', item_id='concha_pequena', seed=4404, builder='concha_pequena',
         tri_budget=(200, 3000)),
    dict(name='Item_YescaHongo', item_id='yesca_hongo', seed=4405, builder='yesca_hongo', tri_budget=(200, 3000)),
    dict(name='Item_Corteza', item_id='corteza', seed=4406, builder='corteza', tri_budget=(200, 3000)),
    dict(name='Item_Cuerda', item_id='cuerda', seed=4407, builder='cuerda', tri_budget=(500, 6000)),
    dict(name='Item_PiedraPlana', item_id='piedra_plana', seed=4408, builder='piedra_plana', tri_budget=(150, 2500)),
    dict(name='Item_Obsidiana', item_id='obsidiana', seed=4409, builder='obsidiana', tri_budget=(150, 2500)),
    dict(name='Item_CascaraCoco', item_id='cascara_coco', seed=4410, builder='cascara_coco', tri_budget=(300, 4000)),
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
# HUESOS
# ---------------------------------------------------------------------------
def _bone_col(length, z0=0.0):
    def f(co):
        t = abs((co.z - z0) / length - 0.5) * 2  # 0 centro .. 1 extremos
        c = I.lerp3(NPAL['bone'], NPAL['bone_end'], max(0.0, t - 0.55) / 0.45)
        stain = max(0.0, mnoise.noise(Vector((co.x * 60, co.y * 60, co.z * 25))) - 0.25)
        return I.lerp3(c, NPAL['bone_stain'], min(1.0, stain * 1.5))
    return f


@_register('hueso_largo')
def _b_hueso_largo(v, rnd, name):
    """Fémur de jabalí/cerdo asilvestrado (34 cm): diáfisis algo curva,
    cabeza redonda con cuello en un extremo y dos cóndilos en el otro."""
    p = K.Parts()
    L = 0.30
    n = 12
    pts = [(0.006 * math.sin(i / n * math.pi), 0.0, L * i / n) for i in range(n + 1)]
    radii = [0.013 + 0.013 * max(0.0, abs(i / n - 0.5) * 2 - 0.4) ** 1.6 * 2.2 for i in range(n + 1)]
    shaft = I.sweep('Shaft', pts, radii, segs=10)
    M.assign(shaft, ['M_Stone'])
    col = _bone_col(L)
    I.color_fn(shaft, col, rnd, 0.015)
    p.add(shaft, 'none')
    # cabeza + trocánter arriba
    for nm, c, sc in (('Head', (-0.018, 0.0, L + 0.018), (0.022, 0.021, 0.021)),
                      ('Troch', (0.012, 0.0, L + 0.004), (0.017, 0.016, 0.02)),
                      ('Neck', (-0.006, 0.0, L + 0.006), (0.016, 0.014, 0.016))):
        b = C.make_blob(nm, c, 1.0, v['seed'] + len(nm), subdivisions=2, noise_strength=0.05, scale=sc,
                        relax_iterations=1)
        M.assign(b, ['M_Stone'])
        I.color_fn(b, col, rnd, 0.015)
        p.add(b, 'none')
    # cóndilos abajo
    for k, dy in enumerate((-0.013, 0.013)):
        b = C.make_blob(f'Cond{k}', (0.006, dy, -0.006), 1.0, v['seed'] + 10 + k, subdivisions=2,
                        noise_strength=0.05, scale=(0.02, 0.014, 0.018), relax_iterations=1)
        M.assign(b, ['M_Stone'])
        I.color_fn(b, col, rnd, 0.015)
        p.add(b, 'none')
    I.lay_along_x(p)
    return p.finish(name)


@_register('hueso_pequeno')
def _b_hueso_pequeno(v, rnd, name):
    """Hueso de ave/pescado grande partido en astilla: nudillo articular en
    un extremo y punta aguzada en el otro (sirve de punzón)."""
    p = K.Parts()
    L = 0.11
    n = 10
    pts = I.crooked(L, rnd, n=n, bend=0.04, wobble=0.005)
    radii = [0.0075 * (1 - max(0.0, (i / n - 0.45) / 0.55) ** 1.3 * 0.95) for i in range(n + 1)]
    o = I.sweep('Splinter', pts, radii, segs=8)
    # astilla: aplasta un lado en la mitad de la punta (corte de la fractura)
    for vv in o.data.vertices:
        if vv.co.z > L * 0.4 and vv.co.y > 0:
            vv.co.y *= 0.35
    o.data.update()
    M.assign(o, ['M_Stone'])
    col = _bone_col(L)
    I.color_fn(o, lambda co: I.lerp3(col(co), (0.86, 0.80, 0.64), 0.6) if (co.y > -0.0005 and co.z > L * 0.45)
               else col(co), rnd, 0.015)
    p.add(o, 'none')
    knob = C.make_blob('Knob', (0.0, 0.0, -0.002), 1.0, v['seed'], subdivisions=2, noise_strength=0.1,
                       scale=(0.012, 0.01, 0.009), relax_iterations=1)
    M.assign(knob, ['M_Stone'])
    I.color_fn(knob, col, rnd, 0.015)
    p.add(knob, 'none')
    I.lay_along_x(p)
    return p.finish(name)


# ---------------------------------------------------------------------------
# CONCHAS
# ---------------------------------------------------------------------------
def _shell_paint(o, width, height, out, rib, inn, inn_edge, ribs, concave_up):
    """Pinta una valva de clam_shell() ya girada: costillas radiales por
    fuera, interior liso con el borde de otro color. La cara exterior es la
    de abajo si `concave_up`."""
    me = o.data
    me.update()
    normals = {vv.index: vv.normal.copy() for vv in me.vertices}

    def f(vv):
        co = vv.co
        ang = math.atan2(co.x, max(1e-5, co.y if not concave_up else co.y))
        dist = math.hypot(co.x, co.y) / height
        n = normals[vv.index]
        outside = (n.z < 0) if concave_up else (n.z > 0)
        wave = 0.5 + 0.5 * math.cos(ang * (ribs + 0.5) * 2)
        if outside:
            band = 0.5 + 0.5 * math.sin(dist * 22)
            c = I.lerp3(out, rib, wave * 0.7)
            return I.lerp3(c, rib, band * 0.25) + (0.0,)
        return I.lerp3(inn, inn_edge, max(0.0, dist - 0.72) / 0.28) + (0.0,)
    C.set_vertex_colors(o, f)


@_register('concha_grande')
def _b_concha_grande(v, rnd, name):
    """Valva de almeja gigante (30 cm) boca arriba: costillas festoneadas
    por fuera, nácar crema por dentro con el borde violeta del manto."""
    p = K.Parts()
    W, H = 0.30, 0.24
    o = I.clam_shell('Valve', W, H, 0.075, ribs=5, thick=0.016, segs_u=50, segs_v=10, scallop=0.03,
                     rib_depth=0.14)
    # convexa (-Y) hacia abajo: +Y -> +Z
    o.data.transform(Matrix.Rotation(math.pi / 2, 4, 'X'))
    M.assign(o, ['M_Stone'])
    _shell_paint(o, W, H, NPAL['tridacna_out'], NPAL['tridacna_rib'], NPAL['tridacna_in'], NPAL['tridacna_lip'],
                 5, concave_up=True)
    p.add(o, 'none')
    obj = p.finish(name)
    I.smooth_all(obj)
    return obj


@_register('concha_pequena')
def _b_concha_pequena(v, rnd, name):
    """Berberecho (6 cm) boca abajo: costillas finas naranja y teja."""
    p = K.Parts()
    W, H = 0.06, 0.055
    o = I.clam_shell('Valve', W, H, 0.02, ribs=9, thick=0.003, segs_u=38, segs_v=8, scallop=0.008,
                     rib_depth=0.07)
    # convexa (-Y) hacia arriba: -Y -> +Z
    o.data.transform(Matrix.Rotation(-math.pi / 2, 4, 'X'))
    M.assign(o, ['M_Stone'])
    _shell_paint(o, W, H, NPAL['cockle'], NPAL['cockle_rib'], NPAL['cockle_in'], NPAL['cockle_rib'], 9,
                 concave_up=False)
    p.add(o, 'none')
    obj = p.finish(name)
    I.smooth_all(obj)
    return obj


# ---------------------------------------------------------------------------
# VEGETALES
# ---------------------------------------------------------------------------
@_register('yesca_hongo')
def _b_yesca_hongo(v, rnd, name):
    """Hongo yesquero (casco de caballo) arrancado del tronco: bandas
    concéntricas pardas y ocres por arriba, poros crema por debajo y la
    cara plana por donde estaba pegado."""
    p = K.Parts()
    # casco en escalones: perfil de torno (media vuelta) alrededor del eje
    # de anclaje; los anillos de crecimiento son rebordes del perfil
    R, h, n = 0.07, 0.055, 6
    prof = [(0.0, 0.0), (R * 0.98, 0.0), (R, 0.006)]
    for i in range(n):
        t0 = i / n
        r = R * (1 - t0) ** 0.9
        z = 0.008 + (h - 0.008) * (1 - (1 - t0) ** 1.8)
        prof.append((r * 1.0, z + 0.004))            # reborde del anillo
        prof.append((r * 0.93, z + 0.009))           # escalón hacia dentro
    prof.append((0.0, h + 0.004))
    # media luna hacia -Y: la cara de anclaje plana queda detrás (y = 0)
    o = I.lathe('Conk', prof, segs=16, sy=-1.15, arc=math.pi)
    for vv in o.data.vertices:
        rr = math.hypot(vv.co.x, vv.co.y) / R
        vv.co.z += 0.003 * math.sin(math.atan2(vv.co.y, vv.co.x) * 5) * min(1.0, vv.co.z / 0.02) * min(1.0, rr * 2)
    o.data.update()
    M.assign(o, ['M_Wood'])

    def col(vv):
        co = vv.co
        ring = 0 if vv.index == 0 else (vv.index - 1) // 17 + 1  # punto del perfil
        if co.z < 0.003 or ring in (1, 2) and co.z < 0.012:
            c = NPAL['fungus_pore']                 # poros crema y labio de crecimiento
        elif co.y > -0.0015:
            c = I.lerp3(NPAL['fungus_top'][0], PAL['wood_cut'], 0.35)  # carne del anclaje
        else:
            # rebordes ocres y escalones pardos oscuros: anillos concéntricos
            c = NPAL['fungus_top'][1] if (ring - 3) % 2 == 0 else NPAL['fungus_top'][0]
        return c + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    return p.finish(name)


@_register('corteza')
def _b_corteza(v, rnd, name):
    """Tira de corteza de 40 cm arrancada a lo largo, abarquillada en
    canaleta: surcos oscuros por fuera y fibra clara por dentro."""
    import bmesh
    p = K.Parts()
    L, Wd = 0.40, 0.12
    nu, nv = 14, 8
    bm = bmesh.new()
    grid = []
    axis_z = []
    for i in range(nu + 1):
        t = i / nu
        w = Wd * (0.85 + 0.15 * math.sin(t * math.pi)) * (0.85 if i in (0, nu) else 1.0)
        arc = (0.55 + 0.45 * t) * math.pi * 0.75  # se abarquilla más hacia un extremo
        R = w / arc
        axis_z.append(R)
        row = []
        for j in range(nv + 1):
            s_ = j / nv - 0.5
            a = s_ * arc
            ridge = (0.0025 if j % 2 == 0 else -0.0015) if 0 < j < nv else 0.0
            x = math.sin(a) * (R + ridge)
            z = R - math.cos(a) * (R + ridge)
            y = L * t + (rnd.uniform(-0.012, 0.012) if i in (0, nu) else 0.0)  # extremos rasgados
            row.append(bm.verts.new((x, y, z)))
        grid.append(row)
    for i in range(nu):
        for j in range(nv):
            bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new('Bark')
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new('Bark', me)
    C.link_object(o)
    N = len(o.data.vertices)
    I.solidify(o, 0.009)
    M.assign(o, ['M_Wood'])
    verts = o.data.vertices

    def axis_dist(idx):
        co = verts[idx].co
        row = min(nu, max(0, int(round(co.y / L * nu))))
        return math.hypot(co.x, co.z - axis_z[row])

    # solidify duplica la capa: el vértice i y el i+N son pareja; el más
    # alejado del eje de curvatura es la cara exterior (corteza)
    outer = set()
    for i in range(min(N, len(verts) - N)):
        outer.add(i if axis_dist(i) > axis_dist(i + N) else i + N)

    def col(vv):
        co = vv.co
        if vv.index in outer:
            streak = 0.5 + 0.5 * math.sin(co.x * 260 + math.sin(co.y * 40) * 2)
            return I.lerp3(PAL['bark'][2], PAL['bark'][1], streak) + (0.0,)
        fib = 0.5 + 0.5 * math.sin(co.x * 400)
        return I.lerp3(NPAL['bark_in'], PAL['wood_cut'], fib * 0.6) + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    # caída boca abajo (lo normal al arrancarla): la corteza queda a la vista
    o.data.transform(Matrix.Rotation(math.pi, 4, 'Y'))
    o.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Z'))
    return p.finish(name)


@_register('cuerda')
def _b_cuerda(v, rnd, name):
    """Rollo de cuerda gruesa de tres cabos, con los chicotes rematados."""
    p = K.Parts()
    o = IM._coil('Coil', 3.4, 0.085, 0.006, 0.0075, rnd, pts_per_turn=26, tail=0.16)
    M.assign(o, ['M_Fabric'])
    I.cord_stripes(o, PAL['cord'][0], I.lerp3(PAL['cord'][0], PAL['coir'][2], 0.6), 6, period=2)
    p.add(o, 'none')
    # remate (ligada) del chicote suelto: sale del último punto del rollo
    last = Vector(o.data.vertices[-1].co)
    tip = C.make_cylinder('Whip', 0.0085, 0.022, segments=10, center=(0, 0, 0))
    first = Vector(o.data.vertices[-7].co)
    d = (last - first).normalized() if (last - first).length > 1e-5 else Vector((1, 0, 0))
    C.orient_and_place_zaxis(tip, last - d * 0.012, d)
    M.assign(tip, ['M_Fabric'])
    I.tint(tip, PAL['cord_red'], rnd, 0.02)
    p.add(tip, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# PIEDRAS
# ---------------------------------------------------------------------------
@_register('piedra_plana')
def _b_piedra_plana(v, rnd, name):
    """Laja de río plana (20 cm), cantos redondeados y una veta clara:
    yunque, tapa o piedra de moler."""
    p = K.Parts()
    o = C.make_blob('Slab', (0, 0, 0), 1.0, v['seed'], subdivisions=4, noise_scale=1.0, noise_strength=0.12,
                    relax_iterations=2)
    for vv in o.data.vertices:
        x, y, z = vv.co
        z = math.copysign(abs(z) ** 1.6, z)  # caras planas
        vv.co = Vector((x * 0.10, y * 0.072, z * 0.022))
    o.data.update()
    M.assign(o, ['M_Stone'])

    def col(co):
        vein = abs(co.x * 0.6 - co.y + 0.01) < 0.005
        c = I.lerp3(NPAL['slate'], NPAL['slate_light'], 0.5 + 0.5 * mnoise.noise(Vector((co.x * 30, co.y * 30, 0.5))))
        return PAL['quartz'] if vein else c
    I.color_fn(o, col, rnd, 0.015)
    p.add(o, 'none')
    return p.finish(name)


@_register('obsidiana')
def _b_obsidiana(v, rnd, name):
    """Nódulo de obsidiana (12 cm): vidrio negro con destellos violeta,
    caras de fractura concoide planas y un resto de córtex pardo."""
    p = K.Parts()
    o = C.make_blob('Nodule', (0, 0, 0), 1.0, v['seed'], subdivisions=4, noise_scale=1.2, noise_strength=0.1,
                    relax_iterations=1)
    r = random.Random(v['seed'])
    planes = []
    for _ in range(7):
        n = Vector((r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-0.3, 1))).normalized()
        planes.append((n, r.uniform(0.55, 0.75)))
    facet = {}
    for vv in o.data.vertices:
        co = vv.co.copy()
        for k, (n, d) in enumerate(planes):
            h = co.dot(n)
            if h > d:
                # cuenco muy poco profundo: fractura concoide
                co -= n * (h - d) * 1.0
                co -= n * 0.03 * (1 - min(1.0, (co - n * d).length / 0.6))
                facet[vv.index] = k
        vv.co = Vector((co.x * 0.062, co.y * 0.05, co.z * 0.045))
    o.data.update()
    M.assign(o, ['M_Stone'])

    def col(vv):
        if vv.index not in facet:
            return NPAL['obsidian_cortex'] + (0.0,)
        # cada cara de fractura con un reflejo violeta distinto
        return I.lerp3(NPAL['obsidian'], NPAL['obsidian_glint'], 0.15 * (facet[vv.index] % 3)) + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
@_register('cascara_coco')
def _b_cascara_coco(v, rnd, name):
    """Media cáscara de coco partida a golpes (borde roto), con restos de
    fibra pegados por fuera; ladeada en el suelo."""
    p = K.Parts()
    o = IC._coconut_half('Shell', rnd, 0.07, 0.066, jag=0.014, seed=3)
    p.add(o, 'none')
    for k in range(7):
        a = rnd.uniform(0, 2 * math.pi)
        z = rnd.uniform(0.012, 0.04)
        rr = 0.07 * math.sin(min(1.0, z / 0.066) * math.pi / 2) + 0.001
        base = Vector((math.cos(a) * rr, math.sin(a) * rr, z))
        out = Vector((math.cos(a), math.sin(a), rnd.uniform(-0.3, 0.3))).normalized()
        pts = [base - out * 0.002, base + out * 0.008 + Vector((0, 0, 0.004)),
               base + out * 0.014 + Vector((rnd.uniform(-0.006, 0.006), rnd.uniform(-0.006, 0.006), 0.0))]
        f = I.sweep(f'Fib{k}', pts, [0.003, 0.0025, 0.0012], segs=4)
        M.assign(f, ['M_Fabric'])
        I.tint(f, PAL['coir'][k % 3], rnd, 0.02)
        p.add(f, 'none')
    p.transform(Matrix.Rotation(0.35, 4, 'Y'))
    return p.finish(name)
