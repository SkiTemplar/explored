"""
Tools/Blender/animals/quadrupeds.py — Jabalí, Canela (perra) y mono
capuchino, kit de fauna.

Un único constructor paramétrico (build) cubre las tres especies. Técnica
2ª pasada (feedback de arte: las primitivas quedaban «pegadas» y sin
encanto): cada pieza es una malla orgánica vía el modificador Skin
(rig.py: build_blob_piece/build_capsule_piece/build_chain/build_blade_piece,
todas sobre common.make_skin_*), no cápsulas ahusadas hechas a mano — la
cabeza y el hocico son una única cadena de Skin soldada (sin costura de
unión), las patas se solapan con el cuerpo, y las orejas son una «hoja»
orgánica (build_blade_piece) en vez de un tubo: permite tanto la oreja
puntiaguda y erguida del jabalí como la oreja caída de Canela con el
mismo constructor. Cabezas y ojos más grandes a propósito (proporción
«cartoon adorable» del encargo). Convención de ejes de la especie: X
adelante (hocico), Y derecha, Z arriba; el suelo está en Z=0.
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
        hip_height_cm=52.0, body_radii_cm=(36.0, 23.0, 24.0), body_end_taper=0.62,
        body_color=(0.32, 0.25, 0.21), body_dark=(0.14, 0.11, 0.10),
        head_len_cm=30.0, head_dir=(1.0, 0.0, 0.05),
        head_profile_cm=[(9.5, 9.0), (12.5, 11.0), (7.5, 6.0), (3.0, 2.6)],
        ear_len_cm=9.0, ear_width_base_cm=5.0, ear_width_tip_cm=1.5, ear_curve_cm=1.0,
        ear_dir=(-0.30, 1.0, 0.60), ear_up_hint=(0.0, 0.0, 1.0),
        leg_upper_cm=19.0, leg_lower_cm=18.0, leg_radius_upper=6.0, leg_radius_lower=4.2,
        tail_len_cm=12.0, tail_dir=(-0.5, 0.0, 0.6), tail_radii_cm=[3.0, 1.2],
        eye_offset_cm=(10.0, 9.5, 3.3), eye_radius_cm=1.3,
        stride_length_cm=46.0, total_length_cm=95.0,
        habitat='Selva, meseta', behavior='Pasta, embiste si lo acorralas',
        use='Carne, cuero, hueso', diet='herbívoro',
    ),
    dict(
        species='Dog', seed=4102,
        hip_height_cm=40.0, body_radii_cm=(23.0, 14.5, 15.0), body_end_taper=0.78,
        body_color=(0.82, 0.55, 0.32), body_dark=(0.58, 0.36, 0.19),
        chest_color=(0.95, 0.93, 0.88),
        head_len_cm=19.0, head_dir=(1.0, 0.0, 0.10),
        head_profile_cm=[(7.4, 7.2), (10.4, 10.0), (6.8, 6.0), (2.8, 2.5)],
        ear_len_cm=11.0, ear_width_base_cm=5.5, ear_width_tip_cm=3.0, ear_curve_cm=4.5,
        ear_dir=(-0.15, 1.0, -0.35), ear_up_hint=(1.0, 0.0, 0.0),
        leg_upper_cm=15.0, leg_lower_cm=14.5, leg_radius_upper=4.0, leg_radius_lower=2.8,
        tail_seg_len_cm=[8.0, 7.0, 6.0],
        tail_seg_dir=[(-0.60, 0.0, 0.60), (-0.40, 0.0, 0.85), (0.0, 0.0, 1.0)],
        tail_seg_radii=[[2.6, 2.1], [2.1, 1.5], [1.5, 0.7]],
        eye_offset_cm=(6.5, 8.7, 3.0), eye_radius_cm=1.35,
        stride_length_cm=52.0, total_length_cm=72.0,
        habitat='Compañera (todas las islas)', behavior='Sigue a Rodrigo; olfato para rastros',
        use='Compañera, caza, rastreo', diet='omnívoro',
    ),
    dict(
        species='Monkey', seed=4103,
        hip_height_cm=19.0, body_radii_cm=(11.5, 8.5, 9.0), body_end_taper=0.62,
        body_color=(0.55, 0.40, 0.22), body_dark=(0.32, 0.22, 0.12),
        chest_color=(0.86, 0.78, 0.60),
        head_len_cm=9.5, head_dir=(1.0, 0.0, 0.05),
        head_profile_cm=[(4.5, 4.3), (5.8, 5.6), (3.6, 3.4), (1.8, 1.7)],
        ear_len_cm=2.6, ear_width_base_cm=2.4, ear_width_tip_cm=1.8, ear_curve_cm=0.3,
        ear_dir=(-0.10, 1.0, 0.20), ear_up_hint=(1.0, 0.0, 0.0),
        leg_upper_cm=9.0, leg_lower_cm=8.5, leg_radius_upper=2.1, leg_radius_lower=1.5,
        tail_seg_len_cm=[9.0, 8.5, 8.0, 7.0],
        tail_seg_dir=[(-0.70, 0.0, 0.55), (-0.55, 0.0, 0.70),
                      (-0.30, 0.0, 0.85), (0.0, 0.0, 1.0)],
        tail_seg_radii=[[1.6, 1.4], [1.4, 1.1], [1.1, 0.8], [0.8, 0.5]],
        eye_offset_cm=(3.3, 4.9, 1.7), eye_radius_cm=0.85,
        stride_length_cm=20.0, total_length_cm=36.0,
        habitat='Esmeralda', behavior='Tropa; roba objetos si los dejas en el suelo',
        use='Molestia, carisma', diet='frugívoro', locomotion_type='biped_climber',
    ),
]


def _legs(species, cfg, color_fn):
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    half_w = body_ry * 0.68
    front_x = body_rx * 0.58
    back_x = -body_rx * 0.60
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
            radii_profiles_cm=[[ru, ru * 0.85], [ru * 0.80, rl]],
            color_fn=color_fn,
        )
        pieces.extend(chain)
    return pieces


def _build_head(species, cfg, body_color_fn, seed):
    """Cráneo + hocico como UNA sola cadena de Skin (rig.skin_chain): sin
    la costura de unir un cráneo y un hocico aparte, la cabeza entera es
    una superficie continua que se ensancha en la frente y se afina hacia
    el morro. Proporción deliberadamente grande frente al cuerpo (cabeza
    ~35-40% del ancho del pecho) para la lectura «cartoon adorable»."""
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    head_pivot = (body_rx * 0.88, 0.0, hip_h + body_rz * 0.35)

    d_head = tuple(C.Vector(cfg['head_dir']).normalized())
    head_obj = rig.skin_chain(d_head, cfg['head_len_cm'], cfg['head_profile_cm'],
                               overlap_start=True, overlap_end=False)
    head_obj.name = f'{species}_Head'

    eye_off = cfg['eye_offset_cm']
    eye_r = cfg['eye_radius_cm']
    eyeL, eyeR = eye_off, (eye_off[0], -eye_off[1], eye_off[2])
    head_obj, eye_centers = rig.attach_eyes(head_obj, [eyeL, eyeR], eye_r, seed=seed)
    head_fn = C.with_eye_dots(body_color_fn, [tuple(c) for c in eye_centers],
                               eye_r / 100.0 * 1.05)
    head_piece = rig.finalize_piece('Head', 'Body', head_pivot, 'head', head_obj,
                                     species, head_fn)
    return head_piece, head_pivot


def _build_ears(species, cfg, head_pivot, color_fn):
    # el cráneo (el nodo más ancho del perfil de la cabeza, índice 1 de 4)
    # está a 1/3 de la longitud de la cabeza desde su pivote (el perfil
    # tiene 4 nodos -> 3 tramos iguales), no en el propio pivote (que es
    # el punto de anclaje trasero, hacia el cuello).
    hx, hy = cfg['head_profile_cm'][1]
    d_head = tuple(C.Vector(cfg['head_dir']).normalized())
    skull_t = cfg['head_len_cm'] / 3.0
    base = (head_pivot[0] + d_head[0] * skull_t, 0.0,
            head_pivot[2] + d_head[2] * skull_t + hy * 0.75)
    d = cfg['ear_dir']
    up_hint = cfg['ear_up_hint']
    pieces = []
    for side, sign in (('EarL', 1.0), ('EarR', -1.0)):
        pivot = (base[0], sign * hy * 0.60, base[2])
        dir_side = (d[0], d[1] * sign, d[2])
        piece = rig.build_blade_piece(
            side, 'Head', pivot, 'ear', species, dir_side, up_hint,
            cfg['ear_len_cm'], cfg['ear_width_base_cm'], cfg['ear_width_tip_cm'],
            cfg['ear_curve_cm'], color_fn, segments=5)
        pieces.append(piece)
    return pieces


def build(variant):
    cfg = variant
    species = cfg['species']
    seed = cfg['seed']
    rnd = C.seeded_rng(seed)
    pieces = []

    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    body_pivot = (0.0, 0.0, cfg['hip_height_cm'])
    # «torso_fn» es el tinte compartido por TODAS las piezas (cabeza, orejas,
    # cola...); el parche de pecho, en cambio, es una zona en el espacio
    # LOCAL del propio Body y NO debe pasarse a otras piezas -cada una
    # construye su malla cerca de su propio origen local, así que una
    # comprobación de distancia pensada para el Body podría caer «dentro»
    # por coincidencia de escala en una pieza que no tiene nada que ver
    # (así apareció el parche blanco fantasma en la oreja al probarlo)-.
    torso_fn = C.gradient_along_axis(cfg['body_color'], cfg['body_dark'], 'z',
                                      -body_rz / 100.0, body_rz / 100.0,
                                      curve=1.4, jitter=0.015, rnd=rnd)
    body_color_fn = torso_fn
    if 'chest_color' in cfg:
        chest_center = C.Vector((body_rx * 0.35 / 100.0, 0.0, -body_rz * 0.20 / 100.0))
        chest_radius = body_ry * 0.62 / 100.0

        def body_color_fn(v, _base=torso_fn, _c=chest_center, _r=chest_radius,
                           _col=cfg['chest_color']):
            if (v.co - _c).length <= _r:
                return (*_col, 0.0)
            return _base(v)

    body = rig.build_blob_piece('Body', None, body_pivot, 'body', species,
                                 center_offset_cm=(0.0, 0.0, 0.0),
                                 radii_cm=cfg['body_radii_cm'], color_fn=body_color_fn,
                                 end_taper=cfg.get('body_end_taper', 0.62))
    pieces.append(body)

    limb_fn = C.constant_tint(cfg['body_dark'], alpha=0.0, jitter=0.015, rnd=rnd)

    head_piece, head_pivot = _build_head(species, cfg, torso_fn, seed)
    pieces.append(head_piece)
    pieces.extend(_build_ears(species, cfg, head_pivot, torso_fn))

    pieces.extend(_legs(species, cfg, limb_fn))

    if 'tail_len_cm' in cfg:
        tail_pivot = (-body_rx * 0.92, 0.0, cfg['hip_height_cm'] + body_rz * 0.25)
        d_tail = tuple(C.Vector(cfg['tail_dir']).normalized())
        tail = rig.build_capsule_piece('Tail', 'Body', tail_pivot, 'tail', species,
                                        d_tail, cfg['tail_len_cm'], cfg['tail_radii_cm'],
                                        limb_fn)
        pieces.append(tail)
    else:
        tail_pivot = (-body_rx * 0.92, 0.0, cfg['hip_height_cm'] + body_rz * 0.30)
        names = [f'Tail{i + 1}' for i in range(len(cfg['tail_seg_len_cm']))]
        dirs = [tuple(C.Vector(d).normalized()) for d in cfg['tail_seg_dir']]
        chain = rig.build_chain(names, 'Body', tail_pivot, 'tail', species,
                                 directions=dirs, lengths_cm=cfg['tail_seg_len_cm'],
                                 radii_profiles_cm=cfg['tail_seg_radii'],
                                 color_fn=torso_fn)
        pieces.extend(chain)

    locomotion = dict(type=cfg.get('locomotion_type', 'quadruped'),
                       stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg['hip_height_cm'], total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
