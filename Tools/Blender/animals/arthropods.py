"""
Tools/Blender/animals/arthropods.py — cangrejo y cangrejo de los cocoteros.

Un único constructor paramétrico (_build_crab): caparazón (blob aplanado),
8 patas de 2 segmentos repartidas 4+4 a los lados (postura clásica de
cangrejo, patas abiertas hacia fuera y ligeramente hacia abajo), una pinza
grande de 2 segmentos por lado y un único pedúnculo ocular (EyeStalks) con
dos bultos de ojo en la punta. Locomoción: octopod (8 patas +
pinzas, sin contar como «patas» para el ciclo de marcha).
"""

import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import rig  # noqa: E402

CATEGORY = 'arthropod'

VARIANTS = [
    dict(
        species='HermitCrab', seed=4201,
        body_radii_cm=(9.0, 12.0, 5.0), hip_height_cm=6.0,
        color=(0.78, 0.32, 0.16), color_dark=(0.45, 0.15, 0.08),
        leg_upper_cm=8.0, leg_lower_cm=7.0, leg_radius=1.5,
        claw_upper_cm=6.0, claw_lower_cm=5.5, claw_radius=2.4,
        eye_len_cm=2.2, eye_radius_cm=0.55,
        stride_length_cm=9.0, total_length_cm=24.0,
        habitat='Playas', behavior='Cambia de concha y huye',
        use='Cebo, comida', diet='carroñero',
    ),
    dict(
        species='CoconutCrab', seed=4202,
        body_radii_cm=(16.0, 20.0, 9.0), hip_height_cm=11.0,
        color=(0.30, 0.22, 0.55), color_dark=(0.14, 0.10, 0.30),
        leg_upper_cm=15.0, leg_lower_cm=13.0, leg_radius=2.6,
        claw_upper_cm=11.0, claw_lower_cm=10.0, claw_radius=4.2,
        eye_len_cm=3.2, eye_radius_cm=0.85,
        stride_length_cm=16.0, total_length_cm=40.0,
        habitat='Palmerales, noche', behavior='Trepa palmeras y roba',
        use='Comida, logro', diet='omnívoro',
    ),
]


def _legs(species, cfg, color_fn):
    rx, ry, rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    upper, lower, r = cfg['leg_upper_cm'], cfg['leg_lower_cm'], cfg['leg_radius']
    pieces = []
    n_per_side = 4
    for side, sign in (('L', 1.0), ('R', -1.0)):
        for i in range(n_per_side):
            t = (i + 0.5) / n_per_side  # 0..1 de morro a popa
            x = rx * (0.75 - 1.5 * t)
            hip = (x, sign * ry * 0.78, hip_h)
            out_ang = math.radians(35 + i * 12)
            dir_up = (math.sin(out_ang) * -0.25, sign * math.cos(out_ang), -0.55)
            dir_lo = (math.sin(out_ang) * 0.15, sign * math.cos(out_ang) * 0.5, -1.0)
            d_up = tuple(C.Vector(dir_up).normalized())
            d_lo = tuple(C.Vector(dir_lo).normalized())
            chain = rig.build_chain(
                [f'Leg{side}{i + 1}_Upper', f'Leg{side}{i + 1}_Lower'], 'Body', hip, 'leg',
                species, directions=[d_up, d_lo], lengths_cm=[upper, lower],
                radii_profiles_cm=[[r, r * 0.8], [r * 0.75, r * 0.35]],
                color_fn=color_fn, segments=6)
            pieces.extend(chain)
    return pieces


def _claws(species, cfg, color_fn):
    rx, ry, rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    upper, lower, r = cfg['claw_upper_cm'], cfg['claw_lower_cm'], cfg['claw_radius']
    pieces = []
    for side, sign in (('L', 1.0), ('R', -1.0)):
        hip = (rx * 0.85, sign * ry * 0.5, hip_h * 1.1)
        d_up = tuple(C.Vector((0.75, sign * 0.45, -0.15)).normalized())
        d_lo = tuple(C.Vector((0.9, sign * 0.1, 0.05)).normalized())
        chain = rig.build_chain(
            [f'Claw{side}_Upper', f'Claw{side}_Lower'], 'Body', hip, 'claw', species,
            directions=[d_up, d_lo], lengths_cm=[upper, lower],
            radii_profiles_cm=[[r, r * 0.75], [(r * 0.9, r * 0.5), (r * 0.3, r * 0.5)]],
            color_fn=color_fn, segments=7)
        pieces.extend(chain)
    return pieces


def _eyestalks(species, cfg, color_fn, seed):
    rx, ry, rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    pivot = (rx * 0.85, 0.0, hip_h + rz * 0.6)
    length = cfg['eye_len_cm'] / 100.0
    stalks = []
    for side, sign in ((0, 1.0), (1, -1.0)):
        stalk = C.make_tapered_capsule(f'{species}_Stalk{side}', (0.4, sign * 0.4, 0.85),
                                        length, [0.006, 0.005], segments=5,
                                        cap_start=True, cap_end=True)
        stalks.append(stalk)
    obj = C.join_objects(stalks, f'{species}_EyeStalks')
    tip0 = C.Vector((0.4, 0.4, 0.85)).normalized() * length
    tip1 = C.Vector((0.4, -0.4, 0.85)).normalized() * length
    obj, eye_centers = rig.attach_eyes(obj, [tuple(v * 100.0 for v in tip0),
                                              tuple(v * 100.0 for v in tip1)],
                                        cfg['eye_radius_cm'], seed=seed)
    eyed_fn = C.with_eye_dots(color_fn, eye_centers, cfg['eye_radius_cm'] / 100.0 * 1.1)
    return rig.finalize_piece('EyeStalks', 'Body', pivot, 'eye', obj, species, eyed_fn)


def build(variant):
    cfg = variant
    species = cfg['species']
    seed = cfg['seed']
    rnd = C.seeded_rng(seed)
    pieces = []

    rx, ry, rz = cfg['body_radii_cm']
    body_pivot = (0.0, 0.0, cfg['hip_height_cm'])
    body_fn = C.gradient_along_axis(cfg['color_dark'], cfg['color'], 'z',
                                     -rz / 100.0, rz / 100.0, curve=1.2,
                                     jitter=0.03, rnd=rnd)
    body = rig.build_blob_piece('Body', None, body_pivot, 'body', species,
                                 center_offset_cm=(0.0, 0.0, 0.0), radii_cm=cfg['body_radii_cm'],
                                 color_fn=body_fn, seed=seed, subdivisions=2, noise_strength=0.05)
    pieces.append(body)

    limb_fn = C.constant_tint(cfg['color'], alpha=0.0, jitter=0.02, rnd=rnd)
    pieces.extend(_legs(species, cfg, limb_fn))
    pieces.extend(_claws(species, cfg, limb_fn))
    pieces.append(_eyestalks(species, cfg, body_fn, seed))

    locomotion = dict(type='octopod', stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg['hip_height_cm'], total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
