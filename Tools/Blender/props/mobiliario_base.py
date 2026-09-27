"""
mobiliario_base.py — mobiliario y estructuras útiles de la base del jugador
(spec §8.6 y biblia §3.10): cama, mesa de trabajo, estantería de exposición
con huecos y vitrina (museo de tesoros, spec §7), depósito de agua de
lluvia, secadero, ahumadero, muelle (tramo y final) y parcela de huerto con
bordes (troncos o piedras).

Reutiliza las primitivas y paletas del kit modular (kit_construccion.py)
para que el mobiliario comparta materiales, biseles y tonos con las
paredes y suelos donde se va a colocar. Pivote en la base, escala real.
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _materials as M  # noqa: E402
import _shapes as S  # noqa: E402
import kit_construccion as K  # noqa: E402

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix  # noqa: E402

CATEGORY = 'base_furniture'
GROUP = 'MobiliarioBase'

VARIANTS = [
    dict(name='Base_Bed', seed=2801, builder='bed', tri_budget=(200, 5000)),
    dict(name='Base_Workbench', seed=2802, builder='workbench', tri_budget=(200, 5000)),
    dict(name='Base_DisplayShelf', seed=2803, builder='display_shelf', tri_budget=(200, 5000),
         collision_complex=True, interactable=True),
    dict(name='Base_DisplayCase', seed=2804, builder='display_case', tri_budget=(200, 4000),
         interactable=True),
    dict(name='Base_RainCollector', seed=2805, builder='rain_collector', tri_budget=(200, 5000),
         interactable=True),
    dict(name='Base_DryingRack', seed=2806, builder='drying_rack', tri_budget=(200, 6000),
         interactable=True),
    dict(name='Base_Smokehouse', seed=2807, builder='smokehouse', tri_budget=(500, 12000),
         collision_complex=True, interactable=True),
    dict(name='Base_Dock', seed=2808, builder='dock', tri_budget=(300, 6000),
         collision_complex=True),
    dict(name='Base_DockEnd', seed=2809, builder='dock_end', tri_budget=(300, 7000),
         collision_complex=True),
    dict(name='Base_GardenPlot_Logs', seed=2810, builder='plot_logs', tri_budget=(200, 6000),
         interactable=True),
    dict(name='Base_GardenPlot_Stones', seed=2811, builder='plot_stones', tri_budget=(200, 7000),
         interactable=True),
    dict(name='Base_MuseumPanel', seed=2812, builder='museum_panel', tri_budget=(200, 6000),
         interactable=True),
]
for _v in VARIANTS:
    _v.setdefault('needs_collision', True)
    _v['group'] = GROUP

PAL = dict(K.PAL)
PAL.update({
    'soil': [(0.13, 0.07, 0.03), (0.16, 0.09, 0.04), (0.11, 0.06, 0.03)],
    'sprout': [(0.16, 0.38, 0.05), (0.22, 0.45, 0.06), (0.12, 0.32, 0.04)],
    'fish': (0.62, 0.30, 0.12),       # carne abierta y curada, naranja
    'fish_back': (0.26, 0.24, 0.26),  # piel plateada oscura
    'fabric': [(0.62, 0.52, 0.36), (0.55, 0.45, 0.30)],
    'tapa': [(0.42, 0.14, 0.05), (0.66, 0.54, 0.36), (0.18, 0.08, 0.04)],
    'wet_wood': (0.16, 0.12, 0.07),
    'algae': (0.07, 0.10, 0.05),
    'char': (0.04, 0.035, 0.03),
})

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


def _pick(rnd, key):
    return rnd.choice(PAL[key])


def _blob(name, center, half, mat, rgb, rnd, seed, noise=0.18, subdiv=2, jitter=0.03):
    o = C.make_blob(name, center, radius=1.0, seed=seed, subdivisions=subdiv, noise_scale=1.4,
                    noise_strength=noise, scale=half, relax_iterations=1)
    M.assign(o, [mat])
    C.set_vertex_colors(o, C.constant_tint(rgb, alpha=0.0, jitter=jitter, rnd=rnd))
    return o


def _fish(p, rnd, name, top, length=0.3, subdiv=2):
    """Pescado abierto colgado (o tumbado si se rota después): blob
    aplanado con lomo oscuro + cordel hasta `top`."""
    x, y, z = top
    body_c = (x, y, z - 0.04 - length / 2)
    # cara plana hacia ±Y: las varas corren en X y el pescado se ve de
    # frente (la primera versión lo colgaba de canto y era una raya)
    o = C.make_blob(name, body_c, radius=1.0, seed=rnd.randint(0, 9999), subdivisions=subdiv,
                    noise_scale=1.2, noise_strength=0.08, scale=(length * 0.24, 0.03, length / 2),
                    relax_iterations=1)
    M.assign(o, ['M_Leaf'])
    C.set_vertex_colors(o, S.weathered_tint(PAL['fish'], PAL['fish_back'], seed=rnd.random() * 9,
                                            patchiness=6.0, wear_amount=0.5, jitter=0.02, rnd=rnd))
    p.add(o, 'none')
    p.add(K._rod(name + 'Str', (x, y, z - 0.05), (x, y, z + 0.01), 0.005, 'M_Fabric',
                 PAL['lashing'], rnd, segs=6), 'none')


# ---------------------------------------------------------------------------
# Cama (bastidor de madera, somier de cañas, colchón de hoja, tapa doblada)
# ---------------------------------------------------------------------------
@_register('bed')
def _b_bed(v, rnd, name):
    p = K.Parts()
    L, W, leg_h = 2.0, 1.0, 0.36
    for i, (x, y) in enumerate([(-L / 2 + 0.05, -W / 2 + 0.05), (L / 2 - 0.05, -W / 2 + 0.05),
                                (-L / 2 + 0.05, W / 2 - 0.05), (L / 2 - 0.05, W / 2 - 0.05)]):
        p.add(K._box(f'Leg{i}', (0.09, 0.09, leg_h + 0.04), (x, y, (leg_h + 0.04) / 2), 'M_Wood',
                     _pick(rnd, 'wood_dark'), rnd), 'wood')
    for i, y in enumerate((-W / 2 + 0.05, W / 2 - 0.05)):
        p.add(K._box(f'Side{i}', (L, 0.08, 0.13), (0, y, leg_h - 0.05), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    for i, x in enumerate((-L / 2 + 0.05, L / 2 - 0.05)):
        p.add(K._box(f'End{i}', (0.08, W, 0.13), (x, 0, leg_h - 0.05), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    for i in range(9):
        x = -L / 2 + 0.16 + i * (L - 0.32) / 8
        p.add(K._bamboo(f'Slat{i}', (x, -W / 2 + 0.07, leg_h + 0.01), (x, W / 2 - 0.07, leg_h + 0.01), 0.025, rnd,
                        segs=8, node_step=0.45), 'pole')
    # cabecero de cañas
    for i in range(6):
        y = -W / 2 + 0.1 + i * (W - 0.2) / 5
        p.add(K._bamboo(f'Head{i}', (-L / 2 - 0.02, y, 0.0), (-L / 2 - 0.02, y, 1.0), 0.032, rnd, segs=8,
                        node_step=0.35), 'pole')
    p.add(K._bamboo('HeadBar', (-L / 2 + 0.04, -W / 2, 0.92), (-L / 2 + 0.04, W / 2, 0.92), 0.03, rnd, segs=8), 'pole')
    top = leg_h + 0.035
    p.add(_blob('Mattress', (0.02, 0, top + 0.11), (0.94, 0.45, 0.12), 'M_Leaf', PAL['thatch'][0], rnd,
                v['seed'], noise=0.1, subdiv=3), 'none')
    p.add(_blob('Pillow', (-0.72, 0, top + 0.27), (0.16, 0.34, 0.08), 'M_Fabric', PAL['fabric'][0], rnd,
                v['seed'] + 1, noise=0.12), 'none')
    # tapa doblada a los pies: una caja por franja (una caja sola con color
    # por vértice solo tiene 8 vértices y las franjas salían en degradado)
    stripes = PAL['tapa'] + PAL['tapa'][:2]
    sw = 0.55 / len(stripes)
    for i, col in enumerate(stripes):
        b = K._box(f'Blanket{i}', (sw + 0.002, 0.96, 0.05), (0.28 + sw * (i + 0.5), 0, top + 0.235), 'M_Fabric',
                   col, rnd, jitter=0.015)
        p.add(b, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Mesa de trabajo (tablero grueso, patas de rollizo, balda, herramientas)
# ---------------------------------------------------------------------------
@_register('workbench')
def _b_workbench(v, rnd, name):
    p = K.Parts()
    Lx, Wy, H = 1.6, 0.8, 0.9
    for i, y in enumerate((-0.2, 0.2)):
        p.add(K._box(f'Top{i}', (Lx, 0.39, 0.08), (0, y, H - 0.04), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    for i, (x, y) in enumerate([(-0.68, -0.3), (0.68, -0.3), (-0.68, 0.3), (0.68, 0.3)]):
        p.add(K._rod(f'Leg{i}', (x, y, 0), (x, y, H - 0.08), 0.055, 'M_Wood', _pick(rnd, 'branch'), rnd,
                     taper=0.92), 'pole')
    for i, y in enumerate((-0.3, 0.3)):
        p.add(K._rod(f'Str{i}', (-0.72, y, 0.22), (0.72, y, 0.22), 0.035, 'M_Wood', _pick(rnd, 'branch'), rnd), 'pole')
    p.add(K._box('Shelf', (1.36, 0.64, 0.04), (0, 0, 0.275), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    # leña en la balda
    for i in range(3):
        y = -0.18 + i * 0.16
        p.add(K._rod(f'Log{i}', (-0.45, y, 0.295 + 0.06), (0.4, y, 0.295 + 0.06), 0.06, 'M_Wood',
                     _pick(rnd, 'branch'), rnd, segs=10), 'pole')
    # mazo de piedra y mango, bloque de tallar y cuenco de coco
    p.add(K._rod('Handle', (0.1, -0.15, H + 0.02), (0.5, -0.05, H + 0.02), 0.018, 'M_Wood', _pick(rnd, 'wood'), rnd,
                 segs=8), 'pole')
    p.add(_blob('MalletHead', (0.08, -0.155, H + 0.055), (0.07, 0.06, 0.055), 'M_Stone', _pick(rnd, 'stone'), rnd,
                v['seed'], noise=0.15, subdiv=1), 'none')
    p.add(K._box('Block', (0.3, 0.22, 0.14), (-0.45, 0.1, H + 0.07), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
    bowl = C.make_tube('Bowl', 0.07, 12, [0.07, 0.1, 0.11], [0.055, 0.088, 0.1], cap_bottom=True, cap_top=True,
                       z0=H)
    bowl.data.transform(Matrix.Translation((0.2, 0.2, 0)))
    M.assign(bowl, ['M_Wood'])
    C.set_vertex_colors(bowl, C.constant_tint((0.22, 0.12, 0.05), jitter=0.03, rnd=rnd))
    p.add(bowl, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Estantería de exposición: 3 columnas x 4 alturas de huecos para tesoros
# ---------------------------------------------------------------------------
@_register('display_shelf')
def _b_display_shelf(v, rnd, name):
    p = K.Parts()
    Wx, D, H = 1.8, 0.42, 1.95
    base_h = 0.1
    p.add(K._box('Plinth', (Wx, D - 0.02, base_h), (0, 0, base_h / 2), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
    for i, x in enumerate((-Wx / 2 + 0.025, -Wx / 6, Wx / 6, Wx / 2 - 0.025)):
        p.add(K._box(f'Up{i}', (0.05, D, H - base_h), (x, 0, base_h + (H - base_h) / 2), 'M_Wood',
                     _pick(rnd, 'wood'), rnd), 'wood')
    levels = 4
    step = (H - base_h - 0.04) / levels
    for i in range(levels + 1):
        z = base_h + i * step + 0.02
        p.add(K._box(f'Shelf{i}', (Wx - 0.05, D - 0.03, 0.04), (0, 0, z), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    p.add(K._box('Cornice', (Wx + 0.1, D + 0.06, 0.06), (0, 0, H + 0.03), 'M_Wood', _pick(rnd, 'wood_dark'), rnd),
          'wood')
    # fondo de estera de palma trenzada (resalta los objetos expuestos)
    for j in range(levels):
        z0 = base_h + j * step + 0.04
        s = K._fringe_slab(f'Back{j}', -Wx / 2 + 0.05, Wx / 2 - 0.05, z0, z0 + step - 0.02, 0.015, rnd,
                           PAL['thatch'][j % 3], fringe=0.0, cells=6)
        s.data.transform(Matrix.Translation((0, D / 2 - 0.005, 0)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
        p.add(s, 'none')
    # cresta tallada sobre la cornisa (dientes: motivo navegante)
    for i in range(9):
        x = -Wx / 2 + 0.1 + i * (Wx - 0.2) / 8
        p.add(K._box(f'Tooth{i}', (0.08, 0.05, 0.08), (x, -D / 2 + 0.02, H + 0.1), 'M_Wood',
                     _pick(rnd, 'wood_dark'), rnd), 'wood')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Panel de pared del museo: bastidor de 2 m (un lado de celda) apoyado contra
# la pared, con estera de fondo y dos ganchos para colgar UN tesoro grande
# (remo, tapa, carta). El hueco de artifacts.json va a (0, -6, 110) cm.
# ---------------------------------------------------------------------------
@_register('museum_panel')
def _b_museum_panel(v, rnd, name):
    p = K.Parts()
    Wx, H = 1.9, 2.05
    z0, z1 = 0.42, 1.8
    for i, x in enumerate((-Wx / 2, Wx / 2)):
        p.add(K._box(f'Post{i}', (0.09, 0.09, H), (x, 0.0, H / 2), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
        p.add(K._box(f'Foot{i}', (0.16, 0.2, 0.06), (x, -0.02, 0.03), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
    for i, z in enumerate((z0 - 0.03, z1 + 0.03)):
        p.add(K._box(f'Rail{i}', (Wx, 0.08, 0.07), (0, 0.0, z), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    # tablero de fondo (tablas verticales) y estera trenzada delante
    n = 7
    for i in range(n):
        x = -Wx / 2 + 0.045 + (i + 0.5) * (Wx - 0.09) / n
        p.add(K._box(f'Board{i}', ((Wx - 0.09) / n - 0.006, 0.03, z1 - z0), (x, 0.02, (z0 + z1) / 2), 'M_Wood',
                     _pick(rnd, 'wood'), rnd), 'wood')
    # estera de palma trenzada en damero (tiras claras y oscuras alternas,
    # con el relieve del trenzado en la propia malla)
    mx0, mx1, mz0, mz1 = -Wx / 2 + 0.07, Wx / 2 - 0.07, z0 + 0.03, z1 - 0.03
    nx, nz = 30, 20
    bm = bmesh.new()
    grid = [[bm.verts.new((mx0 + (mx1 - mx0) * i / nx, 0.0, mz0 + (mz1 - mz0) * j / nz)) for j in range(nz + 1)]
            for i in range(nx + 1)]
    for i in range(nx):
        for j in range(nz):
            bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
    for i in range(nx + 1):
        for j in range(nz + 1):
            grid[i][j].co.y = -0.006 * (1 + math.sin(i * math.pi / 2) * math.sin(j * math.pi / 2))
    me = bpy.data.meshes.new('Mat')
    bm.to_mesh(me)
    bm.free()
    mat = bpy.data.objects.new('Mat', me)
    C.link_object(mat)
    mod = mat.modifiers.new('Solid', 'SOLIDIFY')
    mod.thickness = 0.008
    C.select_only(mat)
    bpy.context.view_layer.objects.active = mat
    bpy.ops.object.modifier_apply(modifier=mod.name)
    M.assign(mat, ['M_Leaf'])
    cell_w, cell_h = (mx1 - mx0) / (nx / 2), (mz1 - mz0) / (nz / 2)

    def weave(vv):
        ci, cj = int((vv.co.x - mx0) / cell_w + 1e-4), int((vv.co.z - mz0) / cell_h + 1e-4)
        c = PAL['thatch'][2] if (ci + cj) % 2 else PAL['thatch'][1]
        if (ci // 3 + cj // 3) % 2 and (ci + cj) % 2 == 0:
            c = PAL['tapa'][0]  # rombos rojos teñidos en el trenzado
        return (c[0], c[1], c[2], 0.0)
    C.set_vertex_colors(mat, weave)
    p.add(mat, 'none')
    # ganchos: tacos de madera que salen y se levantan en la punta
    for sx in (-1, 1):
        x = sx * 0.55
        p.add(K._rod(f'Peg{sx}', (x, 0.0, 1.06), (x, -0.13, 1.06), 0.026, 'M_Wood', _pick(rnd, 'wood_dark'), rnd,
                     segs=8), 'pole')
        p.add(K._rod(f'PegTip{sx}', (x, -0.13, 1.04), (x, -0.14, 1.14), 0.024, 'M_Wood', _pick(rnd, 'wood_dark'),
                     rnd, segs=8), 'pole')
        for k in range(3):
            ring = C.make_cylinder(f'PegLash{sx}{k}', radius=0.032, depth=0.014, segments=8, center=(0, 0, 0))
            ring.data.transform(Matrix.Translation((x, -0.02 - k * 0.014, 1.06)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
            M.assign(ring, ['M_Fabric'])
            C.set_vertex_colors(ring, C.constant_tint(PAL['tapa'][0] if k % 2 else PAL['fabric'][0], jitter=0.02,
                                                      rnd=rnd))
            p.add(ring, 'none')
    # cresta: dientes tallados y disco estelar en el centro (motivo navegante)
    p.add(K._box('Cap', (Wx + 0.14, 0.12, 0.06), (0, 0.0, H + 0.03), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
    for i in range(10):
        x = -Wx / 2 + 0.08 + i * (Wx - 0.16) / 9
        if abs(x) < 0.2:
            continue
        tooth = C.make_cylinder(f'Tooth{i}', radius=0.05, depth=0.1, segments=4, center=(x, -0.02, H + 0.11),
                                radius2=0.004)
        tooth.data.transform(Matrix.Translation((x, 0, 0)) @ Matrix.Rotation(math.pi / 4, 4, 'Z')
                             @ Matrix.Translation((-x, 0, 0)))
        M.assign(tooth, ['M_Wood'])
        K._tint(tooth, _pick(rnd, 'wood'), rnd)
        p.add(tooth, 'none')
    star_m = Matrix.Translation((0, -0.02, H + 0.2)) @ Matrix.Rotation(math.pi / 2, 4, 'X')
    disc = C.make_cylinder('Star', radius=0.15, depth=0.04, segments=20, center=(0, 0, 0))
    disc.data.transform(star_m)
    M.assign(disc, ['M_Wood'])
    K._tint(disc, _pick(rnd, 'wood_dark'), rnd)
    for i in range(8):
        L = 0.13 if i % 2 == 0 else 0.095
        ray = C.make_box(f'Ray{i}', (0.022, L, 0.014), center=(0, L / 2, 0.024))
        ray.data.transform(star_m @ Matrix.Rotation(i * math.pi / 4, 4, 'Z'))
        M.assign(ray, ['M_Wood'])
        K._tint(ray, PAL['tapa'][1], rnd)
        p.add(ray, 'none')
    eye = C.make_cylinder('StarEye', radius=0.035, depth=0.02, segments=12, center=(0, 0, 0.03))
    eye.data.transform(star_m)
    M.assign(eye, ['M_Fabric'])
    K._tint(eye, PAL['tapa'][0], rnd)
    p.add(eye, 'soft')
    p.add(disc, 'soft')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Vitrina: mueble bajo + urna de cristal recuperado del Albatros
# ---------------------------------------------------------------------------
@_register('display_case')
def _b_display_case(v, rnd, name):
    p = K.Parts()
    Wx, D, H = 1.0, 0.6, 0.85
    p.add(K._box('Body', (Wx, D, H - 0.1), (0, 0, 0.1 + (H - 0.1) / 2), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    for i, (x, y) in enumerate([(-Wx / 2 + 0.05, -D / 2 + 0.05), (Wx / 2 - 0.05, -D / 2 + 0.05),
                                (-Wx / 2 + 0.05, D / 2 - 0.05), (Wx / 2 - 0.05, D / 2 - 0.05)]):
        p.add(K._box(f'Foot{i}', (0.1, 0.1, 0.12), (x, y, 0.06), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
    p.add(K._box('Top', (Wx + 0.06, D + 0.06, 0.05), (0, 0, H + 0.025), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
    for i, z in enumerate((0.35, 0.62)):
        p.add(K._box(f'Panel{i}', (Wx - 0.16, 0.02, 0.2), (0, -D / 2 - 0.005, z), 'M_Wood',
                     _pick(rnd, 'wood_dark'), rnd), 'wood')
    gz0, gh = H + 0.05, 0.42
    gw, gd = Wx - 0.1, D - 0.1
    # aristas del marco de la urna
    for sx in (-1, 1):
        for sy in (-1, 1):
            p.add(K._box(f'FrV{sx}{sy}', (0.035, 0.035, gh), (sx * gw / 2, sy * gd / 2, gz0 + gh / 2), 'M_Wood',
                         _pick(rnd, 'wood_dark'), rnd), 'wood')
    for sy in (-1, 1):
        for zz in (gz0 + 0.0175, gz0 + gh - 0.0175):
            p.add(K._box(f'FrX{sy}{zz:.2f}', (gw, 0.035, 0.035), (0, sy * gd / 2, zz), 'M_Wood',
                         _pick(rnd, 'wood_dark'), rnd), 'wood')
    for sx in (-1, 1):
        for zz in (gz0 + 0.0175, gz0 + gh - 0.0175):
            p.add(K._box(f'FrY{sx}{zz:.2f}', (0.035, gd, 0.035), (sx * gw / 2, 0, zz), 'M_Wood',
                         _pick(rnd, 'wood_dark'), rnd), 'wood')
    glass = C.make_box('Glass', (gw - 0.01, gd - 0.01, gh - 0.01), center=(0, 0, gz0 + gh / 2))
    M.assign(glass, ['M_Glass'])
    C.set_vertex_colors(glass, C.constant_tint((0.75, 0.9, 0.85), jitter=0.0))
    p.add(glass, 'none')
    p.add(_blob('Cushion', (0, 0, gz0 + 0.035), (0.3, 0.18, 0.035), 'M_Fabric', PAL['tapa'][0], rnd,
                v['seed'], noise=0.05, subdiv=2), 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Depósito de agua de lluvia: barril de duelas sobre plataforma + embudo
# de hoja de palma
# ---------------------------------------------------------------------------
@_register('rain_collector')
def _b_rain_collector(v, rnd, name):
    p = K.Parts()
    st_h = 0.55
    for i, (x, y) in enumerate([(-0.38, -0.38), (0.38, -0.38), (-0.38, 0.38), (0.38, 0.38)]):
        p.add(K._rod(f'Leg{i}', (x, y, 0), (x, y, st_h - 0.06), 0.05, 'M_Wood', _pick(rnd, 'branch'), rnd), 'pole')
    for i in range(5):
        y = -0.4 + i * 0.2
        p.add(K._box(f'Deck{i}', (0.95, 0.18, 0.06), (0, y, st_h - 0.03), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    bh = 0.95
    rs = [0.40, 0.44, 0.46, 0.44, 0.40]
    barrel = C.make_tube('Barrel', bh, 16, rs, [r - 0.035 for r in rs], cap_bottom=True, cap_top=True, z0=st_h)
    M.assign(barrel, ['M_Wood'])
    cols = [_pick(rnd, 'wood') for _ in range(16)]

    def stave(vv):
        a = math.atan2(vv.co.y, vv.co.x)
        return tuple(cols[int((a + math.pi) / (2 * math.pi) * 16) % 16]) + (0.0,)
    C.set_vertex_colors(barrel, stave)
    p.add(barrel, 'none')
    # tapa interior (agua no: fondo cerrado a media altura para leer lleno)
    water = C.make_cylinder('Water', radius=0.39, depth=0.02, segments=16, center=(0, 0, st_h + bh - 0.12))
    M.assign(water, ['M_Glass'])
    C.set_vertex_colors(water, C.constant_tint((0.2, 0.45, 0.5), jitter=0.0))
    p.add(water, 'none')
    for i, (zf, r) in enumerate([(0.12, 0.425), (0.5, 0.465), (0.88, 0.425)]):
        hoop = C.make_tube(f'Hoop{i}', 0.05, 16, [r + 0.012, r + 0.012], [r - 0.01, r - 0.01], cap_bottom=True,
                           cap_top=True, z0=st_h + bh * zf - 0.025)
        M.assign(hoop, ['M_Wood'])
        C.set_vertex_colors(hoop, C.constant_tint(PAL['lashing'], jitter=0.02, rnd=rnd))
        p.add(hoop, 'none')
    # embudo de hoja de palma apoyado en el borde
    fz = st_h + bh - 0.02
    funnel = C.make_tube('Funnel', 0.32, 12, [0.3, 0.52, 0.72], [0.28, 0.5, 0.7], cap_bottom=True, cap_top=True,
                         z0=fz)
    M.assign(funnel, ['M_Leaf'])
    lcols = [_pick(rnd, 'thatch') for _ in range(12)]

    def leaf(vv):
        a = math.atan2(vv.co.y, vv.co.x)
        c = lcols[int((a + math.pi) / (2 * math.pi) * 12) % 12]
        f = 0.8 + 0.3 * min(1.0, (vv.co.z - fz) / 0.32)
        return (c[0] * f, c[1] * f, c[2] * f, 0.0)
    C.set_vertex_colors(funnel, leaf)
    p.add(funnel, 'none')
    # grifo de caña
    p.add(K._bamboo('Spout', (0.4, 0, st_h + 0.14), (0.62, 0, st_h + 0.1), 0.028, rnd, segs=8), 'pole')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Secadero: dos caballetes en A, cumbrera y varas con pescado abierto
# ---------------------------------------------------------------------------
@_register('drying_rack')
def _b_drying_rack(v, rnd, name):
    p = K.Parts()
    top = 1.6
    spread = 0.65
    for i, x in enumerate((-1.0, 1.0)):
        for k, sy in enumerate((-1, 1)):
            p.add(K._rod(f'A{i}{k}', (x, sy * spread, 0), (x, -sy * 0.06, top + 0.1), 0.04, 'M_Wood',
                         _pick(rnd, 'branch'), rnd), 'pole')
        p.add(K._rod(f'Lash{i}', (x - 0.05, 0, top - 0.02), (x + 0.05, 0, top - 0.02), 0.055, 'M_Wood', PAL['lashing'],
                     rnd), 'pole')
    p.add(K._bamboo('Ridge', (-1.15, 0, top), (1.15, 0, top), 0.035, rnd, segs=8), 'pole')
    zr = 1.05
    yr = spread * (1 - zr / top) * 0.98
    for k, sy in enumerate((-1, 1)):
        p.add(K._bamboo(f'Bar{k}', (-1.15, sy * yr, zr), (1.15, sy * yr, zr), 0.028, rnd, segs=8), 'pole')
    for i in range(6):
        x = -0.8 + i * 0.32
        _fish(p, rnd, f'FishR{i}', (x, 0, top - 0.035), length=rnd.uniform(0.3, 0.4))
        for k, sy in enumerate((-1, 1)):
            if (i + k) % 2 == 0:
                _fish(p, rnd, f'Fish{i}{k}', (x + 0.08, sy * yr, zr - 0.028), length=rnd.uniform(0.25, 0.33))
    return p.finish(name)


# ---------------------------------------------------------------------------
# Ahumadero: fogón de piedras, 4 postes, paredes de palma en 3 lados
# (reutiliza la pared del kit, escalada), tejado cónico con respiradero y
# pescado colgado sobre el humo
# ---------------------------------------------------------------------------
@_register('smokehouse')
def _b_smokehouse(v, rnd, name):
    p = K.Parts()
    side, post_h = 1.6, 1.7
    h = side / 2
    for i, (x, y) in enumerate([(-h, -h), (h, -h), (-h, h), (h, h)]):
        p.add(K._rod(f'Post{i}', (x, y, 0), (x, y, post_h + 0.05), 0.055, 'M_Wood', _pick(rnd, 'branch'), rnd), 'pole')
    sc = (side - 0.1) / K.GRID
    for rot, (x, y) in ((0.0, (0, h)), (math.pi / 2, (-h, 0)), (-math.pi / 2, (h, 0))):
        w = K._wall_parts('Palm', rnd, 1.25, [])
        w.transform(Matrix.Translation((x, y, 0.35)) @ Matrix.Rotation(rot, 4, 'Z') @ Matrix.Scale(sc, 4, (1, 0, 0)))
        p.extend(w)
    # fogón
    n = 8
    for i in range(n):
        a = 2 * math.pi * i / n
        p.add(_blob(f'Ring{i}', (math.cos(a) * 0.32, math.sin(a) * 0.32, 0.07), (0.1, 0.1, 0.08), 'M_Stone',
                    _pick(rnd, 'stone'), rnd, v['seed'] * 7 + i, noise=0.25, subdiv=1), 'none')
    p.add(_blob('Embers', (0, 0, 0.02), (0.24, 0.24, 0.04), 'M_Stone', PAL['char'], rnd, v['seed'], noise=0.2,
                subdiv=2), 'none')
    # varas y pescado
    for k, y in enumerate((-0.3, 0.0, 0.3)):
        p.add(K._rod(f'Bar{k}', (-h, y, 1.45), (h, y, 1.45), 0.022, 'M_Wood', _pick(rnd, 'branch'), rnd, segs=8), 'pole')
        for i in range(4):
            # dentro del ahumadero apenas se ven: icosfera de 1 subdivisión
            _fish(p, rnd, f'F{k}{i}', (-0.5 + i * 0.33 + (k % 2) * 0.1, y, 1.45 - 0.022), length=0.3, subdiv=1)
    # tejado cónico en capas, con respiradero arriba
    z = post_h
    for i, (r0, r1, dh) in enumerate([(1.35, 0.95, 0.32), (1.05, 0.6, 0.34), (0.7, 0.24, 0.36)]):
        cone = C.make_cylinder(f'Roof{i}', radius=r0, depth=dh, segments=12, center=(0, 0, z + dh / 2),
                               radius2=r1)
        M.assign(cone, ['M_Leaf'])
        cols = [_pick(rnd, 'thatch') for _ in range(12)]
        z0_ = z

        def fn(vv, cols=cols, z0_=z0_, dh=dh):
            a = math.atan2(vv.co.y, vv.co.x)
            c = cols[int((a + math.pi) / (2 * math.pi) * 12) % 12]
            f = 1.1 - 0.4 * max(0.0, min(1.0, (vv.co.z - z0_) / dh))
            return (c[0] * f, c[1] * f, c[2] * f, 0.0)
        C.set_vertex_colors(cone, fn)
        p.add(cone, 'thatch')
        z += dh * 0.72
    vent_z = z + 0.1
    for i in range(4):
        a = math.pi / 4 + i * math.pi / 2
        p.add(K._rod(f'Vent{i}', (math.cos(a) * 0.2, math.sin(a) * 0.2, z - 0.05), (math.cos(a) * 0.12,
                     math.sin(a) * 0.12, vent_z + 0.05), 0.02, 'M_Wood', _pick(rnd, 'branch'), rnd, segs=8), 'pole')
    cap = C.make_cylinder('Cap', radius=0.38, depth=0.16, segments=12, center=(0, 0, vent_z + 0.13), radius2=0.05)
    M.assign(cap, ['M_Leaf'])
    C.set_vertex_colors(cap, C.constant_tint(PAL['thatch'][1], jitter=0.03, rnd=rnd))
    p.add(cap, 'thatch')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Muelle: tramo de 2 x 4 m sobre pilotes (cubierta a +2 m sobre el fondo) y
# tramo final con bolardos y escala
# ---------------------------------------------------------------------------
DOCK_W, DOCK_L, DOCK_H = 2.0, 4.0, 2.0


def _dock_parts(rnd):
    p = K.Parts()
    for i, x in enumerate((-DOCK_W / 2 + 0.12, DOCK_W / 2 - 0.12)):
        for k, y in enumerate((-1.2, 1.2)):
            pile = K._rod(f'Pile{i}{k}', (x, y, 0), (x, y, DOCK_H - 0.08), 0.12, 'M_Wood', PAL['wet_wood'], rnd,
                          segs=12, taper=0.95)
            C.set_vertex_colors(pile, S.weathered_tint(PAL['wood_dark'][1], PAL['algae'], seed=i * 3 + k,
                                                       patchiness=2.0, wear_amount=0.3, jitter=0.02, rnd=rnd))
            p.add(pile, 'pole')
            # franja de algas bajo el nivel de marea
            band = K._rod(f'Algae{i}{k}', (x, y, 0.0), (x, y, 0.7), 0.126, 'M_Wood', PAL['algae'], rnd, segs=12)
            p.add(band, 'pole')
        p.add(K._box(f'Stringer{i}', (0.14, DOCK_L, 0.16), (x, 0, DOCK_H - 0.14), 'M_Wood',
                     _pick(rnd, 'wood_dark'), rnd), 'wood')
    for k, y in enumerate((-1.2, 1.2)):
        p.add(K._box(f'Cap{k}', (DOCK_W + 0.1, 0.16, 0.14), (0, y, DOCK_H - 0.29), 'M_Wood',
                     _pick(rnd, 'wood_dark'), rnd), 'wood')
        # arriostrado en X entre pilotes
        p.add(K._rod(f'BrA{k}', (-DOCK_W / 2 + 0.12, y - 0.13, 0.5), (DOCK_W / 2 - 0.12, y - 0.13, DOCK_H - 0.4),
                     0.04, 'M_Wood', _pick(rnd, 'wood_dark'), rnd, segs=8), 'pole')
    n = 14
    pw = DOCK_L / n
    for i in range(n):
        y = -DOCK_L / 2 + pw * (i + 0.5)
        dz = rnd.uniform(-0.008, 0.0)
        p.add(K._box(f'Plank{i}', (DOCK_W + rnd.uniform(0.05, 0.14), pw * 0.9, 0.06),
                     (rnd.uniform(-0.03, 0.03), y, DOCK_H - 0.03 + dz), 'M_Wood', _pick(rnd, 'wood'), rnd,
                     rot_z=rnd.uniform(-0.015, 0.015)), 'wood')
    return p


@_register('dock')
def _b_dock(v, rnd, name):
    return _dock_parts(rnd).finish(name)


@_register('dock_end')
def _b_dock_end(v, rnd, name):
    p = _dock_parts(rnd)
    for i, x in enumerate((-DOCK_W / 2 + 0.15, DOCK_W / 2 - 0.15)):
        p.add(K._rod(f'Bollard{i}', (x, DOCK_L / 2 - 0.2, DOCK_H), (x, DOCK_L / 2 - 0.2, DOCK_H + 0.45), 0.1,
                     'M_Wood', _pick(rnd, 'wood_dark'), rnd, segs=12), 'pole')
        p.add(K._rod(f'Rope{i}', (x, DOCK_L / 2 - 0.2, DOCK_H + 0.28), (x, DOCK_L / 2 - 0.2, DOCK_H + 0.34), 0.115,
                     'M_Fabric', PAL['lashing'], rnd, segs=12), 'pole')
    # escala de bajada al agua en el extremo
    ly = DOCK_L / 2 + 0.06
    for i, x in enumerate((-0.25, 0.25)):
        p.add(K._rod(f'LadR{i}', (x, ly, 0.3), (x, ly, DOCK_H + 0.2), 0.035, 'M_Wood', _pick(rnd, 'wood'), rnd,
                     segs=8), 'pole')
    for k in range(6):
        z = 0.45 + k * 0.28
        p.add(K._rod(f'Rung{k}', (-0.28, ly, z), (0.28, ly, z), 0.025, 'M_Wood', _pick(rnd, 'wood'), rnd, segs=8),
              'pole')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Parcela de huerto 2 x 2 m: tierra en surcos, brotes, borde de troncos o
# de piedras
# ---------------------------------------------------------------------------
def _plot_soil(p, rnd, seed, inner=1.75):
    p.add(K._box('Soil', (inner, inner, 0.14), (0, 0, 0.07), 'M_Stone', PAL['soil'][1], rnd, jitter=0.02), 'soft')
    for k, y in enumerate((-0.55, 0.0, 0.55)):
        ridge = C.make_blob(f'Row{k}', (0, y, 0.14), radius=1.0, seed=seed + k, subdivisions=3, noise_scale=1.3,
                            noise_strength=0.06, scale=(inner / 2 - 0.08, 0.2, 0.09), relax_iterations=1)
        M.assign(ridge, ['M_Stone'])
        C.set_vertex_colors(ridge, C.constant_tint(_pick(rnd, 'soil'), jitter=0.02, rnd=rnd))
        p.add(ridge, 'none')
        for i in range(6):
            x = -0.68 + i * 0.27 + rnd.uniform(-0.03, 0.03)
            _sprout(p, rnd, f'Sp{k}{i}', (x, y, 0.21))


def _sprout(p, rnd, name, base):
    x, y, z = base
    h = rnd.uniform(0.07, 0.12)
    g = _pick(rnd, 'sprout')
    p.add(K._rod(name + 'St', (x, y, z - 0.02), (x, y, z + h), 0.008, 'M_Leaf', g, rnd, segs=6), 'none')
    for s in (-1, 1):
        a = rnd.uniform(0, math.pi)
        leaf = C.make_blob(name + f'L{s}', (0, 0, 0), radius=1.0, seed=rnd.randint(0, 999), subdivisions=1,
                           noise_scale=1.0, noise_strength=0.02, scale=(0.045, 0.02, 0.008))
        leaf.data.transform(Matrix.Translation((x, y, z + h)) @ Matrix.Rotation(a + (0 if s > 0 else math.pi), 4, 'Z')
                            @ Matrix.Translation((0.04, 0, 0.01)) @ Matrix.Rotation(-0.35, 4, 'Y'))
        M.assign(leaf, ['M_Leaf'])
        C.set_vertex_colors(leaf, C.constant_tint(g, jitter=0.03, rnd=rnd))
        p.add(leaf, 'none')


@_register('plot_logs')
def _b_plot_logs(v, rnd, name):
    p = K.Parts()
    r = 0.11
    for i, (a, b) in enumerate([((-1.0, -0.89), (1.0, -0.89)), ((-1.0, 0.89), (1.0, 0.89)),
                                ((-0.89, -0.78), (-0.89, 0.78)), ((0.89, -0.78), (0.89, 0.78))]):
        z = r if i < 2 else r * 0.95
        p.add(K._rod(f'Log{i}', (a[0], a[1], z), (b[0], b[1], z), r * rnd.uniform(0.93, 1.0), 'M_Wood',
                     _pick(rnd, 'branch'), rnd, segs=12), 'pole')
    for i, (x, y) in enumerate([(-0.99, -0.99), (0.99, -0.99), (-0.99, 0.99), (0.99, 0.99)]):
        p.add(K._rod(f'Stake{i}', (x, y, 0), (x, y, 0.36), 0.035, 'M_Wood', _pick(rnd, 'branch'), rnd, segs=8,
                     taper=0.7), 'pole')
    _plot_soil(p, rnd, v['seed'], inner=1.6)
    return p.finish(name)


@_register('plot_stones')
def _b_plot_stones(v, rnd, name):
    p = K.Parts()
    k = 0
    for side in range(4):
        for i in range(7):
            t = -0.93 + i * 0.31
            x, y = [(t, -0.93), (0.93, t), (-t, 0.93), (-0.93, -t)][side]
            sz = rnd.uniform(0.13, 0.17)
            p.add(_blob(f'St{k}', (x, y, sz * 0.75), (sz, sz, sz * 0.9), 'M_Stone', _pick(rnd, 'stone'), rnd,
                        v['seed'] * 11 + k, noise=0.25, subdiv=2), 'none')
            k += 1
    _plot_soil(p, rnd, v['seed'], inner=1.6)
    return p.finish(name)
