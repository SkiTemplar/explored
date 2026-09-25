"""
Sotobosque — Tools/Blender/assets/shrub.py

4 plantas: helecho, arbusto de hoja ancha, taro (oreja de elefante) y
bambú en mata. Presupuesto orientativo: 1 000-4 000 triángulos.
"""

import os
import sys
import math

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

CATEGORY = 'shrub'

VARIANTS = [
    dict(name='ShrubFern', index=1, seed=3001, kind='fern'),
    dict(name='ShrubBroadleaf', index=1, seed=3002, kind='broadleaf'),
    dict(name='ShrubTaro', index=1, seed=3003, kind='taro'),
    dict(name='ShrubBamboo', index=1, seed=3004, kind='bamboo'),
]


def _build_fern(rnd, seed):
    parts = []
    n_fronds = rnd.randint(11, 16)
    length = rnd.uniform(0.35, 0.6)
    for i in range(n_fronds):
        ang = (2.0 * math.pi * i / n_fronds) + rnd.uniform(-0.35, 0.35)
        frond = C.make_frond_object(
            f'Frond_{i:02d}', length=length * rnd.uniform(0.8, 1.15),
            width=length * 0.42, leaflet_count=rnd.randint(18, 24),
            droop=length * rnd.uniform(0.05, 0.20), seed=seed * 10 + i,
            curl=0.25, rachis_width_ratio=0.015,
        )
        elevation = rnd.uniform(math.radians(15), math.radians(55))
        forward = C.Vector((math.cos(ang), math.sin(ang), math.sin(elevation) * 1.6))
        C.orient_and_place(frond, C.Vector((0, 0, 0.02)), forward, C.Vector((0, 0, 1)))
        C.assign_materials(frond, ['M_Leaf'])
        C.set_vertex_colors(frond, C.tint_along_axis(
            (0.09, 0.30, 0.10), 'y', 0.0, length, curve=0.8, jitter=0.04, rnd=rnd))
        parts.append(frond)
    return parts


def _build_broadleaf(rnd, seed):
    parts = []
    trunk, lean_dir = C.make_curved_trunk(
        'Stem', height=rnd.uniform(0.5, 0.8), base_radius=rnd.uniform(0.02, 0.035),
        tip_radius=rnd.uniform(0.008, 0.015), curvature=rnd.uniform(0.05, 0.12),
        n_points=5, bevel_resolution=3, rnd=rnd,
    )
    C.assign_materials(trunk, ['M_Bark'])
    C.set_vertex_colors(trunk, C.constant_tint((0.22, 0.16, 0.10), alpha=0.0, jitter=0.02, rnd=rnd))
    parts.append(trunk)

    n_leaves = rnd.randint(15, 21)
    stem_top = trunk.dimensions.z
    for i in range(n_leaves):
        leaf_len = rnd.uniform(0.14, 0.24)
        leaf = C.make_leaf_blade(
            f'Leaf_{i:02d}', length=leaf_len, width_base=leaf_len * 0.55,
            width_tip=leaf_len * 0.08, curve_amount=leaf_len * 0.3,
            segments=6, double_sided=True,
        )
        ang = rnd.uniform(0, 2 * math.pi)
        elevation = rnd.uniform(math.radians(10), math.radians(70))
        origin = C.Vector((0, 0, rnd.uniform(stem_top * 0.35, stem_top * 0.95)))
        forward = C.Vector((math.cos(ang), math.sin(ang), math.sin(elevation)))
        C.orient_and_place(leaf, origin, forward, C.Vector((0, 0, 1)))
        C.assign_materials(leaf, ['M_Leaf'])
        C.set_vertex_colors(leaf, C.tint_along_axis(
            (0.10, 0.33, 0.13), 'y', 0.0, leaf_len, curve=0.9, jitter=0.05, rnd=rnd))
        parts.append(leaf)
    return parts


def _build_taro(rnd, seed):
    parts = []
    n_leaves = rnd.randint(3, 5)
    for i in range(n_leaves):
        petiole_h = rnd.uniform(0.35, 0.55)
        petiole, _ = C.make_curved_trunk(
            f'Petiole_{i:02d}', height=petiole_h, base_radius=rnd.uniform(0.012, 0.02),
            tip_radius=rnd.uniform(0.008, 0.014), curvature=petiole_h * rnd.uniform(0.15, 0.3),
            n_points=5, bevel_resolution=2, rnd=rnd,
        )
        C.assign_materials(petiole, ['M_Grass'])
        C.set_vertex_colors(petiole, C.constant_tint((0.24, 0.40, 0.16), alpha=0.0, jitter=0.02, rnd=rnd))
        ang = (2.0 * math.pi * i / n_leaves) + rnd.uniform(-0.3, 0.3)
        C.orient_and_place(petiole, C.Vector((0, 0, 0)),
                            C.Vector((math.cos(ang) * 0.3, math.sin(ang) * 0.3, 1.0)),
                            C.Vector((0, 0, 1)))
        parts.append(petiole)

        leaf_len = rnd.uniform(0.35, 0.5)
        leaf = C.make_leaf_blade(
            f'Blade_{i:02d}', length=leaf_len, width_base=leaf_len * 0.7,
            width_tip=leaf_len * 0.55, curve_amount=leaf_len * 0.22,
            segments=5, double_sided=False,
        )
        top = C.Vector((math.cos(ang) * petiole_h * 0.3, math.sin(ang) * petiole_h * 0.3, petiole_h))
        tilt = rnd.uniform(math.radians(30), math.radians(60))
        forward = C.Vector((math.cos(ang), math.sin(ang), -math.sin(tilt) * 0.3))
        C.orient_and_place(leaf, top, forward, C.Vector((0, 0, 1)))
        C.assign_materials(leaf, ['M_Leaf'])
        C.set_vertex_colors(leaf, C.tint_along_axis(
            (0.09, 0.34, 0.14), 'y', 0.0, leaf_len, curve=1.0, jitter=0.04, rnd=rnd))
        parts.append(leaf)
    return parts


def _build_bamboo(rnd, seed):
    parts = []
    n_culms = rnd.randint(4, 6)
    for i in range(n_culms):
        h = rnd.uniform(2.2, 3.6)
        base_r = rnd.uniform(0.025, 0.045)
        culm, lean_dir = C.make_curved_trunk(
            f'Culm_{i:02d}', height=h, base_radius=base_r, tip_radius=base_r * 0.7,
            curvature=h * rnd.uniform(0.03, 0.09), n_points=6, bevel_resolution=2,
            wobble=h * 0.005, rnd=rnd,
        )
        C.add_ring_bumps(culm, spacing=h / rnd.uniform(7, 10), amplitude=base_r * 0.35,
                          rnd=rnd, sharpness=8)
        ang = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0.0, 0.18)
        culm.location = (math.cos(ang) * r, math.sin(ang) * r, 0)
        C.select_only(culm)
        import bpy
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        C.assign_materials(culm, ['M_Grass'])
        C.set_vertex_colors(culm, C.tint_along_axis(
            (0.55, 0.62, 0.28), 'z', 0.0, h, curve=1.0, jitter=0.04, rnd=rnd))
        parts.append(culm)

        top = C.spline_point(h, 1.0, h * 0.05, lean_dir) + C.Vector(culm.location)
        n_leaves = rnd.randint(3, 6)
        for j in range(n_leaves):
            leaf_len = rnd.uniform(0.18, 0.3)
            leaf = C.make_leaf_blade(
                f'BambooLeaf_{i:02d}_{j:02d}', length=leaf_len, width_base=leaf_len * 0.16,
                width_tip=leaf_len * 0.01, curve_amount=leaf_len * 0.35, segments=3,
                double_sided=False,
            )
            lang = rnd.uniform(0, 2 * math.pi)
            forward = C.Vector((math.cos(lang), math.sin(lang), rnd.uniform(0.1, 0.5)))
            C.orient_and_place(leaf, top, forward, C.Vector((0, 0, 1)))
            C.assign_materials(leaf, ['M_Leaf'])
            C.set_vertex_colors(leaf, C.tint_along_axis(
                (0.20, 0.40, 0.15), 'y', 0.0, leaf_len, curve=0.9, jitter=0.03, rnd=rnd))
            parts.append(leaf)
    return parts


_BUILDERS = {
    'fern': _build_fern,
    'broadleaf': _build_broadleaf,
    'taro': _build_taro,
    'bamboo': _build_bamboo,
}


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    parts = _BUILDERS[variant['kind']](rnd, variant['seed'])
    obj = C.join_objects(parts, 'SM_' + variant['name'])
    C.shade_smooth_auto(obj, angle_deg=45.0)
    C.add_basic_uv(obj)
    return obj
