"""
Tools/Blender/animals/birds.py — gaviota, fragata, loro, tucán y gallina
silvestre (biblia §6). Constructor paramétrico único (build): cuerpo (blob),
cuello+cabeza+pico, dos patas biarticuladas finas, dos alas de 2 segmentos
(en su pose de reposo PLEGADA contra el cuerpo —el aleteo lo hace el C++
rotando estas mismas piezas, WingFlap/WingFold de ProceduralGait— ) y una
cola de una pieza aplanada. Locomoción: bird (el ciclo de patas no se anima
en tierra por este sistema; lo que mueve el C++ es el aleteo y la cola).
"""

import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import rig  # noqa: E402

CATEGORY = 'bird'

VARIANTS = [
    dict(
        species='Gull', seed=5001,
        hip_height_cm=14.0, body_radii_cm=(11.0, 7.0, 7.5),
        body_color=(0.95, 0.95, 0.93), body_dark=(0.55, 0.60, 0.66),
        head_radii_cm=(4.2, 3.6, 3.8), head_offset_cm=(2.0, 0.0, 0.6),
        neck_len_cm=3.5, neck_dir=(0.60, 0.0, 0.70), neck_radii_cm=[2.6, 2.4],
        beak_len_cm=3.8, beak_dir=(0.95, 0.0, -0.15),
        beak_radii_cm=[(0.9, 0.7), (0.25, 0.2)], beak_color=(0.95, 0.75, 0.20),
        eye_offset_cm=(2.6, 3.06, 1.0), eye_radius_cm=0.30,
        leg_upper_cm=5.5, leg_lower_cm=5.0, leg_radius=0.55, leg_color=(0.85, 0.35, 0.20),
        wing_len_cm=28.0, wing_width_base_cm=5.2, wing_width_tip_cm=1.2,
        wing_dir=(-0.75, 0.55, 0.10),
        tail_len_cm=8.0, tail_radii_cm=[(3.0, 0.6), (1.0, 0.3)], tail_dir=(-1.0, 0.0, 0.05),
        stride_length_cm=14.0, total_length_cm=45.0,
        habitat='Costa', behavior='Bandadas, roban pescado',
        use='Plumas, huevos', diet='piscívoro',
    ),
    dict(
        species='Frigatebird', seed=5002,
        hip_height_cm=10.0, body_radii_cm=(10.0, 6.0, 6.5),
        body_color=(0.18, 0.16, 0.18), body_dark=(0.07, 0.06, 0.07),
        head_radii_cm=(3.6, 3.0, 3.2), head_offset_cm=(1.8, 0.0, 0.5),
        neck_len_cm=3.0, neck_dir=(0.60, 0.0, 0.70), neck_radii_cm=[2.2, 2.0],
        beak_len_cm=5.5, beak_dir=(0.95, 0.0, -0.25),
        beak_radii_cm=[(0.7, 0.55), (0.15, 0.15)], beak_color=(0.15, 0.12, 0.10),
        eye_offset_cm=(2.2, 2.55, 0.8), eye_radius_cm=0.26,
        leg_upper_cm=3.0, leg_lower_cm=2.8, leg_radius=0.40, leg_color=(0.15, 0.12, 0.10),
        wing_len_cm=48.0, wing_width_base_cm=7.2, wing_width_tip_cm=1.4,
        wing_dir=(-0.70, 0.60, 0.15),
        tail_len_cm=16.0, tail_radii_cm=[(2.6, 0.5), (0.6, 0.2)], tail_dir=(-1.0, 0.0, 0.0),
        stride_length_cm=8.0, total_length_cm=40.0,
        habitat='Los Dientes', behavior='Planea en térmicas',
        use='Ambiente', diet='piscívoro',
    ),
    dict(
        species='Parrot', seed=5003,
        hip_height_cm=9.0, body_radii_cm=(7.0, 5.5, 6.0),
        body_color=(0.20, 0.65, 0.22), body_dark=(0.09, 0.38, 0.12),
        head_radii_cm=(3.4, 3.0, 3.2), head_offset_cm=(1.6, 0.0, 0.6),
        neck_len_cm=1.8, neck_dir=(0.50, 0.0, 0.75), neck_radii_cm=[2.0, 1.9],
        beak_len_cm=2.0, beak_dir=(0.9, 0.0, -0.4),
        beak_radii_cm=[(1.1, 0.9), (0.3, 0.5)], beak_color=(0.12, 0.10, 0.09),
        eye_offset_cm=(2.0, 2.55, 0.9), eye_radius_cm=0.28,
        leg_upper_cm=2.6, leg_lower_cm=2.4, leg_radius=0.42, leg_color=(0.45, 0.40, 0.30),
        wing_len_cm=19.0, wing_width_base_cm=4.0, wing_width_tip_cm=1.0,
        wing_dir=(-0.75, 0.55, 0.10),
        tail_len_cm=13.0, tail_radii_cm=[(1.8, 0.4), (0.7, 0.15)], tail_dir=(-1.0, 0.0, -0.05),
        stride_length_cm=6.0, total_length_cm=30.0,
        habitat='Selva', behavior='Grupos ruidosos, imitan sonidos',
        use='Plumas, aviso', diet='frugívoro',
    ),
    dict(
        species='Toucan', seed=5004,
        hip_height_cm=11.0, body_radii_cm=(8.0, 6.0, 7.0),
        body_color=(0.08, 0.08, 0.10), body_dark=(0.04, 0.04, 0.05),
        head_radii_cm=(3.6, 3.2, 3.4), head_offset_cm=(1.6, 0.0, 0.6),
        neck_len_cm=1.6, neck_dir=(0.50, 0.0, 0.75), neck_radii_cm=[2.1, 2.0],
        beak_len_cm=15.0, beak_dir=(0.95, 0.0, -0.05),
        beak_radii_cm=[(1.1, 2.2), (0.25, 0.5)], beak_color=(0.95, 0.65, 0.05),
        eye_offset_cm=(2.0, 2.72, 0.9), eye_radius_cm=0.30,
        leg_upper_cm=3.2, leg_lower_cm=3.0, leg_radius=0.48, leg_color=(0.25, 0.45, 0.55),
        wing_len_cm=17.0, wing_width_base_cm=3.6, wing_width_tip_cm=0.9,
        wing_dir=(-0.75, 0.55, 0.10),
        tail_len_cm=10.0, tail_radii_cm=[(1.6, 0.4), (0.6, 0.15)], tail_dir=(-1.0, 0.0, 0.10),
        stride_length_cm=7.0, total_length_cm=32.0,
        habitat='Selva', behavior='Solitario, come fruta',
        use='Ambiente', diet='frugívoro',
    ),
    dict(
        species='Chicken', seed=5005,
        hip_height_cm=13.0, body_radii_cm=(9.5, 7.5, 8.5),
        body_color=(0.72, 0.55, 0.28), body_dark=(0.40, 0.27, 0.12),
        head_radii_cm=(3.2, 2.8, 3.0), head_offset_cm=(1.6, 0.0, 0.6),
        neck_len_cm=4.5, neck_dir=(0.55, 0.0, 0.80), neck_radii_cm=[2.0, 1.9],
        beak_len_cm=1.6, beak_dir=(0.9, 0.0, -0.25),
        beak_radii_cm=[(0.6, 0.5), (0.15, 0.15)], beak_color=(0.85, 0.65, 0.20),
        eye_offset_cm=(1.8, 2.38, 0.8), eye_radius_cm=0.25,
        leg_upper_cm=5.5, leg_lower_cm=5.0, leg_radius=0.62, leg_color=(0.80, 0.65, 0.35),
        wing_len_cm=14.5, wing_width_base_cm=4.4, wing_width_tip_cm=1.1,
        wing_dir=(-0.75, 0.55, 0.05),
        tail_len_cm=9.0, tail_radii_cm=[(2.4, 0.6), (1.0, 0.3)], tail_dir=(-0.6, 0.0, 0.85),
        stride_length_cm=12.0, total_length_cm=38.0,
        habitat='Selva baja', behavior='Huye, se puede domesticar',
        use='Huevos', diet='omnívoro',
    ),
]


def _build_head(species, cfg, body_color_fn, seed):
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    neck_pivot = (body_rx * 0.80, 0.0, hip_h + body_rz * 0.35)
    d_neck = tuple(C.Vector(cfg['neck_dir']).normalized())
    neck = rig.build_capsule_piece('Neck', 'Body', neck_pivot, 'neck', species,
                                    d_neck, cfg['neck_len_cm'], cfg['neck_radii_cm'],
                                    body_color_fn, segments=7)
    head_pivot = tuple(neck_pivot[i] + d_neck[i] * cfg['neck_len_cm'] for i in range(3))

    hx, hy, hz = cfg['head_radii_cm']
    ox, oy, oz = cfg['head_offset_cm']
    head_obj = C.make_skin_blob('Head', tuple(v / 100.0 for v in (ox, oy, oz)),
                                 (hx / 100.0, hy / 100.0, hz / 100.0), subsurf_levels=2)
    eye_off = cfg['eye_offset_cm']
    eyeL, eyeR = eye_off, (eye_off[0], -eye_off[1], eye_off[2])
    head_obj, eye_centers = rig.attach_eyes(head_obj, [eyeL, eyeR], cfg['eye_radius_cm'], seed=seed)
    head_fn = C.with_eye_dots(body_color_fn, [tuple(c) for c in eye_centers],
                               cfg['eye_radius_cm'] / 100.0 * 1.05)
    head = rig.finalize_piece('Head', 'Neck', head_pivot, 'head', head_obj, species, head_fn)

    beak_pivot = (head_pivot[0] + ox * 0.5, 0.0, head_pivot[2] + oz + hz * 0.05)
    beak_fn = C.constant_tint(cfg['beak_color'], alpha=0.0, jitter=0.01)
    beak = rig.build_capsule_piece('Beak', 'Head', beak_pivot, 'beak', species,
                                    tuple(C.Vector(cfg['beak_dir']).normalized()),
                                    cfg['beak_len_cm'], cfg['beak_radii_cm'],
                                    beak_fn, segments=6)
    return [neck, head, beak], head_pivot


def _legs(species, cfg, color_fn):
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    bottom_z = hip_h - body_rz * 0.75
    upper, lower, r = cfg['leg_upper_cm'], cfg['leg_lower_cm'], cfg['leg_radius']
    pieces = []
    for side, sign in (('L', 1.0), ('R', -1.0)):
        hip = (body_rx * 0.05, sign * body_ry * 0.32, bottom_z)
        d_up = tuple(C.Vector((0.08, 0.0, -1.0)).normalized())
        d_lo = tuple(C.Vector((-0.06, 0.0, -1.0)).normalized())
        chain = rig.build_chain(
            [f'Leg{side}_Upper', f'Leg{side}_Lower'], 'Body', hip, 'leg', species,
            directions=[d_up, d_lo], lengths_cm=[upper, lower],
            radii_profiles_cm=[[r, r * 0.75], [r * 0.7, r * 0.4]],
            color_fn=color_fn, segments=6)
        pieces.extend(chain)
    return pieces


def _wings(species, cfg, color_fn):
    """Ala como UNA «hoja» orgánica (rig.build_blade_piece) en vez de una
    cadena de cápsulas: se lee como un ala plegada de verdad -perfil
    curvo, silueta ancha en la base y afilada en la punta- en lugar del
    «palillo» que criticó el encargo. Una sola pieza por ala (no
    upper/lower): ProceduralGait.h anima el aleteo completo con
    WingFlap/WingFold, no por segmento."""
    body_rx, body_ry, body_rz = cfg['body_radii_cm']
    hip_h = cfg['hip_height_cm']
    d = cfg['wing_dir']
    pieces = []
    for side, sign in (('L', 1.0), ('R', -1.0)):
        shoulder = (body_rx * 0.20, sign * body_ry * 0.90, hip_h + body_rz * 0.30)
        d_side = (d[0], d[1] * sign, d[2])
        piece = rig.build_blade_piece(
            f'Wing{side}', 'Body', shoulder, 'wing', species, d_side, (0.0, 1.0, 0.0),
            cfg['wing_len_cm'], cfg['wing_width_base_cm'], cfg['wing_width_tip_cm'],
            cfg['wing_width_base_cm'] * 0.15, color_fn)
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
    body_fn = C.gradient_along_axis(cfg['body_color'], cfg['body_dark'], 'z',
                                     -body_rz / 100.0, body_rz / 100.0, curve=1.3,
                                     jitter=0.02, rnd=rnd)
    body = rig.build_blob_piece('Body', None, body_pivot, 'body', species,
                                 center_offset_cm=(0.0, 0.0, 0.0),
                                 radii_cm=cfg['body_radii_cm'], color_fn=body_fn,
                                 seed=seed, subdivisions=2, noise_strength=0.05)
    pieces.append(body)

    head_pieces, _head_pivot = _build_head(species, cfg, body_fn, seed)
    pieces.extend(head_pieces)

    leg_fn = C.constant_tint(cfg['leg_color'], alpha=0.0, jitter=0.02, rnd=rnd)
    pieces.extend(_legs(species, cfg, leg_fn))

    wing_fn = C.gradient_along_axis(cfg['body_dark'], cfg['body_color'], 'z',
                                     -0.02, 0.02, curve=1.0, jitter=0.02, rnd=rnd)
    pieces.extend(_wings(species, cfg, wing_fn))

    tail_pivot = (-body_rx * 0.90, 0.0, cfg['hip_height_cm'] + body_rz * 0.25)
    tail = rig.build_capsule_piece('Tail', 'Body', tail_pivot, 'tail', species,
                                    tuple(C.Vector(cfg['tail_dir']).normalized()),
                                    cfg['tail_len_cm'], cfg['tail_radii_cm'],
                                    body_fn, segments=6)
    pieces.append(tail)

    locomotion = dict(type=cfg.get('locomotion_type', 'bird'),
                       stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg['hip_height_cm'], total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
