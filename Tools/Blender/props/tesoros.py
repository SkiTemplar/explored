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
    # piezas únicas/raras de artifacts.json que antes compartían malla
    dict(name='Treasure_FishHook_Whalebone', seed=3110, builder='hook_whalebone', tri_budget=(200, 4000)),
    dict(name='Treasure_Breastplate_Pearl', seed=3111, builder='breastplate', tri_budget=(200, 4000)),
    dict(name='Treasure_TurtlePendant', seed=3112, builder='turtle_pendant', tri_budget=(200, 4000)),
    dict(name='Treasure_StoneFigure_SkyGazer', seed=3113, builder='figure_skygazer', tri_budget=(300, 6000)),
    dict(name='Treasure_StickChart_Swell', seed=3114, builder='stick_chart_swell', tri_budget=(300, 7000)),
    dict(name='Treasure_Tapa_Stars', seed=3115, builder='tapa_stars', tri_budget=(500, 7000)),
    dict(name='Treasure_Paddle_DoubleCanoe', seed=3116, builder='paddle_double_canoe', tri_budget=(300, 6000)),
]
for _v in VARIANTS:
    _v['group'] = GROUP
    _v['needs_collision'] = True
    _v['interactable'] = True

PAL = {
    'bone': [(0.78, 0.70, 0.52), (0.72, 0.64, 0.46)],
    'pearl': [(0.80, 0.78, 0.70), (0.70, 0.74, 0.72), (0.82, 0.72, 0.66)],
    # nácar de labio negro: irisado cálido (rosa, oro, verde agua) que no
    # se lea blanco-gris en el render
    'nacre': [(0.78, 0.55, 0.50), (0.72, 0.62, 0.34), (0.40, 0.62, 0.58), (0.66, 0.48, 0.62)],
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
def _cloth(name, W, D, nx, ny, pattern, fold=0.0, wave=0.012):
    """Tela tendida con ondulación suave (malla sólida de 4 mm) y color por
    vértice según pattern(u, v) en [0, 1]^2. fold > 0 dobla la esquina +X+Y
    sobre sí misma por una diagonal (fracción de la diagonal), con un rizo
    de 2 cm en el pliegue: tela recién desplegada."""
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=nx, y_segments=ny, size=0.5)
    uv = []
    n = Vector((1 / W, 1 / D, 0)).normalized()
    c0 = (1.0 - fold) / Vector((1 / W, 1 / D)).length
    r = 0.02
    for vv in bm.verts:
        uv.append((vv.co.x + 0.5, vv.co.y + 0.5))
        vv.co.x *= W
        vv.co.y *= D
        vv.co.z = wave * math.sin(vv.co.x * 9.0) * math.cos(vv.co.y * 6.0) + wave
        if fold:
            dist = vv.co.x * n.x + vv.co.y * n.y - c0
            if dist > 0:
                if dist < math.pi * r:
                    along, lift = r * math.sin(dist / r), r * (1 - math.cos(dist / r))
                else:
                    along, lift = -(dist - math.pi * r), 2 * r
                vv.co.x += n.x * (along - dist)
                vv.co.y += n.y * (along - dist)
                vv.co.z = wave + lift
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    mod = o.modifiers.new('Solid', 'SOLIDIFY')
    mod.thickness = 0.004
    C.select_only(o)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier=mod.name)
    M.assign(o, ['M_Fabric'])
    # solidify duplica los vértices en el mismo orden: el color sale de la
    # (u, v) original aunque el pliegue haya movido el vértice
    nv = len(uv)

    def fn(vv):
        c = pattern(*uv[vv.index % nv])
        return (c[0], c[1], c[2], 0.0)
    C.set_vertex_colors(o, fn)
    return o


@_register('tapa')
def _b_tapa(v, rnd, name):
    p = K.Parts()

    def pattern(x, y):
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
        return c
    p.add(_cloth('Tapa', 1.2, 0.85, 30, 22, pattern), 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# PIEZAS ÚNICAS / RARAS (antes compartían malla con la versión común)
# ---------------------------------------------------------------------------
def _ribbon_slab(name, rows, th, z0=0.0):
    """Losa fina a partir de filas de puntos 2D (rows[0] = borde exterior,
    rows[-1] = borde interior, todas con N puntos): cara de arriba y de abajo
    en rejilla, paredes en los dos bordes y tapas en los extremos. Sirve para
    formas cóncavas (media luna) sin n-gonos."""
    bm = bmesh.new()
    top = [[bm.verts.new((x, y, z0 + th)) for x, y in r] for r in rows]
    bot = [[bm.verts.new((x, y, z0)) for x, y in r] for r in rows]
    m, n = len(rows), len(rows[0])
    for a in range(m - 1):
        for i in range(n - 1):
            bm.faces.new((top[a][i], top[a][i + 1], top[a + 1][i + 1], top[a + 1][i]))
            bm.faces.new((bot[a][i], bot[a + 1][i], bot[a + 1][i + 1], bot[a][i + 1]))
    for a in (0, m - 1):
        for i in range(n - 1):
            bm.faces.new((top[a][i], bot[a][i], bot[a][i + 1], top[a][i + 1]))
    for i in (0, n - 1):
        for a in range(m - 1):
            bm.faces.new((top[a][i], top[a + 1][i], bot[a + 1][i], bot[a][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    return o


def _cone_along(name, p0, p1, r0, r1, segs=8):
    return _sweep(name, [p0, p1], [r0, r1], segs=segs)


@_register('hook_whalebone')
def _b_hook_whalebone(v, rnd, name):
    """Anzuelo ceremonial de hueso de ballena: grande (17 cm), sin lengüeta,
    con muescas en la caña, borla roja y una cabecita del pueblo en la
    cabeza del anzuelo (mira hacia arriba cuando descansa tumbado)."""
    p = K.Parts()
    s = 0.17
    bone = (0.82, 0.75, 0.58)
    _hook(p, rnd, 'M_Stone', bone, size=s, barb=False, prefix='W')
    for i in range(3):
        ring = C.make_cylinder(f'Notch{i}', radius=s * 0.074, depth=s * 0.014, segments=10,
                               center=(0, 0, s * (0.42 + 0.09 * i)))
        M.assign(ring, ['M_Stone'])
        _tint(ring, (0.36, 0.26, 0.14), rnd)
        p.add(ring, 'none')
    head = R._head(rnd, v['seed'], 'HH', w=0.8)
    # a 4 cm los dientes de la diadema se leían como dentadura de calavera
    for o in [o for o in head if 'Wave' in o.name]:
        head.remove(o)
        bpy.data.objects.remove(o)
    k = s * 0.26
    for o in head:
        o.data.transform(Matrix.Translation((0, 0, s * 0.96)) @ Matrix.Scale(k, 4) @ Matrix.Rotation(math.pi, 4, 'Z'))
    _stone_objs(head, rnd, bone)
    for o in head:
        p.add(o, 'none')
    # borla de fibra roja y crema que cuelga de la atadura
    for i in range(4):
        a = math.radians(200 + 14 * i)
        p0 = (-s * 0.07, 0, s * 0.84)
        p1 = (p0[0] + math.cos(a) * s * 0.02, (i - 1.5) * s * 0.03, p0[2] - s * 0.1)
        p2 = (p1[0] - s * 0.02, (i - 1.5) * s * 0.05, p0[2] - s * 0.3)
        t = _sweep(f'Tassel{i}', [p0, p1, p2], [s * 0.012, s * 0.011, s * 0.006], segs=5)
        M.assign(t, ['M_Fabric'])
        _tint(t, PAL['red_cord'] if i % 2 == 0 else (0.62, 0.50, 0.32), rnd)
        p.add(t, 'none')
    p.transform(Matrix.Rotation(math.pi / 2, 4, 'X'))
    return p.finish(name)


@_register('breastplate')
def _b_breastplate(v, rnd, name):
    """Pectoral de nácar en media luna (concha de labio negro): borde
    oscuro, bandas irisadas, fila incisa de dientes, colgantes de hueso
    y cordón trenzado para el cuello."""
    p = K.Parts()
    n, rows = 32, []
    RX, RY = 0.15, 0.13
    fr = [0.0, 0.12, 0.35, 0.62, 1.0]
    for f in fr:
        row = []
        for i in range(n):
            a = math.pi + math.pi * i / (n - 1)
            ro = (RX * math.cos(a), RY * math.sin(a))
            ri = ((RX - 0.012) * math.cos(a), RY * 0.4 * math.sin(a))
            row.append((ro[0] + (ri[0] - ro[0]) * f, ro[1] + (ri[1] - ro[1]) * f + 0.03))
        rows.append(row)
    plate = _ribbon_slab('Plate', rows, 0.008)
    M.assign(plate, ['M_Stone'])
    nrow = len(rows) * n

    def col(vv):
        k = vv.index % nrow
        a, i = k // n, k % n
        if a == 0:
            c = (0.12, 0.09, 0.06)
        elif a == 1:
            c = (0.30, 0.20, 0.12)
        elif a == 2:
            c = PAL['tapa_dark'] if i % 2 else PAL['nacre'][1]
        else:
            c = PAL['nacre'][(i // 3 + a) % 4]
        return (c[0], c[1], c[2], 0.0)
    C.set_vertex_colors(plate, col)
    p.add(plate, 'none')
    # colgantes de hueso: dientes que salen del borde inferior
    for j in range(5):
        t = 0.3 + 0.1 * j
        a = math.pi + math.pi * t
        x, y = RX * math.cos(a), RY * math.sin(a) + 0.03
        d = Vector((math.cos(a), math.sin(a), 0)).normalized()
        L = 0.045 if j == 2 else 0.034
        tooth = _cone_along(f'Tooth{j}', (x - d.x * 0.006, y - d.y * 0.006, 0.004),
                            (x + d.x * L, y + d.y * L, 0.004), 0.006, 0.0015, segs=6)
        M.assign(tooth, ['M_Stone'])
        _tint(tooth, PAL['bone'][j % 2], rnd)
        p.add(tooth, 'none')
    # agujeros de las puntas con nudo y cordón que pasa por detrás del cuello
    for sx in (-1, 1):
        knot = C.make_blob(f'Knot{sx}', (sx * (RX - 0.008), 0.03, 0.009), radius=1.0, seed=sx + 5, subdivisions=1,
                           noise_scale=1.0, noise_strength=0.05, scale=(0.009, 0.009, 0.006))
        M.assign(knot, ['M_Fabric'])
        _tint(knot, PAL['red_cord'], rnd)
        p.add(knot, 'none')
    cord = _sweep('Cord', _arc(0, 0.03, RX - 0.008, 0.0, math.pi, 30, z=0.005), 0.0035, segs=6)
    for vv in cord.data.vertices:
        vv.co.y = 0.03 + (vv.co.y - 0.03) * 1.25
    M.assign(cord, ['M_Fabric'])
    C.set_vertex_colors(cord, lambda vv: (PAL['cord'][0] if int(vv.co.x * 120) % 2 else PAL['red_cord']) + (0.0,))
    p.add(cord, 'none')
    return p.finish(name)


def _tortoiseshell(centers):
    amber, dark = (0.58, 0.30, 0.07), (0.13, 0.05, 0.02)

    def fn(vv):
        x, y = vv.co.x, vv.co.y
        ds = sorted(math.hypot(x - cx, y - cy) for cx, cy in centers)
        m = 0.5 + 0.5 * math.sin(x * 260 + math.sin(y * 190) * 2.0) * math.cos(y * 210)
        c = tuple(dark[k] + (amber[k] - dark[k]) * (0.35 + 0.65 * m) for k in range(3))
        if len(ds) > 1 and ds[1] - ds[0] < 0.004:
            c = tuple(v_ * 0.45 for v_ in dark)
        return (c[0], c[1], c[2], 0.0)
    return fn


@_register('turtle_pendant')
def _b_turtle_pendant(v, rnd, name):
    """Colgante de carey en forma de tortuga (honu): caparazón con placas
    y moteado ámbar, aletas, cabeza con ojos y cordón con cuentas."""
    p = K.Parts()
    shell = C.make_blob('Carapace', (0, 0, 0.011), radius=1.0, seed=v['seed'], subdivisions=3, noise_scale=1.0,
                        noise_strength=0.0, scale=(0.034, 0.044, 0.012))
    for vv in shell.data.vertices:
        if vv.co.z < 0.011:
            vv.co.z = 0.011 - (0.011 - vv.co.z) * 0.35  # panza plana
    M.assign(shell, ['M_Stone'])
    scutes = [(0, 0), (0, 0.024), (0, -0.024), (0.02, 0.012), (-0.02, 0.012), (0.02, -0.013), (-0.02, -0.013)]
    C.set_vertex_colors(shell, _tortoiseshell(scutes))
    p.add(shell, 'none')
    skin = (0.42, 0.22, 0.06)
    for i, (x, y, sx, sy, rz) in enumerate([(0.034, 0.018, 0.022, 0.009, -0.5), (-0.034, 0.018, 0.022, 0.009, 0.5),
                                            (0.026, -0.034, 0.013, 0.007, 0.6), (-0.026, -0.034, 0.013, 0.007, -0.6)]):
        f = C.make_blob(f'Flipper{i}', (0, 0, 0), radius=1.0, seed=i, subdivisions=2, noise_scale=1.0,
                        noise_strength=0.0, scale=(sx, sy, 0.004))
        f.data.transform(Matrix.Translation((x, y, 0.006)) @ Matrix.Rotation(rz, 4, 'Z'))
        M.assign(f, ['M_Stone'])
        C.set_vertex_colors(f, _tortoiseshell([(x, y)]))
        p.add(f, 'none')
    head = C.make_blob('Head', (0, 0.053, 0.009), radius=1.0, seed=3, subdivisions=2, noise_scale=1.0,
                       noise_strength=0.0, scale=(0.012, 0.015, 0.008))
    M.assign(head, ['M_Stone'])
    _tint(head, skin, rnd)
    p.add(head, 'none')
    for sx in (-1, 1):
        eye = C.make_blob(f'Eye{sx}', (sx * 0.008, 0.058, 0.013), radius=1.0, seed=4, subdivisions=1,
                          noise_scale=1.0, noise_strength=0.0, scale=(0.0025, 0.0025, 0.002))
        M.assign(eye, ['M_Stone'])
        _tint(eye, PAL['eye'], rnd, 0.0)
        p.add(eye, 'none')
    tail = _cone_along('Tail', (0, -0.04, 0.007), (0, -0.056, 0.004), 0.004, 0.001, segs=6)
    M.assign(tail, ['M_Stone'])
    _tint(tail, skin, rnd)
    p.add(tail, 'none')
    # cordón en anillo que sale de la cabeza, con cuentas de concha y carey
    cord = _sweep('Cord', _arc(0, 0.13, 0.07, -math.pi / 2 + 0.22, 3 * math.pi / 2 - 0.22, 30, z=0.004), 0.0028,
                  segs=6)
    M.assign(cord, ['M_Fabric'])
    _tint(cord, PAL['cord'][1], rnd)
    p.add(cord, 'none')
    for j, s_ in enumerate((-1, 1)):
        for k in range(3):
            a = -math.pi / 2 + s_ * (0.45 + 0.28 * k)
            b = C.make_blob(f'Bead{j}{k}', (math.cos(a) * 0.07, 0.13 + math.sin(a) * 0.07, 0.006), radius=1.0,
                            seed=k, subdivisions=1, noise_scale=1.0, noise_strength=0.0,
                            scale=(0.0065, 0.0065, 0.006))
            M.assign(b, ['M_Stone'])
            _tint(b, PAL['pearl'][k] if k % 2 == 0 else (0.45, 0.22, 0.05), rnd)
            p.add(b, 'none')
    return p.finish(name)


@_register('figure_skygazer')
def _b_figure_skygazer(v, rnd, name):
    """Figura de basalto arrodillada que mira al cielo y alza con las dos
    manos el disco estelar. Viene de una ruina sumergida: algas en la base
    y alguna incrustación de coral."""
    p = K.Parts()
    s = 0.1
    basalt = (0.15, 0.11, 0.08)
    legs = R._rounded_box('Legs', (1.0 * s, 0.95 * s, 0.38 * s), (0, 0.05 * s, 0.19 * s), cuts=2, bulge=0.5,
                          seed=1, noise=0.0)
    body = R._rounded_box('Body', (0.78 * s, 0.6 * s, 0.85 * s), (0, 0, 0.36 * s + 0.4 * s), cuts=2, bulge=0.45,
                          seed=2, noise=0.0)
    head = R._head(rnd, v['seed'], 'H', w=0.85)
    for o in head:
        o.data.transform(Matrix.Translation((0, 0.02 * s, 1.14 * s)) @ Matrix.Rotation(math.radians(-38), 4, 'X')
                         @ Matrix.Scale(s, 4))
    arms = []
    for sx in (-1, 1):
        a = _sweep(f'Arm{sx}', [(sx * 0.34 * s, -0.02 * s, 1.02 * s), (sx * 0.5 * s, -0.16 * s, 1.42 * s),
                                (sx * 0.46 * s, -0.14 * s, 1.95 * s), (sx * 0.3 * s, -0.16 * s, 2.3 * s)],
                   [0.14 * s, 0.12 * s, 0.1 * s, 0.1 * s], segs=10)
        arms.append(a)
    disc = C.make_cylinder('Disc', radius=0.3 * s, depth=0.07 * s, segments=20, center=(0, 0, 0))
    disc_m = Matrix.Translation((0, -0.16 * s, 2.36 * s)) @ Matrix.Rotation(math.radians(80), 4, 'X')
    disc.data.transform(disc_m)
    objs = [legs, body, disc] + head + arms
    _stone_objs(objs, rnd, basalt)
    # rayos del disco en toba clara: la estrella de 8 puntas que alzan
    rays = []
    for i in range(8):
        L = 0.26 * s if i % 2 == 0 else 0.19 * s
        ray = C.make_box(f'Ray{i}', (0.035 * s, L, 0.02 * s), center=(0, L / 2, 0.045 * s))
        ray.data.transform(disc_m @ Matrix.Rotation(i * math.pi / 4, 4, 'Z'))
        M.assign(ray, ['M_Stone'])
        _tint(ray, (0.48, 0.34, 0.20), rnd)
        rays.append(ray)
    # base de basalto con algas
    base = R._rounded_box('Base', (1.25 * s, 1.15 * s, 0.16 * s), (0, 0.02 * s, 0.0), cuts=2, bulge=0.4, seed=3,
                          noise=0.0)
    M.assign(base, ['M_Stone'])
    C.set_vertex_colors(base, lambda vv: ((0.09, 0.12, 0.05) if vv.co.z > 0.03 * s else basalt) + (0.0,))
    for o in objs + rays + [base]:
        o.data.transform(Matrix.Translation((0, 0, 0.08 * s)))
        p.add(o, 'none')
    for i, (x, y, z, r) in enumerate([(0.42, -0.4, 0.2, 0.09), (-0.45, 0.3, 0.22, 0.07), (0.36, 0.2, 0.62, 0.06),
                                      (-0.3, -0.3, 0.9, 0.05), (0.1, 0.52, 0.2, 0.08)]):
        c = C.make_blob(f'Coral{i}', (x * s, y * s, z * s), radius=1.0, seed=i + 40, subdivisions=1,
                        noise_scale=2.0, noise_strength=0.35, scale=(r * s, r * s, r * s * 0.8))
        M.assign(c, ['M_Stone'])
        _tint(c, (0.70, 0.30, 0.22) if i % 2 == 0 else (0.78, 0.52, 0.30), rnd, 0.04)
        p.add(c, 'none')
    return p.finish(name)


@_register('stick_chart_swell')
def _b_stick_chart_swell(v, rnd, name):
    """Carta del oleaje de las siete islas: marco hexagonal, tres ejes que
    se cruzan en el centro, frentes de mar de fondo curvos que atraviesan
    la carta y anillos de rebote alrededor de cada concha-isla."""
    p = K.Parts()
    R_ = 0.48
    r = 0.006
    hexv = [(math.cos(math.pi / 3 * i + math.pi / 6) * R_, math.sin(math.pi / 3 * i + math.pi / 6) * R_)
            for i in range(6)]
    for i in range(6):
        a, b = hexv[i], hexv[(i + 1) % 6]
        p.add(K._rod(f'Frame{i}', (a[0], a[1], r), (b[0], b[1], r), r * 1.2, 'M_Wood', PAL['wood_light'][0], rnd,
                     segs=6), 'none')
    for i in range(3):
        a, b = hexv[i], hexv[i + 3]
        p.add(K._rod(f'Spine{i}', (a[0], a[1], r), (b[0], b[1], r), r, 'M_Wood', PAL['wood_light'][1], rnd,
                     segs=6), 'none')
    z = r + 1.7 * r
    # frentes de mar de fondo: arcos de un centro lejano, recortados al hexágono
    inner = R_ * math.cos(math.pi / 6) - 0.02
    for k, rr in enumerate((0.72, 0.9, 1.08)):
        pts = [(1.0 + math.cos(a) * rr, 0.1 + math.sin(a) * rr, z)
               for a in [math.pi * (0.55 + 0.9 * i / 80) for i in range(81)]]
        pts = [q for q in pts if math.hypot(q[0], q[1]) < inner]
        if len(pts) > 3:
            o = _sweep(f'Front{k}', pts, r * 0.9, segs=6)
            M.assign(o, ['M_Wood'])
            _tint(o, PAL['wood_light'][k % 2], rnd)
            p.add(o, 'none')
    islands = [(0.0, 0.0, 1.2), (0.22, 0.13, 0.9), (-0.21, 0.17, 1.0), (0.12, -0.23, 0.8), (-0.19, -0.16, 0.9),
               (0.31, -0.06, 0.7), (-0.04, 0.3, 0.8)]
    for k, (x, y, sz) in enumerate(islands):
        # anillos de rebote del oleaje (abiertos a sotavento, hacia -X)
        for j, rr in enumerate((0.055, 0.085)):
            o = _sweep(f'Ring{k}{j}', _arc(x, y, rr * sz, -2.2, 2.2, 14, z=z), r * 0.75, segs=6)
            M.assign(o, ['M_Wood'])
            _tint(o, PAL['wood_light'][(k + j) % 2], rnd)
            p.add(o, 'none')
        if k == 0:
            isl = _shell_disc('Island0', 0.03, (x, y, z + 0.004), rnd)
        else:
            isl = C.make_blob(f'Island{k}', (x, y, z + 0.012), radius=1.0, seed=k + 17, subdivisions=2,
                              noise_scale=1.0, noise_strength=0.03, scale=(0.017 * sz, 0.024 * sz, 0.012))
            M.assign(isl, ['M_Stone'])
            _tint(isl, rnd.choice(PAL['cowrie']), rnd)
        p.add(isl, 'none')
        lash = C.make_cylinder(f'Lash{k}', radius=0.02 * sz, depth=0.008, segments=10, center=(x, y, z - 0.002))
        M.assign(lash, ['M_Fabric'])
        _tint(lash, PAL['red_cord'] if k == 0 else PAL['cord'][0], rnd)
        p.add(lash, 'none')
    for i, (x, y) in enumerate(hexv):
        knot = C.make_blob(f'Knot{i}', (x, y, r * 1.5), radius=1.0, seed=i, subdivisions=1, noise_scale=1.0,
                           noise_strength=0.1, scale=(0.016, 0.016, 0.011))
        M.assign(knot, ['M_Fabric'])
        _tint(knot, PAL['cord'][1], rnd)
        p.add(knot, 'none')
    return p.finish(name)


def _star8(u, v, cx, cy, R_):
    dx, dy = (u - cx) * 1.3 / 0.9, v - cy  # la tela es más ancha que alta
    d = math.hypot(dx, dy)
    if d > R_:
        return 0
    a = math.atan2(dy, dx)
    edge = R_ * (0.42 + 0.58 * abs(math.cos(4 * a)) ** 4)
    if d < R_ * 0.22:
        return 2
    return 1 if d < edge else 0


@_register('tapa_stars')
def _b_tapa_stars(v, rnd, name):
    """Tapa de las estrellas: fondo oscuro teñido, estrellas de rumbo de
    8 puntas en crema con centro rojo, orla de triángulos y banda de olas.
    Una esquina doblada sobre sí misma."""
    p = K.Parts()
    ground = (0.12, 0.06, 0.03)
    light = PAL['tapa_base']
    stars = [(0.3, 0.38, 0.11), (0.52, 0.62, 0.14), (0.72, 0.42, 0.10), (0.6, 0.24, 0.07), (0.36, 0.72, 0.08),
             (0.2, 0.58, 0.06), (0.84, 0.66, 0.07)]

    def pattern(x, y):
        if x < 0.05 or x > 0.95 or y < 0.06 or y > 0.94:
            return light
        if x < 0.1 or x > 0.9 or y < 0.12 or y > 0.88:
            t = (x if (y < 0.12 or y > 0.88) else y) * 22
            return PAL['tapa_red'] if (t % 1) < 0.5 else ground
        if y < 0.2:
            return light if abs((y - 0.16) - 0.025 * math.sin(x * 40)) < 0.018 else ground
        for cx, cy, R_ in stars:
            k = _star8(x, y, cx, cy, R_)
            if k:
                return PAL['tapa_red'] if k == 2 else light
        return ground
    p.add(_cloth('Tapa', 1.3, 0.9, 48, 34, pattern, fold=0.2), 'none')
    return p.finish(name)


@_register('paddle_double_canoe')
def _b_paddle_double_canoe(v, rnd, name):
    """Remo de gobierno de la canoa doble (casi 2 m, tumbado): hoja de hoja
    de laurel con nervio central, bandas de dientes, la canoa doble pintada
    en crema y empuñadura en T con ataduras rojas. Salió del agua: un par
    de bellotas de mar en el borde."""
    p = K.Parts()
    wood = (0.22, 0.10, 0.045)
    cream = (0.64, 0.50, 0.30)
    L_shaft, L, W = 1.1, 0.82, 0.12
    r = 0.026
    z = 0.032
    p.add(K._rod('Shaft', (-L_shaft, 0, z), (0.03, 0, z), r, 'M_Wood', wood, rnd, segs=10), 'none')
    grip = R._rounded_box('Grip', (0.06, 0.22, 0.05), (-L_shaft - 0.02, 0, z), cuts=1, bulge=0.6, seed=1, noise=0)
    M.assign(grip, ['M_Wood'])
    _tint(grip, wood, rnd)
    p.add(grip, 'none')
    for i in range(4):
        band = C.make_cylinder(f'Band{i}', radius=r * 1.22, depth=0.02, segments=10, center=(0, 0, 0))
        x = -L_shaft + 0.12 + i * 0.03 if i < 2 else -0.1 - (i - 2) * 0.03
        band.data.transform(Matrix.Translation((x, 0, z)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
        M.assign(band, ['M_Fabric'])
        _tint(band, PAL['red_cord'] if i % 2 == 0 else cream, rnd)
        p.add(band, 'none')
    # hoja: rejilla paramétrica (densa a lo largo) para poder pintar el motivo
    nx, ny = 44, 10
    bm = bmesh.new()
    top, bot = [], []

    def wd(t):
        return W * max(0.12, math.sin(math.pi * (0.1 + 0.86 * t)) ** 0.7)

    def th(t, yn):
        return (0.017 * (1 - 0.55 * t)) * math.sqrt(max(0.0, 1 - yn * yn)) + 0.005 * max(0.0, 1 - abs(yn) * 5)
    for i in range(nx + 1):
        t = i / nx
        x = t * L
        rt, rb = [], []
        for j in range(ny + 1):
            yn = -1 + 2 * j / ny
            y = yn * wd(t)
            h = th(t, yn) + 0.002
            rt.append(bm.verts.new((x, y, z + h)))
            rb.append(bm.verts.new((x, y, z - h)))
        top.append(rt)
        bot.append(rb)
    for i in range(nx):
        for j in range(ny):
            bm.faces.new((top[i][j], top[i + 1][j], top[i + 1][j + 1], top[i][j + 1]))
            bm.faces.new((bot[i][j], bot[i][j + 1], bot[i + 1][j + 1], bot[i + 1][j]))
        for j in (0, ny):
            bm.faces.new((top[i][j], bot[i][j], bot[i + 1][j], top[i + 1][j]))
    for i in (0, nx):
        for j in range(ny):
            bm.faces.new((top[i][j], top[i][j + 1], bot[i][j + 1], bot[i][j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new('Blade')
    bm.to_mesh(me)
    bm.free()
    blade = bpy.data.objects.new('Blade', me)
    C.link_object(blade)
    M.assign(blade, ['M_Wood'])

    def paint(vv):
        t = vv.co.x / L
        yn = vv.co.y / max(1e-6, wd(t))
        c = wood
        if abs(yn) < 0.12:
            c = (0.14, 0.06, 0.03)
        if 0.06 < t < 0.13 or 0.84 < t < 0.9:
            c = PAL['tapa_red'] if ((t * 70) % 2 < 1) ^ (yn > 0) else cream
        # canoa doble vista desde arriba: dos cascos en huso y tres travesaños
        if 0.3 < t < 0.7:
            u = (t - 0.5) / 0.2
            for yc in (-0.5, 0.5):
                if abs(yn - yc) < 0.13 * math.sqrt(max(0.0, 1 - u * u)):
                    c = cream
            if abs(yn) < 0.5 and any(abs(u - q) < 0.07 for q in (-0.5, 0.0, 0.5)):
                c = cream
        if math.hypot((t - 0.78) * L, yn * wd(t)) < 0.018:
            c = cream
        return (c[0], c[1], c[2], 0.0)
    C.set_vertex_colors(blade, paint)
    p.add(blade, 'none')
    for i, (t, sgn) in enumerate([(0.55, 1), (0.62, 1), (0.4, -1)]):
        y = sgn * wd(t) * 0.8
        b = C.make_cylinder(f'Barnacle{i}', radius=0.011, depth=0.012, segments=8,
                            center=(t * L, y, z + th(t, 0.8) + 0.004), radius2=0.005)
        M.assign(b, ['M_Stone'])
        _tint(b, (0.72, 0.66, 0.52), rnd, 0.04)
        p.add(b, 'none')
    return p.finish(name)
