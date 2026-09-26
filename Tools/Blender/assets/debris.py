"""
Restos de suelo — Tools/Blender/assets/debris.py

Nuevo en la segunda pasada de arte (2026-09-26): tronco caído con musgo,
tocón, rama seca y un grupo de cocos caídos sueltos (los cocos que cuelgan
de la palmera ya existen en palm.py; estos son los que ruedan por el
suelo). Categoría de manifest: 'debris'.
Presupuesto orientativo: 100-4 500 triángulos.
"""

import os
import random
import sys
import math

import bpy
from mathutils import Vector
from mathutils import noise as mnoise

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

CATEGORY = 'debris'

VARIANTS = [
    dict(name='DebrisLogMoss', index=1, seed=6001, kind='log_moss'),
    dict(name='DebrisStump', index=1, seed=6002, kind='stump'),
    dict(name='DebrisBranch', index=1, seed=6003, kind='branch'),
    dict(name='DebrisCoconuts', index=1, seed=6004, kind='coconuts'),
]


def _clamp01(x):
    return max(0.0, min(1.0, x))


def _log_moss_color_fn(length, seed, bark_dark=(0.16, 0.11, 0.07),
                        bark_light=(0.30, 0.23, 0.15), moss=(0.22, 0.38, 0.14)):
    """Corteza con degradado a lo largo del tronco (co.x, tras orientarlo)
    y parches de musgo en las caras que miran hacia arriba (normal.z > 0),
    modulados con ruido para que no sea una banda perfecta."""
    rnd = random.Random(seed)
    offset = Vector((rnd.uniform(-60, 60), rnd.uniform(-60, 60), rnd.uniform(-60, 60)))
    cache = {}

    def fn(v):
        if v.index not in cache:
            n = mnoise.noise(v.co * 4.0 + offset)
            patch = max(0.0, n)
            up = max(0.0, v.normal.z)
            moss_amount = min(1.0, patch * up * 2.2)
            j = rnd.uniform(-0.025, 0.025)
            cache[v.index] = (moss_amount, j)
        moss_amount, j = cache[v.index]
        t = _clamp01((v.co.x + length * 0.5) / length) if length > 0 else 0.5
        r = bark_dark[0] + (bark_light[0] - bark_dark[0]) * t
        g = bark_dark[1] + (bark_light[1] - bark_dark[1]) * t
        b = bark_dark[2] + (bark_light[2] - bark_dark[2]) * t
        r = r * (1.0 - moss_amount) + moss[0] * moss_amount + j
        g = g * (1.0 - moss_amount) + moss[1] * moss_amount + j
        b = b * (1.0 - moss_amount) + moss[2] * moss_amount + j
        return (_clamp01(r), _clamp01(g), _clamp01(b), 0.0)
    return fn


def _break_top(obj, height, rnd, amount):
    """Rompe el aro superior de un tronco cortado (tocón): desplaza en Z
    (con algo de ruido horizontal) solo los vértices cercanos a la parte
    alta, para que la sección no quede perfectamente plana."""
    me = obj.data
    offset = Vector((rnd.uniform(-40, 40), rnd.uniform(-40, 40)))
    for v in me.vertices:
        if v.co.z > height * 0.82:
            n = mnoise.noise(Vector((v.co.x * 6.0 + offset.x, v.co.y * 6.0 + offset.y, 0.0)))
            v.co.z += n * amount
    me.update()


def _build_log_moss(rnd, seed):
    length = rnd.uniform(2.5, 4.5)
    radius = rnd.uniform(0.16, 0.26)
    log, _ = C.make_curved_trunk(
        'Log', height=length, base_radius=radius, tip_radius=radius * 0.7,
        curvature=length * rnd.uniform(0.02, 0.05), n_points=7, bevel_resolution=4,
        wobble=length * 0.01, rnd=rnd,
    )
    ang = rnd.uniform(0, 2 * math.pi)
    direction = Vector((math.cos(ang), math.sin(ang), rnd.uniform(-0.03, 0.03)))
    C.orient_and_place_zaxis(log, Vector((0, 0, radius * 0.85)), direction, Vector((0, 0, 1)))
    C.assign_materials(log, ['M_Bark'])
    C.set_vertex_colors(log, _log_moss_color_fn(length, seed))
    return [log]


def _build_stump(rnd, seed):
    height = rnd.uniform(0.45, 0.75)
    radius = rnd.uniform(0.22, 0.34)
    stump, _ = C.make_curved_trunk(
        'Stump', height=height, base_radius=radius, tip_radius=radius * 0.94,
        curvature=height * 0.03, n_points=4, bevel_resolution=5, rnd=rnd,
    )
    _break_top(stump, height, rnd, amount=radius * 0.35)
    C.assign_materials(stump, ['M_Bark'])
    C.set_vertex_colors(stump, C.bark_streaks_tint(
        (0.15, 0.10, 0.07), (0.27, 0.20, 0.13), height, seed, streak_count=10))
    parts = [stump]

    n_flare = rnd.randint(4, 6)
    for i in range(n_flare):
        ang = (2.0 * math.pi * i / n_flare) + rnd.uniform(-0.2, 0.2)
        fin_h = height * rnd.uniform(0.5, 0.8)
        fin = C.make_leaf_blade(
            f'Flare_{i:02d}', length=fin_h, width_base=radius * rnd.uniform(0.7, 1.0),
            width_tip=radius * 0.1, curve_amount=fin_h * 0.2, segments=3, double_sided=False,
        )
        C.orient_and_place(fin, Vector((0, 0, 0)), Vector((0, 0, 1)),
                            Vector((math.cos(ang), math.sin(ang), 0.1)))
        C.assign_materials(fin, ['M_Bark'])
        C.set_vertex_colors(fin, C.bark_streaks_tint(
            (0.13, 0.09, 0.06), (0.24, 0.18, 0.11), fin_h, seed + i, streak_count=4))
        parts.append(fin)
    return parts


def _build_branch(rnd, seed):
    length = rnd.uniform(1.4, 2.6)
    radius = rnd.uniform(0.045, 0.09)
    branch, _ = C.make_curved_trunk(
        'DeadBranch', height=length, base_radius=radius, tip_radius=radius * 0.3,
        curvature=length * rnd.uniform(0.08, 0.16), n_points=6, bevel_resolution=3,
        wobble=length * 0.02, rnd=rnd,
    )
    ang = rnd.uniform(0, 2 * math.pi)
    direction = Vector((math.cos(ang), math.sin(ang), rnd.uniform(-0.05, 0.1)))
    C.orient_and_place_zaxis(branch, Vector((0, 0, radius * 0.8)), direction, Vector((0, 0, 1)))
    dry_dark = (0.22, 0.18, 0.13)
    dry_light = (0.40, 0.34, 0.25)
    C.assign_materials(branch, ['M_Bark'])
    C.set_vertex_colors(branch, C.bark_streaks_tint(dry_dark, dry_light, length, seed, streak_count=7))
    parts = [branch]

    n_stubs = rnd.randint(0, 2)
    for i in range(n_stubs):
        stub_len = length * rnd.uniform(0.12, 0.22)
        stub_radius = radius * 0.4
        stub, _ = C.make_curved_trunk(
            f'Stub_{i:02d}', height=stub_len, base_radius=stub_radius,
            tip_radius=stub_radius * 0.3, curvature=stub_len * 0.2,
            n_points=3, bevel_resolution=2, rnd=rnd,
        )
        t = rnd.uniform(0.2, 0.8)
        along = direction.normalized() * (length * t)
        s_ang = rnd.uniform(0, 2 * math.pi)
        s_dir = Vector((math.cos(s_ang), math.sin(s_ang), rnd.uniform(0.2, 0.6)))
        C.orient_and_place_zaxis(stub, Vector((0, 0, radius * 0.8)) + along, s_dir, Vector((0, 0, 1)))
        C.assign_materials(stub, ['M_Bark'])
        C.set_vertex_colors(stub, C.bark_streaks_tint(dry_dark, dry_light, stub_len, seed + 50 + i,
                                                         streak_count=4))
        parts.append(stub)
    return parts


def _build_coconuts(rnd, seed):
    parts = []
    n = rnd.randint(3, 5)
    for i in range(n):
        ang = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0.0, 0.35)
        radius = rnd.uniform(0.09, 0.13)
        center = Vector((math.cos(ang) * r, math.sin(ang) * r, radius * 0.6))
        coco = C.make_blob(f'Coco_{i:02d}', center, radius=radius, seed=seed * 100 + i,
                            subdivisions=2, noise_scale=2.2, noise_strength=0.14,
                            scale=(1.0, 1.0, rnd.uniform(0.85, 1.0)))
        C.assign_materials(coco, ['M_Bark'])
        husked = rnd.random() < 0.4
        base = (0.42, 0.36, 0.22) if husked else (0.28, 0.20, 0.13)
        C.set_vertex_colors(coco, C.constant_tint(base, alpha=0.0, jitter=0.03, rnd=rnd))
        parts.append(coco)
    return parts


_BUILDERS = {
    'log_moss': _build_log_moss,
    'stump': _build_stump,
    'branch': _build_branch,
    'coconuts': _build_coconuts,
}


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    parts = _BUILDERS[variant['kind']](rnd, variant['seed'])
    obj = C.join_objects(parts, 'SM_' + variant['name'])
    C.shade_smooth_auto(obj, angle_deg=45.0)
    C.add_basic_uv(obj)
    return obj
