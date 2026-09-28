"""
items_materiales.py — materiales básicos que el jugador recoge en las
primeras horas (items.json): ramas, palos, troncos, bambú, fibra, cordel,
liana, hoja de palma, canto rodado y nódulo de pedernal.

Objetos sueltos: pivote en la base (z = 0) y centrados en XY, tumbados como
quedan en el suelo (a lo largo de +X los alargados). Escala real.
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

VARIANTS = [
    dict(name='Item_RamaSeca', item_id='rama_seca', seed=4201, builder='rama_seca', tri_budget=(200, 3000)),
    dict(name='Item_RamaVerde', item_id='rama_verde', seed=4202, builder='rama_verde', tri_budget=(200, 3500)),
    dict(name='Item_PaloRecto', item_id='palo_recto', seed=4203, builder='palo_recto', tri_budget=(100, 2000)),
    dict(name='Item_TroncoPequeno', item_id='tronco_pequeno', seed=4204, builder='tronco_pequeno',
         tri_budget=(300, 4000)),
    dict(name='Item_BambuFino', item_id='bambu_fino', seed=4205, builder='bambu_fino', tri_budget=(150, 2500)),
    dict(name='Item_BambuGrueso', item_id='bambu_grueso', seed=4206, builder='bambu_grueso', tri_budget=(300, 4000)),
    dict(name='Item_FibraCoco', item_id='fibra_coco', seed=4207, builder='fibra_coco', tri_budget=(300, 4000)),
    dict(name='Item_Cordel', item_id='cordel', seed=4208, builder='cordel', tri_budget=(300, 4000)),
    dict(name='Item_Liana', item_id='liana', seed=4209, builder='liana', tri_budget=(300, 4000)),
    dict(name='Item_HojaPalma', item_id='hoja_palma', seed=4210, builder='hoja_palma', tri_budget=(200, 4000)),
    dict(name='Item_CantoRodado', item_id='canto_rodado', seed=4211, builder='canto_rodado', tri_budget=(200, 2500)),
    dict(name='Item_Pedernal', item_id='pedernal', seed=4212, builder='pedernal', tri_budget=(200, 3000)),
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


def _end_cap(name, center, direction, radius, rnd, rings=3, segs=10):
    """Corte transversal de madera: disco de anillos concéntricos (albura
    clara, duramen oscuro, anillos alternos), orientado según `direction`."""
    pts_rings = []
    for k in range(rings, 0, -1):
        r = radius * k / rings
        pts_rings.append([Vector((math.cos(2 * math.pi * i / segs) * r, math.sin(2 * math.pi * i / segs) * r, 0.0))
                          for i in range(segs)])
    o = C.ring_loft(name, pts_rings, cap_end=True)
    C.orient_and_place_zaxis(o, Vector(center), Vector(direction).normalized())
    M.assign(o, ['M_Wood'])
    c0 = Vector(center)

    def col(co):
        d = (co - c0).length / radius
        ring = 0.5 + 0.5 * math.cos(d * rings * math.pi * 2)
        c = I.lerp3(PAL['wood_core'], PAL['wood_cut'], min(1.0, d * 1.6))
        return I.lerp3(c, I.lerp3(c, PAL['wood_core'], 0.5), ring * 0.4)
    I.color_fn(o, col, rnd, 0.015)
    return o


def _bark_col(dark, light, freq=55.0):
    def f(co):
        streak = 0.5 + 0.5 * math.sin(co.z * freq + math.sin(co.x * 400) * 1.5)
        return I.lerp3(dark, light, streak)
    return f


def _branch(p, rnd, length, r0, r1, bark, twigs=2, broken_color=None, leaves=0, name='Br'):
    """Rama torcida a lo largo de +Z con ramitas laterales; extremos rotos
    de madera clara."""
    o, pts = I.stick(name, length, r0, r1, rnd, n=10, bend=0.05, wobble=0.012, segs=8, knots=2)
    M.assign(o, ['M_Wood'])
    dark, light = bark
    top = pts[-1][2]

    def col(co):
        if co.z > top - 0.004 or co.z < 0.004:
            return broken_color or PAL['wood_cut']
        return _bark_col(dark, light)(co)
    I.color_fn(o, col, rnd, 0.02)
    p.add(o, 'none')
    for t in range(twigs):
        i = rnd.randint(3, 8)
        base = Vector(pts[i])
        # tras tumbar la rama (+Z -> +X) las ramitas deben quedar casi
        # horizontales: salen hacia ±Y con poca componente en X (vertical)
        d = Vector((rnd.uniform(-0.3, 0.1), 1.0 if t % 2 == 0 else -1.0, 1.2)).normalized()
        L = length * rnd.uniform(0.18, 0.3)
        tw_pts = [base + d * L * k / 4 + Vector((0, 0, 0.01 * k * k)) for k in range(5)]
        rr = r0 + (r1 - r0) * i / 10
        tw = I.sweep(f'{name}Twig{t}', tw_pts, [rr * 0.6, rr * 0.5, rr * 0.42, rr * 0.34, rr * 0.25], segs=6)
        M.assign(tw, ['M_Wood'])
        I.color_fn(tw, _bark_col(dark, light), rnd, 0.02)
        p.add(tw, 'none')
        for lf in range(leaves):
            tip = tw_pts[-1 - lf % 2]
            leaf = I.leaf(f'{name}Leaf{t}_{lf}', 0.13, 0.055, 0.15, rnd)
            C.orient_and_place(leaf, Vector(tip), Vector((rnd.uniform(-0.15, 0.05), d.y, 0.6)), (-1, 0, 0))
            p.add(leaf, 'none')
    return o, pts


@_register('rama_seca')
def _b_rama_seca(v, rnd, name):
    p = K.Parts()
    _branch(p, rnd, 0.72, 0.017, 0.009, (PAL['bark'][0], (0.30, 0.20, 0.11)), twigs=3,
            broken_color=(0.62, 0.46, 0.26))
    I.lay_along_x(p)
    return p.finish(name)


@_register('rama_verde')
def _b_rama_verde(v, rnd, name):
    p = K.Parts()
    _branch(p, rnd, 0.8, 0.018, 0.009, PAL['bark_green'], twigs=3, broken_color=(0.60, 0.62, 0.30), leaves=2)
    I.lay_along_x(p)
    return p.finish(name)


@_register('palo_recto')
def _b_palo_recto(v, rnd, name):
    """Palo recto descortezado a medias (mango/asta en bruto)."""
    p = K.Parts()
    o, pts = I.stick('Pole', 1.1, 0.02, 0.017, rnd, n=12, bend=0.008, wobble=0.002, segs=9)
    M.assign(o, ['M_Wood'])

    def col(co):
        # parches de corteza que quedan sin pelar
        patch = math.sin(co.z * 9.0 + 1.3) * math.sin(math.atan2(co.y, co.x) * 2 + co.z * 4)
        if co.z < 0.003 or co.z > 1.097:
            return PAL['wood_cut']
        return PAL['bark'][1] if patch > 0.45 else I.lerp3(PAL['wood_light'][0], PAL['wood_cut'],
                                                          0.5 + 0.5 * math.sin(co.z * 70))
    I.color_fn(o, col, rnd, 0.02)
    p.add(o, 'none')
    I.lay_along_x(p)
    return p.finish(name)


@_register('tronco_pequeno')
def _b_tronco_pequeno(v, rnd, name):
    """Tronco de 1.1 m cortado a hacha: corteza agrietada, un muñón de rama
    y los dos cortes con anillos."""
    p = K.Parts()
    L, R = 1.1, 0.1
    n = 10
    pts = I.crooked(L, rnd, n=n, bend=0.02, wobble=0.003)
    radii = [R * (1.04 - 0.1 * i / n) * rnd.uniform(0.97, 1.03) for i in range(n + 1)]
    segs = 14
    o = I.sweep('Log', pts, radii, segs=segs, cap=False)
    # grietas de corteza: surcos longitudinales
    for vv in o.data.vertices:
        ring, k = divmod(vv.index, segs)
        if 0 < ring < n and k % 3 == 0:
            c = Vector(pts[ring])
            d = vv.co - c
            d.z = 0
            vv.co -= d.normalized() * R * 0.07
    o.data.update()
    M.assign(o, ['M_Wood'])

    def bark(vv):
        ring, k = divmod(vv.index, segs)
        c = PAL['bark'][2] if k % 3 == 0 else PAL['bark'][ring % 2]
        return c + (0.0,)
    C.set_vertex_colors(o, bark)
    p.add(o, 'none')
    p.add(_end_cap('CapA', pts[0], Vector(pts[0]) - Vector(pts[1]), radii[0] * 0.99, rnd), 'none')
    p.add(_end_cap('CapB', pts[-1], Vector(pts[-1]) - Vector(pts[-2]), radii[-1] * 0.99, rnd), 'none')
    # muñón de rama cortada
    base = Vector(pts[6])
    d = Vector((0.8, 0.3, 0.5)).normalized()
    stub = I.sweep('Stub', [base, base + d * 0.13, base + d * 0.16], [0.035, 0.03, 0.028], segs=8, cap=False)
    M.assign(stub, ['M_Wood'])
    I.color_fn(stub, _bark_col(PAL['bark'][0], PAL['bark'][1]), rnd, 0.02)
    p.add(stub, 'none')
    p.add(_end_cap('CapStub', base + d * 0.16, d, 0.028, rnd, rings=2, segs=8), 'none')
    I.lay_along_x(p)
    return p.finish(name)


def _bamboo(p, rnd, name, L, R, node_every, hollow=False, leaves=0):
    n = int(L / node_every * 6)
    pts = [(0.0, 0.0, L * i / n) for i in range(n + 1)]
    radii = []
    for i in range(n + 1):
        z = L * i / n
        ph = (z % node_every) / node_every
        node = 1.0 + 0.1 * max(0.0, 1 - min(ph, 1 - ph) / 0.06) if 0 < i < n else 1.0
        radii.append(R * (1 - 0.08 * i / n) * node)
    base_c = rnd.choice(PAL['bamboo'])

    def col(co):
        ph = (co.z % node_every) / node_every
        k = max(0.0, 1 - min(ph, 1 - ph) / 0.08)
        c = I.lerp3(base_c, I.lerp3(base_c, (0.62, 0.56, 0.20), 0.5), 0.5 + 0.5 * math.sin(co.z * 7))
        return I.lerp3(c, PAL['bamboo_node'], k)
    if hollow:
        o = C.make_tube(name, L, 14, radii, [R * 0.72] * (n + 1), cap_bottom=True, cap_top=True)
        M.assign(o, ['M_Wood'])
        inner = R * 0.8

        def col_h(co):
            r = math.hypot(co.x, co.y)
            if r < inner:
                return PAL['bamboo_cut']
            if co.z < 0.002 or co.z > L - 0.002:
                return I.lerp3(PAL['bamboo_cut'], base_c, 0.3)
            return col(co)
        I.color_fn(o, col_h, rnd, 0.015)
    else:
        o = I.sweep(name, pts, radii, segs=8)
        M.assign(o, ['M_Wood'])
        I.color_fn(o, lambda co: PAL['bamboo_cut'] if (co.z < 0.001 or co.z > L - 0.001) else col(co), rnd, 0.015)
    p.add(o, 'none')
    for lf in range(leaves):
        z = node_every * (lf + 1) + 0.01
        leaf = I.leaf(f'{name}Leaf{lf}', 0.17, 0.03, 0.1, rnd)
        C.orient_and_place(leaf, Vector((0, R * 0.9, z)), Vector((-0.05, 1.0, 0.8)), (-1, 0, 0))
        p.add(leaf, 'none')
    return o


@_register('bambu_fino')
def _b_bambu_fino(v, rnd, name):
    p = K.Parts()
    _bamboo(p, rnd, 'Cane', 1.3, 0.016, 0.26, leaves=2)
    I.lay_along_x(p)
    return p.finish(name)


@_register('bambu_grueso')
def _b_bambu_grueso(v, rnd, name):
    p = K.Parts()
    _bamboo(p, rnd, 'Cane', 1.0, 0.048, 0.3, hollow=True)
    I.lay_along_x(p)
    return p.finish(name)


@_register('fibra_coco')
def _b_fibra_coco(v, rnd, name):
    """Manojo de fibra de coco atado por el centro, con las puntas abiertas."""
    p = K.Parts()
    k = 22
    for i in range(k):
        a = rnd.uniform(0, 2 * math.pi)
        r = 0.012 * math.sqrt(rnd.random())
        spread = rnd.uniform(0.02, 0.045)
        L = rnd.uniform(0.11, 0.14)
        pts = []
        for j in range(7):
            t = j / 6 * 2 - 1  # -1..1
            f = r + spread * abs(t) ** 1.6
            sag = -0.012 * (1 - t * t)
            pts.append((t * L, math.cos(a) * f + rnd.uniform(-0.002, 0.002),
                        math.sin(a) * f * 0.6 + sag + rnd.uniform(-0.002, 0.002)))
        s = I.sweep(f'Fib{i}', pts, [0.0022, 0.003, 0.0032, 0.0032, 0.0032, 0.003, 0.0022], segs=4)
        M.assign(s, ['M_Fabric'])
        I.tint(s, PAL['coir'][i % 3], rnd, 0.03)
        p.add(s, 'none')
    tie = I.helix_wrap('Tie', -0.012, 0.012, 0.017, 0.003, rnd, segs=5)
    tie.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
    tie.data.transform(Matrix.Translation((0, 0, -0.012)))
    M.assign(tie, ['M_Fabric'])
    I.cord_stripes(tie, PAL['cord'][0], PAL['cord'][1], 5)
    p.add(tie, 'none')
    return p.finish(name)


def _coil(name, turns, r0, dr, cord_r, rnd, pts_per_turn=22, tail=0.12, flat=0.35, wobble=0.004):
    """Rollo de cuerda: espiral apilada con un cabo suelto."""
    pts = []
    n = int(turns * pts_per_turn)
    for i in range(n + 1):
        t = i / pts_per_turn
        a = t * 2 * math.pi
        r = r0 + dr * math.sin(t * 1.7) + rnd.uniform(-wobble, wobble)
        pts.append((math.cos(a) * r, math.sin(a) * r * (1 - flat * 0.3), cord_r + cord_r * 1.9 * t * flat
                    + 0.004 * math.sin(a * 2)))
    last = Vector(pts[-1])
    tang = (last - Vector(pts[-2])).normalized()
    for k in range(1, 6):
        pts.append(tuple(last + tang * tail * k / 5 + Vector((0, 0, -min(last.z - cord_r, 0.006 * k)))))
    return I.sweep(name, pts, cord_r, segs=6)


@_register('cordel')
def _b_cordel(v, rnd, name):
    p = K.Parts()
    o = _coil('Coil', 4.2, 0.05, 0.004, 0.0028, rnd)
    M.assign(o, ['M_Fabric'])
    I.cord_stripes(o, PAL['cord'][0], I.lerp3(PAL['cord'][0], PAL['coir'][2], 0.5), 6, period=3)
    p.add(o, 'none')
    return p.finish(name)


@_register('liana')
def _b_liana(v, rnd, name):
    """Liana enrollada en lazada floja, con nudos y un par de hojas."""
    p = K.Parts()
    o = _coil('Vine', 2.6, 0.12, 0.02, 0.008, rnd, pts_per_turn=20, tail=0.25, wobble=0.012)
    for vv in o.data.vertices:
        ring = vv.index // 6
        if ring % 9 == 4:
            vv.co.z += 0.0015
    M.assign(o, ['M_Wood'])
    I.color_fn(o, lambda co: I.lerp3(PAL['vine'][0], PAL['vine'][1], 0.5 + 0.5 * math.sin(co.x * 60 + co.y * 45)),
               rnd, 0.02)
    p.add(o, 'none')
    for i, (a, s) in enumerate(((0.6, 0.08), (2.9, 0.07), (4.6, 0.09))):
        pos = Vector((math.cos(a) * 0.125, math.sin(a) * 0.125 * 0.9, 0.02))
        leaf = I.leaf(f'Leaf{i}', s, s * 0.5, 0.1, rnd)
        C.orient_and_place(leaf, pos, Vector((math.cos(a), math.sin(a), 0.08)), (0, 0, 1))
        p.add(leaf, 'none')
    return p.finish(name)


@_register('hoja_palma')
def _b_hoja_palma(v, rnd, name):
    """Fronda de cocotero cortada (1.4 m): raquis arqueado y ~24 pares de
    folíolos largos que caen a los lados, pecíolo leñoso cortado."""
    p = K.Parts()
    L = 1.1
    n = 10
    rach = [(0.0, L * i / n, 0.05 * math.sin(math.pi * i / n)) for i in range(n + 1)]
    r = I.sweep('Rachis', rach, [0.014 * (1 - 0.8 * i / n) + 0.002 for i in range(n + 1)], segs=6)
    M.assign(r, ['M_Wood'])
    I.color_fn(r, lambda co: I.lerp3((0.30, 0.30, 0.06), (0.16, 0.26, 0.04), co.y / L), rnd, 0.02)
    p.add(r, 'none')
    pet = I.sweep('Petiole', [(0, -0.3, 0.012), (0, -0.15, 0.008), (0, 0.0, 0.0)], [0.02, 0.017, 0.014], segs=7)
    M.assign(pet, ['M_Wood'])
    I.color_fn(pet, lambda co: PAL['wood_cut'] if co.y < -0.297 else (0.32, 0.28, 0.07), rnd, 0.02)
    p.add(pet, 'none')
    pairs = 22
    greens = [((0.05, 0.20, 0.03), (0.22, 0.40, 0.06)), ((0.07, 0.24, 0.03), (0.28, 0.44, 0.07))]
    for i in range(pairs):
        t = (i + 0.5) / pairs
        y = L * (0.04 + 0.95 * t)
        z = 0.05 * math.sin(math.pi * y / L)
        ln = 0.52 * math.sin(math.pi * (0.18 + 0.72 * t)) + 0.08
        for side in (-1, 1):
            g0, g1 = greens[(i + (side > 0)) % 2]
            lf = I.leaf(f'Lf{i}_{side}', ln, 0.034, 0.12, rnd, segments=5, fold=0.4, rgb0=g0, rgb1=g1)
            d = Vector((side * 1.0, 0.55 + 0.25 * t, -0.18))
            C.orient_and_place(lf, Vector((side * 0.006, y, z)), d, (0, 0, 1))
            p.add(lf, 'none')
    return p.finish(name)


@_register('canto_rodado')
def _b_canto_rodado(v, rnd, name):
    """Canto rodado de playa: ovoide pulido con una veta de cuarzo."""
    p = K.Parts()
    o = C.make_blob('Cobble', (0, 0, 0), 1.0, v['seed'], subdivisions=3, noise_scale=0.9, noise_strength=0.1,
                    scale=(0.075, 0.055, 0.042), relax_iterations=2)
    M.assign(o, ['M_Stone'])
    base = PAL['cobble'][2]

    def col(co):
        if abs(co.x * 0.7 + co.y - 0.006) < 0.0065:
            return PAL['quartz']
        speck = math.sin(co.x * 700) * math.sin(co.y * 610) * math.sin(co.z * 830)
        return I.lerp3(base, PAL['cobble_light'], 0.5 + 0.5 * math.sin(co.z * 60)) if speck < 0.6 else \
            PAL['cobble'][1]
    I.color_fn(o, col, rnd, 0.02)
    p.add(o, 'none')
    return p.finish(name)


@_register('pedernal')
def _b_pedernal(v, rnd, name):
    """Nódulo de pedernal: córtex cremoso con una cara ya lascada que deja
    ver el sílex color caramelo."""
    p = K.Parts()
    o = C.make_blob('Nodule', (0, 0, 0), 1.0, v['seed'], subdivisions=3, noise_scale=1.6, noise_strength=0.22,
                    scale=(0.07, 0.05, 0.045), relax_iterations=1)
    # plano de lascado: aplana la cara -Y (la que mira a la cámara en la lámina)
    cut = -0.022
    for vv in o.data.vertices:
        if vv.co.y < cut:
            vv.co.y = cut + (vv.co.y - cut) * 0.12 + 0.002 * math.sin(vv.co.x * 180) * math.sin(vv.co.z * 170)
    o.data.update()
    M.assign(o, ['M_Stone'])

    def col(co):
        if co.y < cut + 0.004:
            return I.lerp3(PAL['flint'], PAL['flint_edge'], 0.5 + 0.5 * math.sin(co.x * 150 + co.z * 90))
        return I.lerp3(PAL['flint_cortex'], (0.40, 0.30, 0.17), 0.5 + 0.5 * math.sin(co.z * 120 + co.x * 40))
    I.color_fn(o, col, rnd, 0.03)
    p.add(o, 'none')
    return p.finish(name)
