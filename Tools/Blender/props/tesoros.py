"""
tesoros.py — artefactos coleccionables del pueblo navegante (spec §7,
biblia §10) que el jugador expone en la estantería/vitrina de la base:
anzuelos de hueso tallado, adornos de concha, figuras de piedra pequeñas,
carta de navegación de varillas y conchas, remo ceremonial y tapa (tela de
corteza pintada).

Mismo lenguaje que las ruinas (ruinas_polinesias.py): las figuras
reutilizan la cabeza de ojos redondos que mira al cielo de las estatuas,
así el jugador reconoce el "estilo" del pueblo en objetos de 20 cm y de
2,5 m. Pivote en la base, escala real (los anzuelos miden ~10 cm).
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _materials as M  # noqa: E402
import kit_construccion as K  # noqa: E402
import ruinas_polinesias as R  # noqa: E402

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

CATEGORY = 'treasures'
GROUP = 'Tesoros'

VARIANTS = [
    dict(name='Treasure_FishHook_Bone', seed=3101, builder='hook_bone', tri_budget=(100, 2500)),
    dict(name='Treasure_FishHook_Shell', seed=3102, builder='hook_shell', tri_budget=(100, 2500)),
    dict(name='Treasure_ShellPendant', seed=3103, builder='shell_pendant', tri_budget=(100, 3000)),
    dict(name='Treasure_ShellNecklace', seed=3104, builder='shell_necklace', tri_budget=(200, 5000)),
    dict(name='Treasure_StoneFigure_Navigator', seed=3105, builder='figure_navigator', tri_budget=(300, 5000)),
    dict(name='Treasure_StoneFigure_Twins', seed=3106, builder='figure_twins', tri_budget=(300, 6000)),
    dict(name='Treasure_StickChart', seed=3107, builder='stick_chart', tri_budget=(300, 6000)),
    dict(name='Treasure_CeremonialPaddle', seed=3108, builder='paddle', tri_budget=(200, 4000)),
    dict(name='Treasure_Tapa', seed=3109, builder='tapa', tri_budget=(500, 6000)),
]
for _v in VARIANTS:
    _v['group'] = GROUP
    _v['needs_collision'] = True
    _v['interactable'] = True

PAL = {
    'bone': [(0.78, 0.70, 0.52), (0.72, 0.64, 0.46)],
    'pearl': [(0.80, 0.78, 0.70), (0.70, 0.74, 0.72), (0.82, 0.72, 0.66)],
    'cord': [(0.40, 0.24, 0.09), (0.50, 0.30, 0.10)],
    'red_cord': (0.45, 0.08, 0.03),
    'jade_stone': [(0.10, 0.16, 0.12), (0.12, 0.18, 0.13)],
    'hardwood': [(0.20, 0.09, 0.04), (0.24, 0.11, 0.05)],
    'wood_light': [(0.50, 0.33, 0.14), (0.44, 0.28, 0.11)],
    'cowrie': [(0.75, 0.62, 0.42), (0.68, 0.50, 0.30), (0.80, 0.72, 0.56)],
    'tapa_base': (0.70, 0.56, 0.36),
    'tapa_dark': (0.08, 0.05, 0.03),
    'tapa_red': (0.40, 0.10, 0.04),
    'eye': (0.03, 0.03, 0.03),
}

_BUILDERS = {}


def _register(key):
    def deco(fn):
        _BUILDERS[key] = fn
        return fn
    return deco


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    obj = _BUILDERS[variant['builder']](variant, rnd, 'SM_' + variant['name'])
    return K.ground(obj)


def _tint(o, rgb, rnd, j=0.02):
    C.set_vertex_colors(o, C.constant_tint(rgb, jitter=j, rnd=rnd))


def _sweep(name, pts, radii, segs=10, cap=True):
    """Tubo a lo largo de una polilínea 3D (anillos perpendiculares a la
    tangente, con marco transportado para que no se retuerza)."""
    pts = [Vector(p) for p in pts]
    rings = []
    prev_n = None
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        if prev_n is None:
            ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            n = (ref - t * ref.dot(t)).normalized()
        else:
            n = (prev_n - t * prev_n.dot(t)).normalized()
        prev_n = n
        b = t.cross(n)
        r = radii[i] if isinstance(radii, (list, tuple)) else radii
        rings.append([p + (n * math.cos(2 * math.pi * k / segs) + b * math.sin(2 * math.pi * k / segs)) * r
                      for k in range(segs)])
    return C.ring_loft(name, rings, cap_start=cap, cap_end=cap)


def _arc(cx, cy, r, a0, a1, n, z=0.0):
    return [(cx + math.cos(a0 + (a1 - a0) * i / n) * r, cy + math.sin(a0 + (a1 - a0) * i / n) * r, z)
            for i in range(n + 1)]


# ---------------------------------------------------------------------------
# ANZUELOS (en el plano XZ, tumbados luego sobre Y para que apoyen)
# ---------------------------------------------------------------------------
def _hook(p, rnd, mat, rgb, size=0.1, barb=True, prefix='H'):
    s = size
    shank = [(0, 0, s * 0.95), (0, 0, s * 0.35)]
    bend = [(s * 0.22 * (1 - math.cos(a)), 0, s * 0.35 - s * 0.22 * math.sin(a))
            for a in [math.pi * i / 8 for i in range(1, 9)]]
    tip = [(s * 0.44, 0, s * 0.35 + s * 0.12), (s * 0.4, 0, s * 0.35 + s * 0.24)]
    pts = shank + bend + tip
    radii = [s * 0.07] * 2 + [s * 0.075] * len(bend) + [s * 0.05, s * 0.018]
    o = _sweep(f'{prefix}Body', pts, radii, segs=10)
    M.assign(o, [mat])
    _tint(o, rgb, rnd)
    p.add(o, 'none')
    if barb:
        b = _sweep(f'{prefix}Barb', [(s * 0.42, 0, s * 0.52), (s * 0.34, 0, s * 0.5)], [s * 0.03, s * 0.008], segs=6)
        M.assign(b, [mat])
        _tint(b, rgb, rnd)
        p.add(b, 'none')
    # cabeza con atadura de fibra
    for i in range(4):
        z = s * (0.8 + 0.035 * i)
        ring = C.make_cylinder(f'{prefix}Lash{i}', radius=s * 0.085, depth=s * 0.03, segments=10, center=(0, 0, z))
        M.assign(ring, ['M_Fabric'])
        _tint(ring, PAL['red_cord'] if i % 2 else rnd.choice(PAL['cord']), rnd)
        p.add(ring, 'none')
    knob = C.make_blob(f'{prefix}Knob', (0, 0, s * 0.98), radius=1.0, seed=rnd.randint(0, 99), subdivisions=1,
                       noise_scale=1.0, noise_strength=0.05, scale=(s * 0.09, s * 0.09, s * 0.06))
    M.assign(knob, [mat])
    _tint(knob, rgb, rnd)
    p.add(knob, 'none')


@_register('hook_bone')
def _b_hook_bone(v, rnd, name):
    p = K.Parts()
    _hook(p, rnd, 'M_Stone', PAL['bone'][0], size=0.11)
    p.transform(Matrix.Rotation(math.pi / 2, 4, 'X'))  # tumbado: apoya en la mesa/estante
    return p.finish(name)


@_register('hook_shell')
def _b_hook_shell(v, rnd, name):
    """Anzuelo compuesto de nácar (señuelo de bonito) con punta de hueso."""
    p = K.Parts()
    lure = C.make_blob('Lure', (0, 0, 0.07), radius=1.0, seed=v['seed'], subdivisions=2, noise_scale=1.0,
                       noise_strength=0.05, scale=(0.022, 0.012, 0.07))
    M.assign(lure, ['M_Stone'])
    _tint(lure, PAL['pearl'][1], rnd, 0.04)
    p.add(lure, 'none')
    _hook(p, rnd, 'M_Stone', PAL['bone'][1], size=0.06, prefix='P')
    # la punta cuelga del extremo inferior del señuelo
    for o in p.kinds['none'][1:]:
        o.data.transform(Matrix.Translation((0.004, 0, -0.035)))
    for i in range(3):
        f = C.make_cylinder(f'Tassel{i}', radius=0.003, depth=0.05, segments=5,
                            center=(0.006 * (i - 1), 0, 0.15))
        M.assign(f, ['M_Fabric'])
        _tint(f, (0.55, 0.45, 0.30), rnd)
        p.add(f, 'none')
    p.transform(Matrix.Rotation(math.pi / 2, 4, 'X'))
    return p.finish(name)


# ---------------------------------------------------------------------------
# ADORNOS DE CONCHA
# ---------------------------------------------------------------------------
def _shell_disc(name, r, center, rnd, rot=None):
    """Disco de nácar con perforación central (tubo corto) y borde
    ondulado."""
    o = C.make_tube(name, 0.008, 20, [r, r], [r * 0.28, r * 0.28], cap_bottom=True, cap_top=True, z0=0.0)
    for vv in o.data.vertices:
        rr = math.hypot(vv.co.x, vv.co.y)
        if rr > r * 0.5:
            a = math.atan2(vv.co.y, vv.co.x)
            k = 1.0 + 0.06 * math.sin(a * 10)
            vv.co.x *= k
            vv.co.y *= k
    if rot is not None:
        o.data.transform(rot)
    o.data.transform(Matrix.Translation(Vector(center)))
    M.assign(o, ['M_Stone'])
    cols = PAL['pearl']

    def fn(vv):
        a = math.atan2(vv.co.y - center[1], vv.co.x - center[0])
        c = cols[int((a + math.pi) / (2 * math.pi) * 6) % 3]
        return (c[0], c[1], c[2], 0.0)
    C.set_vertex_colors(o, fn)
    return o


@_register('shell_pendant')
def _b_shell_pendant(v, rnd, name):
    p = K.Parts()
    # colgante tumbado: disco grande + cordón en anillo + cuentas
    p.add(_shell_disc('Disc', 0.06, (0, 0, 0.0), rnd), 'none')
    ring = _sweep('Cord', _arc(0, 0.12, 0.1, -math.pi / 2 - 0.2, 3 * math.pi / 2 + 0.2, 28, z=0.006), 0.004, segs=6,
                  cap=True)
    M.assign(ring, ['M_Fabric'])
    _tint(ring, PAL['cord'][0], rnd)
    p.add(ring, 'none')
    for i in range(6):
        a = -math.pi / 2 + 0.5 + i * 0.35
        b = C.make_blob(f'Bead{i}', (math.cos(a) * 0.1, 0.12 + math.sin(a) * 0.1, 0.008), radius=1.0,
                        seed=i, subdivisions=1, noise_scale=1.0, noise_strength=0.0, scale=(0.009, 0.009, 0.009))
        M.assign(b, ['M_Stone'])
        _tint(b, PAL['red_cord'] if i % 2 else PAL['bone'][0], rnd)
        p.add(b, 'none')
    # grabado: pequeña estrella de 4 puntas incisa (color)
    for i in range(4):
        ray = C.make_box(f'Star{i}', (0.004, 0.022, 0.002), center=(0, 0.034, 0.0085))
        ray.data.transform(Matrix.Rotation(i * math.pi / 2, 4, 'Z'))
        M.assign(ray, ['M_Stone'])
        _tint(ray, PAL['tapa_dark'], rnd)
        p.add(ray, 'none')
    return p.finish(name)


@_register('shell_necklace')
def _b_shell_necklace(v, rnd, name):
    p = K.Parts()
    ring = _sweep('Cord', _arc(0, 0, 0.16, 0.0, 2 * math.pi * 0.97, 40, z=0.004), 0.003, segs=6)
    M.assign(ring, ['M_Fabric'])
    _tint(ring, PAL['cord'][1], rnd)
    p.add(ring, 'none')
    n = 15
    for i in range(n):
        a = 2 * math.pi * i / n
        c = (math.cos(a) * 0.16, math.sin(a) * 0.16, 0.0)
        # cauri: blob alargado con surco
        cw = C.make_blob(f'Cowrie{i}', (c[0], c[1], 0.009), radius=1.0, seed=i + v['seed'], subdivisions=2,
                         noise_scale=1.0, noise_strength=0.03, scale=(0.012, 0.018, 0.009))
        cw.data.transform(Matrix.Translation((c[0], c[1], 0)) @ Matrix.Rotation(a, 4, 'Z')
                          @ Matrix.Translation((-c[0], -c[1], 0)))
        M.assign(cw, ['M_Stone'])
        _tint(cw, rnd.choice(PAL['cowrie']), rnd)
        p.add(cw, 'none')
    p.add(_shell_disc('Center', 0.035, (0, -0.2, 0.0), rnd), 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# FIGURAS DE PIEDRA PEQUEÑAS (mismo canon que las estatuas)
# ---------------------------------------------------------------------------
def _stone_objs(objs, rnd, rgb):
    for o in objs:
        M.assign(o, ['M_Stone'])
        if o.name.split('.')[0].endswith(('Eye-1', 'Eye1', 'Mouth')):
            _tint(o, PAL['eye'], rnd, 0.005)
        else:
            _tint(o, rgb, rnd, 0.015)


@_register('figure_navigator')
def _b_figure_navigator(v, rnd, name):
    p = K.Parts()
    s = 0.1  # 1 unidad de estatua ~ 10 cm: figura de ~25 cm
    body = R._rounded_box('Body', (0.8 * s, 0.6 * s, 1.1 * s), (0, 0, 0.55 * s + 0.02), cuts=2, bulge=0.4, seed=1,
                          noise=0.0)
    base = R._rounded_box('Base', (1.0 * s, 0.8 * s, 0.2 * s), (0, 0, 0.1 * s), cuts=1, bulge=0.4, seed=2, noise=0.0)
    head = R._head(rnd, v['seed'], 'H', w=0.9)
    for o in head:
        o.data.transform(Matrix.Translation((0, 0, 1.1 * s + 0.02)) @ Matrix.Rotation(math.radians(-14), 4, 'X')
                         @ Matrix.Scale(s, 4))
    objs = [body, base] + head
    _stone_objs(objs, rnd, rnd.choice(PAL['jade_stone']))
    for o in objs:
        p.add(o, 'none')
    return p.finish(name)


@_register('figure_twins')
def _b_figure_twins(v, rnd, name):
    """Dos figuras espalda con espalda (los gemelos que miran a dos mares)."""
    p = K.Parts()
    s = 0.09
    base = R._rounded_box('Base', (1.2 * s, 1.6 * s, 0.22 * s), (0, 0, 0.11 * s), cuts=1, bulge=0.4, seed=2, noise=0.0)
    objs = [base]
    for k, rot in enumerate((0.0, math.pi)):
        body = R._rounded_box(f'Body{k}', (0.75 * s, 0.55 * s, 1.0 * s), (0, -0.3 * s, 0.5 * s + 0.02), cuts=2,
                              bulge=0.45, seed=k, noise=0.0)
        head = R._head(rnd, v['seed'] + k, f'H{k}', w=0.85)
        for o in head:
            o.data.transform(Matrix.Translation((0, -0.3 * s, 1.0 * s + 0.02)) @ Matrix.Rotation(math.radians(-10), 4, 'X')
                             @ Matrix.Scale(s, 4))
        for o in [body] + head:
            o.data.transform(Matrix.Rotation(rot, 4, 'Z'))
        objs += [body] + head
    _stone_objs(objs, rnd, (0.24, 0.20, 0.17))
    for o in objs:
        p.add(o, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# CARTA DE NAVEGACIÓN DE VARILLAS Y CONCHAS
# ---------------------------------------------------------------------------
@_register('stick_chart')
def _b_stick_chart(v, rnd, name):
    p = K.Parts()
    L = 0.8
    r = 0.006
    z = r
    # marco y rejilla recta
    for i in range(5):
        t = -L / 2 + i * L / 4
        p.add(K._rod(f'H{i}', (-L / 2, t, z), (L / 2, t, z), r, 'M_Wood', PAL['wood_light'][0], rnd, segs=6), 'none')
        p.add(K._rod(f'V{i}', (t, -L / 2, z + 2 * r), (t, L / 2, z + 2 * r), r, 'M_Wood', PAL['wood_light'][1], rnd,
                     segs=6), 'none')
    # varillas curvas: frentes de oleaje que se doblan alrededor de las islas
    for k, (cx, cy, rr, a0, a1) in enumerate([(0.1, -0.05, 0.22, 0.3, 2.6), (0.1, -0.05, 0.32, 0.5, 2.3),
                                              (-0.15, 0.2, 0.18, -2.2, 0.2), (-0.15, 0.2, 0.28, -1.9, -0.2)]):
        pts = _arc(cx, cy, rr, a0, a1, 14, z=z + 4 * r)
        o = _sweep(f'Swell{k}', pts, r * 0.9, segs=6)
        M.assign(o, ['M_Wood'])
        _tint(o, PAL['wood_light'][k % 2], rnd)
        p.add(o, 'none')
    # diagonales
    for k, (a, b) in enumerate([((-0.38, -0.38), (0.38, 0.38)), ((-0.38, 0.38), (0.1, -0.1))]):
        p.add(K._rod(f'D{k}', (a[0], a[1], z + 2 * r), (b[0], b[1], z + 2 * r), r * 0.9, 'M_Wood',
                     PAL['wood_light'][0], rnd, segs=6), 'none')
    # conchas = islas, atadas en los cruces
    for k, (x, y) in enumerate([(0.1, -0.05), (-0.15, 0.2), (0.3, 0.28), (-0.3, -0.25), (0.25, -0.3)]):
        cw = C.make_blob(f'Island{k}', (x, y, z + 6 * r), radius=1.0, seed=k + 7, subdivisions=2, noise_scale=1.0,
                         noise_strength=0.03, scale=(0.02, 0.028, 0.013))
        M.assign(cw, ['M_Stone'])
        _tint(cw, rnd.choice(PAL['cowrie']), rnd)
        p.add(cw, 'none')
        lash = C.make_cylinder(f'Lash{k}', radius=0.012, depth=0.01, segments=8, center=(x, y, z + 3 * r))
        M.assign(lash, ['M_Fabric'])
        _tint(lash, PAL['cord'][0], rnd)
        p.add(lash, 'none')
    # ataduras en los cruces de la rejilla
    for i in range(5):
        for j in range(5):
            if (i + j) % 2:
                continue
            x, y = -L / 2 + i * L / 4, -L / 2 + j * L / 4
            lash = C.make_cylinder(f'Knot{i}{j}', radius=0.01, depth=0.012, segments=6, center=(x, y, z + r))
            M.assign(lash, ['M_Fabric'])
            _tint(lash, PAL['cord'][1], rnd)
            p.add(lash, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# REMO CEREMONIAL (tumbado; hoja tallada con dientes y ojo estelar)
# ---------------------------------------------------------------------------
@_register('paddle')
def _b_paddle(v, rnd, name):
    p = K.Parts()
    L_shaft, L_blade = 1.0, 0.62
    r = 0.022
    z = 0.03
    wood = PAL['hardwood'][0]
    p.add(K._rod('Shaft', (-L_shaft, 0, z), (0.02, 0, z), r, 'M_Wood', wood, rnd, segs=10), 'none')
    # pomo con cabecita tallada
    knob = R._rounded_box('Knob', (0.07, 0.06, 0.06), (-L_shaft - 0.03, 0, z), cuts=1, bulge=0.6, seed=1, noise=0)
    M.assign(knob, ['M_Wood'])
    _tint(knob, wood, rnd)
    p.add(knob, 'none')
    # hoja: elipse aplanada con quilla central
    blade = C.make_blob('Blade', (L_blade / 2, 0, z), radius=1.0, seed=v['seed'], subdivisions=3, noise_scale=1.0,
                        noise_strength=0.0, scale=(L_blade / 2, 0.11, 0.014))
    for vv in blade.data.vertices:
        t = (vv.co.x) / L_blade
        vv.co.y *= 0.6 + 0.6 * math.sin(math.pi * min(1.0, max(0.0, t)) * 0.8 + 0.3)
    blade.data.update()
    M.assign(blade, ['M_Wood'])

    def carve(vv):
        # bandas de dientes (triángulos) y ojo estelar pintado cerca de la punta
        x, y = vv.co.x, vv.co.y
        c = wood
        if 0.08 < x < 0.14 or 0.44 < x < 0.5:
            c = PAL['tapa_red'] if ((x * 60) % 2 < 1) ^ (y > 0) else (0.55, 0.42, 0.25)
        if math.hypot(x - 0.3, y) < 0.035:
            c = (0.55, 0.42, 0.25)
        return (c[0], c[1], c[2], 0.0)
    C.set_vertex_colors(blade, carve)
    p.add(blade, 'none')
    for i in range(3):
        band = C.make_cylinder(f'Band{i}', radius=r * 1.25, depth=0.018, segments=10, center=(0, 0, 0))
        band.data.transform(Matrix.Translation((-0.08 - i * 0.03, 0, z)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
        M.assign(band, ['M_Fabric'])
        _tint(band, PAL['red_cord'] if i != 1 else PAL['bone'][0], rnd)
        p.add(band, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# TAPA: tela de corteza pintada, con ondulaciones y patrón geométrico
# ---------------------------------------------------------------------------
@_register('tapa')
def _b_tapa(v, rnd, name):
    p = K.Parts()
    W, D = 1.2, 0.85
    nx, ny = 30, 22
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=nx, y_segments=ny, size=0.5)
    for vv in bm.verts:
        vv.co.x *= W
        vv.co.y *= D
        vv.co.z = 0.012 * math.sin(vv.co.x * 9.0) * math.cos(vv.co.y * 6.0) + 0.012
    me = bpy.data.meshes.new('Tapa')
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new('Tapa', me)
    C.link_object(o)
    mod = o.modifiers.new('Solid', 'SOLIDIFY')
    mod.thickness = 0.004
    C.select_only(o)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier=mod.name)
    M.assign(o, ['M_Fabric'])

    def pattern(vv):
        x, y = vv.co.x / W + 0.5, vv.co.y / D + 0.5
        c = PAL['tapa_base']
        # marco: franja oscura y dientes rojos
        if x < 0.06 or x > 0.94 or y < 0.07 or y > 0.93:
            c = PAL['tapa_dark']
        elif x < 0.12 or x > 0.88 or y < 0.14 or y > 0.86:
            c = PAL['tapa_red'] if int((x + y) * 20) % 2 else PAL['tapa_base']
        else:
            # rombos centrales (estrellas de rumbo) en rejilla 3 x 2
            gx, gy = (x - 0.12) / 0.76 * 3, (y - 0.14) / 0.72 * 2
            fx, fy = gx % 1 - 0.5, gy % 1 - 0.5
            d = abs(fx) + abs(fy)
            if d < 0.18:
                c = PAL['tapa_red']
            elif d < 0.32:
                c = PAL['tapa_dark']
            elif abs(fx) < 0.04 or abs(fy) < 0.04:
                c = PAL['tapa_dark']
        return (c[0], c[1], c[2], 0.0)
    C.set_vertex_colors(o, pattern)
    p.add(o, 'none')
    return p.finish(name)
