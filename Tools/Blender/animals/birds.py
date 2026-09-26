"""
Tools/Blender/animals/birds.py — gaviota y fragata (encargo 4ª pasada del
autor: fuera TODA fauna que se detenga o se pose — piquero, loro genérico,
Limón y cualquier ave con patas quedan fuera del juego. Solo quedan
bandadas de aves marinas que SIEMPRE están volando lejos, en silueta:
sin patas -nunca se posan, no hace falta- y con las alas en pose ABIERTA
de vuelo (antes plegada de reposo). Constructor paramétrico único (build):
cuerpo (blob), cuello+cabeza+pico y un ala de una sola pieza por lado
(rig.build_blade_piece). Sin esqueleto: cada pieza es rígida, y
run_animals.py añade el canal de color «Anim» que consume el shader de
aleteo en Unreal (anim=flap) — no hay animación en C++."""

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
        wing_len_cm=28.0, wing_width_base_cm=5.2, wing_width_tip_cm=1.2,
        wing_dir=(-0.15, 1.0, 0.05),
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
        wing_len_cm=48.0, wing_width_base_cm=7.2, wing_width_tip_cm=1.4,
        wing_dir=(-0.10, 1.0, 0.10),
        tail_len_cm=16.0, tail_radii_cm=[(2.6, 0.5), (0.6, 0.2)], tail_dir=(-1.0, 0.0, 0.0),
        stride_length_cm=8.0, total_length_cm=40.0,
        habitat='Los Dientes', behavior='Planea en térmicas',
        use='Ambiente', diet='piscívoro',
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
