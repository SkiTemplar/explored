"""
produccion_base.py — estaciones de producción, huerto y estructura de la
base que Content/Data/building_pieces.json todavía tenía sin malla (lista
«buildingPieces» de meshes_pendientes.json): piedra de trabajo, horno de
barro, chimenea, telar de fibra, espaldera (maracuyá), arriate del limonero
y mesa de cartografía (biblia §3.10 y §7).

Mismo lenguaje visual que el kit modular y el mobiliario (reutiliza sus
primitivas, paletas y biseles vía kit_construccion.py). Pivote en la base,
escala real en metros; las piezas que van dentro de la casa caben en una
celda de 2 x 2 m de la rejilla.
"""

import math
import os
import sys
from itertools import pairwise

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import bpy  # noqa: E402

import _materials as M  # noqa: E402
import _shapes as S  # noqa: E402
import bmesh  # noqa: E402
import common as C  # noqa: E402
import kit_construccion as K  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils import noise as mnoise

CATEGORY = 'base_production'
GROUP = 'ProduccionBase'

VARIANTS = [
    dict(name='Base_WorkStone', seed=2901, builder='work_stone', tri_budget=(150, 4000), interactable=True),
    dict(name='Base_ClayOven', seed=2902, builder='clay_oven', tri_budget=(300, 7000), interactable=True),
    dict(name='Base_Chimney', seed=2903, builder='chimney', tri_budget=(300, 9000), collision_complex=True,
         interactable=True),
    dict(name='Base_Loom', seed=2904, builder='loom', tri_budget=(200, 6000), interactable=True),
    dict(name='Base_Trellis', seed=2905, builder='trellis', tri_budget=(200, 7000), interactable=True),
    dict(name='Base_LemonBed', seed=2906, builder='lemon_bed', tri_budget=(200, 6000), interactable=True),
    dict(name='Base_ChartTable', seed=2907, builder='chart_table', tri_budget=(200, 6000), interactable=True),
]
for _v in VARIANTS:
    _v.setdefault('needs_collision', True)
    _v['group'] = GROUP

PAL = dict(K.PAL)
PAL.update({
    'soil': [(0.13, 0.07, 0.03), (0.16, 0.09, 0.04), (0.11, 0.06, 0.03)],
    'clay': [(0.52, 0.22, 0.08), (0.46, 0.19, 0.07), (0.58, 0.27, 0.10)],
    'clay_dry': (0.66, 0.42, 0.22),
    'soot': (0.05, 0.04, 0.035),
    'ember': (0.9, 0.28, 0.04),
    'leaf': [(0.10, 0.30, 0.04), (0.14, 0.36, 0.05), (0.08, 0.24, 0.04)],
    'passion': (0.22, 0.05, 0.14),
    'fibre': [(0.62, 0.48, 0.24), (0.55, 0.40, 0.18)],
    'dye': [(0.42, 0.10, 0.04), (0.66, 0.54, 0.30), (0.12, 0.16, 0.30), (0.20, 0.08, 0.03)],
    'paper': (0.78, 0.66, 0.44),
    'sea': (0.10, 0.36, 0.42),
    'ink': (0.10, 0.07, 0.05),
    'flint': [(0.18, 0.16, 0.15), (0.30, 0.24, 0.18)],
    'mulch': (0.24, 0.14, 0.06),
    'char': (0.04, 0.035, 0.03),
    'basalt': (0.22, 0.17, 0.13),
    'lichen': (0.46, 0.40, 0.16),
    'worn': (0.40, 0.33, 0.24),
    'greenstone': (0.10, 0.22, 0.14),
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


def _blob(name, center, half, mat, rgb, rnd, seed, noise=0.18, subdiv=2, jitter=0.03, relax=1):
    o = C.make_blob(name, center, radius=1.0, seed=seed, subdivisions=subdiv, noise_scale=1.4,
                    noise_strength=noise, scale=half, relax_iterations=relax)
    M.assign(o, [mat])
    C.set_vertex_colors(o, C.constant_tint(rgb, alpha=0.0, jitter=jitter, rnd=rnd))
    return o


def _lash(p, rnd, name, center, axis='z', r=0.05, h=0.05):
    x, y, z = center
    d = {'x': (h / 2, 0, 0), 'y': (0, h / 2, 0), 'z': (0, 0, h / 2)}[axis]
    p.add(K._rod(name, (x - d[0], y - d[1], z - d[2]), (x + d[0], y + d[1], z + d[2]), r, 'M_Wood',
                 PAL['lashing'], rnd, segs=8), 'pole')


# ---------------------------------------------------------------------------
# Piedra de trabajo: bloque de basalto con la cara de arriba plana y gastada,
# yunque, percutor y lascas de pedernal
# ---------------------------------------------------------------------------
@_register('work_stone')
def _b_work_stone(v, rnd, name):
    p = K.Parts()
    # facetas grandes (subdiv 2, sin relajar): bloque partido, no canto rodado
    base = _blob('Base', (0, 0, 0.2), (0.52, 0.4, 0.27), 'M_Stone', PAL['stone'][2], rnd, v['seed'], noise=0.3,
                 subdiv=2, relax=0)
    # aplana la cara de trabajo: todo lo que pasa de 0,36 m baja a 0,36 con
    # un pelín de ondulación (lee como piedra usada, no cortada)
    for vert in base.data.vertices:
        if vert.co.z > 0.36:
            vert.co.z = 0.36 + (vert.co.z - 0.36) * 0.12
    base.data.update()
    # basalto cálido oscuro con parches de liquen, musgo en la base y la
    # cara de trabajo pulida más clara (la primera versión, con la paleta
    # de sillarejo, salía casi blanca y plana en la lámina)
    wear = S.weathered_tint(PAL['basalt'], PAL['lichen'], seed=v['seed'] * 0.37, patchiness=2.2,
                            wear_amount=0.3, jitter=0.02, rnd=rnd)

    def base_col(vv):
        c = wear(vv)[:3]
        k = min(1.0, max(0.0, vv.co.z / 0.16))
        c = [m * (1 - k) + x * k for m, x in zip(PAL['moss'], c, strict=True)]
        if vv.co.z > 0.355:
            c = [x * 0.5 + w * 0.5 for x, w in zip(c, PAL['worn'], strict=True)]
        return (*c, 0.0)
    C.set_vertex_colors(base, base_col)
    p.add(base, 'none')
    p.add(_blob('Anvil', (0.12, 0.05, 0.41), (0.2, 0.15, 0.06), 'M_Stone', PAL['slate'][3], rnd, v['seed'] + 1,
                noise=0.12, subdiv=2), 'none')
    p.add(_blob('Hammer', (-0.22, -0.12, 0.42), (0.07, 0.06, 0.055), 'M_Stone', PAL['stone'][0], rnd,
                v['seed'] + 2, noise=0.15, subdiv=2), 'none')
    adze = _blob('Adze', (0, 0, 0), (0.1, 0.04, 0.025), 'M_Stone', PAL['greenstone'], rnd, v['seed'] + 3,
                 noise=0.12, subdiv=2)
    adze.data.transform(Matrix.Translation((-0.08, 0.2, 0.39)) @ Matrix.Rotation(0.7, 4, 'Z'))
    p.add(adze, 'none')
    # rollo de cordel de fibra de coco al pie (anillos apilados)
    for i in range(3):
        ring = C.make_tube(f'Coil{i}', 0.03, 12, [0.1 - i * 0.012, 0.1 - i * 0.012], [0.05, 0.05],
                           cap_bottom=True, cap_top=True, z0=i * 0.03)
        ring.data.transform(Matrix.Translation((0.62, -0.3, 0)))
        M.assign(ring, ['M_Fabric'])
        C.set_vertex_colors(ring, C.constant_tint(PAL['fibre'][i % 2], jitter=0.03, rnd=rnd))
        p.add(ring, 'soft')
    for i in range(5):
        a = rnd.uniform(0, 2 * math.pi)
        x, y = 0.12 + math.cos(a) * 0.1, 0.05 + math.sin(a) * 0.08
        sh = _blob(f'Flake{i}', (0, 0, 0), (0.035, 0.022, 0.007), 'M_Stone', _pick(rnd, 'flint'), rnd,
                   v['seed'] + 10 + i, noise=0.3, subdiv=1, relax=0)
        sh.data.transform(Matrix.Translation((x, y, 0.475)) @ Matrix.Rotation(a, 4, 'Z'))
        p.add(sh, 'none')
    # lascas y núcleo en el suelo, alrededor (el sitio "se ha usado")
    for i in range(4):
        a = rnd.uniform(-1.2, 1.4)
        r = rnd.uniform(0.55, 0.7)
        p.add(_blob(f'Chip{i}', (math.cos(a) * r, math.sin(a) * r - 0.1, 0.012), (0.04, 0.03, 0.012), 'M_Stone',
                    _pick(rnd, 'flint'), rnd, v['seed'] + 20 + i, noise=0.3, subdiv=1, relax=0), 'none')
    p.add(_blob('Core', (-0.58, 0.22, 0.06), (0.1, 0.08, 0.07), 'M_Stone', PAL['flint'][0], rnd, v['seed'] + 30,
                noise=0.3, subdiv=2), 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Horno de barro: cúpula de adobe sobre zócalo de piedra, boca en arco con
# hollín, respiradero y leña al lado
# ---------------------------------------------------------------------------
@_register('clay_oven')
def _b_clay_oven(v, rnd, name):
    p = K.Parts()
    plinth_h = 0.55
    # zócalo: anillo de piedras + relleno
    n = 12
    for i in range(n):
        a = 2 * math.pi * i / n
        for k in range(2):
            z = 0.14 + k * 0.27
            aa = a + (math.pi / n if k else 0)
            p.add(_blob(f'Pl{k}_{i}', (math.cos(aa) * 0.66, math.sin(aa) * 0.66, z), (0.2, 0.17, 0.15), 'M_Stone',
                        _pick(rnd, 'stone'), rnd, v['seed'] * 7 + i * 3 + k, noise=0.2, subdiv=2), 'none')
    p.add(K._rod('Fill', (0, 0, 0), (0, 0, plinth_h), 0.66, 'M_Stone', PAL['stone'][4], rnd, segs=16), 'slab')
    p.add(K._rod('Top', (0, 0, plinth_h - 0.01), (0, 0, plinth_h + 0.06), 0.8, 'M_Stone', PAL['clay_dry'], rnd,
                 segs=20), 'soft')
    z0 = plinth_h + 0.06
    # cúpula por anillos (perfil de colmena algo achatado)
    segs, levels = 20, 9
    R, H = 0.68, 0.78
    rings = []
    for li in range(levels):
        t = li / (levels - 1)
        ang = t * math.pi / 2
        r = R * math.cos(ang) * (1.0 + 0.04 * math.sin(t * math.pi))
        z = z0 + H * math.sin(ang) ** 0.9
        r = max(r, 0.09)  # deja el respiradero arriba
        ring = []
        for i in range(segs):
            a = 2 * math.pi * i / segs
            wob = 1.0 + 0.025 * mnoise.noise(Vector((math.cos(a) * 2, math.sin(a) * 2, t * 3 + v['seed'])))
            ring.append(Vector((math.cos(a) * r * wob, math.sin(a) * r * wob, z)))
        rings.append(ring)
    dome = C.ring_loft('Dome', rings, cap_start=False, cap_end=False)
    M.assign(dome, ['M_Stone'])
    base_c = PAL['clay'][0]

    def dome_col(vv):
        # hollín alrededor de la boca (mira a -Y) y en el respiradero
        d_mouth = (Vector((vv.co.x, vv.co.y + R, vv.co.z - z0 - 0.25))).length
        soot = max(0.0, 1.0 - d_mouth / 0.55) ** 1.5
        top = max(0.0, (vv.co.z - z0 - H * 0.85) / (H * 0.15))
        soot = min(1.0, soot + top * 0.8)
        j = rnd.uniform(-0.03, 0.03)
        dry = max(0.0, (vv.co.z - z0) / H) * 0.35
        c = [a * (1 - dry) + b * dry for a, b in zip(base_c, PAL['clay_dry'], strict=True)]
        return (*[max(0.0, min(1.0, a * (1 - soot) + s * soot + j)) for a, s in zip(c, PAL['soot'], strict=True)], 0.0)
    C.set_vertex_colors(dome, dome_col)
    p.add(dome, 'none')
    # respiradero: tubo corto de barro
    vent = C.make_tube('Vent', 0.16, 12, [0.13, 0.11], [0.07, 0.07], cap_bottom=False, cap_top=True,
                       z0=z0 + H - 0.05)
    M.assign(vent, ['M_Stone'])
    C.set_vertex_colors(vent, C.constant_tint(PAL['clay'][2], jitter=0.03, rnd=rnd))
    p.add(vent, 'none')
    p.add(K._rod('VentDark', (0, 0, z0 + H + 0.08), (0, 0, z0 + H + 0.1), 0.072, 'M_Stone', PAL['soot'], rnd,
                 segs=12), 'none')
    # boca: arco de ladrillos de barro con interior negro y brasas
    mw, mh = 0.46, 0.4
    for i in range(9):
        a = math.pi * i / 8
        x, z = math.cos(a) * (mw / 2 + 0.06), math.sin(a) * (mh - 0.1) + 0.12
        b = K._box(f'Arch{i}', (0.11, 0.2, 0.09), (0, 0, 0), 'M_Stone', _pick(rnd, 'clay'), rnd)
        b.data.transform(Matrix.Translation((x, -R + 0.02, z0 + z)) @ Matrix.Rotation(-(a - math.pi / 2), 4, 'Y'))
        p.add(b, 'soft')
    hole = C.make_cylinder('Mouth', radius=mw / 2, depth=0.1, segments=16, center=(0, 0, 0))
    hole.data.transform(Matrix.Translation((0, -R + 0.05, z0 + 0.12)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
    for vv in hole.data.vertices:  # medio disco + rectángulo bajo: silueta de arco
        if vv.co.z < z0 + 0.12:
            vv.co.z = z0 + 0.005
    hole.data.update()
    M.assign(hole, ['M_Stone'])
    C.set_vertex_colors(hole, C.constant_tint(PAL['soot'], jitter=0.01, rnd=rnd))
    p.add(hole, 'none')
    for i in range(3):
        p.add(_blob(f'Ember{i}', (-0.1 + i * 0.1, -R + 0.02, z0 + 0.035), (0.05, 0.04, 0.03), 'M_Wood', PAL['ember'],
                    rnd, v['seed'] + 40 + i, noise=0.2, subdiv=1), 'none')
    # tapa de madera apoyada al lado y pala de horno
    p.add(K._box('Lid', (0.5, 0.05, 0.42), (0.62, -0.72, 0.21), 'M_Wood', _pick(rnd, 'wood'), rnd,
                 rot_z=0.5), 'wood')
    # leña apilada
    for i in range(5):
        y = 0.05 + (i % 3) * 0.15 + (0.07 if i >= 3 else 0)
        z = 0.08 + (0.14 if i >= 3 else 0)
        p.add(K._rod(f'Log{i}', (-0.95, y - 0.35, z), (-0.95 + 0.06, y + 0.35, z), 0.07, 'M_Wood',
                     _pick(rnd, 'branch'), rnd, segs=10), 'pole')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Chimenea de piedra: hogar con boca y dintel, campana y cañón hasta pasar
# el tejado (4,25 m sobre la cara del suelo). Va contra una pared: el cuerpo
# acaba en y=+0.45 (la repisa sobresale 6 cm más); esa cara se apoya en la
# cara interior de la pared.
# ---------------------------------------------------------------------------
def _stone_box(p, rnd, w, d, z0, z1, prefix, openings_front=()):
    """Cuatro paños de sillarejo formando un prisma hueco de w x d."""
    t = 0.22
    for side, (ln, off, rot) in enumerate([(w, -d / 2 + t / 2, 0.0), (w, d / 2 - t / 2, 0.0),
                                           (d - 2 * t, -w / 2 + t / 2, math.pi / 2),
                                           (d - 2 * t, w / 2 - t / 2, math.pi / 2)]):
        q = K.Parts()
        cuts = list(openings_front) if side == 0 else []
        breaks = sorted({c[k] for c in cuts for k in (2, 3)})
        K._stone_courses(q, rnd, -ln / 2, ln / 2, z0, z1, t, cuts, f'{prefix}{side}_', moss=False, breaks=breaks)
        m = (Matrix.Translation((off, 0, 0)) if rot else Matrix.Translation((0, off, 0))) @ Matrix.Rotation(rot, 4, 'Z')
        q.transform(m)
        # sillares de ruina: bisel de 1 segmento (el de 2 disparaba los tris)
        for o in q.kinds.pop('stone', []):
            p.add(o, 'block')


@_register('chimney')
def _b_chimney(v, rnd, name):
    p = K.Parts()
    W, D = 1.5, 0.9
    hearth_h = 1.15
    mouth = (-0.42, 0.42, 0.0, 0.78)
    _stone_box(p, rnd, W, D, 0.0, hearth_h, 'H', openings_front=[mouth])
    # dintel de madera oscura sobre la boca y repisa de piedra
    p.add(K._box('Lintel', (1.06, 0.26, 0.18), (0, -D / 2 + 0.11, mouth[3] + 0.09), 'M_Wood',
                 _pick(rnd, 'wood_dark'), rnd), 'wood')
    p.add(K._box('Mantel', (W + 0.16, D + 0.12, 0.1), (0, 0.0, hearth_h + 0.05), 'M_Stone', PAL['stone'][2], rnd),
          'slab')
    # interior negro (fondo y suelo del hogar) + brasas y troncos
    p.add(K._box('Back', (0.86, 0.04, 0.8), (0, D / 2 - 0.24, 0.4), 'M_Stone', PAL['soot'], rnd, jitter=0.01),
          'none')
    p.add(K._box('Floor', (0.86, D - 0.44, 0.04), (0, 0, 0.02), 'M_Stone', PAL['soot'], rnd, jitter=0.01), 'none')
    for i, (a, b) in enumerate([((-0.3, -0.05, 0.1), (0.25, 0.08, 0.1)), ((-0.2, 0.1, 0.1), (0.3, -0.06, 0.12))]):
        p.add(K._rod(f'Log{i}', a, b, 0.06, 'M_Wood', PAL['char'], rnd,
                     segs=8), 'pole')
    for i in range(3):
        p.add(_blob(f'Ember{i}', (-0.16 + i * 0.16, 0.0, 0.05), (0.07, 0.06, 0.035), 'M_Wood', PAL['ember'], rnd,
                    v['seed'] + i, noise=0.2, subdiv=1), 'none')
    # losa de hogar delante
    p.add(K._box('HearthSlab', (W + 0.1, 0.5, 0.08), (0, -D / 2 - 0.24, 0.04), 'M_Stone', PAL['stone'][0], rnd),
          'slab')
    # campana: dos hiladas que estrechan hasta el cañón
    z = hearth_h + 0.1
    for k, (w, d) in enumerate([(1.2, 0.78), (0.9, 0.66)]):
        _stone_box(p, rnd, w, d, z, z + 0.3, f'C{k}')
        z += 0.3
    # cañón
    top = 4.25
    _stone_box(p, rnd, 0.66, 0.6, z, top - 0.12, 'F')
    p.add(K._box('Cap', (0.82, 0.76, 0.1), (0, 0, top - 0.07), 'M_Stone', PAL['slate'][3], rnd), 'slab')
    p.add(K._box('Flue', (0.3, 0.26, 0.04), (0, 0, top - 0.005), 'M_Stone', PAL['soot'], rnd, jitter=0.01), 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Telar de fibra: bastidor de bambú inclinado, urdimbre de fibra de coco,
# tela a rayas ya tejida abajo y lanzadera
# ---------------------------------------------------------------------------
@_register('loom')
def _b_loom(v, rnd, name):
    p = K.Parts()
    Wx, H = 1.3, 1.7
    lean = 0.25  # el bastidor se inclina hacia atrás (+Y) al subir
    feet = [(-Wx / 2, -0.3), (Wx / 2, -0.3)]
    for i, (x, y) in enumerate(feet):
        p.add(K._bamboo(f'Up{i}', (x, y, 0), (x, y + lean, H), 0.045, rnd, segs=10, node_step=0.38), 'pole')
        p.add(K._bamboo(f'Leg{i}', (x, 0.45, 0), (x, y + lean * 0.75, H * 0.75), 0.035, rnd, segs=8), 'pole')
        p.add(K._bamboo(f'Foot{i}', (x, -0.38, 0.035), (x, 0.52, 0.035), 0.035, rnd, segs=8), 'pole')
        _lash(p, rnd, f'LashL{i}', (x, y + lean * 0.75, H * 0.75), 'x', 0.05, 0.1)

    def at(t, dy=0.0):  # punto sobre el plano del bastidor a altura relativa t
        return -0.3 + lean * t + dy, H * t
    beams = [0.14, 0.93]
    for i, t in enumerate(beams):
        y, z = at(t)
        p.add(K._bamboo(f'Beam{i}', (-Wx / 2 - 0.08, y, z), (Wx / 2 + 0.08, y, z), 0.04, rnd, segs=10), 'pole')
    # urdimbre
    y0, z0 = at(beams[0])
    y1, z1 = at(beams[1])
    nw = 22
    cloth_top = 0.52
    for i in range(nw):
        x = -Wx / 2 + 0.12 + i * (Wx - 0.24) / (nw - 1)
        yc, zc = at(cloth_top)
        p.add(K._rod(f'Warp{i}', (x, yc, zc), (x, y1, z1), 0.0045, 'M_Fabric', _pick(rnd, 'fibre'), rnd, segs=4),
              'none')
    # tela tejida (franjas de color): rebanadas entre la viga baja y cloth_top
    bands = 7
    for b in range(bands):
        ta = beams[0] + (cloth_top - beams[0]) * b / bands
        tb = beams[0] + (cloth_top - beams[0]) * (b + 1) / bands
        ya, za = at(ta)
        yb, zb = at(tb)
        col = PAL['dye'][b % len(PAL['dye'])] if b % 2 else _pick(rnd, 'fibre')
        L = math.hypot(yb - ya, zb - za)
        o = C.make_box(f'Cloth{b}', (Wx - 0.2, 0.012, L + 0.002), center=(0, 0, 0))
        o.data.transform(Matrix.Translation((0, (ya + yb) / 2, (za + zb) / 2))
                         @ Matrix.Rotation(-math.atan2(lean, H), 4, 'X'))
        M.assign(o, ['M_Fabric'])
        C.set_vertex_colors(o, C.constant_tint(col, jitter=0.015, rnd=rnd))
        p.add(o, 'none')
    # varilla de lizos y peine (espada) cruzando la urdimbre
    for i, t in enumerate((0.64, 0.58)):
        y, z = at(t, -0.03 if i else 0.03)
        p.add(K._rod(f'Rod{i}', (-Wx / 2 + 0.02, y, z), (Wx / 2 - 0.02, y, z), 0.014 if i else 0.02, 'M_Wood',
                     _pick(rnd, 'wood'), rnd, segs=8), 'pole')
    # lanzadera con ovillo, sobre la tela
    y, z = at(0.54, -0.05)
    p.add(K._box('Shuttle', (0.42, 0.05, 0.025), (0.2, y, z), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
    p.add(_blob('Yarn', (0.2, y - 0.02, z + 0.02), (0.07, 0.035, 0.035), 'M_Fabric', PAL['dye'][0], rnd,
                v['seed'], noise=0.1, subdiv=2), 'none')
    # cesto de fibra al pie
    basket = C.make_tube('Basket', 0.22, 14, [0.16, 0.2, 0.21], [0.14, 0.18, 0.19], cap_bottom=True, z0=0.0)
    basket.data.transform(Matrix.Translation((Wx / 2 + 0.3, -0.25, 0)))
    M.assign(basket, ['M_Leaf'])
    C.set_vertex_colors(basket, lambda vv: (*[max(0.0, min(1.0, c + (0.08 if int(vv.co.z * 30) % 2 else -0.05)))
                                              for c in PAL['thatch'][0]], 0.0))
    p.add(basket, 'none')
    for i in range(3):
        a = 2 * math.pi * i / 3
        p.add(_blob(f'Ball{i}', (Wx / 2 + 0.3 + math.cos(a) * 0.07, -0.25 + math.sin(a) * 0.07, 0.22),
                    (0.075, 0.075, 0.06), 'M_Fabric', _pick(rnd, 'fibre') if i else PAL['dye'][2], rnd,
                    v['seed'] + 5 + i, noise=0.12, subdiv=2), 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Espaldera de maracuyá: marco de bambú de 2 x 2 m con celosía atada y la
# trepadora cargada de hojas y frutos
# ---------------------------------------------------------------------------
@_register('trellis')
def _b_trellis(v, rnd, name):
    p = K.Parts()
    Wx, H = 2.0, 2.1
    for i, x in enumerate((-Wx / 2 + 0.05, 0.0, Wx / 2 - 0.05)):
        p.add(K._bamboo(f'Post{i}', (x, 0, 0), (x, 0, H), 0.045, rnd, segs=10, node_step=0.4), 'pole')
    p.add(K._bamboo('TopBar', (-Wx / 2 - 0.1, 0, H - 0.06), (Wx / 2 + 0.1, 0, H - 0.06), 0.035, rnd, segs=8), 'pole')
    # celosía en rombos por delante (-Y)
    n = 6
    for i in range(n + 1):
        x = -Wx / 2 + i * Wx / n
        for s in (-1, 1):
            xa, xb = x, x + s * 1.0
            za, zb = 0.25, 0.25 + 1.0 * 1.55
            # recorta al marco
            if xb < -Wx / 2:
                zb = za + (xa + Wx / 2) * 1.55
                xb = -Wx / 2
            if xb > Wx / 2:
                zb = za + (Wx / 2 - xa) * 1.55
                xb = Wx / 2
            if abs(xb - xa) < 0.1:
                continue
            p.add(K._bamboo(f'Lat{i}_{s}', (xa, -0.05 * s, za), (xb, -0.05 * s, zb), 0.014, rnd, segs=6,
                            node_step=0.5), 'none')
    for i, z in enumerate((0.25, 1.1)):
        p.add(K._bamboo(f'Rail{i}', (-Wx / 2, 0.06, z), (Wx / 2, 0.06, z), 0.022, rnd, segs=8), 'pole')
    for i, x in enumerate((-Wx / 2 + 0.05, 0.0, Wx / 2 - 0.05)):
        for k, z in enumerate((1.1, H - 0.06)):
            _lash(p, rnd, f'L{i}_{k}', (x, 0, z), 'z', 0.055, 0.05)
    # tierra al pie y enredadera: tallos que serpentean por la celosía
    p.add(_blob('Mound', (0, -0.15, 0.02), (1.05, 0.3, 0.09), 'M_Stone', PAL['soil'][1], rnd, v['seed'], noise=0.1,
                subdiv=3), 'none')
    stems = [(-0.55, 0.0), (0.45, 0.4)]
    leaves = 0
    for si, (x0, ph) in enumerate(stems):
        pts = []
        for k in range(9):
            t = k / 8
            pts.append(Vector((x0 + 0.35 * math.sin(t * 5 + ph + si), -0.1, 0.05 + t * (H - 0.2))))
        for k in range(len(pts) - 1):
            p.add(K._rod(f'Vine{si}_{k}', pts[k], pts[k + 1], 0.012 * (1.2 - 0.5 * k / 8), 'M_Leaf',
                         (0.12, 0.2, 0.05), rnd, segs=6), 'none')
            for j in range(3):
                q = pts[k].lerp(pts[k + 1], rnd.uniform(0.1, 0.9))
                pos = (q.x + rnd.uniform(-0.12, 0.12), q.y - rnd.uniform(0.04, 0.1), q.z + rnd.uniform(-0.05, 0.05))
                lf = _blob(f'Lf{si}_{k}_{j}', (0, 0, 0), (0.1, 0.075, 0.012), 'M_Leaf', _pick(rnd, 'leaf'), rnd,
                           v['seed'] + leaves, noise=0.15, subdiv=1, relax=0)
                lf.data.transform(Matrix.Translation(pos) @ Matrix.Rotation(rnd.uniform(0, 6.28), 4, 'Y')
                                  @ Matrix.Rotation(math.pi / 2 - 0.3, 4, 'X'))
                p.add(lf, 'none')
                leaves += 1
    # tallos que suben por la barra alta
    for k in range(5):
        x = -0.9 + k * 0.45
        lf = _blob(f'LfTop{k}', (0, 0, 0), (0.11, 0.08, 0.012), 'M_Leaf', _pick(rnd, 'leaf'), rnd, v['seed'] + 90 + k,
                   noise=0.15, subdiv=1, relax=0)
        lf.data.transform(Matrix.Translation((x, -0.08, H - 0.02)) @ Matrix.Rotation(rnd.uniform(-0.4, 0.4), 4, 'X'))
        p.add(lf, 'none')
    # frutos de maracuyá (morados) y alguna flor blanca-violeta
    fruits = [(-0.62, 0.95), (-0.35, 1.45), (0.55, 0.85), (0.3, 1.3), (0.72, 1.62), (-0.1, 1.85)]
    for i, (x, z) in enumerate(fruits):
        p.add(_blob(f'Fruit{i}', (x, -0.16, z), (0.045, 0.045, 0.052), 'M_Leaf',
                    PAL['passion'] if i % 3 else (0.34, 0.30, 0.06), rnd, v['seed'] + 200 + i, noise=0.05, subdiv=2),
              'none')
    for i, (x, z) in enumerate([(-0.2, 1.05), (0.1, 1.7)]):
        p.add(_blob(f'Flower{i}', (x, -0.17, z), (0.06, 0.012, 0.06), 'M_Leaf', (0.6, 0.55, 0.75), rnd,
                    v['seed'] + 300 + i, noise=0.25, subdiv=2), 'none')
        p.add(_blob(f'FlowerC{i}', (x, -0.185, z), (0.025, 0.01, 0.025), 'M_Leaf', (0.25, 0.08, 0.3), rnd,
                    v['seed'] + 310 + i, noise=0.1, subdiv=1), 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Arriate del limonero: anillo elevado de piedras con tierra mullida, sitio
# para el esqueje (el árbol va aparte, por etapas de plants.json), cuenco
# de coco para regar y marca de caña
# ---------------------------------------------------------------------------
@_register('lemon_bed')
def _b_lemon_bed(v, rnd, name):
    p = K.Parts()
    R = 0.8
    n = 14
    for k in range(2):
        for i in range(n):
            a = 2 * math.pi * (i + 0.5 * k) / n
            r = R - 0.04 * k
            sz = rnd.uniform(0.15, 0.19) * (0.92 if k else 1.0)
            z = sz * 0.8 + k * 0.22
            rgb = tuple(c * rnd.uniform(0.7, 0.95) for c in _pick(rnd, 'stone'))
            if k == 0 and rnd.random() < 0.4:
                rgb = tuple(a_ * 0.55 + b_ * 0.45 for a_, b_ in zip(rgb, PAL['moss'], strict=True))
            p.add(_blob(f'St{k}_{i}', (math.cos(a) * r, math.sin(a) * r, z), (sz * 1.15, sz, sz * 0.85), 'M_Stone',
                        rgb, rnd, v['seed'] * 13 + k * 40 + i, noise=0.22, subdiv=2), 'none')
    soil = C.make_blob('Soil', (0, 0, 0.3), radius=1.0, seed=v['seed'], subdivisions=3, noise_scale=1.5,
                       noise_strength=0.05, scale=(R - 0.08, R - 0.08, 0.16), relax_iterations=1)
    M.assign(soil, ['M_Stone'])

    def soil_col(vv):
        r = math.hypot(vv.co.x, vv.co.y)
        k = min(1.0, r / (R - 0.1))
        c = [a * (1 - k) + b * k for a, b in zip(PAL['soil'][1], PAL['mulch'], strict=True)]
        j = rnd.uniform(-0.02, 0.02)
        return (*[max(0.0, min(1.0, x + j)) for x in c], 0.0)
    C.set_vertex_colors(soil, soil_col)
    p.add(soil, 'none')
    # hoyo del esqueje con alcorque y hojarasca de palma alrededor
    p.add(C.make_tube('Well', 0.03, 14, [0.22, 0.2], [0.14, 0.12], cap_bottom=True, z0=0.44), 'none')
    M.assign(p.kinds['none'][-1], ['M_Stone'])
    C.set_vertex_colors(p.kinds['none'][-1], C.constant_tint(PAL['soil'][2], jitter=0.02, rnd=rnd))
    # hojarasca: hojas secas sueltas y algo curvadas sobre la tierra
    for i in range(16):
        a = 2 * math.pi * i / 16 + rnd.uniform(-0.15, 0.15)
        r = rnd.uniform(0.3, 0.6)
        lf = _blob(f'Mulch{i}', (0, 0, 0), (0.07, 0.025, 0.008), 'M_Leaf', _pick(rnd, 'thatch'), rnd,
                   v['seed'] + 60 + i, noise=0.2, subdiv=1, relax=0)
        z = 0.30 + 0.16 * math.sqrt(max(0.0, 1.0 - (r / (R - 0.08)) ** 2)) + 0.005
        lf.data.transform(Matrix.Translation((math.cos(a) * r, math.sin(a) * r, z))
                          @ Matrix.Rotation(rnd.uniform(0, 6.28), 4, 'Z'))
        p.add(lf, 'none')
    # marca de caña con cinta de tapa (se lee a distancia: aquí va el limonero)
    p.add(K._bamboo('Marker', (0.42, 0.3, 0.3), (0.42, 0.3, 1.05), 0.018, rnd, segs=8, node_step=0.3), 'none')
    p.add(K._box('Ribbon', (0.14, 0.01, 0.05), (0.49, 0.3, 0.97), 'M_Fabric', (0.75, 0.62, 0.08), rnd,
                 jitter=0.01), 'none')
    # cuenco de coco para regar, apoyado en el borde
    bowl = C.make_tube('Coco', 0.09, 12, [0.08, 0.1, 0.1], [0.065, 0.088, 0.09], cap_bottom=True, z0=0.0)
    bowl.data.transform(Matrix.Translation((-0.62, -0.58, 0.44)) @ Matrix.Rotation(0.25, 4, 'X'))
    M.assign(bowl, ['M_Wood'])
    C.set_vertex_colors(bowl, C.constant_tint((0.24, 0.12, 0.05), jitter=0.03, rnd=rnd))
    p.add(bowl, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# Mesa de cartografía: tablero de madera con el mapa dibujado a mano (islas,
# rumbos y costas pintados por vértice), piedras pisapapeles, carta de
# varillas, compás de puntas y rollos de mapa en un cesto
# ---------------------------------------------------------------------------
def _map_sheet(name, sx, sy, center, seed, rnd):
    bm = bmesh.new()
    nx, ny = 28, 18
    verts = {}
    for i in range(nx + 1):
        for j in range(ny + 1):
            x = -sx / 2 + sx * i / nx
            y = -sy / 2 + sy * j / ny
            # papel algo ondulado y bordes levantados
            edge = max(abs(x) / (sx / 2), abs(y) / (sy / 2))
            z = 0.004 * mnoise.noise(Vector((x * 4, y * 4, seed))) + (0.008 if edge > 0.96 else 0.0)
            verts[i, j] = bm.verts.new((x, y, z))
    for i in range(nx):
        for j in range(ny):
            bm.faces.new((verts[i, j], verts[i + 1, j], verts[i + 1, j + 1], verts[i, j + 1]))
    # una sola cara: el mapa descansa sobre el tablero y nunca se ve por
    # debajo (bmesh.ops.solidify dejaba el canto con 184 caras de área 0)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    o.data.transform(Matrix.Translation(center))
    M.assign(o, ['M_Paper'])
    cx, cy = center[0], center[1]
    islands = [(-0.28, 0.1, 0.13), (0.05, -0.12, 0.09), (0.3, 0.18, 0.07), (0.34, -0.2, 0.05), (-0.05, 0.24, 0.05)]
    route = [(-0.28, 0.1), (0.05, -0.12), (0.3, 0.18)]

    def col(vv):
        x, y = vv.co.x - cx, vv.co.y - cy
        land = -1.0
        for ix, iy, r in islands:
            d = math.hypot(x - ix, (y - iy) * 1.2)
            n = 0.35 * mnoise.noise(Vector((x * 9, y * 9, seed + 3)))
            land = max(land, 1.0 - d / (r * (1 + n)))
        paper = PAL['paper']
        j = rnd.uniform(-0.015, 0.015)
        if land > 0.12:
            c = (0.30, 0.42, 0.12) if land > 0.45 else (0.62, 0.50, 0.24)
        elif land > 0.0:
            c = PAL['ink']  # costa entintada
        elif land > -0.35:
            k = (land + 0.35) / 0.35
            c = [a * (1 - k * 0.55) + b * k * 0.55 for a, b in zip(paper, PAL['sea'], strict=True)]
        else:
            c = paper
        # rumbo punteado entre islas
        for (ax, ay), (bx, by) in pairwise(route):
            ab = Vector((bx - ax, by - ay))
            t = max(0.0, min(1.0, Vector((x - ax, y - ay)).dot(ab) / ab.length_squared))
            px, py = ax + ab.x * t, ay + ab.y * t
            if math.hypot(x - px, y - py) < 0.012 and int(t * 14) % 2 == 0 and land < 0.0:
                c = (0.55, 0.12, 0.05)
        # rosa de los vientos en la esquina
        if math.hypot(x + 0.36, y + 0.2) < 0.045 and (abs(x + 0.36) < 0.008 or abs(y + 0.2) < 0.008):
            c = PAL['ink']
        # esquinas envejecidas
        edge = max(abs(x) / (sx / 2), abs(y) / (sy / 2))
        if edge > 0.9:
            c = [a * 0.8 for a in c]
        return (*[max(0.0, min(1.0, a + j)) for a in c], 0.0)
    C.set_vertex_colors(o, col)
    return o


@_register('chart_table')
def _b_chart_table(v, rnd, name):
    p = K.Parts()
    Lx, Wy, H = 1.6, 1.0, 0.92
    n = 5
    for i in range(n):
        y = -Wy / 2 + (i + 0.5) * Wy / n
        p.add(K._box(f'Top{i}', (Lx, Wy / n - 0.012, 0.06), (0, y, H - 0.03), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    # faldón con dientes tallados (motivo navegante, como la estantería)
    for i, y in enumerate((-Wy / 2 + 0.05, Wy / 2 - 0.05)):
        p.add(K._box(f'Apron{i}', (Lx - 0.16, 0.05, 0.14), (0, y, H - 0.13), 'M_Wood', _pick(rnd, 'wood_dark'), rnd),
              'wood')
    for i in range(7):
        x = -0.6 + i * 0.2
        p.add(K._box(f'Tooth{i}', (0.06, 0.03, 0.05), (x, -Wy / 2 + 0.02, H - 0.225), 'M_Wood',
                     _pick(rnd, 'wood_dark'), rnd), 'wood')
    for i, (x, y) in enumerate([(-0.7, -0.4), (0.7, -0.4), (-0.7, 0.4), (0.7, 0.4)]):
        p.add(K._box(f'Leg{i}', (0.1, 0.1, H - 0.06), (x, y, (H - 0.06) / 2), 'M_Wood', _pick(rnd, 'wood_dark'), rnd),
              'wood')
    p.add(K._box('Stretcher', (1.4, 0.06, 0.06), (0, 0, 0.2), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    for i, x in enumerate((-0.7, 0.7)):
        p.add(K._box(f'Side{i}', (0.06, 0.8, 0.06), (x, 0, 0.2), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    # mapa y cosas encima
    sheet = _map_sheet('Map', 1.0, 0.66, (-0.1, 0.0, H + 0.003), v['seed'], rnd)
    sheet.data.transform(Matrix.Translation((-0.1, 0, 0)) @ Matrix.Rotation(0.06, 4, 'Z')
                         @ Matrix.Translation((0.1, 0, 0)))
    p.add(sheet, 'none')
    for i, (x, y) in enumerate([(-0.56, 0.3), (0.36, -0.3), (0.36, 0.31)]):
        p.add(_blob(f'Weight{i}', (x, y, H + 0.035), (0.05, 0.045, 0.035), 'M_Stone', _pick(rnd, 'stone'), rnd,
                    v['seed'] + i, noise=0.2, subdiv=2), 'none')
    # compás de puntas (dos varillas abiertas en V) y lápiz de carbón
    for i, a in enumerate((-0.25, 0.25)):
        p.add(K._rod(f'Div{i}', (0.05, -0.05, H + 0.012), (0.05 + math.sin(a) * 0.18, -0.05 - math.cos(a) * 0.18,
                                                              H + 0.012), 0.005, 'M_Wood', PAL['wood_dark'][0], rnd,
                     segs=6), 'none')
    p.add(K._rod('Charcoal', (-0.45, -0.25, H + 0.012), (-0.3, -0.29, H + 0.012), 0.009, 'M_Wood', PAL['soot'], rnd,
                 segs=6), 'none')
    # carta de varillas pequeña a la derecha
    cx, cy = 0.6, 0.05
    for i in range(4):
        x = cx - 0.12 + i * 0.08
        p.add(K._rod(f'Stick{i}', (x, cy - 0.15, H + 0.012), (x, cy + 0.15, H + 0.012), 0.006, 'M_Wood',
                     PAL['branch'][1], rnd, segs=6), 'none')
    for i in range(3):
        y = cy - 0.1 + i * 0.1
        p.add(K._rod(f'StickH{i}', (cx - 0.15, y, H + 0.022), (cx + 0.15, y + 0.02, H + 0.022), 0.005, 'M_Wood',
                     PAL['branch'][0], rnd, segs=6), 'none')
    for i, (dx, dy) in enumerate([(-0.04, 0.0), (0.04, 0.1), (0.0, -0.1)]):
        p.add(_blob(f'Shell{i}', (cx + dx, cy + dy, H + 0.03), (0.018, 0.014, 0.012), 'M_Stone',
                    (0.8, 0.7, 0.55), rnd, v['seed'] + 20 + i, noise=0.1, subdiv=1), 'none')
    # cesto con rollos de mapa bajo la mesa
    basket = C.make_tube('Basket', 0.3, 14, [0.17, 0.2], [0.155, 0.185], cap_bottom=True, z0=0.0)
    basket.data.transform(Matrix.Translation((0.35, 0.1, 0)))
    M.assign(basket, ['M_Leaf'])
    C.set_vertex_colors(basket, C.constant_tint(PAL['thatch'][2], jitter=0.04, rnd=rnd))
    p.add(basket, 'none')
    for i in range(4):
        a = 2 * math.pi * i / 4 + 0.3
        x, y = 0.35 + math.cos(a) * 0.08, 0.1 + math.sin(a) * 0.08
        p.add(K._rod(f'Roll{i}', (x, y, 0.05), (x + 0.03, y, 0.55 + 0.04 * i), 0.035, 'M_Paper', PAL['paper'], rnd,
                     segs=10), 'none')
    return p.finish(name)
