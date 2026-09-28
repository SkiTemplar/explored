"""
Tools/Blender/animals/cephalopod.py — pulpo (Pozas y rocas, biblia §6).

Manto (blob) con los ojos pegados al frente y 8 brazos de 3 segmentos cada
uno repartidos en corona alrededor de la base del manto, curvándose hacia
abajo y hacia dentro (como un pulpo posado camuflándose entre rocas).
Locomoción: octopod (mismo ciclo alterno que los cangrejos: sirve igual
para 8 brazos reptando que para 8 patas, aunque el pulpo en el agua se
mueva casi siempre por propulsión —fuera del alcance de este ciclo—).
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import rig  # noqa: E402

CATEGORY = 'cephalopod'

VARIANTS = [
    dict(
        species='Octopus', seed=4401,
        mantle_radii_cm=(9.0, 7.5, 8.0), mantle_height_cm=9.0,
        color=(0.55, 0.38, 0.42), color_dark=(0.28, 0.17, 0.21),
        eye_offset_cm=(6.4, 3.2, 2.6), eye_radius_cm=1.15,
        n_arms=8,
        arm_seg_len_cm=[10.0, 9.0, 8.0],
        arm_seg_radii_cm=[[1.7, 1.2], [1.2, 0.75], [0.75, 0.2]],
        stride_length_cm=10.0, total_length_cm=58.0,
        habitat='Pozas y rocas', behavior='Se camufla',
        use='Comida', diet='carnívoro', locomotion_type='octopod',
    ),
]


def _arms(species, cfg, color_fn):
    rx, ry, rz = cfg['mantle_radii_cm']
    base_h = cfg['mantle_height_cm']
    n = cfg['n_arms']
    seg_lens = cfg['arm_seg_len_cm']
    seg_radii = cfg['arm_seg_radii_cm']
    pieces = []
    for i in range(n):
        ang = 2.0 * math.pi * i / n
        cos_a, sin_a = math.cos(ang), math.sin(ang)
        hip = (rx * 0.35 * cos_a, ry * 0.35 * sin_a, base_h - rz * 0.75)
        dirs = [
            (cos_a * 0.90, sin_a * 0.90, -0.30),
            (cos_a * 0.60, sin_a * 0.60, -0.75),
            (cos_a * 0.25, sin_a * 0.25, -0.95),
        ]
        dirs = [tuple(C.Vector(d).normalized()) for d in dirs]
        chain = rig.build_chain(
            [f'Arm{i + 1}_Seg1', f'Arm{i + 1}_Seg2', f'Arm{i + 1}_Seg3'],
            'Body', hip, 'tentacle', species,
            directions=dirs, lengths_cm=seg_lens, radii_profiles_cm=seg_radii,
            color_fn=color_fn, segments=6,
        )
        pieces.extend(chain)
    return pieces


def build(variant):
    cfg = variant
    species = cfg['species']
    seed = cfg['seed']
    rnd = C.seeded_rng(seed)
    pieces = []

    rx, ry, rz = cfg['mantle_radii_cm']
    body_pivot = (0.0, 0.0, cfg['mantle_height_cm'])
    body_fn = C.gradient_along_axis(cfg['color_dark'], cfg['color'], 'z',
                                     -rz / 100.0, rz / 100.0, curve=1.1,
                                     jitter=0.03, rnd=rnd)
    body_obj = C.make_blob('Body', (0.0, 0.0, 0.0), 1.0, seed=seed, subdivisions=2,
                            noise_strength=0.07,
                            scale=(rx / 100.0, ry / 100.0, rz / 100.0), relax_iterations=2)

    eye_off = cfg['eye_offset_cm']
    eye_r = cfg['eye_radius_cm']
    body_obj, eye_centers = rig.attach_eyes(
        body_obj, [eye_off, (eye_off[0], -eye_off[1], eye_off[2])], eye_r, seed=seed)
    eyed_fn = C.with_eye_dots(body_fn, eye_centers, eye_r / 100.0 * 1.05)

    body = rig.finalize_piece('Body', None, body_pivot, 'body', body_obj, species, eyed_fn)
    pieces.append(body)

    limb_fn = C.constant_tint(cfg['color'], alpha=0.0, jitter=0.03, rnd=rnd)
    pieces.extend(_arms(species, cfg, limb_fn))

    locomotion = dict(type=cfg.get('locomotion_type', 'octopod'),
                       stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg['mantle_height_cm'], total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
