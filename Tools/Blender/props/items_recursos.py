"""
items_recursos.py — maderas, piedras y materiales de fuego que el jugador
recoge en la primera semana (items.json, biblia §3.1-3.2): madera blanda de
balsa, madera dura de guayabo, madera flotante, vara flexible de bambú,
basalto, arenisca, arcilla roja, carbón vegetal y resina.

Objetos sueltos: pivote en la base (z = 0) y centrados en XY, tumbados como
quedan en el suelo (a lo largo de +X los alargados). Escala real. Las
piedras oscuras (basalto, carbón) llevan siempre un matiz cálido u óxido
para no leerse grises.
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
    dict(name='Item_MaderaBlanda', item_id='madera_blanda', seed=4501, builder='madera_blanda',
         tri_budget=(200, 3000)),
    dict(name='Item_MaderaDura', item_id='madera_dura', seed=4502, builder='madera_dura', tri_budget=(200, 3500)),
    dict(name='Item_MaderaFlotante', item_id='madera_flotante', seed=4503, builder='madera_flotante',
         tri_budget=(200, 3500)),
    dict(name='Item_VaraFlexible', item_id='vara_flexible', seed=4504, builder='vara_flexible',
         tri_budget=(150, 3000)),
    dict(name='Item_Basalto', item_id='basalto', seed=4505, builder='basalto', tri_budget=(150, 3000)),
    dict(name='Item_Arenisca', item_id='arenisca', seed=4506, builder='arenisca', tri_budget=(150, 3000)),
    dict(name='Item_ArcillaRoja', item_id='arcilla_roja', seed=4507, builder='arcilla_roja', tri_budget=(200, 3000)),
    dict(name='Item_CarbonVegetal', item_id='carbon_vegetal', seed=4508, builder='carbon_vegetal',
         tri_budget=(200, 3500)),
    dict(name='Item_Resina', item_id='resina', seed=4509, builder='resina', tri_budget=(200, 3000)),
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


def _cut_face(name, center, direction, radius, rnd, core, sap, rings=4, segs=12, scale=(1.0, 1.0)):
    """Corte transversal con anillos de crecimiento: duramen `core` en el
    centro, albura `sap` fuera. Orientado según `direction`."""
    pts_rings = []
    for k in range(rings, 0, -1):
        r = radius * k / rings
        pts_rings.append([Vector((math.cos(2 * math.pi * i / segs) * r * scale[0],
                                  math.sin(2 * math.pi * i / segs) * r * scale[1], 0.0)) for i in range(segs)])
    o = C.ring_loft(name, pts_rings, cap_end=True)
    C.orient_and_place_zaxis(o, Vector(center), Vector(direction).normalized())
    M.assign(o, ['M_Wood'])
    c0 = Vector(center)

    def col(co):
        d = (co - c0).length / radius
        ring = 0.5 + 0.5 * math.cos(d * rings * math.pi * 2)
        c = I.lerp3(core, sap, min(1.0, max(0.0, (d - 0.45) / 0.4)))
        return I.lerp3(c, I.lerp3(c, core, 0.6), ring * 0.35)
    I.color_fn(o, col, rnd, 0.015)
    return o


# ---------------------------------------------------------------------------
# MADERAS
# ---------------------------------------------------------------------------
@_register('madera_blanda')
def _b_madera_blanda(v, rnd, name):
    """Cuarto de tronco de balsa partido a lo largo: dos caras de raja
    color crema rosado con fibras, corteza fina parda con líquenes."""
    p = K.Parts()
    L, R = 0.50, 0.085
    n_arc = 8
    # sección en cuña (cuarto de círculo) con puntos extra en las caras
    # planas junto a la corteza para que la raja no se tiña de corteza
    sec = [(0.0, 0.0), (R * 0.35, 0.0), (R * 0.7, 0.0), (R * 0.93, 0.0)]
    sec += [(math.cos(a) * R, math.sin(a) * R) for a in [math.pi / 2 * k / n_arc for k in range(n_arc + 1)]]
    sec += [(0.0, R * 0.93), (0.0, R * 0.7), (0.0, R * 0.35)]
    npts = len(sec)
    n = 12
    xs = [-L / 2 + L * i / n for i in range(n + 1)]

    def section(x):
        t = (x + L / 2) / L
        bulge = 1.0 + 0.04 * math.sin(t * math.pi * 3 + 1.0)
        return [(y * bulge + 0.0015 * math.sin(x * 60 + k), z * bulge) for k, (y, z) in enumerate(sec)]
    o = I.loft_x('Billet', xs, section, cap=True)
    # raja irregular en el extremo +X (astillado) y extremo -X cortado recto
    for vv in o.data.vertices:
        ring, k = divmod(vv.index, npts)
        if ring == n:
            vv.co.x -= 0.02 * abs(math.sin(k * 1.3)) + 0.008 * rnd.random()
    o.data.update()
    # tumbado sobre una cara plana (z = 0): la otra raja queda de pie
    # mirando a -Y y la corteza arriba
    M.assign(o, ['M_Wood'])
    cream = (0.66, 0.46, 0.28)
    cream_d = (0.50, 0.32, 0.17)
    bark = (0.26, 0.17, 0.09)
    lichen = (0.34, 0.40, 0.14)

    def col(vv):
        ring, k = divmod(vv.index, npts)
        co = vv.co
        if 4 <= k <= 4 + n_arc and ring not in (0, n):
            n_ = I.noise3(co, 40.0, 1.1)
            c = I.lerp3(bark, (0.44, 0.34, 0.22), 0.5 + 0.5 * math.sin(co.x * 90))
            return (lichen if n_ > 0.35 else c) + (0.0,)
        fib = 0.5 + 0.5 * math.sin((co.y + co.z) * 500 + math.sin(co.x * 25) * 2)
        return I.lerp3(cream, cream_d, fib * 0.55) + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    return p.finish(name)


@_register('madera_dura')
def _b_madera_dura(v, rnd, name):
    """Tocho de guayabo: rama recta y dura con la corteza lisa jaspeada
    (canela, verde oliva y placas claras que se desprenden), muñón de rama
    y cortes de hacha con duramen rojizo."""
    p = K.Parts()
    L = 0.56
    o, pts = I.stick('Billet', L, 0.042, 0.036, rnd, n=12, bend=0.02, wobble=0.004, segs=12, knots=1)
    M.assign(o, ['M_Wood'])
    cinn = (0.40, 0.17, 0.07)
    olive = (0.28, 0.25, 0.10)
    pale = (0.58, 0.40, 0.22)

    def col(co):
        n1 = I.noise3(co, 14.0, 4.2)
        n2 = I.noise3(co, 32.0, 9.3)
        c = I.lerp3(cinn, olive, max(0.0, min(1.0, 0.5 + n1 * 1.6)))
        if n2 > 0.3:
            c = I.lerp3(c, pale, min(1.0, (n2 - 0.3) / 0.12))
        return c
    I.color_fn(o, col, rnd, 0.015)
    p.add(o, 'none')
    heart, sap = (0.30, 0.08, 0.04), (0.62, 0.38, 0.18)
    p.add(_cut_face('CapA', pts[0], Vector(pts[0]) - Vector(pts[1]), 0.0415, rnd, heart, sap), 'none')
    p.add(_cut_face('CapB', pts[-1], Vector(pts[-1]) - Vector(pts[-2]), 0.0355, rnd, heart, sap), 'none')
    # muñón de rama cortado a ras
    i = 7
    base = Vector(pts[i])
    d = Vector((0.9, 0.3, 0.45)).normalized()
    stub = I.sweep('Stub', [base, base + d * 0.035, base + d * 0.05], [0.016, 0.013, 0.012], segs=8)
    M.assign(stub, ['M_Wood'])
    I.color_fn(stub, col, rnd, 0.015)
    p.add(stub, 'none')
    p.add(_cut_face('StubCap', base + d * 0.05, d, 0.0118, rnd, heart, sap, rings=2, segs=8), 'none')
    I.lay_along_x(p)
    return p.finish(name)


@_register('madera_flotante')
def _b_madera_flotante(v, rnd, name):
    """Madera flotante: rama en horquilla pulida por el mar, sin corteza,
    color miel desvaído con grietas longitudinales oscuras y puntas romas."""
    p = K.Parts()
    L = 0.72
    n = 14
    pts = I.crooked(L, rnd, n=n, bend=0.07, wobble=0.005)
    radii = []
    for k in range(n + 1):
        t = k / n
        r = 0.032 - 0.014 * t
        # puntas romas redondeadas (los dos últimos anillos se cierran)
        if k == 0:
            r *= 0.45
        elif k == 1:
            r *= 0.88
        elif k == n:
            r *= 0.4
        elif k == n - 1:
            r *= 0.85
        radii.append(r * (1.0 + 0.05 * math.sin(k * 2.1)))
    main = I.sweep('Main', pts, radii, segs=10)
    M.assign(main, ['M_Wood'])
    honey = (0.48, 0.33, 0.19)
    honey_l = (0.62, 0.46, 0.28)
    crack = (0.18, 0.10, 0.05)

    def col(co):
        a = math.atan2(co.y, co.x)
        g = math.sin(a * 7 + co.z * 18)
        if g > 0.93:
            return crack
        return I.lerp3(honey, honey_l, 0.5 + 0.5 * math.sin(a * 3 + co.z * 11))
    I.color_fn(main, col, rnd, 0.02)
    p.add(main, 'none')
    # horquilla
    k0 = 5
    base = Vector(pts[k0])
    d = Vector((0.0, 1.0, 1.1)).normalized()
    fork_pts = [base + d * 0.26 * j / 5 + Vector((0.01 * math.sin(j), 0, 0)) for j in range(6)]
    fr = [radii[k0] * 0.8, 0.02, 0.018, 0.016, 0.014, 0.006]
    fork = I.sweep('Fork', fork_pts, fr, segs=8)
    M.assign(fork, ['M_Wood'])
    I.color_fn(fork, col, rnd, 0.02)
    p.add(fork, 'none')
    I.lay_along_x(p)
    return p.finish(name)


@_register('vara_flexible')
def _b_vara_flexible(v, rnd, name):
    """Vara de bambú rajada para arco: media caña de 1,4 m curvada en el
    plano del suelo, cara exterior verde amarillenta con nudos y cara
    interior crema; un extremo atado con cordel para que no se abra."""
    p = K.Parts()
    L = 1.40
    n = 40
    W, T = 0.04, 0.014
    # sección de media caña: arco exterior + cara interior casi plana
    arc = [(math.cos(a) * W / 2, math.sin(a) * T) for a in [math.pi * k / 8 for k in range(9)]]
    flat = [(-W / 2 + W * k / 4, -0.0005) for k in range(1, 4)]
    sec = arc + flat[::-1]
    sec = [(y, z) for (y, z) in sec]
    npts = len(sec)
    nodes = [0.12 + 0.31 * k for k in range(5)]
    xs = [-L / 2 + L * i / n for i in range(n + 1)]

    def section(x):
        t = (x + L / 2) / L
        taper = 1.0 - 0.3 * abs(t - 0.5) * 2
        node = 1.0 + 0.12 * max([max(0.0, 1 - abs((x + L / 2) - nd) / 0.012) for nd in nodes])
        return [(y * taper * node, z * taper * node) for (y, z) in sec]
    o = I.loft_x('Stave', xs, section, cap=True)
    # combado suave en planta (flexible) sin salirse del suelo
    for vv in o.data.vertices:
        t = (vv.co.x + L / 2) / L
        vv.co.y += 0.09 * math.sin(math.pi * t)
    o.data.update()
    M.assign(o, ['M_Wood'])
    green = PAL['bamboo'][1]
    green_l = PAL['bamboo'][2]

    def col(vv):
        ring, k = divmod(vv.index, npts)
        co = vv.co
        x = co.x + L / 2
        near = min(abs(x - nd) for nd in nodes)
        if k >= 9 or co.z < 0.0008:
            return I.lerp3(PAL['bamboo_cut'], (0.55, 0.44, 0.20), 0.5 + 0.5 * math.sin(co.x * 300)) + (0.0,)
        c = I.lerp3(green, green_l, 0.5 + 0.5 * math.sin(co.x * 9))
        if near < 0.01:
            c = PAL['bamboo_node']
        return c + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    # atadura en el extremo -X
    x0 = -L / 2 + 0.05
    wrap = I.helix_wrap('Wrap', 0.0, 0.035, 0.0135, 0.0022, rnd, segs=5)
    # hélice a lo largo de +X, aplastada al canto de la media caña
    wrap.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
    for vv in wrap.data.vertices:
        vv.co.z *= 0.34
    t0 = (x0 + L / 2) / L
    wrap.data.transform(Matrix.Translation((x0, 0.09 * math.sin(math.pi * t0), T * 0.3)))
    wrap.data.update()
    M.assign(wrap, ['M_Fabric'])
    I.cord_stripes(wrap, PAL['cord'][0], I.lerp3(PAL['cord'][0], (0.2, 0.12, 0.05), 0.45), 5)
    p.add(wrap, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# PIEDRAS Y TIERRAS
# ---------------------------------------------------------------------------
@_register('basalto')
def _b_basalto(v, rnd, name):
    """Trozo de columna de basalto: prisma hexagonal con la cara de arriba
    rota en bisel, pátina de óxido en los lados y vacuolas oscuras."""
    p = K.Parts()
    R = 0.065
    per = 3  # puntos por lado del hexágono
    base_hex = []
    for s in range(6):
        a0 = math.pi / 3 * s + rnd.uniform(-0.05, 0.05)
        a1 = math.pi / 3 * (s + 1)
        r0 = R * rnd.uniform(0.92, 1.05)
        p0 = Vector((math.cos(a0) * r0, math.sin(a0) * r0, 0.0))
        p1 = Vector((math.cos(a1) * R, math.sin(a1) * R, 0.0))
        for k in range(per):
            base_hex.append(p0.lerp(p1, k / per))
    npts = len(base_hex)
    levels = 7
    rings = []
    for j in range(levels + 1):
        t = j / levels
        ring = []
        for q in base_hex:
            # arriba: fractura inclinada e irregular
            top = 0.09 + 0.045 * (q.x / R) + 0.008 * math.sin(q.y * 140)
            z = top * t
            shrink = 1.0 - 0.05 * max(0.0, t - 0.85) / 0.15
            ring.append(Vector((q.x * shrink, q.y * shrink, z)))
        rings.append(ring)
    o = C.ring_loft('Column', rings, cap_start=True, cap_end=True)
    M.assign(o, ['M_Stone'])
    dark = (0.075, 0.055, 0.050)
    rust = (0.24, 0.10, 0.04)
    fresh = (0.10, 0.075, 0.085)

    def col(vv):
        ring, k = divmod(vv.index, npts)
        co = vv.co
        if ring == levels:
            return I.lerp3(fresh, (0.16, 0.12, 0.12), 0.5 + 0.5 * I.noise3(co, 60.0, 2.0)) + (0.0,)
        n_ = I.noise3(co, 18.0, 6.6)
        c = I.lerp3(dark, rust, max(0.0, min(1.0, 0.45 + n_ * 1.4)))
        if k % per == 0:
            # aristas del prisma algo más claras (desgaste)
            c = I.lerp3(c, (0.30, 0.18, 0.10), 0.45)
        if I.noise3(co, 90.0, 1.3) > 0.45:
            c = (0.03, 0.02, 0.02)
        return c + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'stone')
    obj = p.finish(name)
    # tumbado un poco sobre un lado, como cae de la pared de columnas
    obj.data.transform(Matrix.Rotation(math.radians(12), 4, 'Y'))
    return K.ground(obj)


@_register('arenisca')
def _b_arenisca(v, rnd, name):
    """Laja de arenisca de cantos gastados con estratos ocres, rosados y
    crema bien marcados (sirve de piedra de afilar)."""
    p = K.Parts()
    L, W, T = 0.19, 0.12, 0.065
    n = 10
    sec = I.rrect(W, T, 0.016, corner_segs=3, side_pts=6)
    xs = [-L / 2 + L * i / n for i in range(n + 1)]

    def section(x):
        t = (x + L / 2) / L
        s = math.sin(math.pi * t) ** 0.35
        return [(y * (0.75 + 0.25 * s), z * (0.8 + 0.2 * s)) for (y, z) in sec]
    o = I.loft_x('Slab', xs, section, cap=True)
    C.displace_mesh_noise(o, v['seed'], strength=0.004, scale=18.0)
    M.assign(o, ['M_Stone'])
    bands = [(0.50, 0.26, 0.10), (0.40, 0.19, 0.08), (0.58, 0.36, 0.15), (0.34, 0.15, 0.06), (0.54, 0.31, 0.13)]

    def col(co):
        z = co.z + 0.004 * math.sin(co.x * 40) + T / 2
        b = int(z / T * 7.0) % len(bands)
        c = bands[b]
        # cara de arriba gastada y lisa por el afilado: más clara
        if co.z > T / 2 - 0.004:
            c = I.lerp3(c, (0.66, 0.46, 0.24), 0.3)
        return c
    I.color_fn(o, col, rnd, 0.015)
    o.data.transform(Matrix.Translation((0, 0, T / 2)))
    p.add(o, 'none')
    return p.finish(name)


@_register('arcilla_roja')
def _b_arcilla_roja(v, rnd, name):
    """Pella de arcilla roja amasada: bola aplastada por su peso con dos
    huellas de pulgar y el borde de abajo más claro (ya secándose)."""
    p = K.Parts()
    o = C.make_blob('Clay', (0, 0, 0), 1.0, v['seed'], subdivisions=4, noise_scale=1.2, noise_strength=0.12,
                    scale=(0.065, 0.055, 0.042), relax_iterations=2)
    thumbs = [Vector((0.015, -0.02, 0.035)), Vector((-0.022, -0.012, 0.03))]
    for vv in o.data.vertices:
        # base aplastada contra el suelo
        if vv.co.z < -0.022:
            vv.co.z = -0.022 + (vv.co.z + 0.022) * 0.15
            vv.co.x *= 1.06
            vv.co.y *= 1.06
        for th in thumbs:
            d = (vv.co - th).length
            if d < 0.018:
                vv.co -= (vv.co - Vector((0, 0, 0))).normalized() * 0.008 * (1 - d / 0.018) ** 1.5
    o.data.update()
    M.assign(o, ['M_Stone'])
    red = (0.44, 0.10, 0.035)
    red_l = (0.60, 0.24, 0.10)
    red_d = (0.28, 0.05, 0.02)

    def col(co):
        c = I.lerp3(red, red_l, max(0.0, min(1.0, (-co.z - 0.012) / 0.01)))
        for th in thumbs:
            d = (co - th).length
            if d < 0.016:
                c = I.lerp3(c, red_d, (1 - d / 0.016) * 0.8)
        return I.lerp3(c, red_l, 0.25 * max(0.0, I.noise3(co, 80.0, 3.0)))
    I.color_fn(o, col, rnd, 0.012)
    p.add(o, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# FUEGO
# ---------------------------------------------------------------------------
@_register('carbon_vegetal')
def _b_carbon_vegetal(v, rnd, name):
    """Puñado de carbón vegetal: tres trozos de rama carbonizada con las
    grietas en damero, los cortes con anillos pardos y algún resto de
    corteza tostada."""
    p = K.Parts()
    black = (0.030, 0.022, 0.020)
    sheen = (0.09, 0.07, 0.075)
    brown = (0.16, 0.08, 0.04)
    pieces = [((0.0, 0.0, 0.0), 0.12, 0.022, 0.0), ((0.035, 0.045, 0.0), 0.085, 0.018, 0.9),
              ((-0.03, 0.045, 0.02), 0.07, 0.016, -0.5)]
    for i, (pos, length, r, yaw) in enumerate(pieces):
        segs = 9
        n = 8
        xs = [-length / 2 + length * k / n for k in range(n + 1)]

        def section(x, r=r, length=length, segs=segs, i=i):
            t = (x + length / 2) / length
            end = 1.0 - 0.18 * max(0.0, abs(t - 0.5) * 2 - 0.7) / 0.3
            out = []
            for k in range(segs):
                a = 2 * math.pi * k / segs
                # grietas transversales y longitudinales (damero)
                crack = 0.12 if (int(t * 6) % 2 == 0 and k % 3 == 0) else 0.0
                rr = r * end * (1.0 - crack) * (1.0 + 0.08 * math.sin(a * 2 + i))
                out.append((math.cos(a) * rr, math.sin(a) * rr))
            return out
        o = I.loft_x(f'Char{i}', xs, section, cap=True)
        M.assign(o, ['M_Stone'])

        def col(vv, length=length, r=r, segs=segs, i=i):
            k = vv.index % segs
            co = vv.co
            t = (co.x + length / 2) / length
            c = black
            if int(t * 6) % 2 == 0 and k % 3 == 0:
                c = (0.01, 0.008, 0.008)
            elif I.noise3(co, 50.0, i * 3.3) > 0.3:
                c = brown
            elif co.z > r * 0.5:
                c = sheen
            return c + (0.0,)
        C.set_vertex_colors(o, col)
        cap_a = _cut_face(f'CapA{i}', (-length / 2 - 0.0005, 0, 0), (-1, 0, 0), r * 0.8, rnd, (0.02, 0.015, 0.012),
                          (0.14, 0.07, 0.035), rings=3, segs=9)
        cap_b = _cut_face(f'CapB{i}', (length / 2 + 0.0005, 0, 0), (1, 0, 0), r * 0.8, rnd, (0.02, 0.015, 0.012),
                          (0.14, 0.07, 0.035), rings=3, segs=9)
        M.assign(cap_a, ['M_Stone'])
        M.assign(cap_b, ['M_Stone'])
        grp = C.join_objects([o, cap_a, cap_b], f'Piece{i}')
        grp.data.transform(Matrix.Rotation(yaw, 4, 'Z'))
        tilt = Matrix.Rotation(0.25 if pos[2] > 0 else 0.0, 4, 'X')
        grp.data.transform(tilt)
        grp.data.transform(Matrix.Translation((pos[0], pos[1], pos[2] + r * 0.9)))
        p.add(grp, 'none')
    return p.finish(name)


@_register('resina')
def _b_resina(v, rnd, name):
    """Resina recogida: tres lágrimas de ámbar pegadas a una lasca de
    corteza abarquillada, con el núcleo dorado y los bordes caramelo."""
    p = K.Parts()
    # corteza abarquillada boca arriba
    L, W = 0.13, 0.06
    n, m = 10, 6

    def bark_sec(x):
        t = (x + L / 2) / L
        w = W * (0.8 + 0.2 * math.sin(math.pi * t))
        out = []
        for k in range(m + 1):
            u = (k / m - 0.5) * 2
            out.append((u * w / 2, 0.012 * u * u + 0.004 * math.sin(x * 60)))
        for k in range(m, -1, -1):
            u = (k / m - 0.5) * 2
            out.append((u * w / 2 * 0.96, 0.012 * u * u - 0.007 + 0.004 * math.sin(x * 60)))
        return out
    xs = [-L / 2 + L * i / n for i in range(n + 1)]
    bark = I.loft_x('Bark', xs, bark_sec, cap=True)
    M.assign(bark, ['M_Wood'])
    npts = 2 * (m + 1)

    def bcol(vv):
        ring, k = divmod(vv.index, npts)
        co = vv.co
        if k <= m:
            # cara interior: fibra clara
            return I.lerp3((0.36, 0.20, 0.09), (0.26, 0.13, 0.05), 0.5 + 0.5 * math.sin(co.y * 300)) + (0.0,)
        return I.lerp3(PAL['bark'][0], (0.28, 0.17, 0.08), 0.5 + 0.5 * math.sin(co.x * 120 + co.y * 40)) + (0.0,)
    C.set_vertex_colors(bark, bcol)
    bark.data.transform(Matrix.Translation((0, 0, 0.004)))
    p.add(bark, 'none')
    amber = (0.78, 0.36, 0.03)
    amber_d = (0.34, 0.09, 0.01)
    drops = [((-0.025, 0.004, 0.006), 0.017, 0.012), ((0.018, -0.006, 0.006), 0.013, 0.010),
             ((0.042, 0.010, 0.008), 0.009, 0.008)]
    for i, (c0, r, h) in enumerate(drops):
        o = C.make_blob(f'Drop{i}', (0, 0, 0), 1.0, v['seed'] + i, subdivisions=3, noise_strength=0.08,
                        scale=(r, r * 0.85, h))
        for vv in o.data.vertices:
            # lágrima: la parte de arriba se estira hacia +X
            if vv.co.z > 0:
                vv.co.x += (vv.co.z / h) ** 2 * r * 0.6
            if vv.co.z < -h * 0.4:
                vv.co.z = -h * 0.4 + (vv.co.z + h * 0.4) * 0.2
        o.data.update()
        o.data.transform(Matrix.Translation(Vector(c0) + Vector((0, 0, h * 0.4))))
        M.assign(o, ['M_Stone'])
        cz = Vector(c0) + Vector((0, 0, h * 0.4))
        I.color_fn(o, lambda co, cz=cz, r=r: I.lerp3((0.90, 0.58, 0.10), amber,
                                                      min(1.0, (co - cz).length / r * 0.9))
                   if (co - cz).length < r * 0.5 else I.lerp3(amber, amber_d, min(1.0, (co - cz).length / r - 0.4)),
                   rnd, 0.01)
        p.add(o, 'none')
    return p.finish(name)
