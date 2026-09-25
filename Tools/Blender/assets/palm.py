"""
Palmeras cocoteras — Tools/Blender/assets/palm.py

3 variantes de la misma especie (Cocos nucifera estilizada): tronco curvado
y anillado, 7-10 hojas pinnadas curvadas hacia abajo y cocos opcionales.
Presupuesto orientativo: 3 000-6 000 triángulos.
"""

import os
import sys
import math

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

CATEGORY = 'palm'

VARIANTS = [
    dict(name='PalmCoconut', index=1, seed=1001, has_coconuts=True),
    dict(name='PalmCoconut', index=2, seed=1002, has_coconuts=False),
    dict(name='PalmCoconut', index=3, seed=1003, has_coconuts=True),
]


def build(variant):
    """Construye una palmera cocotera y devuelve el objeto final único."""
    rnd = C.seeded_rng(variant['seed'])

    height = rnd.uniform(6.5, 10.5)
    base_radius = rnd.uniform(0.16, 0.22)
    tip_radius = base_radius * rnd.uniform(0.32, 0.42)
    curvature = height * rnd.uniform(0.10, 0.24) * rnd.choice((-1.0, 1.0))

    trunk, lean_dir = C.make_curved_trunk(
        'Trunk', height, base_radius, tip_radius, curvature,
        n_points=10, bevel_resolution=4, wobble=height * 0.01, rnd=rnd,
    )
    C.add_ring_bumps(trunk, spacing=height / rnd.uniform(9, 13),
                      amplitude=base_radius * 0.09, rnd=rnd, sharpness=5)
    C.assign_materials(trunk, ['M_Bark'])
    C.set_vertex_colors(trunk, C.tint_along_axis(
        (0.34, 0.24, 0.15), 'z', 0.0, height, curve=1.0, jitter=0.03, rnd=rnd))

    crown = C.spline_point(height, 1.0, curvature, lean_dir)
    crown_tangent = C.spline_tangent(height, 1.0, curvature, lean_dir)

    parts = [trunk]
    n_fronds = rnd.randint(9, 12)
    frond_length = height * rnd.uniform(0.34, 0.44)
    frond_width = frond_length * rnd.uniform(0.34, 0.42)

    for i in range(n_fronds):
        ang = (2.0 * math.pi * i / n_fronds) + rnd.uniform(-0.25, 0.25)
        elevation = rnd.uniform(math.radians(8), math.radians(35))  # sobre la horizontal
        droop = frond_length * rnd.uniform(0.45, 0.65)

        frond = C.make_frond_object(
            f'Frond_{i:02d}', frond_length, frond_width,
            leaflet_count=rnd.randint(40, 48), droop=droop,
            seed=variant['seed'] * 10 + i, curl=0.18,
        )

        out_dir = (math.cos(ang), math.sin(ang), math.sin(elevation) * 1.4)
        forward = C.Vector(out_dir)
        up = crown_tangent
        C.orient_and_place(frond, crown, forward, up)
        C.assign_materials(frond, ['M_Leaf'])
        C.set_vertex_colors(frond, C.tint_along_axis(
            (0.08, 0.30, 0.11), 'y', 0.0, frond_length, curve=0.8,
            jitter=0.04, rnd=rnd))
        parts.append(frond)

    if variant['has_coconuts']:
        n_coco = rnd.randint(3, 6)
        for i in range(n_coco):
            ang = rnd.uniform(0, 2 * math.pi)
            r = base_radius * rnd.uniform(1.2, 2.0)
            offset = C.Vector((math.cos(ang) * r, math.sin(ang) * r, rnd.uniform(-0.35, -0.05)))
            coco = C.make_blob(f'Coco_{i:02d}', crown + offset,
                                radius=rnd.uniform(0.09, 0.13),
                                seed=variant['seed'] * 100 + i,
                                subdivisions=1, noise_scale=2.5, noise_strength=0.15)
            C.assign_materials(coco, ['M_Bark'])
            C.set_vertex_colors(coco, C.constant_tint((0.30, 0.24, 0.15), alpha=0.0,
                                                        jitter=0.02, rnd=rnd))
            parts.append(coco)

    obj = C.join_objects(parts, 'SM_' + variant['name'])
    C.shade_smooth_auto(obj, angle_deg=42.0)
    C.add_basic_uv(obj)
    return obj
