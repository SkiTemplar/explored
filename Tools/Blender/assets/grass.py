"""
Hierba y flor — Tools/Blender/assets/grass.py

Segunda pasada de arte (2026-09-26): 2 matas de hierba alta (1-1,5 m) y una
mata de hierba baja más ancha y densa, más la flor tropical. El triángulo
por tarjeta no cambia con la longitud (solo escala el mundo, no la
topología), así que «hierba alta» no dispara el presupuesto.
Presupuesto orientativo: < 700 triángulos.
"""

import os
import sys
import math

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

CATEGORY = 'grass'

VARIANTS = [
    dict(name='GrassTallA', index=1, seed=5001, kind='tall_a'),
    dict(name='GrassTallB', index=1, seed=5002, kind='tall_b'),
    dict(name='GrassLowWide', index=1, seed=5003, kind='low_wide'),
    dict(name='FlowerTropical', index=1, seed=5004, kind='flower'),
]


def _build_clump(rnd, n_blades, length_range, width_ratio, curve_ratio, spread,
                  dark=(0.14, 0.32, 0.10), light=(0.34, 0.58, 0.22),
                  cell_names=('grass_blade_a', 'grass_blade_b')):
    parts = []
    for i in range(n_blades):
        length = rnd.uniform(*length_range)
        blade = C.make_leaf_blade(
            f'Blade_{i:02d}', length=length, width_base=length * width_ratio,
            width_tip=length * width_ratio * 0.05, curve_amount=length * curve_ratio,
            segments=5, double_sided=False, uv_cell=rnd.choice(cell_names),
        )
        ang = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0.0, spread)
        origin = C.Vector((math.cos(ang) * r, math.sin(ang) * r, 0.0))
        elevation = rnd.uniform(math.radians(55), math.radians(85))
        forward = C.Vector((rnd.uniform(-0.3, 0.3), rnd.uniform(-0.3, 0.3), math.sin(elevation)))
        C.orient_and_place(blade, origin, forward, C.Vector((0, 0, 1)))
        C.assign_materials(blade, ['M_Grass'])
        C.set_vertex_colors(blade, C.gradient_along_axis(
            dark, light, 'y', 0.0, length, curve=0.8, jitter=0.04, rnd=rnd))
        parts.append(blade)
    return parts


def _build_flower(rnd, seed):
    parts = []
    stem_h = rnd.uniform(0.18, 0.28)
    stem, _ = C.make_curved_trunk(
        'Stem', height=stem_h, base_radius=0.01, tip_radius=0.006,
        curvature=stem_h * rnd.uniform(0.1, 0.25), n_points=4, bevel_resolution=2, rnd=rnd,
        uv_rect=C.atlas_uv_rect('stem_swatch'),  # material M_Grass: ver nota de uv_rect
    )
    C.assign_materials(stem, ['M_Grass'])
    C.set_vertex_colors(stem, C.gradient_along_axis(
        (0.14, 0.30, 0.10), (0.28, 0.46, 0.18), 'z', 0.0, stem_h, curve=1.0, jitter=0.02, rnd=rnd))
    parts.append(stem)

    n_petals = rnd.randint(5, 6)
    petal_len = rnd.uniform(0.06, 0.10)
    hue = rnd.choice([(0.85, 0.18, 0.28), (0.95, 0.55, 0.10), (0.85, 0.10, 0.45)])
    for i in range(n_petals):
        petal = C.make_leaf_blade(
            f'Petal_{i:02d}', length=petal_len, width_base=petal_len * 0.55,
            width_tip=petal_len * 0.15, curve_amount=petal_len * 0.35, segments=2,
            double_sided=False, uv_cell='flower_petal',
        )
        ang = (2.0 * math.pi * i / n_petals) + rnd.uniform(-0.08, 0.08)
        forward = C.Vector((math.cos(ang), math.sin(ang), 0.35))
        C.orient_and_place(petal, C.Vector((0, 0, stem_h)), forward, C.Vector((0, 0, 1)))
        C.assign_materials(petal, ['M_Leaf'])
        C.set_vertex_colors(petal, C.constant_tint(hue, alpha=0.0, jitter=0.03, rnd=rnd))
        parts.append(petal)

    core = C.make_blob('Core', (0, 0, stem_h), radius=petal_len * 0.18, seed=seed,
                        subdivisions=1, noise_strength=0.05)
    C.assign_materials(core, ['M_Leaf'])
    C.set_vertex_colors(core, C.constant_tint((0.85, 0.72, 0.12), alpha=0.0, jitter=0.02, rnd=rnd))
    C.sphere_uv_into_cell(core, 'stem_swatch')  # M_Leaf Masked: celda opaca, ver nota
    parts.append(core)
    return parts


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    kind = variant['kind']

    if kind == 'tall_a':
        parts = _build_clump(rnd, n_blades=rnd.randint(9, 13), length_range=(1.0, 1.5),
                              width_ratio=0.028, curve_ratio=0.28, spread=0.12)
    elif kind == 'tall_b':
        parts = _build_clump(rnd, n_blades=rnd.randint(7, 10), length_range=(1.1, 1.5),
                              width_ratio=0.035, curve_ratio=0.4, spread=0.10,
                              dark=(0.16, 0.30, 0.09), light=(0.40, 0.56, 0.20))
    elif kind == 'low_wide':
        parts = _build_clump(rnd, n_blades=rnd.randint(20, 28), length_range=(0.18, 0.32),
                              width_ratio=0.075, curve_ratio=0.35, spread=0.22,
                              dark=(0.15, 0.33, 0.11), light=(0.36, 0.60, 0.24))
    else:
        parts = _build_flower(rnd, variant['seed'])

    obj = C.join_objects(parts, 'SM_' + variant['name'])
    C.shade_smooth_auto(obj, angle_deg=50.0)
    return obj
