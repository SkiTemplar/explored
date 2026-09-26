"""
Tools/Blender/animals/bat.py — murciélago frugívoro (Cuevas, biblia §6).

Cuerpo pequeño con orejas grandes, sin cola (los frugívoros Pteropodidae no
la tienen, a diferencia de los micromurciélagos) y dos alas membranosas de
2 segmentos, mucho más largas y planas que las de un ave, en su pose de
reposo COLGADA: se envuelven hacia abajo y hacia atrás alrededor del cuerpo,
como en un murciélago posado boca abajo. Locomoción: bird (reutiliza
WingFlap/WingFold de ProceduralGait; no hay ciclo de patas para colgarse).
"""

import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import rig  # noqa: E402

CATEGORY = 'bat'

VARIANTS = [
    dict(
        species='FruitBat', seed=5101,
        hip_height_cm=8.0, body_radii_cm=(5.0, 3.5, 3.8),
        body_color=(0.32, 0.26, 0.20), body_dark=(0.14, 0.11, 0.08),
        head_radii_cm=(2.6, 2.2, 2.3), head_offset_cm=(1.3, 0.0, 0.3),
        neck_len_cm=1.0, neck_dir=(0.50, 0.0, 0.60), neck_radii_cm=[1.6, 1.5],
        snout_len_cm=1.2, snout_dir=(0.9, 0.0, -0.2),
        snout_radii_cm=[(0.8, 0.7), (0.4, 0.4)],
        ear_len_cm=2.2, ear_radii_cm=[(1.3, 0.4), (0.6, 0.2)], ear_dir=(-0.10, 1.0, 0.40),
        eye_offset_cm=(1.6, 1.0, 0.3), eye_radius_cm=0.22,
        leg_upper_cm=2.0, leg_lower_cm=1.8, leg_radius=0.35,
        wing_upper_cm=9.0, wing_lower_cm=11.0,
        wing_radii_cm=[[(1.6, 0.25), (1.1, 0.20)], [(1.1, 0.20), (0.4, 0.15)]],
        wing_dir_upper=(-0.15, 1.0, -0.10), wing_dir_lower=(-0.55, 0.75, -0.50),
        stride_length_cm=4.0, total_length_cm=14.0,
        habitat='Cuevas', behavior='Salen al atardecer',
        use='Guano', diet='frugívoro',
    ),
]


def _build_head(species, cfg, body_color_fn, seed):
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    neck_pivot = (body_rx * 0.75, 0.0, hip_h + body_rz * 0.30)
    d_neck = tuple(C.Vector(cfg['neck_dir']).normalized())
    neck = rig.build_capsule_piece('Neck', 'Body', neck_pivot, 'neck', species,
                                    d_neck, cfg['neck_len_cm'], cfg['neck_radii_cm'],
                                    body_color_fn, segments=6)
    head_pivot = tuple(neck_pivot[i] + d_neck[i] * cfg['neck_len_cm'] for i in range(3))

    hx, hy, hz = cfg['head_radii_cm']
    head_obj = C.make_blob(f'{species}_Head', (0.0, 0.0, 0.0), 1.0, seed=seed, subdivisions=2,
                            noise_strength=0.03, scale=(hx / 100.0, hy / 100.0, hz / 100.0),
                            relax_iterations=1)
    eye_off = cfg['eye_offset_cm']
    eyeL, eyeR = eye_off, (eye_off[0], -eye_off[1], eye_off[2])
    head_obj, eye_centers = rig.attach_eyes(head_obj, [eyeL, eyeR], cfg['eye_radius_cm'], seed=seed)
    head_fn = C.with_eye_dots(body_color_fn, [tuple(c) for c in eye_centers],
                               cfg['eye_radius_cm'] / 100.0 * 1.05)
    head = rig.finalize_piece('Head', 'Neck', head_pivot, 'head', head_obj, species, head_fn)

    snout_pivot = (head_pivot[0] + hx * 0.6, 0.0, head_pivot[2])
    snout = rig.build_capsule_piece('Snout', 'Head', snout_pivot, 'head', species,
                                     tuple(C.Vector(cfg['snout_dir']).normalized()),
                                     cfg['snout_len_cm'], cfg['snout_radii_cm'],
                                     body_color_fn, segments=6)

    ear_pieces = []
    d_ear = tuple(C.Vector(cfg['ear_dir']).normalized())
    for side, sign in (('EarL', 1.0), ('EarR', -1.0)):
        pivot = (head_pivot[0] - hx * 0.1, sign * hy * 0.7, head_pivot[2] + hz * 0.6)
        dir_side = (d_ear[0], d_ear[1] * sign, d_ear[2])
        obj = C.make_tapered_capsule(f'{species}_{side}', dir_side, cfg['ear_len_cm'] / 100.0,
                                      [(r[0] / 100.0, r[1] / 100.0) for r in cfg['ear_radii_cm']],
                                      segments=6, cap_start=True, cap_end=True)
        ear_pieces.append(rig.finalize_piece(side, 'Head', pivot, 'ear', obj, species, body_color_fn))

    return [neck, head, snout] + ear_pieces, head_pivot


def _legs(species, cfg, color_fn):
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    bottom_z = hip_h - body_rz * 0.80
    upper, lower, r = cfg['leg_upper_cm'], cfg['leg_lower_cm'], cfg['leg_radius']
    pieces = []
    for side, sign in (('L', 1.0), ('R', -1.0)):
        hip = (-body_rx * 0.55, sign * body_ry * 0.55, bottom_z)
        d_up = tuple(C.Vector((-0.10, sign * 0.20, -1.0)).normalized())
        d_lo = tuple(C.Vector((0.05, sign * 0.10, -1.0)).normalized())
        chain = rig.build_chain(
            [f'Leg{side}_Upper', f'Leg{side}_Lower'], 'Body', hip, 'leg', species,
            directions=[d_up, d_lo], lengths_cm=[upper, lower],
            radii_profiles_cm=[[r, r * 0.75], [r * 0.7, r * 0.35]],
            color_fn=color_fn, segments=5)
        pieces.extend(chain)
    return pieces


def _wings(species, cfg, color_fn):
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    upper, lower = cfg['wing_upper_cm'], cfg['wing_lower_cm']
    prof_upper, prof_lower = cfg['wing_radii_cm']
    d_up = tuple(C.Vector(cfg['wing_dir_upper']).normalized())
    d_lo = tuple(C.Vector(cfg['wing_dir_lower']).normalized())
    pieces = []
    for side, sign in (('L', 1.0), ('R', -1.0)):
        shoulder = (body_rx * 0.15, sign * body_ry * 0.85, hip_h + body_rz * 0.25)
        dirs = [(d_up[0], d_up[1] * sign, d_up[2]), (d_lo[0], d_lo[1] * sign, d_lo[2])]
        chain = rig.build_chain(
            [f'Wing{side}_Upper', f'Wing{side}_Lower'], 'Body', shoulder, 'wing', species,
            directions=dirs, lengths_cm=[upper, lower],
            radii_profiles_cm=[prof_upper, prof_lower],
            color_fn=color_fn, segments=6)
        pieces.extend(chain)
    return pieces


def build(variant):
    cfg = variant
    species = cfg['species']
    seed = cfg['seed']
    rnd = C.seeded_rng(seed)
    pieces = []

    body_pivot = (0.0, 0.0, cfg['hip_height_cm'])
    body_fn = C.constant_tint(cfg['body_color'], alpha=0.0, jitter=0.025, rnd=rnd)
    body = rig.build_blob_piece('Body', None, body_pivot, 'body', species,
                                 center_offset_cm=(0.0, 0.0, 0.0),
                                 radii_cm=cfg['body_radii_cm'], color_fn=body_fn,
                                 seed=seed, subdivisions=2, noise_strength=0.05)
    pieces.append(body)

    head_pieces, _head_pivot = _build_head(species, cfg, body_fn, seed)
    pieces.extend(head_pieces)

    limb_fn = C.constant_tint(cfg['body_dark'], alpha=0.0, jitter=0.02, rnd=rnd)
    pieces.extend(_legs(species, cfg, limb_fn))
    pieces.extend(_wings(species, cfg, limb_fn))

    locomotion = dict(type=cfg.get('locomotion_type', 'bird'),
                       stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg['hip_height_cm'], total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
