"""
Tools/Blender/animals/reptiles.py — iguana, gecko y cocodrilo de estuario
(biblia §6). Constructor paramétrico único (build) compartido por las tres:
solo cambian proporciones, colores y si llevan hocico alargado (el
cocodrilo). A diferencia de quadrupeds.py (patas casi verticales bajo el
cuerpo), aquí las patas van «en aspa» —el fémur/húmero sale casi horizontal
hacia fuera y solo la parte baja de la pata baja al suelo—, la postura
característica de un reptil reptando en vez de trotando. Locomoción:
reptile (columna ondulante vía SpineYaw además del ciclo de las 4 patas).
"""

import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import rig  # noqa: E402

CATEGORY = 'reptile'

VARIANTS = [
    dict(
        species='Iguana', seed=4701,
        hip_height_cm=6.0, body_radii_cm=(9.0, 4.0, 4.0),
        color=(0.28, 0.52, 0.24), color_dark=(0.13, 0.27, 0.11),
        head_radii_cm=(4.0, 2.6, 2.8), head_offset_cm=(2.0, 0.0, 0.3),
        jaw_len_cm=2.0, jaw_dir=(0.9, 0.0, -0.3), jaw_radii_cm=[1.0, 0.6],
        eye_offset_cm=(2.6, 2.21, 0.9), eye_radius_cm=0.35,
        leg_upper_cm=4.0, leg_lower_cm=4.0, leg_radius_upper=0.9, leg_radius_lower=0.55,
        tail_seg_len_cm=[8.0, 7.0, 6.0, 5.0, 4.0],
        tail_seg_dir=[(-1.0, 0.10, 0.04), (-1.0, -0.12, 0.0), (-1.0, 0.10, 0.0),
                      (-1.0, -0.08, -0.02), (-1.0, 0.0, 0.0)],
        tail_seg_radii=[[1.0, 0.8], [0.8, 0.6], [0.6, 0.45], [0.45, 0.3], [0.3, 0.1]],
        stride_length_cm=10.0, total_length_cm=32.0,
        habitat='Rocas soleadas', behavior='Toma el sol y huye al agua',
        use='Carne', diet='herbívoro',
    ),
    dict(
        species='Gecko', seed=4702,
        hip_height_cm=1.2, body_radii_cm=(3.0, 1.6, 1.3),
        color=(0.62, 0.55, 0.40), color_dark=(0.34, 0.29, 0.19),
        head_radii_cm=(1.4, 1.1, 1.0), head_offset_cm=(0.8, 0.0, 0.1),
        jaw_len_cm=0.8, jaw_dir=(0.9, 0.0, -0.3), jaw_radii_cm=[0.4, 0.25],
        eye_offset_cm=(1.0, 0.94, 0.35), eye_radius_cm=0.16,
        leg_upper_cm=1.3, leg_lower_cm=1.3, leg_radius_upper=0.30, leg_radius_lower=0.20,
        tail_seg_len_cm=[2.5, 2.2, 1.8, 1.4],
        tail_seg_dir=[(-1.0, 0.15, 0.03), (-1.0, -0.15, 0.0),
                      (-1.0, 0.10, 0.0), (-1.0, 0.0, 0.0)],
        tail_seg_radii=[[0.40, 0.32], [0.32, 0.24], [0.24, 0.16], [0.16, 0.05]],
        stride_length_cm=2.0, total_length_cm=6.0,
        habitat='Nocturno', behavior='Paredes y refugios',
        use='Ambiente', diet='insectívoro',
    ),
    dict(
        species='EstuaryCrocodile', seed=4703,
        hip_height_cm=22.0, body_radii_cm=(70.0, 22.0, 18.0),
        color=(0.24, 0.30, 0.20), color_dark=(0.10, 0.14, 0.08),
        head_radii_cm=(18.0, 12.0, 10.0), head_offset_cm=(9.0, 0.0, -0.5),
        snout_len_cm=42.0, snout_radii_cm=[(9.0, 6.5), (4.0, 3.0)],
        eye_offset_cm=(15.0, 10.2, 6.5), eye_radius_cm=2.1,
        leg_upper_cm=28.0, leg_lower_cm=22.0, leg_radius_upper=8.0, leg_radius_lower=5.2,
        tail_seg_len_cm=[45.0, 40.0, 35.0, 28.0, 20.0],
        tail_seg_dir=[(-1.0, 0.10, 0.02), (-1.0, -0.14, 0.0), (-1.0, 0.14, 0.0),
                      (-1.0, -0.10, 0.0), (-1.0, 0.0, 0.0)],
        tail_seg_radii=[[16.0, 13.0], [13.0, 10.0], [10.0, 7.0], [7.0, 4.0], [4.0, 1.4]],
        stride_length_cm=60.0, total_length_cm=200.0,
        habitat='Manglar', behavior='Emboscada en el agua',
        use='Peligro principal', diet='carnívoro',
    ),
]


def _legs_sprawled(species, cfg, color_fn):
    """Patas «en aspa»: la parte superior sale casi horizontal hacia fuera
    del cuerpo y solo la inferior dobla hacia el suelo (postura reptante,
    frente a las patas casi verticales de quadrupeds.py)."""
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    half_w = body_ry * 0.85
    front_x = body_rx * 0.55
    back_x = -body_rx * 0.58
    bottom_z = hip_h - body_rz * 0.5
    upper, lower = cfg['leg_upper_cm'], cfg['leg_lower_cm']
    ru, rl = cfg['leg_radius_upper'], cfg['leg_radius_lower']

    specs = [
        ('LegFL', (front_x, half_w, bottom_z), (0.30, 1.0, -0.30), (-0.15, 0.30, -1.0)),
        ('LegFR', (front_x, -half_w, bottom_z), (0.30, -1.0, -0.30), (-0.15, -0.30, -1.0)),
        ('LegBL', (back_x, half_w, bottom_z), (-0.20, 1.0, -0.30), (0.20, 0.25, -1.0)),
        ('LegBR', (back_x, -half_w, bottom_z), (-0.20, -1.0, -0.30), (0.20, -0.25, -1.0)),
    ]
    pieces = []
    for prefix, hip_pivot, dir_up, dir_lo in specs:
        d_up = tuple(C.Vector(dir_up).normalized())
        d_lo = tuple(C.Vector(dir_lo).normalized())
        chain = rig.build_chain(
            [f'{prefix}_Upper', f'{prefix}_Lower'], 'Body', hip_pivot, 'leg', species,
            directions=[d_up, d_lo], lengths_cm=[upper, lower],
            radii_profiles_cm=[[ru, ru * 0.8], [ru * 0.75, rl]],
            color_fn=color_fn, segments=7,
        )
        pieces.extend(chain)
    return pieces


def _build_head(species, cfg, body_color_fn, seed):
    """Cráneo (+ hocico si lo hay) como UNA sola cadena de Skin
    (rig.skin_profile_chain): sin la costura de unir un cráneo y un
    hocico aparte con merge_by_distance. El hocico usa longitudes de
    segmento EXPLÍCITAS (no un muestreo uniforme) porque en el cocodrilo
    el hocico es mucho más largo que el propio cráneo -muestrear a
    intervalos regulares lo habría comprimido-."""
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    head_pivot = (body_rx * 0.92, 0.0, hip_h + body_rz * 0.20)

    hx, hy, hz = cfg['head_radii_cm']
    snout_len = cfg.get('snout_len_cm')
    if snout_len:
        snout_radii = [tuple(r) if isinstance(r, (tuple, list)) else (r, r)
                       for r in cfg['snout_radii_cm']]
        seg_lens = [hx * 0.6, hx * 0.8, snout_len]
        radii = [(hy * 0.5, hz * 0.5), (hy, hz), snout_radii[0], snout_radii[1]]
    else:
        seg_lens = [hx * 0.6, hx * 1.0]
        radii = [(hy * 0.5, hz * 0.5), (hy, hz), (hy * 0.25, hz * 0.25)]

    head_obj = rig.skin_profile_chain((1.0, 0.0, 0.0), seg_lens, radii, overlap_start=True)
    head_obj.name = f'{species}_Head'

    eye_off = cfg['eye_offset_cm']
    eye_r_cm = cfg['eye_radius_cm']
    eyeL, eyeR = eye_off, (eye_off[0], -eye_off[1], eye_off[2])
    head_obj, eye_centers = rig.attach_eyes(head_obj, [eyeL, eyeR], eye_r_cm, seed=seed)
    head_fn = C.with_eye_dots(body_color_fn, [tuple(c) for c in eye_centers], eye_r_cm / 100.0 * 1.05)
    head_piece = rig.finalize_piece('Head', 'Body', head_pivot, 'head', head_obj, species, head_fn)

    pieces = [head_piece]
    jaw_len = cfg.get('jaw_len_cm')
    if jaw_len:
        jaw_pivot = (head_pivot[0] + hx * 0.35, 0.0, head_pivot[2] - hz * 0.45)
        jaw = rig.build_capsule_piece('Jaw', 'Head', jaw_pivot, 'jaw', species,
                                       tuple(C.Vector(cfg['jaw_dir']).normalized()),
                                       jaw_len, cfg['jaw_radii_cm'], body_color_fn)
        pieces.append(jaw)
    return pieces, head_pivot


def build(variant):
    cfg = variant
    species = cfg['species']
    seed = cfg['seed']
    rnd = C.seeded_rng(seed)
    pieces = []

    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    body_pivot = (0.0, 0.0, cfg['hip_height_cm'])
    body_fn = C.gradient_along_axis(cfg['color_dark'], cfg['color'], 'z',
                                     -body_rz / 100.0, body_rz / 100.0, curve=1.2,
                                     jitter=0.025, rnd=rnd)
    body = rig.build_blob_piece('Body', None, body_pivot, 'body', species,
                                 center_offset_cm=(0.0, 0.0, 0.0),
                                 radii_cm=cfg['body_radii_cm'], color_fn=body_fn,
                                 seed=seed, subdivisions=2, noise_strength=0.05, scale_extra=(1.0, 1.0, 0.9))
    pieces.append(body)

    head_pieces, _head_pivot = _build_head(species, cfg, body_fn, seed)
    pieces.extend(head_pieces)

    limb_fn = C.constant_tint(cfg['color_dark'], alpha=0.0, jitter=0.02, rnd=rnd)
    pieces.extend(_legs_sprawled(species, cfg, limb_fn))

    tail_pivot = (-body_rx * 0.92, 0.0, cfg['hip_height_cm'] + body_rz * 0.10)
    names = [f'Tail{i + 1}' for i in range(len(cfg['tail_seg_len_cm']))]
    dirs = [tuple(C.Vector(d).normalized()) for d in cfg['tail_seg_dir']]
    tail_chain = rig.build_chain(names, 'Body', tail_pivot, 'tail', species,
                                  directions=dirs, lengths_cm=cfg['tail_seg_len_cm'],
                                  radii_profiles_cm=cfg['tail_seg_radii'],
                                  color_fn=body_fn, segments=6)
    pieces.extend(tail_chain)

    locomotion = dict(type=cfg.get('locomotion_type', 'reptile'),
                       stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg['hip_height_cm'], total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
