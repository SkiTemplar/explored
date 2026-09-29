"""
items_contenedores.py — contenedores y equipo de carga de las primeras
horas (items.json): cesta de hoja, recipiente de coco, vasija de barro,
mochilas (del Albatros, de fibra y de cuero con armazón de bambú),
cinturón de cuero, angarillas y los dos recipientes rescatados del avión
(cantimplora y bolsa estanca).

Objetos sueltos: pivote en la base (z = 0), centrados en XY, en su postura
de reposo (las mochilas de pie, la espalda hacia -Y; las angarillas con las
varas a lo largo de +X). Escala real en metros.
"""

import math
import os
import sys
from itertools import pairwise

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _items as I  # noqa: E402
import _materials as M  # noqa: E402
import common as C  # noqa: E402
import kit_construccion as K  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

GROUP = I.GROUP
PAL = I.PAL

# colores propios del lote (lineales)
CPAL = {
    'leaf_weave': [(0.54, 0.40, 0.13), (0.24, 0.16, 0.05)],
    'coir_weave': [(0.40, 0.22, 0.07), (0.17, 0.08, 0.025)],
    'coconut': (0.14, 0.065, 0.025),
    'coconut_fiber': (0.30, 0.15, 0.05),
    'copra': (0.78, 0.72, 0.56),
    'clay': (0.48, 0.16, 0.055),
    'clay_fire': (0.16, 0.06, 0.025),
    'clay_slip': (0.66, 0.42, 0.20),
    'leather': (0.30, 0.12, 0.04),
    'leather_dark': (0.16, 0.06, 0.02),
    'canvas': (0.44, 0.33, 0.12),
    'canvas_dark': (0.30, 0.21, 0.07),
    'brass': (0.55, 0.36, 0.09),
    'olive': (0.17, 0.21, 0.06),
    'olive_dark': (0.10, 0.13, 0.035),
    'drybag': (0.72, 0.16, 0.03),
    'drybag_band': (0.04, 0.06, 0.16),
    'bone': (0.72, 0.62, 0.44),
    'stitch': (0.70, 0.55, 0.30),
    'patch_blue': (0.04, 0.12, 0.36),
}

VARIANTS = [
    dict(name='Item_Cesta', item_id='cesta', seed=4301, builder='cesta', tri_budget=(500, 6000)),
    dict(name='Item_RecipienteCoco', item_id='recipiente_coco', seed=4302, builder='recipiente_coco',
         tri_budget=(300, 4000)),
    dict(name='Item_VasijaBarro', item_id='vasija_barro', seed=4303, builder='vasija_barro',
         tri_budget=(400, 5000)),
    dict(name='Item_Cantimplora', item_id='cantimplora', seed=4304, builder='cantimplora',
         tri_budget=(400, 6000)),
    dict(name='Item_BolsaImpermeable', item_id='bolsa_impermeable', seed=4305, builder='bolsa_impermeable',
         tri_budget=(400, 6000)),
    dict(name='Item_Mochila', item_id='mochila', seed=4306, builder='mochila', tri_budget=(800, 9000),
         preview_rot_z=3.7),
    dict(name='Item_MochilaFibra', item_id='mochila_fibra', seed=4307, builder='mochila_fibra',
         tri_budget=(800, 9000), preview_rot_z=3.7),
    dict(name='Item_MochilaCueroBambu', item_id='mochila_cuero_bambu', seed=4308, builder='mochila_cuero_bambu',
         tri_budget=(800, 10000), preview_rot_z=3.9),
    dict(name='Item_CinturonCuero', item_id='cinturon_cuero', seed=4309, builder='cinturon_cuero',
         tri_budget=(400, 6000)),
    dict(name='Item_Angarillas', item_id='angarillas', seed=4310, builder='angarillas', tri_budget=(1000, 12000),
         preview_rot_z=0.6),
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
# piezas comunes
# ---------------------------------------------------------------------------
def _ring_cord(p, rnd, z, radius, cord_r, rgb, name, sx=1.0, sy=1.0, segs=36, cx=0.0, cy=0.0):
    """Aro de cordel cerrado (borde de cesta, gollete atado)."""
    pts = [(cx + math.cos(2 * math.pi * k / segs) * radius * sx, cy + math.sin(2 * math.pi * k / segs) * radius * sy, z)
           for k in range(segs + 1)]
    o = I.sweep(name, pts, cord_r, segs=6, cap=False)
    M.assign(o, ['M_Fabric'])
    I.cord_stripes(o, rgb, I.lerp3(rgb, (0.08, 0.04, 0.015), 0.45), 6)
    p.add(o, 'none')
    return o


def _strap(p, rnd, pts, width, thick, rgb, name, mat='M_Fabric', stitch=None):
    """Correa plana a lo largo de una polilínea: sección rectangular
    (ancho `width` en la dirección `side`, grosor `thick`). Cada punto es
    (pos, side) con side el vector del ancho."""
    rings = []
    for i, (pos, side) in enumerate(pts):
        pos = Vector(pos)
        t = (Vector(pts[min(i + 1, len(pts) - 1)][0]) - Vector(pts[max(i - 1, 0)][0])).normalized()
        s = Vector(side).normalized()
        n = t.cross(s).normalized()
        hw, ht = width / 2, thick / 2
        rings.append([pos + s * hw + n * ht, pos - s * hw + n * ht, pos - s * hw - n * ht, pos + s * hw - n * ht])
    o = C.ring_loft(name, rings, cap_start=True, cap_end=True)
    M.assign(o, [mat])
    st = stitch or I.lerp3(rgb, CPAL['stitch'], 0.6)

    def col(v):
        ring, k = divmod(v.index, 4)
        # costura clara en un canto cada dos anillos
        return (st if (k in (0, 1) and ring % 2 == 0 and 0 < ring < len(rings) - 1) else rgb) + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    return o


def _buckle(p, rnd, center, normal, w, h, rgb, name, mat='M_Metal', r=0.0025):
    """Hebilla rectangular (aro) apoyada en el plano de normal `normal`."""
    nrm = Vector(normal).normalized()
    up = Vector((0, 0, 1)) if abs(nrm.z) < 0.9 else Vector((1, 0, 0))
    u = up.cross(nrm).normalized()
    vv = nrm.cross(u).normalized()
    c = Vector(center) + nrm * r
    corners = [c + u * (w / 2) * sx + vv * (h / 2) * sy for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1), (1, 1))]
    pts = []
    for a, b in pairwise(corners):
        for k in range(3):
            pts.append(a.lerp(b, k / 3))
    pts.append(corners[0])
    o = I.sweep(name, pts, r, segs=5, cap=False)
    M.assign(o, [mat])
    I.tint(o, rgb, rnd, 0.02)
    p.add(o, 'none')
    return o


def _knot(p, rnd, pos, r, rgb, name):
    o = C.make_blob(name, pos, r, rnd.randint(0, 9999), subdivisions=2, noise_strength=0.2, relax_iterations=1)
    M.assign(o, ['M_Fabric'])
    I.tint(o, rgb, rnd, 0.02)
    p.add(o, 'none')
    return o


def _vessel_prof(outer, wall, rim_round=True):
    """Perfil de recipiente: `outer` [(r, z)] de la base (r=0) al labio;
    devuelve el perfil completo bajando por dentro (pared de grosor
    `wall`) hasta el eje."""
    inner = []
    for r, z in reversed(outer[1:]):
        inner.append((max(0.0, r - wall), z))
    top_r, top_z = outer[-1]
    rim = [(top_r - wall * 0.15, top_z + wall * 0.35), (top_r - wall * 0.85, top_z + wall * 0.35)] if rim_round else []
    # el fondo interior queda `wall` por encima del exterior
    inner = [(r, max(z, outer[0][1] + wall)) for r, z in inner]
    return outer + rim + inner[1:] + [(0.0, outer[0][1] + wall)]


# ---------------------------------------------------------------------------
# RECIPIENTES
# ---------------------------------------------------------------------------
@_register('cesta')
def _b_cesta(v, rnd, name):
    """Cesta de hoja de palma trenzada, boca ancha, borde de cordel y dos
    asas de lazo."""
    p = K.Parts()
    n = 16
    outer = [(0.0, 0.0)]
    for j in range(n + 1):
        t = j / n
        r = 0.085 + 0.045 * math.sin(t * math.pi * 0.6) + 0.012 * t
        z = 0.004 + 0.16 * t
        if j == 0:
            r = 0.075
            z = 0.0
        outer.append((r, z))
    prof = _vessel_prof(outer, 0.006)
    body = I.lathe('Body', prof, segs=48)
    n_around, pitch = 12, 0.02
    I.weave_bump(body, n_around, pitch, 0.0022, z0=0.004, zmin=0.006, zmax=0.155)
    M.assign(body, ['M_Leaf'])
    wa, wb = CPAL['leaf_weave']
    wcol = I.weave_col(wa, wb, n_around, pitch, z0=0.004)

    def col(co):
        if co.z < 0.004:
            return I.lerp3(wb, (0.1, 0.06, 0.02), 0.3)
        return wcol(co)
    I.color_fn(body, col, rnd, 0.02)
    p.add(body, 'none')
    rim_r = outer[-1][0] - 0.002
    _ring_cord(p, rnd, 0.164, rim_r, 0.0075, PAL['cord'][0], 'Rim')
    _ring_cord(p, rnd, 0.150, rim_r - 0.001, 0.004, PAL['cord_red'], 'RimLow')
    # asas: lazos de cordel trenzado a ambos lados
    for side in (1, -1):
        pts = []
        for k in range(13):
            a = math.pi * k / 12
            pts.append((side * (rim_r + 0.004 + math.sin(a) * 0.035), math.cos(a) * 0.04, 0.155 + math.sin(a) * 0.03))
        o = I.sweep(f'Handle{side}', pts, 0.0055, segs=6)
        M.assign(o, ['M_Fabric'])
        I.cord_stripes(o, PAL['cord'][1], I.lerp3(PAL['cord'][1], PAL['coir'][2], 0.5), 6)
        p.add(o, 'none')
    return p.finish(name)


def _coconut_half(name, rnd, R, h, jag=0.0, seed=0):
    """Media cáscara de coco boca arriba: exterior pardo y fibroso, pulpa
    blanca por dentro. `jag` deja el borde roto (irregular)."""
    n = 12
    outer = [(0.0, 0.0)]
    for j in range(1, n + 1):
        t = j / n
        a = t * math.pi / 2
        outer.append((R * math.sin(a) * (1.0 - 0.08 * (1 - t)), h * (1 - math.cos(a)) + 0.004 * t))
    outer[1] = (outer[1][0] * 1.4, 0.0)  # base aplanada para que no ruede
    prof = _vessel_prof(outer, 0.009)
    segs = 26
    o = I.lathe(name, prof, segs=segs)
    rim_z = outer[-1][1]
    n_out = len(outer)  # puntos de perfil exteriores (con el polo)
    if jag:
        # borde roto: dientes irregulares que bajan el labio
        for vv in o.data.vertices:
            if vv.co.z > rim_z - 0.014:
                a = math.atan2(vv.co.y, vv.co.x)
                d = jag * max(0.0, math.sin(a * 3 + seed) * 0.6 + math.sin(a * 7 + seed * 2) * 0.4)
                vv.co.z -= d * (vv.co.z - (rim_z - 0.014)) / 0.014
        o.data.update()
    M.assign(o, ['M_Wood'])

    def col(v):
        ring = I.lathe_ring(v.index, segs)
        co = v.co
        if ring >= n_out:
            # labio y pulpa: blanco en el borde, crema hacia el fondo
            return I.lerp3(CPAL['copra'], (0.66, 0.54, 0.34), max(0.0, 1 - co.z / h) * 0.6) + (0.0,)
        stripe = 0.5 + 0.5 * math.sin(math.atan2(co.y, co.x) * 18 + co.z * 120)
        return I.lerp3(CPAL['coconut'], CPAL['coconut_fiber'], stripe * 0.7) + (0.0,)
    C.set_vertex_colors(o, col)
    return o


@_register('recipiente_coco')
def _b_recipiente_coco(v, rnd, name):
    """Cuenco de medio coco con el borde pulido y un aro de cordel."""
    p = K.Parts()
    o = _coconut_half('Bowl', rnd, 0.068, 0.062)
    p.add(o, 'none')
    _ring_cord(p, rnd, 0.042, 0.0625, 0.0035, PAL['cord_red'], 'Band')
    return p.finish(name)


@_register('vasija_barro')
def _b_vasija_barro(v, rnd, name):
    """Vasija de barro cocido: panza redonda, cuello corto con labio
    vuelto, manchas de cocción y una banda de zigzag de engobe claro."""
    p = K.Parts()
    outer = [(0.0, 0.0)] + I.smooth_profile(
        [(0.05, 0.0), (0.075, 0.012), (0.105, 0.045), (0.12, 0.09), (0.118, 0.13), (0.105, 0.165),
         (0.082, 0.19), (0.068, 0.205), (0.066, 0.218), (0.072, 0.232), (0.082, 0.242)], per_seg=2)
    prof = _vessel_prof(outer, 0.008)
    o = I.lathe('Pot', prof, segs=40)
    M.assign(o, ['M_Stone'])
    seed = v['seed']
    from mathutils import noise as mnoise

    def col(co):
        rr = math.hypot(co.x, co.y)
        if co.z > 0.02 and rr < 0.058 and co.z < 0.236:
            return I.lerp3(CPAL['clay'], CPAL['clay_fire'], 0.5)  # interior en sombra
        fire = max(0.0, mnoise.noise(Vector((co.x * 14 + seed, co.y * 14, co.z * 10))) * 1.6)
        c = I.lerp3(CPAL['clay'], CPAL['clay_fire'], min(1.0, fire) * 0.75)
        a = math.atan2(co.y, co.x)
        # banda de trazos oblicuos de engobe (10 por vuelta = 4 lados cada uno)
        if 0.143 < co.z < 0.168 and ((a / (2 * math.pi) * 10 + (co.z - 0.143) * 25) % 1.0) < 0.4:
            c = CPAL['clay_slip']
        if 0.133 < co.z < 0.141 or 0.170 < co.z < 0.178:
            c = I.lerp3(c, CPAL['clay_slip'], 0.8)
        return c
    I.color_fn(o, col, rnd, 0.015)
    p.add(o, 'none')
    # cordel atado al cuello para colgarla
    _ring_cord(p, rnd, 0.205, 0.0705, 0.004, PAL['cord'][0], 'Neck')
    _knot(p, rnd, (0.071, 0.0, 0.203), 0.008, PAL['cord'][0], 'NeckKnot')
    return p.finish(name)


@_register('cantimplora')
def _b_cantimplora(v, rnd, name):
    """Cantimplora del Albatros: botella de aluminio en funda de lona
    verde oliva con broches, tapón de rosca atado con cadenita y asa de
    correa."""
    p = K.Parts()
    body = I.soft_box('Body', (0.15, 0.07, 0.19), center=(0, 0, 0.095), roundness=0.7, cuts=5)
    # riñón: la cara de la espalda (-Y) algo cóncava
    for vv in body.data.vertices:
        if vv.co.y < 0:
            vv.co.y += 0.012 * (1 - (vv.co.x / 0.075) ** 2)
    body.data.update()
    M.assign(body, ['M_Fabric'])

    def body_col(co):
        if co.z > 0.172:
            return (0.55, 0.50, 0.40)  # hombro de aluminio pulido (cálido por el sol)
        c = CPAL['olive'] if co.z > 0.012 else CPAL['olive_dark']
        # costura de la funda en los cantos laterales y ribete bajo el hombro
        if abs(abs(co.x) - 0.072) < 0.004 or 0.160 < co.z < 0.172:
            c = CPAL['olive_dark']
        return c
    I.color_fn(body, body_col, rnd, 0.015)
    p.add(body, 'none')
    # cuello y tapón
    neck = C.make_cylinder('Neck', 0.016, 0.02, segments=14, center=(0, 0, 0.196))
    M.assign(neck, ['M_Metal'])
    I.tint(neck, (0.55, 0.50, 0.40), rnd, 0.01)
    p.add(neck, 'none')
    cap = C.make_cylinder('Cap', 0.019, 0.022, segments=16, center=(0, 0, 0.214), radius2=0.0175)
    for vv in cap.data.vertices:
        # moleteado del tapón
        a = math.atan2(vv.co.y, vv.co.x)
        vv.co.x *= 1.0 + 0.04 * math.cos(a * 16)
        vv.co.y *= 1.0 + 0.04 * math.cos(a * 16)
    cap.data.update()
    M.assign(cap, ['M_Metal'])
    I.tint(cap, CPAL['brass'], rnd, 0.02)
    p.add(cap, 'none')
    # cadenita del tapón al cuello
    chain = I.sweep('Chain', [(0.019, 0, 0.214), (0.03, 0.0, 0.205), (0.034, 0.0, 0.19), (0.028, 0.0, 0.18)],
                    0.0014, segs=4)
    M.assign(chain, ['M_Metal'])
    I.tint(chain, CPAL['brass'], rnd, 0.01)
    p.add(chain, 'none')
    # tapeta con broches en la cara frontal (+Y)
    flap = I.soft_box('Flap', (0.075, 0.008, 0.05), center=(0, 0.036, 0.15), roundness=0.6, cuts=3)
    M.assign(flap, ['M_Fabric'])
    I.tint(flap, CPAL['olive_dark'], rnd, 0.01)
    p.add(flap, 'none')
    for sx in (-0.022, 0.022):
        snap = C.make_cylinder(f'Snap{sx}', 0.0055, 0.004, segments=10, center=(sx, 0.041, 0.137))
        snap.data.transform(Matrix.Translation((-sx, -0.041, -0.137)))
        snap.data.transform(Matrix.Rotation(math.pi / 2, 4, 'X'))
        snap.data.transform(Matrix.Translation((sx, 0.041, 0.137)))
        M.assign(snap, ['M_Metal'])
        I.tint(snap, CPAL['brass'], rnd, 0.01)
        p.add(snap, 'none')
    # asa de correa marrón sobre el hombro
    pts = []
    for k in range(9):
        a = math.pi * k / 8
        pts.append(((math.cos(a) * 0.058, 0.0, 0.17 + math.sin(a) * 0.06), (0, 1, 0)))
    _strap(p, rnd, pts, 0.018, 0.004, CPAL['leather'], 'Strap', mat='M_Fabric')
    return p.finish(name)


@_register('bolsa_impermeable')
def _b_bolsa_impermeable(v, rnd, name):
    """Bolsa estanca del kit de supervivencia: cuerpo de lona engomada
    roja, cierre enrollado azul marino con hebilla y cinta reflectante."""
    p = K.Parts()
    n = 14
    H = 0.24
    outer = [(0.0, 0.0)]
    for j in range(n + 1):
        t = j / n
        r = 0.068 * (1.0 + 0.06 * math.sin(t * math.pi))
        z = H * t
        if j == 0:
            r, z = 0.062, 0.0
        outer.append((r, z))
    outer.append((0.0, H + 0.006))
    body = I.lathe('Body', outer, segs=24)
    # la boca se aplasta en Y hacia el enrollado
    for vv in body.data.vertices:
        t = max(0.0, (vv.co.z - H * 0.62) / (H * 0.38))
        vv.co.y *= 1.0 - 0.8 * t ** 1.2
        vv.co.x *= 1.0 + 0.12 * t
        # arrugas horizontales suaves
        vv.co.x += 0.0015 * math.sin(vv.co.z * 150) * (1 if vv.co.x > 0 else -1)
    body.data.update()
    M.assign(body, ['M_Fabric'])

    def col(co):
        if 0.10 < co.z < 0.118:
            return (0.80, 0.72, 0.42)  # cinta reflectante (crema, no gris)
        return CPAL['drybag'] if co.z > 0.006 else I.lerp3(CPAL['drybag'], (0.1, 0.02, 0.01), 0.5)
    I.color_fn(body, col, rnd, 0.015)
    p.add(body, 'none')
    # rollo del cierre
    roll = I.sweep('Roll', [(-0.083, 0, H + 0.008), (0.0, 0, H + 0.012), (0.083, 0, H + 0.008)],
                   [0.017, 0.019, 0.017], segs=12)
    M.assign(roll, ['M_Fabric'])
    I.color_fn(roll, lambda co: I.lerp3(CPAL['drybag_band'], (0.08, 0.12, 0.30),
                                        0.5 + 0.5 * math.sin(math.atan2(co.z - H - 0.01, co.y) * 3)), rnd, 0.01)
    p.add(roll, 'none')
    # correas del cierre que bajan a los costados hasta la hebilla
    for side in (1, -1):
        pts = [((side * 0.085, 0, H + 0.01), (0, 1, 0)), ((side * 0.095, 0, H - 0.01), (0, 1, 0)),
               ((side * 0.088, 0, H - 0.05), (0, 1, 0)), ((side * 0.078, 0, H - 0.075), (0, 1, 0))]
        _strap(p, rnd, pts, 0.02, 0.003, CPAL['drybag_band'], f'Strap{side}')
    buckle = I.soft_box('Buckle', (0.012, 0.03, 0.026), center=(0.081, 0.0, H - 0.085), roundness=0.5, cuts=2)
    M.assign(buckle, ['M_Stone'])
    I.tint(buckle, (0.05, 0.05, 0.06), rnd, 0.005)
    p.add(buckle, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# MOCHILAS Y CARGA
# ---------------------------------------------------------------------------
def _shoulder_straps(p, rnd, w, depth, h, rgb, name='Sh'):
    """Dos hombreras en la espalda (-Y): bajan en arco desde lo alto."""
    for side in (1, -1):
        x = side * w * 0.22
        pts = []
        for k in range(9):
            t = k / 8
            z = h * (0.92 - 0.8 * t)
            y = -depth / 2 - 0.012 - 0.045 * math.sin(t * math.pi)
            pts.append(((x + side * 0.02 * t, y, z), (1, 0, 0)))
        _strap(p, rnd, pts, 0.045, 0.012, rgb, f'{name}{side}')


@_register('mochila')
def _b_mochila(v, rnd, name):
    """Mochila de lona del Albatros: cuerpo abultado, solapa con dos
    correas de cuero y hebillas de latón, bolsillo frontal y parche azul
    con el ala de la compañía."""
    p = K.Parts()
    W, D, H = 0.30, 0.17, 0.40
    body = I.soft_box('Body', (W, D, H), center=(0, 0, H / 2), roundness=0.45, cuts=6)
    # panza: la cara frontal se abomba y la base se ensancha un poco
    for vv in body.data.vertices:
        t = vv.co.z / H
        if vv.co.y > 0:
            vv.co.y += 0.02 * math.sin(t * math.pi) * (1 - (vv.co.x / (W / 2)) ** 2)
        vv.co.x *= 1.0 + 0.06 * (1 - t)
    body.data.update()
    M.assign(body, ['M_Fabric'])
    from mathutils import noise as mnoise

    def body_col(co):
        stain = max(0.0, mnoise.noise(Vector((co.x * 9, co.y * 9, co.z * 9))))
        c = I.lerp3(CPAL['canvas'], CPAL['canvas_dark'], stain * 0.8)
        if co.z < 0.03:
            c = I.lerp3(CPAL['leather'], CPAL['leather_dark'], 0.3)  # culo de cuero
        return c
    I.color_fn(body, body_col, rnd, 0.015)
    p.add(body, 'none')
    # solapa: tapa sobre la boca + faldón que cae por delante
    lid = I.soft_box('Lid', (W * 1.02, D * 1.08, 0.035), center=(0, 0.004, H + 0.004), roundness=0.6, cuts=5)
    for vv in lid.data.vertices:
        vv.co.z += 0.012 * (1 - (vv.co.x / (W / 2)) ** 2) * (1 - (vv.co.y / (D / 2)) ** 2)
    lid.data.update()
    M.assign(lid, ['M_Fabric'])
    I.tint(lid, CPAL['canvas_dark'], rnd, 0.015)
    p.add(lid, 'none')
    skirt = I.soft_box('FlapFront', (W * 0.96, 0.022, 0.15), center=(0, D / 2 + 0.017, H - 0.06), roundness=0.6,
                       cuts=4)
    for vv in skirt.data.vertices:
        # borde inferior redondeado y abombado sobre la panza
        t = (H + 0.015 - vv.co.z) / 0.15
        vv.co.y += 0.012 * math.sin(max(0.0, min(1.0, t)) * math.pi * 0.7)
        vv.co.z += 0.03 * (vv.co.x / (W / 2)) ** 2 * max(0.0, t - 0.5)
    skirt.data.update()
    M.assign(skirt, ['M_Fabric'])
    I.color_fn(skirt, lambda co: CPAL['stitch'] if abs(co.z - (H - 0.125)) < 0.003 else CPAL['canvas_dark'],
               rnd, 0.015)
    p.add(skirt, 'none')
    # bolsillo frontal
    front_y = D / 2 + 0.018
    pocket = I.soft_box('Pocket', (0.19, 0.05, 0.14), center=(0, front_y + 0.008, 0.12), roundness=0.5, cuts=4)
    M.assign(pocket, ['M_Fabric'])
    I.color_fn(pocket, lambda co: CPAL['canvas_dark'] if co.z > 0.175 else CPAL['canvas'], rnd, 0.015)
    p.add(pocket, 'none')
    # parche azul con ala crema en el bolsillo
    patch = C.make_cylinder('Patch', 0.028, 0.004, segments=16, center=(0, 0, 0))
    patch.data.transform(Matrix.Rotation(math.pi / 2, 4, 'X'))
    patch.data.transform(Matrix.Translation((0, front_y + 0.035, 0.115)))
    M.assign(patch, ['M_Fabric'])
    I.color_fn(patch, lambda co: (0.80, 0.70, 0.45) if (abs(co.z - 0.115 - 0.2 * abs(co.x)) < 0.004
                                                        and abs(co.x) < 0.02) else CPAL['patch_blue'], rnd, 0.0)
    p.add(patch, 'none')
    # dos correas de cuero de la solapa al cuerpo, con hebilla
    for sx in (-0.085, 0.085):
        pts = [((sx, -0.02, H + 0.028), (1, 0, 0)), ((sx, D / 2 + 0.01, H + 0.03), (1, 0, 0)),
               ((sx, D / 2 + 0.034, H + 0.005), (1, 0, 0)), ((sx, D / 2 + 0.042, H - 0.06), (1, 0, 0)),
               ((sx, D / 2 + 0.046, H - 0.16), (1, 0, 0)), ((sx, D / 2 + 0.044, H - 0.21), (1, 0, 0))]
        _strap(p, rnd, pts, 0.025, 0.005, CPAL['leather'], f'FlapStrap{sx}')
        _buckle(p, rnd, (sx, D / 2 + 0.045, H - 0.14), (0, 1, 0), 0.032, 0.026, CPAL['brass'], f'Buckle{sx}')
    _shoulder_straps(p, rnd, W, D, H, CPAL['leather'])
    # asa superior
    pts = [((-0.04, -0.03, H + 0.02), (0, 1, 0)), ((-0.03, -0.03, H + 0.05), (0, 1, 0)),
           ((0.03, -0.03, H + 0.05), (0, 1, 0)), ((0.04, -0.03, H + 0.02), (0, 1, 0))]
    _strap(p, rnd, pts, 0.02, 0.008, CPAL['leather_dark'], 'TopHandle')
    return p.finish(name)


@_register('mochila_fibra')
def _b_mochila_fibra(v, rnd, name):
    """Morral de fibra de coco trenzada: saco ovalado cerrado con cordón
    fruncido y dos tirantes de cuerda."""
    p = K.Parts()
    n = 40
    H = 0.40

    def sack_r(t):
        return 0.10 + 0.05 * math.sin(min(1.0, t * 1.2) * math.pi * 0.85) - 0.06 * max(0.0, t - 0.8) / 0.2
    outer = [(0.0, 0.0)]
    for j in range(n + 1):
        t = j / n
        r, z = sack_r(t), H * t
        if j == 0:
            r, z = 0.09, 0.0
        outer.append((r, z))
    outer.append((0.0, H - 0.01))
    body = I.lathe('Sack', outer, segs=48, sx=1.25, sy=0.7)
    n_around, pitch = 12, 0.03
    I.weave_bump(body, n_around, pitch, 0.003, zmin=0.01, zmax=H * 0.8)
    # frunce: pliegues radiales en la boca
    for vv in body.data.vertices:
        if vv.co.z > H * 0.8:
            a = math.atan2(vv.co.y, vv.co.x)
            f = 1.0 + 0.18 * math.sin(a * 9) * (vv.co.z - H * 0.8) / (H * 0.2)
            vv.co.x *= f
            vv.co.y *= f
    body.data.update()
    M.assign(body, ['M_Fabric'])
    wcol = I.weave_col(CPAL['coir_weave'][0], CPAL['coir_weave'][1], n_around, pitch)
    I.color_fn(body, lambda co: wcol(co) if co.z < H * 0.8 else I.lerp3(CPAL['coir_weave'][0], PAL['straw'], 0.3),
               rnd, 0.02)
    p.add(body, 'none')
    # cordón del frunce
    _ring_cord(p, rnd, H * 0.8, 0.105, 0.006, PAL['cord'][0], 'Draw', sx=1.2, sy=0.72)
    _knot(p, rnd, (0.0, 0.08, H * 0.8), 0.012, PAL['cord'][0], 'DrawKnot')
    for k, dx in enumerate((-0.012, 0.014)):
        # cabos del cordón: cuelgan pegados a la panza delantera
        pts = []
        for q in range(5):
            z = H * (0.8 - 0.05 * q)
            pts.append((dx * (1 + q * 0.4), sack_r(z / H) * 0.7 + 0.006, z))
        tail = I.sweep(f'Tail{k}', pts, 0.0045, segs=6)
        M.assign(tail, ['M_Fabric'])
        I.tint(tail, PAL['cord'][0], rnd, 0.02)
        p.add(tail, 'none')
    # tirantes de cuerda por la espalda (-Y)
    for side in (1, -1):
        pts = []
        for k in range(11):
            t = k / 10
            z = H * (0.8 - 0.7 * t)
            x = side * (0.05 + 0.05 * t)
            # sobre la superficie trasera (-Y) del saco ovalado, en comba
            back = sack_r(z / H) * 0.7 * math.sqrt(max(0.0, 1 - (x / (sack_r(z / H) * 1.25)) ** 2))
            pts.append((x, -back - 0.008 - 0.035 * math.sin(t * math.pi) ** 1.5, z))
        o = I.sweep(f'Rope{side}', pts, 0.008, segs=6)
        M.assign(o, ['M_Fabric'])
        I.cord_stripes(o, PAL['cord'][1], I.lerp3(PAL['cord'][1], PAL['coir'][2], 0.5), 6)
        p.add(o, 'none')
    return p.finish(name)


@_register('mochila_cuero_bambu')
def _b_mochila_cuero_bambu(v, rnd, name):
    """Mochila de armazón: marco en A de bambú atado, saco de cuero con
    solapa y costuras, y un rollo de estera atado arriba."""
    p = K.Parts()
    H = 0.66
    bam = PAL['bamboo'][1]

    def bamboo(pts, r, nm):
        o = I.sweep(nm, pts, r, segs=8)
        M.assign(o, ['M_Wood'])
        a0 = Vector(pts[0])
        I.color_fn(o, lambda co: I.lerp3(bam, PAL['bamboo_node'],
                                         max(0.0, 1 - abs((((co - a0).length + 0.05) % 0.22) - 0.11) / 0.012)),
                   rnd, 0.02)
        p.add(o, 'none')
        # tapas de corte claras
        for end in (pts[0], pts[-1]):
            b = C.make_blob(f'{nm}Cut{end[2]:.2f}', end, r * 0.95, 1, subdivisions=1, noise_strength=0.0,
                            scale=(1, 1, 0.35))
            M.assign(b, ['M_Wood'])
            I.tint(b, PAL['bamboo_cut'], rnd, 0.01)
            p.add(b, 'none')
    # dos largueros verticales (ligeramente convergentes arriba) por la espalda
    for side in (1, -1):
        bamboo([(side * 0.17, -0.06, 0.0), (side * 0.15, -0.075, H * 0.5), (side * 0.12, -0.07, H)], 0.014,
               f'Rail{side}')
    for k, z in enumerate((0.07, 0.36, 0.60)):
        half = 0.17 - 0.05 * z / H + 0.02
        bamboo([(-half, -0.075, z), (half, -0.075, z)], 0.011, f'Bar{k}')
        for side in (1, -1):
            x = side * (0.17 - 0.05 * z / H)
            o = I.helix_wrap(f'Lash{k}{side}', z - 0.018, z + 0.018, 0.017, 0.0028, rnd, segs=5,
                             center=(x, -0.07))
            M.assign(o, ['M_Fabric'])
            I.tint(o, PAL['cord'][k % 2], rnd, 0.02)
            p.add(o, 'none')
    # saco de cuero delante del marco
    bag = I.soft_box('Bag', (0.30, 0.16, 0.36), center=(0, 0.03, 0.30), roundness=0.5, cuts=6)
    for vv in bag.data.vertices:
        if vv.co.y > 0.03:
            vv.co.y += 0.02 * math.sin((vv.co.z - 0.12) / 0.36 * math.pi)
    bag.data.update()
    M.assign(bag, ['M_Fabric'])

    def bag_col(co):
        c = CPAL['leather']
        if abs(abs(co.x) - 0.135) < 0.004 or abs(co.z - 0.14) < 0.003:
            c = CPAL['stitch']  # costura de tendón
        spot = 0.5 + 0.5 * math.sin(co.x * 40 + co.z * 31) * math.sin(co.z * 23)
        return I.lerp3(c, CPAL['leather_dark'], spot * 0.35)
    I.color_fn(bag, bag_col, rnd, 0.015)
    p.add(bag, 'none')
    flap = I.soft_box('Flap', (0.31, 0.02, 0.15), center=(0, 0.13, 0.40), roundness=0.6, cuts=4)
    for vv in flap.data.vertices:
        vv.co.y += 0.02 * ((vv.co.z - 0.40) / 0.075) ** 2 * -0.5
    flap.data.update()
    M.assign(flap, ['M_Fabric'])
    I.color_fn(flap, lambda co: CPAL['stitch'] if abs(co.z - 0.335) < 0.004 else CPAL['leather_dark'], rnd, 0.015)
    p.add(flap, 'none')
    # botón de hueso con lazo
    tog = I.sweep('Toggle', [(-0.02, 0.145, 0.33), (0.02, 0.145, 0.33)], 0.006, segs=8)
    M.assign(tog, ['M_Stone'])
    I.tint(tog, CPAL['bone'], rnd, 0.02)
    p.add(tog, 'none')
    # estera enrollada atada en lo alto del marco
    mat = I.sweep('Mat', [(-0.2, -0.02, 0.56), (0.2, -0.02, 0.56)], 0.05, segs=14)
    M.assign(mat, ['M_Leaf'])
    I.color_fn(mat, lambda co: I.lerp3(CPAL['leaf_weave'][0], CPAL['leaf_weave'][1],
                                       0.5 + 0.5 * math.sin(math.atan2(co.z - 0.56, co.y + 0.02) * 4 + co.x * 60)),
               rnd, 0.02)
    p.add(mat, 'none')
    for sx in (-0.13, 0.13):
        _ring_cord(p, rnd, 0.0, 0.052, 0.004, PAL['cord_red'], f'MatTie{sx}', segs=20)
        o = p.kinds['none'][-1]
        o.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
        o.data.transform(Matrix.Translation((sx, -0.02, 0.56)))
    return p.finish(name)


@_register('cinturon_cuero')
def _b_cinturon_cuero(v, rnd, name):
    """Cinturón de cuero enrollado de canto, hebilla de hueso tallado y dos
    enganches de madera para colgar herramientas."""
    p = K.Parts()
    Wd, T = 0.04, 0.005
    turns = 1.6
    n = 70
    rings = []
    path = []
    for i in range(n + 1):
        t = i / n
        a = t * turns * 2 * math.pi
        r = 0.07 + 0.045 * t
        c = Vector((math.cos(a) * r, math.sin(a) * r, Wd / 2 + 0.001))
        radial = Vector((math.cos(a), math.sin(a), 0.0))
        up = Vector((0, 0, 1))
        rings.append([c + radial * T / 2 + up * Wd / 2, c - radial * T / 2 + up * Wd / 2,
                      c - radial * T / 2 - up * Wd / 2, c + radial * T / 2 - up * Wd / 2])
        path.append((c, radial, a))
    # punta redondeada al principio (dentro): estrecha los últimos anillos
    for i in range(4):
        f = 0.3 + 0.7 * i / 4
        c = path[i][0]
        rings[i] = [c + Vector((q.x - c.x, q.y - c.y, (q.z - c.z) * f)) for q in rings[i]]
    o = C.ring_loft('Belt', rings, cap_start=True, cap_end=True)
    M.assign(o, ['M_Fabric'])

    def col(vv):
        ring, k = divmod(vv.index, 4)
        c = CPAL['leather'] if k in (0, 3) else I.lerp3(CPAL['leather'], CPAL['stitch'], 0.25)  # carne más clara
        if k in (1, 2) and ring < 22 and ring % 5 == 2:
            c = CPAL['leather_dark']  # agujeros de la hebilla (dentro)
        return c + (0.0,)
    C.set_vertex_colors(o, col)
    p.add(o, 'none')
    # hebilla de hueso en el extremo exterior
    c_end, radial, a_end = path[-1]
    tang = Vector((-math.sin(a_end), math.cos(a_end), 0.0))
    bc = c_end + tang * 0.012 + radial * 0.004
    buckle = I.soft_box('Buckle', (0.05, 0.012, 0.056), roundness=0.5, cuts=3)
    # el hueco del aro va pintado (oscuro) en la cara exterior
    rot = Matrix((tang, radial, Vector((0, 0, 1)))).transposed().to_4x4()
    buckle.data.transform(rot)
    buckle.data.transform(Matrix.Translation(bc))
    M.assign(buckle, ['M_Stone'])

    def bcol(co):
        d = co - bc
        u, w = d.dot(tang), d.z
        hole = abs(u) < 0.014 and abs(w) < 0.018
        return (0.20, 0.13, 0.07) if hole and d.dot(radial) > 0 else I.lerp3(CPAL['bone'], (0.55, 0.42, 0.26),
                                                                             abs(w) / 0.03)
    I.color_fn(buckle, bcol, rnd, 0.015)
    p.add(buckle, 'none')
    # dos enganches de madera (ganchos en J) cosidos por fuera
    for k, frac in enumerate((0.55, 0.78)):
        c, rad, a = path[int(frac * n)]
        tg = Vector((-math.sin(a), math.cos(a), 0.0))
        base = c + rad * (T / 2 + 0.004)
        pts = [base + Vector((0, 0, 0.012)), base + rad * 0.012 + Vector((0, 0, 0.006)),
               base + rad * 0.018 - Vector((0, 0, 0.004)), base + rad * 0.012 + tg * 0.004 - Vector((0, 0, 0.012)),
               base + rad * 0.003 + tg * 0.006 - Vector((0, 0, 0.01))]
        hook = I.sweep(f'Hook{k}', pts, 0.0035, segs=6)
        M.assign(hook, ['M_Wood'])
        I.tint(hook, PAL['wood_handle'][0], rnd, 0.02)
        p.add(hook, 'none')
    return p.finish(name)


@_register('angarillas')
def _b_angarillas(v, rnd, name):
    """Angarillas: dos varas de 2.3 m con travesaños atados y un lecho de
    cuerda trenzada en rombos para arrastrar o cargar entre dos."""
    p = K.Parts()
    L, gap, r = 2.3, 0.56, 0.028
    for side in (1, -1):
        o, pts = I.stick(f'Pole{side}', L, r * 1.05, r * 0.9, rnd, n=12, bend=0.006, wobble=0.002, segs=9)
        o.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
        o.data.transform(Matrix.Translation((-L / 2, side * gap / 2, r)))
        M.assign(o, ['M_Wood'])

        def pcol(co):
            if abs(co.x) > L / 2 - 0.004:
                return PAL['wood_cut']
            # mangos pelados y claros en los extremos
            if abs(co.x) > L / 2 - 0.3:
                return I.lerp3(PAL['wood_light'][0], PAL['wood_cut'], 0.5 + 0.5 * math.sin(co.x * 50))
            return I.lerp3(PAL['bark'][0], PAL['bark'][1], 0.5 + 0.5 * math.sin(co.x * 45 + co.y * 300))
        I.color_fn(o, pcol, rnd, 0.02)
        p.add(o, 'none')
    bars = [-0.6, -0.2, 0.2, 0.6]
    for k, x in enumerate(bars):
        o, _ = I.stick(f'Bar{k}', gap + 0.12, 0.016, 0.015, rnd, n=4, bend=0.01, wobble=0.003, segs=7)
        o.data.transform(Matrix.Translation((0, 0, -(gap + 0.12) / 2)))
        o.data.transform(Matrix.Rotation(math.pi / 2, 4, 'X'))
        o.data.transform(Matrix.Translation((x, 0, 2 * r + 0.012)))
        M.assign(o, ['M_Wood'])
        I.color_fn(o, lambda co: I.lerp3(PAL['bark'][1], PAL['bark'][2], 0.5 + 0.5 * math.sin(co.y * 60)), rnd, 0.02)
        p.add(o, 'none')
        for side in (1, -1):
            # atadura en X donde el travesaño cruza la vara
            cx, cy = x, side * gap / 2
            for s in (1, -1):
                pts = []
                for q in range(13):
                    a = 2 * math.pi * q / 12
                    d = Vector((math.cos(a) * 0.036, 0.0, math.sin(a) * 0.036))
                    d = Matrix.Rotation(s * math.pi / 4, 3, 'Z') @ d
                    pts.append(Vector((cx, cy, 2 * r - 0.004)) + d)
                w = I.sweep(f'Lash{k}{side}{s}', pts, 0.004, segs=5, cap=False)
                M.assign(w, ['M_Fabric'])
                I.tint(w, PAL['cord'][(k + s) % 2], rnd, 0.02)
                p.add(w, 'none')
    # lecho: rombos de cuerda tendidos entre las varas (entre el 1.º y el
    # último travesaño), con una comba suave hacia abajo
    x0, x1 = bars[0], bars[-1]
    zb = 2 * r + 0.008
    for d in (1, -1):
        m = 7
        for i in range(-2, m + 3):
            pts = []
            for q in range(9):
                t = q / 8
                y = -gap / 2 + gap * t
                x = x0 + (x1 - x0) * i / m + d * (t - 0.5) * gap * 0.9
                if x < x0 - 0.001 or x > x1 + 0.001:
                    continue
                sag = 0.03 * math.sin(t * math.pi)
                pts.append((x, y, zb - sag))
            if len(pts) < 3:
                continue
            o = I.sweep(f'Net{d}{i}', pts, 0.005, segs=5)
            M.assign(o, ['M_Fabric'])
            I.tint(o, PAL['cord'][0] if d > 0 else PAL['coir'][1], rnd, 0.02)
            p.add(o, 'none')
    # cuerdas laterales del lecho a lo largo de las varas
    for side in (1, -1):
        o = I.sweep(f'Edge{side}', [(x0, side * (gap / 2 - 0.02), zb), (x1, side * (gap / 2 - 0.02), zb)],
                    0.006, segs=6)
        M.assign(o, ['M_Fabric'])
        I.cord_stripes(o, PAL['cord'][1], I.lerp3(PAL['cord'][1], PAL['coir'][2], 0.5), 6)
        p.add(o, 'none')
    return p.finish(name)
