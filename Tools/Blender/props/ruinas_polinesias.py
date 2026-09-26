"""
ruinas_polinesias.py — ruinas del antiguo pueblo de navegantes (spec §6,
biblia §9): marae de plataformas escalonadas (completos y en piezas
modulares para montar yacimientos), piedras erguidas, estatuas, muros de
piedra seca, losas con petroglifos, restos de canoa doble y altares.

Dirección de arte (spec §15): basalto oscuro erosionado con musgo y
líquenes, sin ornamento excesivo — "respeto por delante de espectáculo".
Las estatuas son un diseño PROPIO de este pueblo ficticio, no copias de
moai ni de tiki reales: figura rechoncha de ojos grandes y redondos que
MIRA HACIA ARRIBA (a las estrellas), con una diadema de olas y las manos
sosteniendo un disco estelar sobre el vientre.

Rejilla modular del marae (ver docs/art/kit-construccion.md §Ruinas):
terraza de 2 m de ancho, escalón de TIER_H = 0,45 m, retranqueo de
TIER_INSET = 0,8 m; la escalinata sube exactamente un escalón de terraza.
"""

import math
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _materials as M  # noqa: E402
import kit_construccion as K  # noqa: E402

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector, noise as mnoise  # noqa: E402

CATEGORY = 'ruins'
GROUP_SITE = 'RuinasMarae'
GROUP_ART = 'RuinasTallas'

TIER_H = 0.45
TIER_INSET = 0.8

VARIANTS = [
    # --- yacimiento: marae, muros, piedras erguidas ---
    dict(name='Ruin_Marae_Small', seed=3001, builder='marae_small', group=GROUP_SITE,
         tri_budget=(500, 9000), collision_complex=True),
    dict(name='Ruin_Marae_Large', seed=3002, builder='marae_large', group=GROUP_SITE,
         tri_budget=(1000, 16000), collision_complex=True),
    dict(name='Ruin_Marae_TerraceStraight', seed=3003, builder='terrace_straight', group=GROUP_SITE,
         tri_budget=(200, 5000), collision_complex=True),
    dict(name='Ruin_Marae_TerraceCorner', seed=3004, builder='terrace_corner', group=GROUP_SITE,
         tri_budget=(200, 6000), collision_complex=True),
    dict(name='Ruin_Marae_Paving', seed=3005, builder='paving', group=GROUP_SITE,
         tri_budget=(100, 4000)),
    dict(name='Ruin_Marae_Steps', seed=3006, builder='steps', group=GROUP_SITE,
         tri_budget=(100, 4000), collision_complex=True),
    dict(name='Ruin_DryWall_Straight', seed=3007, builder='drywall_straight', group=GROUP_SITE,
         tri_budget=(300, 6000)),
    dict(name='Ruin_DryWall_Ruined', seed=3008, builder='drywall_ruined', group=GROUP_SITE,
         tri_budget=(300, 6000), collision_complex=True),
    dict(name='Ruin_DryWall_Corner', seed=3009, builder='drywall_corner', group=GROUP_SITE,
         tri_budget=(300, 6000)),
    dict(name='Ruin_StandingStone_A', seed=3010, builder='standing_stone', group=GROUP_SITE,
         tri_budget=(100, 2500), height=2.3, width=0.7),
    dict(name='Ruin_StandingStone_B', seed=3011, builder='standing_stone', group=GROUP_SITE,
         tri_budget=(100, 2500), height=1.5, width=0.55),
    dict(name='Ruin_StandingStone_C', seed=3012, builder='standing_stone', group=GROUP_SITE,
         tri_budget=(100, 2500), height=3.1, width=0.85),
    # --- piezas talladas: estatuas, petroglifos, altares, canoa ---
    dict(name='Ruin_Statue_Navigator', seed=3020, builder='statue_navigator', group=GROUP_ART,
         tri_budget=(500, 9000)),
    dict(name='Ruin_Statue_Seated', seed=3021, builder='statue_seated', group=GROUP_ART,
         tri_budget=(500, 9000)),
    dict(name='Ruin_Statue_HeadFallen', seed=3022, builder='statue_head_fallen', group=GROUP_ART,
         tri_budget=(300, 7000)),
    dict(name='Ruin_Petroglyph_Honu', seed=3030, builder='petroglyph', group=GROUP_ART,
         tri_budget=(500, 6000), motif='honu'),
    dict(name='Ruin_Petroglyph_Canoe', seed=3031, builder='petroglyph', group=GROUP_ART,
         tri_budget=(500, 6000), motif='canoe'),
    dict(name='Ruin_Petroglyph_Star', seed=3032, builder='petroglyph', group=GROUP_ART,
         tri_budget=(500, 6000), motif='star'),
    dict(name='Ruin_Petroglyph_Bird', seed=3033, builder='petroglyph', group=GROUP_ART,
         tri_budget=(500, 6000), motif='bird'),
    dict(name='Ruin_Canoe_DoubleWreck', seed=3040, builder='canoe_wreck', group=GROUP_ART,
         tri_budget=(500, 8000), collision_complex=True),
    dict(name='Ruin_Altar_Table', seed=3050, builder='altar_table', group=GROUP_ART,
         tri_budget=(300, 6000), interactable=True),
    dict(name='Ruin_Altar_OfferingStone', seed=3051, builder='altar_offering', group=GROUP_ART,
         tri_budget=(300, 5000), interactable=True),
]
for _v in VARIANTS:
    _v.setdefault('needs_collision', True)

PAL = {
    # basalto cálido oscuro (lineal) — nunca gris neutro
    'basalt': [(0.15, 0.13, 0.11), (0.12, 0.11, 0.10), (0.18, 0.15, 0.12), (0.14, 0.12, 0.12),
               (0.20, 0.16, 0.13)],
    # toba volcánica rojiza de las estatuas (se lee mejor que el basalto)
    'tuff': [(0.30, 0.20, 0.13), (0.26, 0.17, 0.11), (0.33, 0.23, 0.15)],
    'moss': (0.09, 0.15, 0.04),
    'lichen': (0.45, 0.42, 0.24),
    'lichen_orange': (0.55, 0.30, 0.08),
    'groove': (0.42, 0.36, 0.28),
    'sand': [(0.62, 0.50, 0.30), (0.58, 0.46, 0.27)],
    'driftwood': [(0.30, 0.25, 0.19), (0.36, 0.30, 0.22), (0.26, 0.22, 0.17)],
    'shell': [(0.80, 0.66, 0.55), (0.75, 0.55, 0.45), (0.85, 0.78, 0.66)],
    'eye': (0.05, 0.045, 0.04),
    'cord': (0.40, 0.26, 0.10),
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


# ---------------------------------------------------------------------------
# Piedra erosionada: caja subdividida + ruido + tinte de basalto con musgo
# en caras que miran arriba y manchas de liquen
# ---------------------------------------------------------------------------
def _stone_tint(rgb, seed, rnd, moss=0.35, lichen=0.25, jitter=0.02):
    off = Vector((seed * 1.37 % 50, seed * 2.11 % 50, seed * 0.73 % 50))
    cache = {}

    def fn(v):
        if v.index in cache:
            return cache[v.index]
        c = list(rgb)
        n = mnoise.noise(v.co * 2.2 + off)
        if moss and v.normal.z > 0.35 and n > 0.25 - moss:
            k = min(1.0, (n - (0.25 - moss)) * 2.5) * min(1.0, (v.normal.z - 0.35) * 3)
            c = [a * (1 - k) + b * k for a, b in zip(c, PAL['moss'])]
        n2 = mnoise.noise(v.co * 6.5 + off * 1.7)
        if lichen and n2 > 0.55 - lichen:
            lc = PAL['lichen'] if n2 < 0.62 else PAL['lichen_orange']
            k = min(1.0, (n2 - (0.55 - lichen)) * 3.0) * 0.8
            c = [a * (1 - k) + b * k for a, b in zip(c, lc)]
        j = rnd.uniform(-jitter, jitter)
        out = (max(0, c[0] + j), max(0, c[1] + j), max(0, c[2] + j), 0.0)
        cache[v.index] = out
        return out
    return fn


def _rock(name, size, center, rnd, seed=None, cuts=1, noise=0.03, rot_z=0.0, tilt=(0.0, 0.0),
          pal='basalt', moss=0.35, lichen=0.25, rgb=None):
    seed = rnd.randint(0, 99999) if seed is None else seed
    o = C.make_box(name, size, center=(0, 0, 0))
    if cuts:
        bm = bmesh.new()
        bm.from_mesh(o.data)
        bmesh.ops.subdivide_edges(bm, edges=bm.edges, cuts=cuts, use_grid_fill=True)
        bm.to_mesh(o.data)
        bm.free()
    if noise:
        C.displace_mesh_noise(o, seed, strength=noise, scale=1.8)
    m = Matrix.Translation(Vector(center)) @ Matrix.Rotation(rot_z, 4, 'Z') \
        @ Matrix.Rotation(tilt[0], 4, 'X') @ Matrix.Rotation(tilt[1], 4, 'Y')
    o.data.transform(m)
    o.data.update()
    M.assign(o, ['M_Stone'])
    C.set_vertex_colors(o, _stone_tint(rgb or rnd.choice(PAL[pal]), seed, rnd, moss=moss, lichen=lichen))
    return o


def _pebble(name, center, half, rnd, pal='basalt', noise=0.25, subdiv=1, rgb=None, mat='M_Stone',
            moss=0.2, lichen=0.1):
    o = C.make_blob(name, center, radius=1.0, seed=rnd.randint(0, 99999), subdivisions=subdiv, noise_scale=1.5,
                    noise_strength=noise, scale=half, relax_iterations=1)
    M.assign(o, [mat])
    C.set_vertex_colors(o, _stone_tint(rgb or rnd.choice(PAL[pal]), rnd.randint(0, 999), rnd, moss=moss,
                                       lichen=lichen))
    return o


# ---------------------------------------------------------------------------
# MARAE: escalones de losas de canto + relleno + enlosado
# ---------------------------------------------------------------------------
def _facing_run(p, rnd, a, b, z0, h, depth, prefix, inward):
    """Fila de losas de canto de a→b (horizontal) con altura h desde z0.
    `inward` es el vector unitario hacia el interior de la plataforma."""
    a, b = Vector(a), Vector(b)
    d = b - a
    L = d.length
    dirv = d.normalized()
    ang = math.atan2(dirv.y, dirv.x)
    s = 0.0
    k = 0
    while s < L - 1e-3:
        ln = rnd.uniform(0.6, 1.1)
        if L - (s + ln) < 0.4:
            ln = L - s
        c = a + dirv * (s + ln / 2) + Vector(inward) * (depth / 2)
        hh = h * rnd.uniform(0.96, 1.04)
        p.add(_rock(f'{prefix}{k}', (ln - 0.03, depth, hh), (c.x, c.y, z0 + hh / 2), rnd,
                    noise=0.035, rot_z=ang + rnd.uniform(-0.02, 0.02),
                    tilt=(rnd.uniform(-0.03, 0.03), 0.0)), 'block')
        s += ln
        k += 1


def _flagstones(p, rnd, x0, x1, y0, y1, z, prefix, th=0.08, gap=0.04):
    y = y0
    r = 0
    while y < y1 - 1e-3:
        rh = rnd.uniform(0.45, 0.7)
        if y1 - (y + rh) < 0.3:
            rh = y1 - y
        x = x0
        k = 0
        while x < x1 - 1e-3:
            ln = rnd.uniform(0.45, 0.85)
            if x1 - (x + ln) < 0.3:
                ln = x1 - x
            p.add(_rock(f'{prefix}{r}_{k}', (ln - gap, rh - gap, th), (x + ln / 2, y + rh / 2, z + th / 2), rnd,
                        noise=0.02, rot_z=rnd.uniform(-0.04, 0.04), moss=0.5), 'block')
            x += ln
            k += 1
        y += rh
        r += 1


def _platform(p, rnd, w, d, z0, h, prefix, pave=False):
    """Un escalón rectangular w x d (centrado) de altura h: perímetro de
    losas de canto + relleno oscuro + (opcional) enlosado encima."""
    dep = 0.32
    hw, hd = w / 2, d / 2
    _facing_run(p, rnd, (-hw, -hd), (hw, -hd), z0, h, dep, f'{prefix}F', (0, 1))
    _facing_run(p, rnd, (hw, hd), (-hw, hd), z0, h, dep, f'{prefix}B', (0, -1))
    _facing_run(p, rnd, (-hw, hd - dep), (-hw, -hd + dep), z0, h, dep, f'{prefix}L', (1, 0))
    _facing_run(p, rnd, (hw, -hd + dep), (hw, hd - dep), z0, h, dep, f'{prefix}R', (-1, 0))
    p.add(_rock(f'{prefix}Fill', (w - 2 * dep + 0.02, d - 2 * dep + 0.02, h - 0.04), (0, 0, z0 + (h - 0.04) / 2),
                rnd, cuts=0, noise=0, moss=0.35, rgb=PAL['basalt'][1]), 'stone')
    if pave:
        _flagstones(p, rnd, -hw + dep, hw - dep, -hd + dep, hd - dep, z0 + h - 0.06, f'{prefix}P')


@_register('marae_small')
def _b_marae_small(v, rnd, name):
    p = K.Parts()
    _platform(p, rnd, 4.2, 3.2, 0.0, TIER_H, 'T0')
    _platform(p, rnd, 4.2 - 2 * TIER_INSET, 3.2 - 2 * TIER_INSET, TIER_H - 0.02, TIER_H, 'T1', pave=True)
    # dos piedras erguidas en el borde trasero
    for i, x in enumerate((-0.9, 0.9)):
        p.add(_standing(rnd, f'SS{i}', 1.1 + i * 0.25, 0.4, (x, 1.25, TIER_H - 0.05)), 'stone')
    return p.finish(name)


@_register('marae_large')
def _b_marae_large(v, rnd, name):
    p = K.Parts()
    w, d = 8.0, 5.6
    for i in range(3):
        _platform(p, rnd, w - 2 * TIER_INSET * i, d - 2 * TIER_INSET * i, i * (TIER_H - 0.02), TIER_H, f'T{i}',
                  pave=(i == 2))
    # escalinata central delantera (misma pieza que Ruin_Marae_Steps)
    _steps(p, rnd, (0, -d / 2), 1.6, 'St')
    # hilera de piedras erguidas en la terraza alta
    top = 3 * (TIER_H - 0.02)
    for i, x in enumerate((-2.0, -0.7, 0.7, 2.0)):
        p.add(_standing(rnd, f'SS{i}', rnd.uniform(1.0, 1.6), 0.42, (x, 1.1, top - 0.05)), 'stone')
    return p.finish(name)


@_register('terrace_straight')
def _b_terrace_straight(v, rnd, name):
    """Tramo modular de 2 m (en X) de terraza de dos escalones: frente del
    escalón bajo en y=-1, del alto en y=-1+TIER_INSET; se encadena en X."""
    p = K.Parts()
    dep = 0.32
    for i in range(2):
        yf = -1.0 + i * TIER_INSET
        z0 = i * (TIER_H - 0.02)
        _facing_run(p, rnd, (-1.0, yf), (1.0, yf), z0, TIER_H, dep, f'F{i}', (0, 1))
        p.add(_rock(f'Fill{i}', (2.0, 1.0 - yf - dep, TIER_H - 0.04), (0, (yf + dep + 1.0) / 2, z0 + (TIER_H - 0.04) / 2),
                    rnd, cuts=0, noise=0, moss=0.35, rgb=PAL['basalt'][1]), 'stone')
    _flagstones(p, rnd, -1.0, 1.0, -1.0 + TIER_INSET + dep, 1.0, 2 * TIER_H - 0.1, 'P')
    return p.finish(name)


@_register('terrace_corner')
def _b_terrace_corner(v, rnd, name):
    """Esquina exterior de la terraza: frentes en -Y y -X."""
    p = K.Parts()
    dep = 0.32
    for i in range(2):
        f = -1.0 + i * TIER_INSET
        z0 = i * (TIER_H - 0.02)
        _facing_run(p, rnd, (f, f), (1.0, f), z0, TIER_H, dep, f'F{i}', (0, 1))
        _facing_run(p, rnd, (f, 1.0), (f, f + dep), z0, TIER_H, dep, f'L{i}', (1, 0))
        p.add(_rock(f'Fill{i}', (1.0 - f - dep, 1.0 - f - dep, TIER_H - 0.04),
                    ((f + dep + 1.0) / 2, (f + dep + 1.0) / 2, z0 + (TIER_H - 0.04) / 2),
                    rnd, cuts=0, noise=0, moss=0.35, rgb=PAL['basalt'][1]), 'stone')
    f = -1.0 + TIER_INSET + dep
    _flagstones(p, rnd, f, 1.0, f, 1.0, 2 * TIER_H - 0.1, 'P')
    return p.finish(name)


@_register('paving')
def _b_paving(v, rnd, name):
    """Losa de patio 2 x 2 m (se encadena en X/Y) con alguna laja hundida."""
    p = K.Parts()
    _flagstones(p, rnd, -1.0, 1.0, -1.0, 1.0, 0.0, 'P', th=0.1, gap=0.05)
    return p.finish(name)


def _steps(p, rnd, front, width, prefix):
    """Dos peldaños que suben un escalón de terraza (TIER_H) en 0,7 m."""
    fx, fy = front
    for i in range(2):
        z1 = TIER_H * (i + 1) / 2
        run = 0.35
        y0 = fy - 0.7 + i * run
        k = 0
        x = fx - width / 2
        while x < fx + width / 2 - 1e-3:
            ln = min(fx + width / 2 - x, rnd.uniform(0.5, 0.8))
            if fx + width / 2 - (x + ln) < 0.25:
                ln = fx + width / 2 - x
            p.add(_rock(f'{prefix}{i}_{k}', (ln - 0.03, 0.7 - i * run, z1), (x + ln / 2, y0 + (0.7 - i * run) / 2, z1 / 2),
                        rnd, noise=0.025, moss=0.45), 'block')
            x += ln
            k += 1


@_register('steps')
def _b_steps(v, rnd, name):
    p = K.Parts()
    _steps(p, rnd, (0, 0.7), 1.6, 'S')
    return p.finish(name)


# ---------------------------------------------------------------------------
# PIEDRAS ERGUIDAS
# ---------------------------------------------------------------------------
def _standing(rnd, name, h, w, base):
    """Losa vertical ahusada y erosionada (más estrecha arriba, cabeza
    redondeada), algo inclinada."""
    o = C.make_box(name, (w, w * 0.45, h), center=(0, 0, h / 2))
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges, cuts=3, use_grid_fill=True)
    for vv in bm.verts:
        t = vv.co.z / h
        taper = 1.0 - 0.3 * t
        vv.co.x *= taper
        vv.co.y *= (1.0 - 0.15 * t)
        if t > 0.8:  # cabeza redondeada
            k = (t - 0.8) / 0.2
            vv.co.z -= (abs(vv.co.x) / (w / 2)) ** 2 * 0.18 * h * 0.3 * k
    bm.to_mesh(o.data)
    bm.free()
    seed = rnd.randint(0, 9999)
    C.displace_mesh_noise(o, seed, strength=0.04 + 0.02 * h, scale=1.6)
    o.data.transform(Matrix.Translation(Vector(base)) @ Matrix.Rotation(rnd.uniform(-0.3, 0.3), 4, 'Z')
                     @ Matrix.Rotation(rnd.uniform(-0.05, 0.05), 4, 'X') @ Matrix.Rotation(rnd.uniform(-0.05, 0.05), 4, 'Y'))
    o.data.update()
    M.assign(o, ['M_Stone'])
    C.set_vertex_colors(o, _stone_tint(rnd.choice(PAL['basalt']), seed, rnd, moss=0.3, lichen=0.3))
    return o


@_register('standing_stone')
def _b_standing_stone(v, rnd, name):
    p = K.Parts()
    p.add(_standing(rnd, 'Slab', v['height'], v['width'], (0, 0, -0.1)), 'stone')
    # piedras de calzo en la base
    for i in range(3):
        a = rnd.uniform(0, 2 * math.pi)
        r = v['width'] * 0.55
        s = rnd.uniform(0.12, 0.2)
        p.add(_pebble(f'Wedge{i}', (math.cos(a) * r, math.sin(a) * r * 0.6, s * 0.4), (s, s * 0.8, s * 0.6), rnd), 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# MUROS DE PIEDRA SECA (2 m, 1,1 m de alto, talud hacia arriba)
# ---------------------------------------------------------------------------
def _drywall(p, rnd, L, H, prefix, profile=None, xform=None):
    """Hiladas de piedras irregulares a matajuntas con talud (más estrecho
    arriba). profile(x) -> altura máxima en ese punto (muro arruinado)."""
    thick0, thick1 = 0.75, 0.5
    z = 0.0
    row = 0
    objs = []
    while z < H - 0.05:
        ch = rnd.uniform(0.2, 0.28) * (1.15 if row == 0 else 1.0)
        t = thick0 + (thick1 - thick0) * (z / H)
        x = -L / 2 - (0.1 if row % 2 else 0.0)
        k = 0
        while x < L / 2 - 0.05:
            ln = rnd.uniform(0.3, 0.55) * (1.2 if row == 0 else 1.0)
            a, b = max(x, -L / 2), min(x + ln, L / 2)
            xc = (a + b) / 2
            if b - a > 0.08 and (profile is None or z + ch * 0.5 < profile(xc)):
                o = _rock(f'{prefix}{row}_{k}', (b - a - 0.02, t * rnd.uniform(0.9, 1.0), ch - 0.02),
                          (xc, rnd.uniform(-0.03, 0.03), z + ch / 2), rnd, noise=0.045,
                          rot_z=rnd.uniform(-0.06, 0.06), tilt=(rnd.uniform(-0.05, 0.05), rnd.uniform(-0.05, 0.05)),
                          moss=0.45)
                objs.append(o)
            x += ln
            k += 1
        z += ch
        row += 1
    # albardilla de piedras de canto
    if profile is None:
        x = -L / 2
        k = 0
        while x < L / 2 - 0.05:
            w = rnd.uniform(0.14, 0.22)
            o = _rock(f'{prefix}Cope{k}', (w, thick1 * 0.9, 0.24), (x + w / 2, 0, z + 0.1), rnd, noise=0.03,
                      tilt=(0, rnd.uniform(-0.15, 0.15)), moss=0.6)
            objs.append(o)
            x += w + 0.01
            k += 1
    for o in objs:
        if xform is not None:
            o.data.transform(xform)
            o.data.update()
        p.add(o, 'block')


@_register('drywall_straight')
def _b_drywall_straight(v, rnd, name):
    p = K.Parts()
    _drywall(p, rnd, 2.0, 1.0, 'W')
    return p.finish(name)


@_register('drywall_ruined')
def _b_drywall_ruined(v, rnd, name):
    p = K.Parts()
    _drywall(p, rnd, 2.0, 1.0, 'W', profile=lambda x: 0.95 - 0.7 * max(0.0, math.sin((x + 0.4) * 1.8)) ** 2)
    for i in range(6):
        s = rnd.uniform(0.14, 0.24)
        p.add(_pebble(f'Fallen{i}', (rnd.uniform(-0.3, 0.9), rnd.choice((-1, 1)) * rnd.uniform(0.5, 0.85), s * 0.5),
                      (s * 1.3, s, s * 0.75), rnd, noise=0.2, moss=0.5), 'none')
    return p.finish(name)


@_register('drywall_corner')
def _b_drywall_corner(v, rnd, name):
    """Esquina en L: dos tramos de 1 m que se encuentran en el origen
    (encaja con tramos rectos de 2 m centrados en ±1 m)."""
    p = K.Parts()
    _drywall(p, rnd, 1.35, 1.0, 'A', xform=Matrix.Translation((0.33, 0, 0)))
    _drywall(p, rnd, 1.0, 1.0, 'B', xform=Matrix.Translation((0, 0.5 + 0.2, 0)) @ Matrix.Rotation(math.pi / 2, 4, 'Z'))
    return p.finish(name)


# ---------------------------------------------------------------------------
# ESTATUAS (diseño propio): cabeza ancha de ojos redondos mirando al cielo,
# diadema de olas, manos con disco estelar
# ---------------------------------------------------------------------------
def _statue_mat(o, rnd, seed, rgb=None):
    M.assign(o, ['M_Stone'])
    C.set_vertex_colors(o, _stone_tint(rgb or PAL['tuff'][0], seed, rnd, moss=0.3, lichen=0.35))


def _rounded_box(name, size, center, cuts=3, bulge=0.25, seed=0, noise=0.02):
    """Caja subdividida y "inflada" hacia una superelipse: volumen blando
    de talla erosionada."""
    o = C.make_box(name, size, center=(0, 0, 0))
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges, cuts=cuts, use_grid_fill=True)
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    for vv in bm.verts:
        u = Vector((vv.co.x / hx, vv.co.y / hy, vv.co.z / hz))
        n = u.length
        if n > 1e-6:
            # interpola entre caja y esfera (proyección radial): bulge=0
            # deja la caja, bulge=1 la convierte en elipsoide
            inf = max(abs(u.x), abs(u.y), abs(u.z))
            f = (1 - bulge) + bulge * (inf / n)
            vv.co.x *= f
            vv.co.y *= f
            vv.co.z *= f
    bm.to_mesh(o.data)
    bm.free()
    if noise:
        C.displace_mesh_noise(o, seed, strength=noise, scale=2.5)
    o.data.transform(Matrix.Translation(Vector(center)))
    o.data.update()
    return o


def _head(rnd, seed, prefix, w=0.8):
    """Cabeza en marco local: base del cuello en z=0, cara hacia -Y.
    Devuelve una lista de objetos (se transforman juntos)."""
    objs = []
    hh, dd = w * 0.82, w * 0.72
    head = _rounded_box(f'{prefix}Skull', (w * 1.08, dd * 1.1, hh * 1.1), (0, 0, hh / 2), cuts=4, bulge=0.55,
                        seed=seed, noise=0.02)
    _statue_mat(head, rnd, seed)
    objs.append(head)
    # ojos: discos grandes hundidos con aro claro (rasgo distintivo)
    for s in (-1, 1):
        ring = C.make_cylinder(f'{prefix}EyeRing{s}', radius=w * 0.17, depth=0.06, segments=16,
                               center=(0, 0, 0))
        ring.data.transform(Matrix.Translation((s * w * 0.22, -dd / 2 + 0.005, hh * 0.58))
                            @ Matrix.Rotation(math.pi / 2, 4, 'X'))
        _statue_mat(ring, rnd, seed + 1, rgb=PAL['tuff'][2])
        objs.append(ring)
        eye = C.make_cylinder(f'{prefix}Eye{s}', radius=w * 0.11, depth=0.06, segments=16, center=(0, 0, 0))
        eye.data.transform(Matrix.Translation((s * w * 0.22, -dd / 2 - 0.012, hh * 0.58))
                           @ Matrix.Rotation(math.pi / 2, 4, 'X'))
        M.assign(eye, ['M_Stone'])
        C.set_vertex_colors(eye, C.constant_tint(PAL['eye'], jitter=0.01, rnd=rnd))
        objs.append(eye)
    # nariz ancha y chata
    nose = _rounded_box(f'{prefix}Nose', (w * 0.22, w * 0.14, hh * 0.24), (0, -dd / 2 - w * 0.03, hh * 0.38),
                        cuts=1, bulge=0.5, seed=seed + 2, noise=0.0)
    _statue_mat(nose, rnd, seed + 2)
    objs.append(nose)
    # boca: surco ancho y sereno
    mouth = C.make_box(f'{prefix}Mouth', (w * 0.42, 0.04, hh * 0.05), center=(0, -dd / 2 + 0.008, hh * 0.2))
    M.assign(mouth, ['M_Stone'])
    C.set_vertex_colors(mouth, C.constant_tint(PAL['eye'], jitter=0.01, rnd=rnd))
    objs.append(mouth)
    # orejas pequeñas y redondas (lejos de las orejas largas de los moai)
    for s in (-1, 1):
        ear = _rounded_box(f'{prefix}Ear{s}', (w * 0.1, w * 0.16, hh * 0.24), (s * w * 0.5, 0, hh * 0.52), cuts=1,
                           bulge=0.6, seed=seed + 3, noise=0.0)
        _statue_mat(ear, rnd, seed + 3)
        objs.append(ear)
    # diadema: banda con dientes de ola
    band = C.make_tube(f'{prefix}Band', hh * 0.12, 16, [w * 0.52, w * 0.52], [w * 0.4, w * 0.4], cap_bottom=True,
                       cap_top=True, z0=hh * 0.8)
    band.data.transform(Matrix.Scale(dd / w, 4, (0, 1, 0)))
    _statue_mat(band, rnd, seed + 4, rgb=PAL['tuff'][1])
    objs.append(band)
    for i in range(7):
        a = math.pi * (0.15 + 0.7 * i / 6)
        x, y = math.cos(a) * w * 0.47, -math.sin(a) * dd * 0.47
        tooth = C.make_cylinder(f'{prefix}Wave{i}', radius=w * 0.07, depth=hh * 0.18, segments=6,
                                center=(x, y, hh * 0.92 + hh * 0.09), radius2=0.005)
        _statue_mat(tooth, rnd, seed + 5 + i, rgb=PAL['tuff'][1])
        objs.append(tooth)
    return objs


def _place_objs(objs, m):
    for o in objs:
        o.data.transform(m)
        o.data.update()


@_register('statue_navigator')
def _b_statue_navigator(v, rnd, name):
    p = K.Parts()
    seed = v['seed']
    plinth = _rock('Plinth', (1.0, 0.9, 0.35), (0, 0, 0.175), rnd, noise=0.04, moss=0.6)
    p.add(plinth, 'slab')
    body_h, bw = 1.35, 0.78
    body = _rounded_box('Body', (bw, bw * 0.72, body_h), (0, 0, 0.35 + body_h / 2), cuts=3, bulge=0.3, seed=seed,
                        noise=0.025)
    # ensancha las caderas y estrecha los hombros un poco
    for vv in body.data.vertices:
        t = (vv.co.z - 0.35) / body_h
        vv.co.x *= 1.08 - 0.16 * t
    _statue_mat(body, rnd, seed)
    p.add(body, 'none')
    # brazos en bajorrelieve: del hombro a las manos sobre el vientre
    for s in (-1, 1):
        sh = Vector((s * bw * 0.48, -0.02, 0.35 + body_h * 0.86))
        el = Vector((s * bw * 0.52, -bw * 0.3, 0.35 + body_h * 0.5))
        hd = Vector((s * 0.1, -bw * 0.39, 0.35 + body_h * 0.42))
        for i, (a, b, r) in enumerate(((sh, el, 0.11), (el, hd, 0.095))):
            arm = K._rod(f'Arm{s}{i}', a, b, r, 'M_Stone', PAL['tuff'][0], rnd, segs=10)
            _statue_mat(arm, rnd, seed + 10 + i)
            p.add(arm, 'none')
    # disco estelar entre las manos
    disc = C.make_cylinder('StarDisc', radius=0.17, depth=0.07, segments=16, center=(0, 0, 0))
    disc.data.transform(Matrix.Translation((0, -bw * 0.42, 0.35 + body_h * 0.5)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
    _statue_mat(disc, rnd, seed + 20, rgb=PAL['tuff'][2])
    p.add(disc, 'none')
    for i in range(8):
        a = i * math.pi / 4
        ln = 0.12 if i % 2 == 0 else 0.08
        ray = C.make_box(f'Ray{i}', (0.03, 0.03, ln), center=(0, 0, 0.05 + ln / 2))
        ray.data.transform(Matrix.Translation((0, -bw * 0.42 - 0.04, 0.35 + body_h * 0.5))
                           @ Matrix.Rotation(a, 4, 'Y'))
        M.assign(ray, ['M_Stone'])
        C.set_vertex_colors(ray, C.constant_tint(PAL['eye'], jitter=0.01, rnd=rnd))
        p.add(ray, 'none')
    # cabeza mirando al cielo (inclinada hacia atrás)
    head = _head(rnd, seed + 30, 'H', w=0.86)
    _place_objs(head, Matrix.Translation((0, 0.02, 0.35 + body_h - 0.04)) @ Matrix.Rotation(math.radians(-16), 4, 'X'))
    for o in head:
        p.add(o, 'none')
    return p.finish(name)


@_register('statue_seated')
def _b_statue_seated(v, rnd, name):
    p = K.Parts()
    seed = v['seed']
    base = _rock('Base', (1.2, 1.1, 0.25), (0, 0, 0.125), rnd, noise=0.04, moss=0.6)
    p.add(base, 'slab')
    body = _rounded_box('Body', (0.85, 0.7, 0.8), (0, 0.08, 0.25 + 0.4), cuts=3, bulge=0.5, seed=seed, noise=0.02)
    _statue_mat(body, rnd, seed)
    p.add(body, 'none')
    # rodillas recogidas y brazos que las abrazan
    for s in (-1, 1):
        knee = _rounded_box(f'Knee{s}', (0.3, 0.55, 0.42), (s * 0.19, -0.3, 0.25 + 0.3), cuts=2, bulge=0.6,
                            seed=seed + s, noise=0.015)
        _statue_mat(knee, rnd, seed + 5)
        p.add(knee, 'none')
        arm = K._rod(f'Arm{s}', (s * 0.42, 0.0, 0.25 + 0.68), (s * 0.08, -0.52, 0.25 + 0.46), 0.09, 'M_Stone',
                     PAL['tuff'][0], rnd, segs=10)
        _statue_mat(arm, rnd, seed + 6)
        p.add(arm, 'none')
    head = _head(rnd, seed + 30, 'H', w=0.78)
    _place_objs(head, Matrix.Translation((0, 0.12, 0.25 + 0.78)) @ Matrix.Rotation(math.radians(-22), 4, 'X'))
    for o in head:
        p.add(o, 'none')
    return p.finish(name)


@_register('statue_head_fallen')
def _b_statue_head_fallen(v, rnd, name):
    """Cabeza desprendida tumbada de lado y medio enterrada en arena."""
    p = K.Parts()
    head = _head(rnd, v['seed'], 'H', w=1.1)
    _place_objs(head, Matrix.Translation((0, 0, 0.42)) @ Matrix.Rotation(math.radians(78), 4, 'Y')
                @ Matrix.Rotation(math.radians(-10), 4, 'X') @ Matrix.Translation((0, 0, -0.45)))
    for o in head:
        p.add(o, 'none')
    for i, (c, s) in enumerate([((0.55, 0.1, 0.0), (0.6, 0.7, 0.22)), ((-0.1, 0.45, 0.0), (0.7, 0.4, 0.16)),
                                ((-0.4, -0.35, 0.0), (0.5, 0.45, 0.14))]):
        sand = C.make_blob(f'Sand{i}', c, radius=1.0, seed=v['seed'] + i, subdivisions=2, noise_scale=1.2,
                           noise_strength=0.1, scale=s, relax_iterations=1)
        M.assign(sand, ['M_Stone'])
        C.set_vertex_colors(sand, C.constant_tint(PAL['sand'][i % 2], jitter=0.02, rnd=rnd))
        p.add(sand, 'none')
    # trozo de cuello partido
    p.add(_rock('Neck', (0.5, 0.45, 0.3), (-0.75, -0.1, 0.12), rnd, noise=0.05, pal='tuff', rot_z=0.4,
                tilt=(0.3, 0.2), moss=0.4), 'slab')
    return p.finish(name)


# ---------------------------------------------------------------------------
# PETROGLIFOS: losa tumbada con relieve hundido (geometría real, no solo
# color) y fondo del surco más claro
# ---------------------------------------------------------------------------
def _ellipse(cx, cy, rx, ry, n=16, a0=0.0, a1=2 * math.pi):
    return [(cx + math.cos(a0 + (a1 - a0) * i / n) * rx, cy + math.sin(a0 + (a1 - a0) * i / n) * ry)
            for i in range(n + 1)]


def _motif(kind):
    S = []
    if kind == 'honu':
        S.append(_ellipse(0, 0, 0.26, 0.2))
        S.append([(0, -0.2), (0, 0.2)])
        S.append([(-0.24, 0), (0.24, 0)])
        S.append(_ellipse(0, 0.28, 0.07, 0.07, n=10))
        for s in (-1, 1):
            S.append([(s * 0.2, 0.12), (s * 0.38, 0.24), (s * 0.42, 0.14)])
            S.append([(s * 0.18, -0.13), (s * 0.3, -0.26)])
        S.append([(0, -0.2), (0.03, -0.3)])
    elif kind == 'canoe':
        for dy in (-0.05, 0.08):
            S.append([(-0.45, 0.02 + dy), (-0.3, -0.06 + dy), (0.3, -0.06 + dy), (0.45, 0.02 + dy)])
        for x in (-0.2, 0.0, 0.2):
            S.append([(x, -0.11), (x, 0.03)])
        S.append([(0.0, 0.03), (-0.12, 0.42), (0.18, 0.3), (0.0, 0.03)])
        S.append([(-0.45 + 0.1 * i, -0.3 + (0.04 if i % 2 else 0.0)) for i in range(10)])
    elif kind == 'star':
        S.append(_ellipse(0, 0, 0.33, 0.33, n=24))
        S.append(_ellipse(0, 0, 0.08, 0.08, n=10))
        for i in range(8):
            a = i * math.pi / 4
            r1 = 0.1
            r2 = 0.46 if i % 2 == 0 else 0.38
            S.append([(math.cos(a) * r1, math.sin(a) * r1), (math.cos(a) * r2, math.sin(a) * r2)])
        for i in range(8):
            a = i * math.pi / 4 + math.pi / 8
            S.append(_ellipse(math.cos(a) * 0.25, math.sin(a) * 0.25, 0.025, 0.025, n=6))
    else:  # bird (fragata)
        S.append([(-0.48, 0.12), (-0.25, 0.02), (-0.08, 0.1), (0.0, 0.0), (0.08, 0.1), (0.25, 0.02), (0.48, 0.12)])
        S.append([(0.0, 0.0), (0.0, -0.18)])
        S.append([(0.0, -0.18), (-0.07, -0.34)])
        S.append([(0.0, -0.18), (0.07, -0.34)])
        S.append(_ellipse(0.0, 0.07, 0.05, 0.05, n=8))
        S.append([(-0.3, -0.32), (-0.2, -0.28), (-0.1, -0.32)])
    return S


@_register('petroglyph')
def _b_petroglyph(v, rnd, name):
    p = K.Parts()
    L, W, T = 1.4, 1.05, 0.28
    body = _rock('Body', (L, W, T), (0, 0, T / 2), rnd, noise=0.05, moss=0.25, lichen=0.2)
    p.add(body, 'stone')
    # cara superior densa (malla de 3,5 cm) para que el grabado sea relieve
    nx, ny = 36, 27
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=nx, y_segments=ny, size=0.5)
    for vv in bm.verts:
        vv.co.x *= (L - 0.14)
        vv.co.y *= (W - 0.14)
    me = bpy.data.meshes.new('Top')
    bm.to_mesh(me)
    bm.free()
    top = bpy.data.objects.new('Top', me)
    C.link_object(top)
    off = Vector((v['seed'] % 37, v['seed'] % 11, 0))
    for vv in me.vertices:
        ex = 1.0 - max(abs(vv.co.x) / ((L - 0.14) / 2), abs(vv.co.y) / ((W - 0.14) / 2))
        edge = min(1.0, ex * 6.0)  # 0 en el borde: se funde con la losa
        vv.co.z = T + 0.012 + 0.02 * mnoise.noise(vv.co * 3.0 + off) - (1 - edge) * 0.05
    me.update()
    scale = 0.95 * min(L, W * 1.35) / 1.0
    strokes = [[(x * scale, y * scale) for x, y in s] for s in _motif(v['motif'])]
    depths = C.carve_strokes(top, strokes, width=0.045, depth=0.02)
    M.assign(top, ['M_Stone'])
    base_rgb = rnd.choice(PAL['basalt'])
    inner = _stone_tint(base_rgb, v['seed'], rnd, moss=0.1, lichen=0.12)

    def fn(vv):
        c = inner(vv)
        t = depths.get(vv.index, 0.0)
        return tuple(c[i] * (1 - t) + PAL['groove'][i] * t for i in range(3)) + (0.0,)
    C.set_vertex_colors(top, fn)
    p.add(top, 'none')
    p.transform(Matrix.Rotation(rnd.uniform(-0.06, 0.06), 4, 'X'))
    return p.finish(name)


# ---------------------------------------------------------------------------
# RESTOS DE CANOA DOBLE (madera petrificada, semienterrada)
# ---------------------------------------------------------------------------
def _hull(name, L, W, D, rnd, t_max=1.0, n=16, prof=6, thick=0.05):
    rings = []
    for i in range(n + 1):
        t = t_max * i / n
        s = max(0.08, math.sin(math.pi * t) ** 0.55)
        w, d = W / 2 * s, D * (0.35 + 0.65 * s)
        lift = 0.28 * (2 * t - 1) ** 4
        x = -L / 2 + L * t
        outer = [(x, math.cos(math.pi + math.pi * k / prof) * w, D + lift + math.sin(math.pi + math.pi * k / prof) * d)
                 for k in range(prof + 1)]
        wi, di = max(0.01, w - thick), max(0.01, d - thick)
        inner = [(x, math.cos(math.pi + math.pi * k / prof) * wi, D + lift + math.sin(math.pi + math.pi * k / prof) * di)
                 for k in reversed(range(prof + 1))]
        rings.append([Vector(q) for q in outer + inner])
    o = C.ring_loft(name, rings, cap_start=True, cap_end=True)
    M.assign(o, ['M_Wood'])
    base = rnd.choice(PAL['driftwood'])
    C.set_vertex_colors(o, _stone_tint(base, rnd.randint(0, 999), rnd, moss=0.1, lichen=0.35))
    return o


@_register('canoe_wreck')
def _b_canoe_wreck(v, rnd, name):
    p = K.Parts()
    L, W, D = 5.6, 0.62, 0.42
    h1 = _hull('HullA', L, W, D, rnd)
    h1.data.transform(Matrix.Translation((0, -1.0, -0.12)) @ Matrix.Rotation(0.12, 4, 'X'))
    p.add(h1, 'none')
    # el casco de estribor está partido: falta el último tercio
    h2 = _hull('HullB', L, W, D, rnd, t_max=0.68)
    h2.data.transform(Matrix.Translation((0.15, 1.0, -0.18)) @ Matrix.Rotation(-0.2, 4, 'X')
                      @ Matrix.Rotation(0.05, 4, 'Z'))
    p.add(h2, 'none')
    # travesaños (iako): uno entero, uno partido, uno caído
    p.add(K._rod('Iako0', (-1.2, -1.05, D + 0.05), (-1.1, 1.05, D + 0.0), 0.07, 'M_Wood', PAL['driftwood'][1], rnd,
                 segs=8), 'pole')
    p.add(K._rod('Iako1', (0.6, -1.05, D + 0.05), (0.65, 0.1, D - 0.05), 0.07, 'M_Wood', PAL['driftwood'][0], rnd,
                 segs=8), 'pole')
    p.add(K._rod('Iako2', (1.9, 0.3, 0.07), (2.4, 1.4, 0.07), 0.07, 'M_Wood', PAL['driftwood'][2], rnd, segs=8), 'pole')
    # tocón del mástil y tablas de la plataforma
    p.add(K._rod('Mast', (-0.3, -0.05, D - 0.1), (-0.25, 0.0, D + 0.9), 0.08, 'M_Wood', PAL['driftwood'][1], rnd,
                 segs=10, taper=0.7), 'pole')
    for i in range(4):
        x = -0.9 + i * 0.3
        p.add(K._box(f'Deck{i}', (0.24, 1.3, 0.04), (x, -0.25, D + 0.1), 'M_Wood', PAL['driftwood'][i % 3], rnd,
                     rot_z=rnd.uniform(-0.1, 0.1)), 'wood')
    # dunas que tapan parte de los cascos
    for i, (c, s) in enumerate([((2.3, -1.0, 0.0), (1.1, 0.8, 0.32)), ((-2.4, 1.1, 0.0), (1.0, 0.9, 0.25)),
                                ((1.2, 1.2, 0.0), (0.9, 0.7, 0.3)), ((-1.6, -1.3, 0.0), (0.8, 0.6, 0.18))]):
        sand = C.make_blob(f'Dune{i}', c, radius=1.0, seed=v['seed'] + i, subdivisions=2, noise_scale=1.1,
                           noise_strength=0.12, scale=s, relax_iterations=1)
        M.assign(sand, ['M_Stone'])
        C.set_vertex_colors(sand, C.constant_tint(PAL['sand'][i % 2], jitter=0.02, rnd=rnd))
        p.add(sand, 'none')
    obj = p.finish(name)
    # recorta lo que queda por debajo del suelo (casco enterrado)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, 0), plane_no=(0, 0, 1), clear_inner=True)
    # el corte deja astillas de área ~0 donde roza vértices existentes
    bmesh.ops.dissolve_degenerate(bm, dist=1e-4, edges=bm.edges)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    return obj


# ---------------------------------------------------------------------------
# ALTARES
# ---------------------------------------------------------------------------
def _offerings(p, rnd, cx, cy, z, prefix, n=4):
    for i in range(n):
        x, y = cx + rnd.uniform(-0.35, 0.35), cy + rnd.uniform(-0.18, 0.18)
        s = rnd.uniform(0.04, 0.07)
        shell = C.make_blob(f'{prefix}Shell{i}', (x, y, z + s * 0.45), radius=1.0, seed=rnd.randint(0, 999),
                            subdivisions=1, noise_scale=1.0, noise_strength=0.1, scale=(s * 1.3, s, s * 0.55))
        M.assign(shell, ['M_Stone'])
        C.set_vertex_colors(shell, C.constant_tint(rnd.choice(PAL['shell']), jitter=0.03, rnd=rnd))
        p.add(shell, 'none')


@_register('altar_table')
def _b_altar_table(v, rnd, name):
    p = K.Parts()
    for i, x in enumerate((-0.55, 0.55)):
        p.add(_rock(f'Leg{i}', (0.35, 0.7, 0.65), (x, 0, 0.325), rnd, noise=0.04, moss=0.5), 'stone')
    p.add(_rock('Top', (1.7, 0.95, 0.2), (0, 0, 0.75), rnd, cuts=2, noise=0.03, moss=0.3, lichen=0.35), 'stone')
    bowl = C.make_tube('Bowl', 0.12, 12, [0.13, 0.17, 0.18], [0.1, 0.14, 0.15], cap_bottom=True, cap_top=True,
                       z0=0.85)
    bowl.data.transform(Matrix.Translation((0.35, 0.1, 0)))
    M.assign(bowl, ['M_Stone'])
    C.set_vertex_colors(bowl, C.constant_tint(PAL['basalt'][4], jitter=0.02, rnd=rnd))
    p.add(bowl, 'none')
    _offerings(p, rnd, -0.3, 0.0, 0.85, 'O', n=5)
    # cordón de fibra atado alrededor de una de las patas
    p.add(K._rod('Cord', (0.55, 0, 0.45), (0.55, 0, 0.5), 0.2, 'M_Fabric', PAL['cord'], rnd, segs=12), 'none')
    for i, x in enumerate((-0.2, 0.2)):
        p.add(_pebble(f'Step{i}', (x, -0.62, 0.05), (0.22, 0.14, 0.07), rnd, moss=0.5), 'none')
    return p.finish(name)


@_register('altar_offering')
def _b_altar_offering(v, rnd, name):
    """Piedra de ofrendas baja con cazoletas talladas (hoyuelos) y conchas."""
    p = K.Parts()
    o = _pebble('Boulder', (0, 0, 0.3), (0.75, 0.6, 0.38), rnd, noise=0.12, subdiv=3, moss=0.3, lichen=0.3)
    # aplana la cara superior y talla 3 cazoletas
    cups = [(-0.25, 0.1), (0.2, -0.08), (0.05, 0.28)]
    for vv in o.data.vertices:
        if vv.co.z > 0.5:
            vv.co.z = 0.5 + (vv.co.z - 0.5) * 0.25
        for cx, cy in cups:
            d = math.hypot(vv.co.x - cx, vv.co.y - cy)
            if d < 0.12 and vv.co.z > 0.45:
                vv.co.z -= 0.06 * (1 - d / 0.12)
    o.data.update()
    p.add(o, 'none')
    _offerings(p, rnd, 0.0, -0.1, 0.52, 'O', n=3)
    for i in range(5):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0.85, 1.1)
        p.add(_pebble(f'Ring{i}', (math.cos(a) * r, math.sin(a) * r * 0.85, 0.06), (0.14, 0.11, 0.08), rnd), 'none')
    return p.finish(name)
