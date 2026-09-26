"""
Tools/Blender/animals/serpent.py — serpiente arborícola (Selva, biblia §6).

Sin patas: el cuerpo entero es una cadena de segmentos de columna
(«Spine01»..«SpineNN», cada vez más finos) que ya arranca con una ligera
onda lateral en S en su propia pose de reposo, más una cabeza pequeña con
mandíbula y ojos en el extremo frontal (pivote compartido con Spine01, pero
creciendo hacia +X mientras la columna crece hacia -X). Locomoción: reptile
—incluso sin patas, SpineYaw es la que anima la ondulación de avance—.
"""

import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import rig  # noqa: E402

CATEGORY = 'serpent'

VARIANTS = [
    dict(
        species='TreeSnake', seed=4901,
        hip_height_cm=8.0,
        color=(0.20, 0.55, 0.30), color_dark=(0.08, 0.28, 0.14),
        head_radii_cm=(3.0, 2.1, 2.2), head_offset_cm=(1.6, 0.0, 0.1),
        jaw_len_cm=1.8, jaw_dir=(0.9, 0.0, -0.30), jaw_radii_cm=[0.9, 0.5],
        eye_offset_cm=(2.0, 1.2, 0.4), eye_radius_cm=0.28,
        spine_seg_len_cm=[16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0, 9.0, 8.0, 7.0],
        spine_seg_dir=[
            (-1.0, 0.25, 0.02), (-1.0, -0.30, 0.0), (-1.0, 0.32, -0.02),
            (-1.0, -0.30, 0.0), (-1.0, 0.28, 0.0), (-1.0, -0.24, 0.0),
            (-1.0, 0.20, 0.0), (-1.0, -0.16, 0.0), (-1.0, 0.10, 0.0), (-1.0, 0.0, 0.0),
        ],
        spine_seg_radii=[
            [2.2, 2.0], [2.0, 1.8], [1.8, 1.6], [1.6, 1.4], [1.4, 1.15],
            [1.15, 0.95], [0.95, 0.75], [0.75, 0.55], [0.55, 0.35], [0.35, 0.12],
        ],
        stride_length_cm=20.0, total_length_cm=125.0,
        habitat='Selva', behavior='Venenosa, pasiva salvo si la pisas',
        use='Peligro, antídoto', diet='carnívoro',
    ),
]


def _build_head(species, cfg, root_pivot, body_color_fn, seed):
    hx, hy, hz = cfg['head_radii_cm']
    ox, oy, oz = cfg['head_offset_cm']
    skull = C.make_blob(f'{species}_HeadSkull', tuple(v / 100.0 for v in (ox, oy, oz)), 1.0,
                         seed=seed, subdivisions=2, noise_strength=0.03,
                         scale=(hx / 100.0, hy / 100.0, hz / 100.0), relax_iterations=1)
    jaw_origin = tuple(v / 100.0 for v in (ox + hx * 0.3, oy, oz - hz * 0.5))
    jaw = C.make_tapered_capsule(
        f'{species}_Jaw', tuple(C.Vector(cfg['jaw_dir']).normalized()), cfg['jaw_len_cm'] / 100.0,
        [(r / 100.0, r / 100.0) for r in cfg['jaw_radii_cm']], segments=6,
        cap_start=True, cap_end=True)
    jaw.location = jaw_origin
    C.select_only(jaw)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    head_obj = C.join_objects([skull, jaw], f'{species}_Head')
    C.merge_by_distance(head_obj, dist=0.0012)

    eye_off = cfg['eye_offset_cm']
    eyeL, eyeR = eye_off, (eye_off[0], -eye_off[1], eye_off[2])
    head_obj, eye_centers = rig.attach_eyes(head_obj, [eyeL, eyeR], cfg['eye_radius_cm'], seed=seed)
    head_fn = C.with_eye_dots(body_color_fn, [tuple(c) for c in eye_centers],
                               cfg['eye_radius_cm'] / 100.0 * 1.05)
    return rig.finalize_piece('Head', 'Spine01', root_pivot, 'head', head_obj, species, head_fn)


def build(variant):
    cfg = variant
    species = cfg['species']
    seed = cfg['seed']
    rnd = C.seeded_rng(seed)

    n = len(cfg['spine_seg_len_cm'])
    names = [f'Spine{i + 1:02d}' for i in range(n)]
    dirs = [tuple(C.Vector(d).normalized()) for d in cfg['spine_seg_dir']]

    color_fns = []
    for i in range(n):
        t = i / (n - 1)
        col = tuple(cfg['color'][k] + (cfg['color_dark'][k] - cfg['color'][k]) * t for k in range(3))
        color_fns.append(C.constant_tint(col, alpha=0.0, jitter=0.02, rnd=rnd))

    root_pivot = (0.0, 0.0, cfg['hip_height_cm'])
    spine = rig.build_chain(names, None, root_pivot, 'spine', species,
                             directions=dirs, lengths_cm=cfg['spine_seg_len_cm'],
                             radii_profiles_cm=cfg['spine_seg_radii'],
                             color_fn=color_fns, segments=7)
    pieces = list(spine)

    head_piece = _build_head(species, cfg, root_pivot, color_fns[0], seed)
    pieces.append(head_piece)

    locomotion = dict(type=cfg.get('locomotion_type', 'reptile'),
                       stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg['hip_height_cm'], total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
