"""
Tools/Blender/animals/quadrupeds.py — Jabalí y Canela (perra), kit de fauna.

Un único constructor paramétrico (_build_quadruped) cubre ambas especies:
solo cambian las proporciones, colores y algunas piezas opcionales (Canela
tiene Neck+Jaw y cola de 3 segmentos; el jabalí no tiene cuello como pieza
propia -la cabeza cuelga directa del cuerpo- y su cola es una sola pieza).
Convención de ejes de la especie: X adelante (hocico), Y derecha, Z arriba;
el suelo está en Z=0. Cada pieza se construye ya en su pose de reposo (de
pie), con el pivote de objeto en la articulación, sin rotar el objeto.
"""

import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import rig  # noqa: E402

CATEGORY = 'quadruped'

VARIANTS = [
    dict(
        species='Boar', seed=4101,
        hip_height_cm=54.0, body_radii_cm=(38.0, 23.0, 25.0),
        body_color=(0.30, 0.24, 0.20), body_dark=(0.13, 0.10, 0.09),
        has_neck=False, has_jaw=False, tail_segments=1,
        head_radii_cm=(15.0, 11.5, 12.0), head_offset_cm=(7.0, 0.0, 1.0),
        snout_len_cm=13.0, snout_radii_cm=[6.0, 4.2],
        ear_len_cm=8.0, ear_radii_cm=[(4.0, 1.4), (1.2, 0.6)],
        ear_dir=(-0.35, 1.0, 0.55),
        leg_upper_cm=21.0, leg_lower_cm=20.0,
        leg_radius_upper=5.4, leg_radius_lower=3.7,
        tusk_color=(0.86, 0.83, 0.74),
        tail_len_cm=14.0, tail_dir=(-0.55, 0.0, 0.55),
        tail_radii_cm=[3.2, 1.4],
        eye_offset_cm=(9.5, 5.6, 2.0), eye_radius_cm=1.05,
        stride_length_cm=48.0, total_length_cm=95.0,
        habitat='Selva, meseta', behavior='Pasta, embiste si lo acorralas',
        use='Carne, cuero, hueso', diet='herbívoro',
    ),
    dict(
        species='Dog', seed=4102,
        hip_height_cm=42.0, body_radii_cm=(25.0, 14.5, 15.5),
        body_color=(0.72, 0.47, 0.27), body_dark=(0.55, 0.34, 0.19),
        chest_color=(0.90, 0.78, 0.62),
        has_neck=True, has_jaw=True, tail_segments=3,
        neck_len_cm=11.0, neck_dir=(0.55, 0.0, 0.75), neck_radii_cm=[6.4, 5.6],
        head_radii_cm=(11.0, 9.6, 9.4), head_offset_cm=(5.6, 0.0, 1.6),
        jaw_len_cm=8.0, jaw_dir=(0.85, 0.0, -0.35), jaw_radii_cm=[3.8, 2.2],
        ear_len_cm=9.5, ear_radii_cm=[(3.4, 1.3), (2.2, 0.7)],
        ear_dir=(-0.30, 1.0, -0.35),
        leg_upper_cm=17.0, leg_lower_cm=16.5,
        leg_radius_upper=4.0, leg_radius_lower=2.7,
        tail_seg_len_cm=[9.0, 8.0, 7.0],
        tail_seg_dir=[(-0.75, 0.0, 0.45), (-0.55, 0.0, 0.80), (-0.15, 0.0, 0.98)],
        tail_seg_radii=[[2.6, 2.2], [2.2, 1.7], [1.7, 0.9]],
        eye_offset_cm=(7.4, 4.6, 1.6), eye_radius_cm=0.95,
        stride_length_cm=55.0, total_length_cm=78.0,
        habitat='Compañera (todas las islas)', behavior='Sigue a Rodrigo; olfato para rastros',
        use='Compañera, caza, rastreo', diet='omnívoro',
    ),
    dict(
        species='Monkey', seed=4103,
        hip_height_cm=20.0, body_radii_cm=(13.0, 9.0, 10.0),
        body_color=(0.55, 0.40, 0.22), body_dark=(0.32, 0.22, 0.12),
        chest_color=(0.86, 0.78, 0.60),
        has_neck=True, has_jaw=True, tail_segments=4,
        neck_len_cm=5.0, neck_dir=(0.55, 0.0, 0.80), neck_radii_cm=[3.2, 3.0],
        head_radii_cm=(7.0, 6.4, 6.6), head_offset_cm=(3.0, 0.0, 1.0),
        jaw_len_cm=3.2, jaw_dir=(0.85, 0.0, -0.35), jaw_radii_cm=[1.6, 1.0],
        ear_len_cm=3.0, ear_radii_cm=[(2.0, 0.6), (1.0, 0.4)],
        ear_dir=(-0.20, 1.0, 0.30),
        leg_upper_cm=9.5, leg_lower_cm=9.0,
        leg_radius_upper=2.3, leg_radius_lower=1.6,
        tail_seg_len_cm=[9.0, 8.5, 8.0, 7.0],
        tail_seg_dir=[(-0.70, 0.0, 0.55), (-0.55, 0.0, 0.70),
                      (-0.30, 0.0, 0.85), (0.0, 0.0, 1.0)],
        tail_seg_radii=[[1.6, 1.4], [1.4, 1.1], [1.1, 0.8], [0.8, 0.5]],
        eye_offset_cm=(4.6, 3.0, 0.8), eye_radius_cm=0.7,
        stride_length_cm=22.0, total_length_cm=40.0,
        habitat='Esmeralda', behavior='Tropa; roba objetos si los dejas en el suelo',
        use='Molestia, carisma', diet='frugívoro', locomotion_type='biped_climber',
    ),
]


def _legs(species, cfg, color_fn):
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    half_w = body_ry * 0.70
    front_x = body_rx * 0.60
    back_x = -body_rx * 0.62
    bottom_z = hip_h - body_rz * 0.55
    upper = cfg['leg_upper_cm']
    lower = cfg['leg_lower_cm']
    ru = cfg['leg_radius_upper']
    rl = cfg['leg_radius_lower']

    specs = [
        ('LegFL', (front_x, half_w, bottom_z), (0.16, 0.0, -1.0), (-0.14, 0.0, -1.0)),
        ('LegFR', (front_x, -half_w, bottom_z), (0.16, 0.0, -1.0), (-0.14, 0.0, -1.0)),
        ('LegBL', (back_x, half_w, bottom_z), (-0.12, 0.0, -1.0), (0.24, 0.0, -1.0)),
        ('LegBR', (back_x, -half_w, bottom_z), (-0.12, 0.0, -1.0), (0.24, 0.0, -1.0)),
    ]
    pieces = []
    for prefix, hip_pivot, dir_up, dir_lo in specs:
        d_up = tuple(C.Vector(dir_up).normalized())
        d_lo = tuple(C.Vector(dir_lo).normalized())
        chain = rig.build_chain(
            [f'{prefix}_Upper', f'{prefix}_Lower'], 'Body', hip_pivot, 'leg', species,
            directions=[d_up, d_lo],
            lengths_cm=[upper, lower],
            radii_profiles_cm=[[ru, ru * 0.82], [ru * 0.78, rl]],
            color_fn=color_fn, segments=7,
        )
        pieces.extend(chain)
    return pieces


def _build_head(species, cfg, body_color_fn, seed):
    """Cráneo (blob) + morro/hocico (cápsula ahusada unida) para que la
    cabeza no sea una simple elipse: el jabalí necesita hocico alargado y
    Canela un morro más corto y redondeado (regla: «cabeza algo grande» en
    Canela para que resulte entrañable)."""
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    head_pivot = (body_rx * 0.92, 0.0, hip_h + body_rz * 0.28)

    hx, hy, hz = cfg['head_radii_cm']
    ox, oy, oz = cfg['head_offset_cm']
    skull = C.make_blob(f'{species}_HeadSkull', tuple(v / 100.0 for v in (ox, oy, oz)), 1.0,
                         seed=seed, subdivisions=2, noise_strength=0.04,
                         scale=(hx / 100.0, hy / 100.0, hz / 100.0), relax_iterations=1)

    snout_len = cfg.get('snout_len_cm')
    if snout_len:
        snout_origin = tuple(v / 100.0 for v in (ox + hx * 0.55, oy, oz - hz * 0.12))
        snout = C.make_tapered_capsule(
            f'{species}_Snout', (1.0, 0.0, -0.08), snout_len / 100.0,
            [(r / 100.0, r / 100.0) if not isinstance(r, (tuple, list)) else
             (r[0] / 100.0, r[1] / 100.0) for r in cfg['snout_radii_cm']],
            segments=8, cap_start=True, cap_end=True)
        snout.location = snout_origin
        C.select_only(snout)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        head_obj = C.join_objects([skull, snout], f'{species}_Head')
        C.merge_by_distance(head_obj, dist=0.0015)
    else:
        head_obj = skull
        head_obj.name = f'{species}_Head'

    eye_off = cfg['eye_offset_cm']
    eye_r_cm = cfg['eye_radius_cm']
    eyeL = eye_off
    eyeR = (eye_off[0], -eye_off[1], eye_off[2])
    head_obj, eye_centers = rig.attach_eyes(head_obj, [eyeL, eyeR], eye_r_cm, seed=seed)
    eye_centers_m = [tuple(c) for c in eye_centers]

    head_fn = C.with_eye_dots(body_color_fn, eye_centers_m, eye_r_cm / 100.0 * 1.05)
    head_piece = rig.finalize_piece('Head', 'Body', head_pivot, 'head', head_obj,
                                     species, head_fn)
    return head_piece, head_pivot


def _build_ears(species, cfg, head_pivot, color_fn):
    hx, hy, hz = cfg['head_radii_cm']
    ox, oy, oz = cfg['head_offset_cm']
    base = (head_pivot[0] + ox * 0.4, 0.0, head_pivot[2] + hz * 0.75 + oz)
    d = tuple(C.Vector(cfg['ear_dir']).normalized())
    pieces = []
    for side, sign in (('EarL', 1.0), ('EarR', -1.0)):
        pivot = (base[0], sign * hy * 0.55, base[2])
        dir_side = (d[0], d[1] * sign, d[2])
        obj = C.make_tapered_capsule(f'{species}_{side}', dir_side, cfg['ear_len_cm'] / 100.0,
                                      [(r if isinstance(r, (tuple, list)) else (r, r))
                                       for r in [(x / 100.0, y / 100.0) for x, y in cfg['ear_radii_cm']]],
                                      segments=6, cap_start=True, cap_end=True)
        pieces.append(rig.finalize_piece(side, 'Head', pivot, 'ear', obj, species, color_fn))
    return pieces


def build(variant):
    cfg = variant
    species = cfg['species']
    seed = cfg['seed']
    rnd = C.seeded_rng(seed)
    pieces = []

    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    body_pivot = (0.0, 0.0, cfg['hip_height_cm'])
    body_fn = C.gradient_along_axis(cfg['body_color'], cfg['body_dark'], 'z',
                                     -body_rz / 100.0, body_rz / 100.0,
                                     curve=1.4, jitter=0.025, rnd=rnd)
    if 'chest_color' in cfg:
        chest_center = C.Vector((body_rx * 0.55 / 100.0, 0.0, -body_rz * 0.25 / 100.0))
        chest_radius = body_ry * 0.55 / 100.0
        base_fn = body_fn

        def body_fn(v, _base=base_fn, _c=chest_center, _r=chest_radius, _col=cfg['chest_color']):
            if (v.co - _c).length <= _r:
                return (*_col, 0.0)
            return _base(v)

    body = rig.build_blob_piece('Body', None, body_pivot, 'body', species,
                                 center_offset_cm=(0.0, 0.0, 0.0),
                                 radii_cm=cfg['body_radii_cm'], color_fn=body_fn,
                                 seed=seed, subdivisions=2, noise_strength=0.05)
    pieces.append(body)

    limb_fn = C.constant_tint(cfg['body_dark'], alpha=0.0, jitter=0.02, rnd=rnd)

    if cfg['has_neck']:
        neck_pivot = (body_rx * 0.85, 0.0, cfg['hip_height_cm'] + body_rz * 0.15)
        d_neck = tuple(C.Vector(cfg['neck_dir']).normalized())
        neck = rig.build_capsule_piece('Neck', 'Body', neck_pivot, 'neck', species,
                                        d_neck, cfg['neck_len_cm'], cfg['neck_radii_cm'],
                                        body_fn, segments=8)
        pieces.append(neck)
        head_root_pivot = [neck_pivot[i] + d_neck[i] * cfg['neck_len_cm'] for i in range(3)]
        head_parent = 'Neck'
    else:
        head_root_pivot = None
        head_parent = 'Body'

    # _build_head calcula su propio pivote absoluto salvo cuando cuelga de Neck
    head_piece, head_pivot = _build_head(species, cfg, body_fn, seed)
    if head_root_pivot is not None:
        head_pivot = tuple(head_root_pivot)
        head_piece = dict(head_piece)
        head_piece['pivot_cm'] = head_pivot
    head_piece['parent'] = head_parent
    pieces.append(head_piece)

    pieces.extend(_build_ears(species, cfg, head_pivot, body_fn))

    if cfg['has_jaw']:
        hx, hy, hz = cfg['head_radii_cm']
        ox, oy, oz = cfg['head_offset_cm']
        jaw_pivot = (head_pivot[0] + ox * 0.3, 0.0, head_pivot[2] + oz - hz * 0.15)
        d_jaw = tuple(C.Vector(cfg['jaw_dir']).normalized())
        jaw = rig.build_capsule_piece('Jaw', 'Head', jaw_pivot, 'jaw', species,
                                       d_jaw, cfg['jaw_len_cm'], cfg['jaw_radii_cm'],
                                       body_fn, segments=6)
        pieces.append(jaw)

    pieces.extend(_legs(species, cfg, limb_fn))

    if cfg['tail_segments'] == 1:
        tail_pivot = (-body_rx * 0.92, 0.0, cfg['hip_height_cm'] + body_rz * 0.25)
        d_tail = tuple(C.Vector(cfg['tail_dir']).normalized())
        tail = rig.build_capsule_piece('Tail', 'Body', tail_pivot, 'tail', species,
                                        d_tail, cfg['tail_len_cm'], cfg['tail_radii_cm'],
                                        limb_fn, segments=6)
        pieces.append(tail)
    else:
        tail_pivot = (-body_rx * 0.92, 0.0, cfg['hip_height_cm'] + body_rz * 0.30)
        names = [f'Tail{i + 1}' for i in range(cfg['tail_segments'])]
        dirs = [tuple(C.Vector(d).normalized()) for d in cfg['tail_seg_dir']]
        chain = rig.build_chain(names, 'Body', tail_pivot, 'tail', species,
                                 directions=dirs, lengths_cm=cfg['tail_seg_len_cm'],
                                 radii_profiles_cm=cfg['tail_seg_radii'],
                                 color_fn=body_fn, segments=6)
        pieces.extend(chain)

    locomotion = dict(type=cfg.get('locomotion_type', 'quadruped'),
                       stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg['hip_height_cm'], total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
