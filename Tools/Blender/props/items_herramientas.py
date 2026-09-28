"""
items_herramientas.py — herramientas de las primeras horas (items.json):
cuchillo, hacha, lanza, pala, estaca, martillo y antorcha de nivel tosco y
tallado (biblia §6: piedra, hueso, concha y madera atados con cordel), más
las tres lascas que el jugador talla antes de tener nada.

Pivote = SOCKET DE MANO (ver _items.py): el origen es el centro del puño,
el mango va por +Z y el filo/cara de golpe mira a +X. Las lascas sueltas
son materiales: pivote en la base, tumbadas.

| Malla | Agarre (origen) |
|---|---|
| SM_Item_Cuchillo | centro del mango de 11 cm; hoja de pedernal hacia +Z, filo +X |
| SM_Item_Hacha | a 12 cm del extremo del mango; cabeza de basalto en z≈0.36, filo +X |
| SM_Item_Lanza | a 0.75 m del regatón (equilibrio para lanzar); punta de obsidiana en +Z |
| SM_Item_Pala | mano alta a 0.25 m del extremo superior; valva de almeja abajo (-Z), cara cóncava +X |
| SM_Item_Estaca | a 0.2 m del extremo romo; punta endurecida al fuego en +Z |
| SM_Item_Martillo | a 9 cm del extremo del mango; canto rodado atado en z≈0.27, cara de golpe +X |
| SM_Item_Antorcha | a 0.14 m del extremo inferior; haz de fibra y resina en +Z |
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
    dict(name='Item_Cuchillo', item_id='cuchillo', seed=4101, builder='cuchillo', tri_budget=(200, 3000)),
    dict(name='Item_Hacha', item_id='hacha', seed=4102, builder='hacha', tri_budget=(300, 4000)),
    dict(name='Item_Lanza', item_id='lanza', seed=4103, builder='lanza', tri_budget=(300, 5000)),
    dict(name='Item_Pala', item_id='pala', seed=4104, builder='pala', tri_budget=(300, 4000),
         preview_rot_z=-1.95),
    dict(name='Item_Estaca', item_id='estaca', seed=4105, builder='estaca', tri_budget=(100, 2000)),
    dict(name='Item_Martillo', item_id='martillo', seed=4106, builder='martillo', tri_budget=(300, 4000)),
    dict(name='Item_Antorcha', item_id='antorcha', seed=4107, builder='antorcha', tri_budget=(300, 4000)),
    dict(name='Item_LascaObsidiana', item_id='lasca_obsidiana', seed=4108, builder='lasca_obsidiana',
         tri_budget=(100, 2500)),
    dict(name='Item_LascaPedernal', item_id='lasca_pedernal', seed=4109, builder='lasca_pedernal',
         tri_budget=(100, 2500)),
    dict(name='Item_LascaTallada', item_id='lasca_tallada', seed=4110, builder='lasca_tallada',
         tri_budget=(100, 2500)),
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
    return _BUILDERS[variant['builder']](variant, rnd, 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# piezas comunes
# ---------------------------------------------------------------------------
def _handle(p, rnd, z0, z1, r0, r1, rgb=None, name='Handle', bend=0.015):
    o, pts = I.stick(name, z1 - z0, r0, r1, rnd, n=8, bend=bend, wobble=0.004, segs=8, z0=z0)
    M.assign(o, ['M_Wood'])
    base = rgb or rnd.choice(PAL['wood_handle'])
    light = I.lerp3(base, PAL['wood_cut'], 0.45)
    # vetas longitudinales suaves: más claro en las zonas gastadas por la mano
    I.color_fn(o, lambda co: I.lerp3(base, light, 0.5 + 0.5 * math.sin(co.z * 40 + co.x * 300)), rnd, 0.02)
    p.add(o, 'none')
    return o, pts


def _wrap(p, rnd, z0, z1, radius, cord_r=0.0035, rgb=None, name='Wrap', center=(0.0, 0.0)):
    o = I.helix_wrap(name, z0, z1, radius + cord_r * 0.6, cord_r, rnd, segs=5, center=center)
    M.assign(o, ['M_Fabric'])
    I.cord_stripes(o, rgb or PAL['cord'][0], I.lerp3(rgb or PAL['cord'][0], (0.2, 0.12, 0.05), 0.45), 5)
    p.add(o, 'none')
    return o


def _cross_lash(p, rnd, center, rx, rz, cord_r, turns=3, name='Lash'):
    """Ataduras en X que abrazan una cabeza (en el plano XZ) contra el
    mango: bucles inclinados ±40° alrededor del eje Y."""
    for side, ang in enumerate((math.radians(40), math.radians(-40))):
        for t in range(turns):
            pts = []
            off = (t - (turns - 1) / 2) * cord_r * 2.2
            for k in range(17):
                a = 2 * math.pi * k / 16
                pts.append(Vector((math.cos(a) * rx, math.sin(a) * rx * 0.9, off + math.sin(a) * 0.0)))
            rot = Matrix.Rotation(ang, 3, 'Y')
            pts = [rot @ Vector((q.x, q.y, q.z * rz / max(rx, 1e-4))) + Vector(center) for q in pts]
            o = I.sweep(f'{name}{side}_{t}', pts, cord_r, segs=5, cap=False)
            M.assign(o, ['M_Fabric'])
            I.tint(o, PAL['cord'][t % 2], rnd, 0.02)
            p.add(o, 'none')


# ---------------------------------------------------------------------------
# LASCAS
# ---------------------------------------------------------------------------
def _flake_obj(name, seed, length, width, thick, core, edge, cortex=None, point=0.7):
    """Lasca pintada: cuencos de cicatriz del color del núcleo, aristas de
    talla y filo perimetral más claros (translúcidos), córtex opcional."""
    o = I.flake(name, length, width, thick, seed, point=point)
    M.assign(o, ['M_Stone'])
    ridges = I.FLAKE_RIDGES.get(o.name, {})
    ridge_rgb = I.lerp3(core, edge, 0.7)

    def col(v):
        co = v.co
        rr = min(1.0, (co.x / (width / 2)) ** 2 + (co.z / (length / 2)) ** 2)
        c = I.lerp3(core, ridge_rgb, ridges.get(v.index, 0.0))
        c = I.lerp3(c, edge, max(0.0, rr - 0.55) / 0.45)
        if cortex is not None and co.y > thick * 0.1 and co.z < -length * 0.22:
            c = I.lerp3(c, cortex, 0.85)
        return c + (0.0,)
    return o, col


def _lasca(v, rnd, name, core, edge, cortex, dims, point):
    length, width, thick = dims
    o, col = _flake_obj(name, v['seed'], length, width, thick, core, edge, cortex, point)
    C.set_vertex_colors(o, col)
    p = K.Parts()
    p.add(o, 'none')
    # tumbada sobre una cara, algo girada
    p.transform(Matrix.Rotation(math.pi / 2, 4, 'X') @ Matrix.Rotation(0.35, 4, 'Y'))
    obj = p.finish(name)
    return I.ground_centered(obj)


@_register('lasca_obsidiana')
def _b_lasca_obsidiana(v, rnd, name):
    return _lasca(v, rnd, name, PAL['obsidian'], PAL['obsidian_edge'], None, (0.10, 0.045, 0.012), 0.8)


@_register('lasca_pedernal')
def _b_lasca_pedernal(v, rnd, name):
    return _lasca(v, rnd, name, PAL['flint'], PAL['flint_edge'], PAL['flint_cortex'], (0.09, 0.05, 0.014), 0.6)


@_register('lasca_tallada')
def _b_lasca_tallada(v, rnd, name):
    # lasca genérica de basalto claro/andesita: canto cálido, no gris
    return _lasca(v, rnd, name, (0.24, 0.11, 0.05), (0.50, 0.28, 0.13), (0.62, 0.50, 0.30),
                  (0.075, 0.055, 0.016), 0.45)


# ---------------------------------------------------------------------------
# HERRAMIENTAS
# ---------------------------------------------------------------------------
@_register('cuchillo')
def _b_cuchillo(v, rnd, name):
    """Hoja de pedernal encajada en un mango de madera con atadura de
    cordel y remate de resina."""
    p = K.Parts()
    _handle(p, rnd, -0.06, 0.055, 0.0135, 0.0125, rgb=PAL['wood_handle'][0], bend=0.0)
    pom = C.make_blob('Pommel', (0, 0, -0.062), 1.0, v['seed'], subdivisions=2, noise_strength=0.05,
                      scale=(0.016, 0.016, 0.012))
    M.assign(pom, ['M_Wood'])
    I.tint(pom, PAL['wood_handle'][1], rnd)
    p.add(pom, 'none')
    blade, col = _flake_obj('Blade', v['seed'] + 1, 0.14, 0.044, 0.009, PAL['flint'],
                            I.lerp3(PAL['flint'], PAL['flint_edge'], 0.6),
                            None, point=0.62)
    C.set_vertex_colors(blade, col)
    blade.data.transform(Matrix.Translation((0.004, 0, 0.115)))
    p.add(blade, 'none')
    # resina que sella la hoja en la ranura del mango
    res = C.make_blob('Resin', (0.0, 0, 0.058), 1.0, v['seed'] + 2, subdivisions=2, noise_strength=0.12,
                      scale=(0.019, 0.011, 0.012))
    M.assign(res, ['M_Stone'])
    I.tint(res, PAL['resin'], rnd)
    p.add(res, 'none')
    _wrap(p, rnd, 0.018, 0.05, 0.0132, cord_r=0.0026, rgb=PAL['cord_red'], name='WrapTop')
    _wrap(p, rnd, -0.045, -0.03, 0.0132, cord_r=0.0026, rgb=PAL['cord'][0], name='WrapBot')
    return p.finish(name)


@_register('hacha')
def _b_hacha(v, rnd, name):
    """Hacha de piedra verde pulida (azuela) con mango de guayabo atado con cordel en X."""
    p = K.Parts()
    _handle(p, rnd, -0.12, 0.43, 0.017, 0.021, bend=0.02)
    # cabeza: cuña de basalto (filo pulido en +X, talón romo en -X)
    head = C.make_blob('Head', (0, 0, 0), 1.0, v['seed'], subdivisions=3, noise_scale=1.2, noise_strength=0.06)
    for vv in head.data.vertices:
        x, y, z = vv.co
        t = max(0.0, min(1.0, (x + 1) / 2))  # 0 talón .. 1 filo
        y *= 1.0 - 0.85 * t ** 1.6
        z *= 0.75 + 0.35 * t
        vv.co = Vector((x * 0.088 + 0.03, y * 0.034, z * 0.05 + 0.36))
    M.assign(head, ['M_Stone'])
    I.color_fn(head, lambda co: I.lerp3(PAL['greenstone'], PAL['greenstone_edge'], (co.x - 0.06) / 0.04), rnd, 0.02)
    p.add(head, 'none')
    _cross_lash(p, rnd, (0.0, 0.0, 0.36), 0.03, 0.03, 0.0035, turns=3)
    _wrap(p, rnd, 0.29, 0.325, 0.02, name='WrapLow')
    _wrap(p, rnd, 0.395, 0.425, 0.021, name='WrapHigh')
    _wrap(p, rnd, -0.10, -0.06, 0.017, rgb=PAL['cord_red'], cord_r=0.003, name='WrapGrip')
    return p.finish(name)


@_register('lanza')
def _b_lanza(v, rnd, name):
    """Lanza de bambú con punta de obsidiana atada y resina."""
    p = K.Parts()
    L = 1.9
    z0 = -0.75
    n = 24
    pts = [(0.0, 0.0, z0 + L * i / n) for i in range(n + 1)]
    # nudos del bambú cada ~0.32 m
    radii = []
    for i in range(n + 1):
        z = L * i / n
        node = 1.0 + 0.12 * max(0.0, 1 - abs(((z + 0.1) % 0.32) - 0.16) / 0.02) if 0 < i < n else 1.0
        radii.append((0.016 - 0.003 * i / n) * node)
    shaft = I.sweep('Shaft', pts, radii, segs=8)
    M.assign(shaft, ['M_Wood'])
    I.color_fn(shaft, lambda co: I.lerp3(PAL['bamboo'][1], PAL['bamboo_node'],
                                         max(0.0, 1 - abs(((co.z - z0 + 0.1) % 0.32) - 0.16) / 0.025)),
               rnd, 0.02)
    p.add(shaft, 'none')
    blade, col = _flake_obj('Point', v['seed'], 0.17, 0.04, 0.011, PAL['obsidian'], PAL['obsidian_edge'],
                            None, point=0.95)
    C.set_vertex_colors(blade, col)
    blade.data.transform(Matrix.Translation((0, 0, z0 + L + 0.06)))
    p.add(blade, 'none')
    res = C.make_blob('Resin', (0, 0, z0 + L - 0.005), 1.0, v['seed'] + 3, subdivisions=2, noise_strength=0.1,
                      scale=(0.017, 0.016, 0.02))
    M.assign(res, ['M_Stone'])
    I.tint(res, PAL['resin'], rnd)
    p.add(res, 'none')
    _wrap(p, rnd, z0 + L - 0.08, z0 + L - 0.01, 0.0135, name='WrapHead')
    _wrap(p, rnd, -0.07, 0.07, 0.0155, rgb=PAL['cord_red'], cord_r=0.003, name='WrapGrip')
    # regatón endurecido al fuego
    butt = C.make_cylinder('Butt', 0.0158, 0.03, segments=8, center=(0, 0, z0 + 0.012), radius2=0.013)
    M.assign(butt, ['M_Wood'])
    I.tint(butt, PAL['char'], rnd)
    p.add(butt, 'none')
    return p.finish(name)


@_register('pala')
def _b_pala(v, rnd, name):
    """Pala de concha: valva de almeja gigante atada a un mango en horquilla."""
    p = K.Parts()
    _handle(p, rnd, -0.72, 0.25, 0.019, 0.017, bend=0.01)
    # muleta en el extremo superior
    tee = C.make_cylinder('Tee', 0.016, 0.13, segments=8, center=(0, 0, 0))
    tee.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
    tee.data.transform(Matrix.Translation((0, 0, 0.26)))
    M.assign(tee, ['M_Wood'])
    I.tint(tee, PAL['wood_handle'][1], rnd)
    p.add(tee, 'none')
    shell = I.clam_shell('Blade', 0.28, 0.26, 0.055, ribs=5, thick=0.008)
    # charnela arriba (contra el mango), borde festoneado abajo, cóncava a +X
    shell.data.transform(Matrix.Rotation(math.pi, 4, 'Y'))
    shell.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Z'))
    shell.data.transform(Matrix.Translation((0.0, 0.0, -0.62)))
    M.assign(shell, ['M_Stone'])

    def shell_col(co):
        wave = 0.5 + 0.5 * math.cos(math.atan2(co.y, -(co.z + 0.62)) * 11 * 2)
        c = I.lerp3(PAL['shell_out'], PAL['shell_rib'], wave * 0.6)
        return c if co.x < 0.004 else I.lerp3(PAL['shell_in'], PAL['shell_in_rib'], wave * 0.5)
    I.color_fn(shell, shell_col, rnd, 0.02)
    p.add(shell, 'none')
    _cross_lash(p, rnd, (0.0, 0.0, -0.66), 0.03, 0.035, 0.004, turns=3)
    _wrap(p, rnd, -0.60, -0.56, 0.0195, name='WrapHi')
    return p.finish(name)


@_register('estaca')
def _b_estaca(v, rnd, name):
    """Estaca de madera dura con punta tallada y endurecida al fuego."""
    p = K.Parts()
    L, z0 = 0.62, -0.2
    n = 18
    pts = I.crooked(L, rnd, n=n, bend=0.012, wobble=0.004, z0=z0)
    radii = []
    for i in range(n + 1):
        t = i / n
        r = 0.02 * (1 - 0.1 * t)
        if t > 0.72:
            r *= max(0.04, 1 - (t - 0.72) / 0.28) ** 0.9
        radii.append(r)
    o = I.sweep('Stake', pts, radii, segs=9)
    M.assign(o, ['M_Wood'])
    tip_z = z0 + L * 0.72

    def col(co):
        if co.z > tip_z:
            return I.lerp3(PAL['wood_cut'], PAL['char'], ((co.z - tip_z) / (L * 0.28)) ** 0.7)
        if co.z < z0 + 0.01:
            return PAL['wood_cut']
        return I.lerp3(PAL['bark'][0], PAL['bark'][1], 0.5 + 0.5 * math.sin(co.z * 60))
    I.color_fn(o, col, rnd, 0.02)
    p.add(o, 'none')
    return p.finish(name)


@_register('martillo')
def _b_martillo(v, rnd, name):
    """Martillo de canto rodado atado a un mango ahorquillado."""
    p = K.Parts()
    _handle(p, rnd, -0.09, 0.30, 0.016, 0.019, bend=0.015)
    head = C.make_blob('Head', (0.0, 0.0, 0.27), 1.0, v['seed'], subdivisions=3, noise_scale=1.1,
                       noise_strength=0.08, scale=(0.065, 0.043, 0.045), relax_iterations=2)
    M.assign(head, ['M_Stone'])
    base = PAL['cobble'][0]
    I.color_fn(head, lambda co: PAL['quartz'] if abs(co.x * 0.8 + co.z - 0.27 - 0.004) < 0.0045
               else I.lerp3(base, PAL['cobble_light'], 0.5 + 0.5 * math.sin(co.x * 90)), rnd, 0.02)
    p.add(head, 'none')
    _cross_lash(p, rnd, (0.0, 0.0, 0.27), 0.047, 0.05, 0.0038, turns=3)
    _wrap(p, rnd, 0.19, 0.225, 0.018, name='WrapLow')
    return p.finish(name)


@_register('antorcha')
def _b_antorcha(v, rnd, name):
    """Antorcha: palo con haz de fibra de coco y hoja seca empapado en resina."""
    p = K.Parts()
    _handle(p, rnd, -0.14, 0.40, 0.016, 0.014, rgb=PAL['bark'][0])
    # haz de fibra: cuerpo macizo en huso + hebras en relieve por encima
    zs = [0.235, 0.26, 0.30, 0.35, 0.39, 0.42, 0.44]
    rs = [0.017, 0.030, 0.037, 0.038, 0.034, 0.026, 0.012]
    body = I.sweep('Bundle', [(0, 0, z) for z in zs], rs, segs=14)
    M.assign(body, ['M_Fabric'])

    def body_col(co):
        stripe = 0.5 + 0.5 * math.sin(math.atan2(co.y, co.x) * 7 + co.z * 30)
        c = I.lerp3(PAL['straw'], PAL['coir'][1], stripe)
        return I.lerp3(c, PAL['resin'], max(0.0, (co.z - 0.36) / 0.07) ** 0.8)
    I.color_fn(body, body_col, rnd, 0.03)
    p.add(body, 'none')
    k = 12
    for i in range(k):
        a = 2 * math.pi * (i + 0.5) / k + rnd.uniform(-0.12, 0.12)
        prof = [(0.24, 0.018), (0.29, 0.035), (0.34, 0.039), (0.39, 0.035), (0.425, 0.024)]
        pts = [(math.cos(a + z * 1.5) * r, math.sin(a + z * 1.5) * r, z) for z, r in prof]
        s_ = I.sweep(f'Fib{i}', pts, [0.004, 0.006, 0.006, 0.005, 0.003], segs=5)
        M.assign(s_, ['M_Fabric'])
        base = PAL['coir'][i % 3]
        I.color_fn(s_, lambda co, base=base: I.lerp3(base, PAL['resin'], max(0.0, (co.z - 0.36) / 0.06)), rnd, 0.02)
        p.add(s_, 'none')
    # núcleo de resina que asoma arriba
    core = C.make_blob('Pitch', (0, 0, 0.435), 1.0, v['seed'], subdivisions=2, noise_strength=0.15,
                       scale=(0.022, 0.022, 0.02))
    M.assign(core, ['M_Stone'])
    I.tint(core, PAL['pitch'], rnd)
    p.add(core, 'none')
    _wrap(p, rnd, 0.25, 0.275, 0.029, cord_r=0.0035, name='WrapA')
    _wrap(p, rnd, 0.33, 0.35, 0.041, cord_r=0.0035, name='WrapB')
    return p.finish(name)
