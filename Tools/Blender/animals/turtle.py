"""
Tools/Blender/animals/turtle.py — tortuga marina (Arenas Blancas, biblia
§6). Caparazón (blob aplanado en cúpula), cuello+cabeza retráctil, cuatro
aletas de 2 segmentos (las delanteras grandes y en forma de ala —el remo
principal al nadar—, las traseras más pequeñas, de timón) y una cola corta.
Locomoción: swimmer —nada la mayor parte del tiempo; el evento de desove en
la playa (biblia §6: «Desova y nada») no se caza y no necesita ciclo de
patas propio—.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import rig  # noqa: E402

CATEGORY = 'turtle'

VARIANTS = [
    dict(
        species='SeaTurtle', seed=5201,
        hip_height_cm=10.0, shell_radii_cm=(45.0, 35.0, 14.0),
        color=(0.22, 0.38, 0.20), color_dark=(0.10, 0.20, 0.10),
        head_radii_cm=(9.0, 7.0, 7.0), head_offset_cm=(4.0, 0.0, 0.5),
        neck_len_cm=10.0, neck_dir=(0.85, 0.0, 0.35), neck_radii_cm=[5.5, 5.0],
        eye_offset_cm=(5.5, 5.95, 1.8), eye_radius_cm=0.7,
        flipper_front_upper_cm=32.0, flipper_front_lower_cm=28.0,
        flipper_front_radii_cm=[[(9.0, 2.2), (7.0, 1.8)], [(7.0, 1.8), (2.5, 1.0)]],
        flipper_front_dir_upper=(0.15, 1.0, -0.15), flipper_front_dir_lower=(-0.10, 0.90, -0.30),
        flipper_back_upper_cm=14.0, flipper_back_lower_cm=12.0,
        flipper_back_radii_cm=[[(4.0, 1.3), (3.0, 1.0)], [(3.0, 1.0), (1.2, 0.5)]],
        flipper_back_dir_upper=(-0.20, 1.0, -0.20), flipper_back_dir_lower=(-0.15, 0.85, -0.35),
        tail_len_cm=6.0, tail_radii_cm=[1.6, 0.6], tail_dir=(-1.0, 0.0, 0.05),
        stride_length_cm=20.0, total_length_cm=110.0,
        habitat='Arenas Blancas', behavior='Desova y nada',
        use='Evento (no se caza)', diet='omnívoro', locomotion_type='swimmer',
    ),
]


def _build_head(species, cfg, body_color_fn, seed):
    rx, ry, rz = cfg['shell_radii_cm']
    hip_h = cfg['hip_height_cm']
    neck_pivot = (rx * 0.85, 0.0, hip_h + rz * 0.55)
    d_neck = tuple(C.Vector(cfg['neck_dir']).normalized())
    neck = rig.build_capsule_piece('Neck', 'Shell', neck_pivot, 'neck', species,
                                    d_neck, cfg['neck_len_cm'], cfg['neck_radii_cm'],
                                    body_color_fn, segments=7)
    head_pivot = tuple(neck_pivot[i] + d_neck[i] * cfg['neck_len_cm'] for i in range(3))

    hx, hy, hz = cfg['head_radii_cm']
    head_obj = C.make_skin_blob('Head', (0.0, 0.0, 0.0), (hx / 100.0, hy / 100.0, hz / 100.0),
                                 subsurf_levels=2)
    eye_off = cfg['eye_offset_cm']
    eyeL, eyeR = eye_off, (eye_off[0], -eye_off[1], eye_off[2])
    head_obj, eye_centers = rig.attach_eyes(head_obj, [eyeL, eyeR], cfg['eye_radius_cm'], seed=seed)
    head_fn = C.with_eye_dots(body_color_fn, [tuple(c) for c in eye_centers],
                               cfg['eye_radius_cm'] / 100.0 * 1.05)
    head = rig.finalize_piece('Head', 'Neck', head_pivot, 'head', head_obj, species, head_fn)
    return [neck, head]


def _flippers(species, cfg, color_fn):
    rx, ry, rz = cfg['shell_radii_cm']
    hip_h = cfg['hip_height_cm']
    pieces = []
    front_du = tuple(C.Vector(cfg['flipper_front_dir_upper']).normalized())
    front_dl = tuple(C.Vector(cfg['flipper_front_dir_lower']).normalized())
    back_du = tuple(C.Vector(cfg['flipper_back_dir_upper']).normalized())
    back_dl = tuple(C.Vector(cfg['flipper_back_dir_lower']).normalized())
    for side, sign in (('L', 1.0), ('R', -1.0)):
        front_hip = (rx * 0.35, sign * ry * 0.80, hip_h + rz * 0.10)
        dirs = [(front_du[0], front_du[1] * sign, front_du[2]),
                (front_dl[0], front_dl[1] * sign, front_dl[2])]
        chain = rig.build_chain(
            [f'FlipperFront{side}_Upper', f'FlipperFront{side}_Lower'], 'Shell', front_hip,
            'flipper', species, directions=dirs,
            lengths_cm=[cfg['flipper_front_upper_cm'], cfg['flipper_front_lower_cm']],
            radii_profiles_cm=cfg['flipper_front_radii_cm'], color_fn=color_fn, segments=7)
        pieces.extend(chain)

        back_hip = (-rx * 0.55, sign * ry * 0.60, hip_h + rz * 0.05)
        dirs_b = [(back_du[0], back_du[1] * sign, back_du[2]),
                  (back_dl[0], back_dl[1] * sign, back_dl[2])]
        chain_b = rig.build_chain(
            [f'FlipperBack{side}_Upper', f'FlipperBack{side}_Lower'], 'Shell', back_hip,
            'flipper', species, directions=dirs_b,
            lengths_cm=[cfg['flipper_back_upper_cm'], cfg['flipper_back_lower_cm']],
            radii_profiles_cm=cfg['flipper_back_radii_cm'], color_fn=color_fn, segments=6)
        pieces.extend(chain_b)
    return pieces


def build(variant):
    cfg = variant
    species = cfg['species']
    seed = cfg['seed']
    rnd = C.seeded_rng(seed)
    pieces = []

    rx, ry, rz = cfg['shell_radii_cm']
    shell_pivot = (0.0, 0.0, cfg['hip_height_cm'])
    shell_fn = C.gradient_along_axis(cfg['color_dark'], cfg['color'], 'z',
                                      -rz / 100.0, rz / 100.0, curve=1.1,
                                      jitter=0.02, rnd=rnd)
    shell = rig.build_blob_piece('Shell', None, shell_pivot, 'shell', species,
                                  center_offset_cm=(0.0, 0.0, 0.0),
                                  radii_cm=cfg['shell_radii_cm'], color_fn=shell_fn,
                                  seed=seed, subdivisions=2, noise_strength=0.04, relax=2)
    pieces.append(shell)

    pieces.extend(_build_head(species, cfg, shell_fn, seed))

    limb_fn = C.constant_tint(cfg['color_dark'], alpha=0.0, jitter=0.02, rnd=rnd)
    pieces.extend(_flippers(species, cfg, limb_fn))

    tail_pivot = (-rx * 0.95, 0.0, cfg['hip_height_cm'] + rz * 0.05)
    tail = rig.build_capsule_piece('Tail', 'Shell', tail_pivot, 'tail', species,
                                    tuple(C.Vector(cfg['tail_dir']).normalized()),
                                    cfg['tail_len_cm'], cfg['tail_radii_cm'],
                                    limb_fn, segments=5)
    pieces.append(tail)

    locomotion = dict(type=cfg.get('locomotion_type', 'swimmer'),
                       stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg['hip_height_cm'], total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
