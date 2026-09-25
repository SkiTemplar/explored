"""
Rocas — Tools/Blender/assets/rock.py

5 rocas: canto rodado, roca plana, peñasco de basalto, roca de playa
redondeada y roca volcánica. Deformación con ruido sobre esfera o cubo
subdividido, siempre suavizada al final (nunca facetado extremo).
Presupuesto orientativo: 1 000-3 000 triángulos.
"""

import os
import sys

import bpy

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

CATEGORY = 'rock'

VARIANTS = [
    dict(name='RockBoulder', index=1, seed=4001, kind='boulder'),
    dict(name='RockFlat', index=1, seed=4002, kind='flat'),
    dict(name='RockBasalt', index=1, seed=4003, kind='basalt'),
    dict(name='RockBeach', index=1, seed=4004, kind='beach'),
    dict(name='RockVolcanic', index=1, seed=4005, kind='volcanic'),
]


def _cube_rock(name, sx, sy, sz, cuts, seed, noise_scale, noise_strength, octaves=2):
    bpy.ops.mesh.primitive_cube_add(size=1.0)
    obj = bpy.context.object
    obj.name = name
    obj.scale = (sx, sy, sz)
    C.select_only(obj)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.subdivide(number_cuts=cuts)
    bpy.ops.object.mode_set(mode='OBJECT')
    C.displace_mesh_noise(obj, seed, strength=noise_strength, scale=noise_scale, octaves=octaves)
    return obj


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    kind = variant['kind']

    if kind == 'boulder':
        radius = rnd.uniform(0.35, 0.55)
        obj = C.make_blob('Rock', (0, 0, radius * 0.85), radius, variant['seed'],
                           subdivisions=4, noise_scale=1.6, noise_strength=0.16,
                           scale=(1.0, rnd.uniform(0.85, 1.1), rnd.uniform(0.75, 0.95)))
        base_color, smooth_angle = (0.34, 0.33, 0.31), 55.0

    elif kind == 'flat':
        sx = rnd.uniform(0.55, 0.85)
        sy = rnd.uniform(0.45, 0.7)
        sz = rnd.uniform(0.10, 0.18)
        obj = _cube_rock('Rock', sx, sy, sz, cuts=9, seed=variant['seed'],
                          noise_scale=2.2, noise_strength=sz * 0.4)
        obj.location.z = sz * 0.5
        C.select_only(obj)
        bpy.ops.object.transform_apply(location=True)
        base_color, smooth_angle = (0.40, 0.39, 0.36), 45.0

    elif kind == 'basalt':
        sx = rnd.uniform(0.28, 0.4)
        sz = rnd.uniform(0.75, 1.15)
        obj = _cube_rock('Rock', sx, sx * rnd.uniform(0.85, 1.1), sz, cuts=9,
                          seed=variant['seed'], noise_scale=1.4, noise_strength=sx * 0.35)
        obj.location.z = sz * 0.5
        C.select_only(obj)
        bpy.ops.object.transform_apply(location=True)
        base_color, smooth_angle = (0.14, 0.13, 0.14), 30.0

    elif kind == 'beach':
        radius = rnd.uniform(0.20, 0.32)
        obj = C.make_blob('Rock', (0, 0, radius * 0.5), radius, variant['seed'],
                           subdivisions=4, noise_scale=2.0, noise_strength=0.11,
                           scale=(1.15, rnd.uniform(0.9, 1.1), 0.55))
        base_color, smooth_angle = (0.52, 0.48, 0.42), 60.0

    else:  # volcanic
        radius = rnd.uniform(0.30, 0.48)
        obj = C.make_blob('Rock', (0, 0, radius * 0.7), radius, variant['seed'],
                           subdivisions=4, noise_scale=1.9, noise_strength=0.30,
                           scale=(1.0, rnd.uniform(0.85, 1.05), rnd.uniform(0.8, 1.0)))
        base_color, smooth_angle = (0.10, 0.09, 0.09), 38.0

    C.assign_materials(obj, ['M_Rock'])
    C.set_vertex_colors(obj, C.constant_tint(base_color, alpha=0.0, jitter=0.045, rnd=rnd))
    C.shade_smooth_auto(obj, angle_deg=smooth_angle)
    C.add_basic_uv(obj, method='CUBE')
    obj.name = 'SM_' + variant['name']
    return obj
